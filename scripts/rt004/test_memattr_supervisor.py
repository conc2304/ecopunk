#!/usr/bin/env python3
"""Offline tests for memattr_supervisor.py. Only dummy processes (sleep/sh)
against fixture /proc files -- never the ExperienceRuntime binary.

  python3 scripts/rt004/test_memattr_supervisor.py -v
"""

import csv
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SUP = os.path.join(HERE, "memattr_supervisor.py")
sys.path.insert(0, HERE)
import memattr_supervisor as ms  # noqa: E402

HEALTHY_MEMINFO = {"MemAvailable": 600 * 1024, "SwapFree": 490 * 1024, "CmaTotal": 256 * 1024, "CmaFree": 160 * 1024}
HEALTHY_STATUS = {"VmRSS": 120 * 1024, "RssAnon": 80 * 1024, "RssFile": 30 * 1024, "RssShmem": 10 * 1024, "VmSwap": 0}


def write_kb(path, values):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        for k, v in values.items():
            f.write("%s:%12d kB\n" % (k, v))
    os.replace(tmp, path)  # atomic: the supervisor never reads a half-written file


def pid_alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


def group_alive(pgid):
    try:
        os.killpg(pgid, 0)
        return True
    except (ProcessLookupError, PermissionError):
        return False


class SupervisorFixtureTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="memattr-test-")
        self.proc_root = os.path.join(self.tmp, "proc")
        self.run_root = os.path.join(self.tmp, "runs")
        os.makedirs(self.proc_root)
        write_kb(os.path.join(self.proc_root, "meminfo"), HEALTHY_MEMINFO)
        write_kb(os.path.join(self.proc_root, "status"), HEALTHY_STATUS)
        write_kb(os.path.join(self.proc_root, "smaps_rollup"), {"Rss": 120 * 1024, "Pss": 100 * 1024, "Anonymous": 80 * 1024})

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    # -- helpers ---------------------------------------------------------------
    def start(self, cmd, thresholds=None, extra=()):
        args = [sys.executable, SUP, "fixture", "--cmd", cmd, "--proc-root", self.proc_root, "--run-root", self.run_root]
        if thresholds:
            args += ["--thresholds", json.dumps(thresholds)]
        args += list(extra)
        return subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def finish(self, sup, timeout=60):
        out, err = sup.communicate(timeout=timeout)
        self.assertEqual(sup.returncode, 0, "supervisor failed: %s" % err)
        run_dir = out.splitlines()[0].strip()
        with open(os.path.join(run_dir, "manifest.json")) as f:
            manifest = json.load(f)
        with open(os.path.join(run_dir, "watchdog.tsv")) as f:
            events = list(csv.DictReader(f, delimiter="\t"))
        return run_dir, manifest, events

    def launched_pid(self, events):
        for e in events:
            if e["event"] == "launch":
                return int(e["detail"].split("pid=")[1].split()[0])
        return None

    def run_fixture(self, cmd, thresholds=None, extra=(), mutate=None):
        sup = self.start(cmd, thresholds, extra)
        if mutate:
            mutate(sup)
        return self.finish(sup)

    # -- tests -----------------------------------------------------------------
    def test_wall_clock_timeout_kills_dummy(self):
        run_dir, m, ev = self.run_fixture("sleep 600", {"wall_max_s": 3})
        self.assertTrue(m["stop_reason"].startswith("wall_clock"), m["stop_reason"])
        pid = self.launched_pid(ev)
        self.assertIsNotNone(pid)
        self.assertFalse(pid_alive(pid))
        self.assertNotEqual(m["app_returncode"], 0)

    def test_cma_absolute_floor_crossing(self):
        def mutate(_sup):
            time.sleep(5)  # 3 s pre-launch + 2 s running
            write_kb(os.path.join(self.proc_root, "meminfo"), dict(HEALTHY_MEMINFO, CmaFree=39 * 1024))
        _, m, ev = self.run_fixture("sleep 600", {"wall_max_s": 30}, mutate=mutate)
        self.assertTrue(m["stop_reason"].startswith("cma_free<=40MiB_floor"), m["stop_reason"])
        self.assertFalse(pid_alive(self.launched_pid(ev)))

    def test_startup_dip_below_preflight_but_above_floor_does_not_stop(self):
        # The measured Blob/ambient startup level (~89 MiB) must not trip the
        # run-time watchdog; only the floor (or a baselined decline) may.
        def mutate(_sup):
            time.sleep(4.5)
            write_kb(os.path.join(self.proc_root, "meminfo"), dict(HEALTHY_MEMINFO, CmaFree=60 * 1024))
        _, m, _ = self.run_fixture("sleep 600", {"wall_max_s": 9}, mutate=mutate)
        self.assertTrue(m["stop_reason"].startswith("wall_clock"), m["stop_reason"])
        self.assertIsNone(m["cma_baseline_mib"], "no marker -> floor only")

    def test_floor_is_continuous_from_launch(self):
        # No startup grace: a floor crossing in the first watchdog ticks stops.
        def mutate(_sup):
            time.sleep(3.6)  # just after launch (3 x 1 s pre-launch samples)
            write_kb(os.path.join(self.proc_root, "meminfo"), dict(HEALTHY_MEMINFO, CmaFree=30 * 1024))
        _, m, ev = self.run_fixture("sleep 600", {"wall_max_s": 30}, mutate=mutate)
        self.assertTrue(m["stop_reason"].startswith("cma_free<=40MiB_floor"), m["stop_reason"])
        self.assertIsNotNone(self.launched_pid(ev))
        stop_mono = float([e for e in ev if e["event"] == "stop"][0]["mono_s"])
        self.assertLess(stop_mono - m["launch_mono_s"], 1.5, "floor must apply from the first watchdog ticks")

    def test_relative_decline_from_stabilized_baseline(self):
        marker = "sh -c 'sleep 1; echo \"ACTION t=0s probeFrame=0 blob-baseline\" >> \"$MEMATTR_RUN_DIR/driver.log\"; sleep 600'"
        def mutate(_sup):
            time.sleep(3.5)
            write_kb(os.path.join(self.proc_root, "meminfo"), dict(HEALTHY_MEMINFO, CmaFree=100 * 1024))
            time.sleep(6)  # marker at ~1 s after launch + 3 s baseline window
            write_kb(os.path.join(self.proc_root, "meminfo"), dict(HEALTHY_MEMINFO, CmaFree=51 * 1024))
        _, m, ev = self.run_fixture(marker, {"wall_max_s": 30}, mutate=mutate)
        self.assertAlmostEqual(m["cma_baseline_mib"], 100.0, places=1)
        self.assertTrue(m["stop_reason"].startswith("cma_decline>=48MiB_from_baseline_100.0"), m["stop_reason"])
        self.assertTrue(any(e["event"] == "cma_baseline" for e in ev))

    def test_mem_available_threshold_crossing(self):
        def mutate(_sup):
            time.sleep(5)
            write_kb(os.path.join(self.proc_root, "meminfo"), dict(HEALTHY_MEMINFO, MemAvailable=200 * 1024))
        _, m, _ = self.run_fixture("sleep 600", {"wall_max_s": 30}, mutate=mutate)
        self.assertTrue(m["stop_reason"].startswith("mem_available<=256MiB"), m["stop_reason"])

    def test_rss_threshold_crossing(self):
        def mutate(_sup):
            time.sleep(5)
            write_kb(os.path.join(self.proc_root, "status"), dict(HEALTHY_STATUS, VmRSS=310 * 1024))
        _, m, _ = self.run_fixture("sleep 600", {"wall_max_s": 30}, mutate=mutate)
        self.assertTrue(m["stop_reason"].startswith("rss>=300MiB"), m["stop_reason"])

    def test_preflight_refuses_launch_when_already_below_threshold(self):
        write_kb(os.path.join(self.proc_root, "meminfo"), dict(HEALTHY_MEMINFO, CmaFree=89 * 1024))
        _, m, ev = self.run_fixture("sleep 600")
        self.assertTrue(m["stop_reason"].startswith("preflight_refused: preflight_cma_free<=96MiB"), m["stop_reason"])
        self.assertIsNone(self.launched_pid(ev), "must not launch")

    def test_missing_required_metric_refuses(self):
        meminfo = dict(HEALTHY_MEMINFO)
        del meminfo["CmaFree"]
        write_kb(os.path.join(self.proc_root, "meminfo"), meminfo)
        run_dir, m, _ = self.run_fixture("sleep 600")
        self.assertIn("metric_unavailable:cma_free_mib", m["stop_reason"])
        with open(os.path.join(run_dir, "samples.tsv")) as f:
            rows = list(csv.DictReader(f, delimiter="\t"))
        self.assertTrue(all(r["meminfo_cmafree_kb"] == "unavailable" for r in rows))

    def test_collector_failure_stops_run(self):
        _, m, ev = self.run_fixture("sleep 600", {"wall_max_s": 30}, extra=["--collector-fail-after", "2"])
        self.assertTrue(m["stop_reason"].startswith("collector_stale_or_failed"), m["stop_reason"])
        self.assertFalse(pid_alive(self.launched_pid(ev)))

    def test_term_ignoring_dummy_is_killed(self):
        _, m, ev = self.run_fixture("sh -c \"trap '' TERM; sleep 600\"", {"wall_max_s": 3})
        details = " ".join(e["detail"] for e in ev if e["event"] == "signal")
        self.assertIn("SIGKILL", details)
        self.assertFalse(pid_alive(self.launched_pid(ev)))

    def test_grandchildren_cleaned_and_unrelated_survives(self):
        unrelated = subprocess.Popen(["sleep", "600"], start_new_session=True)
        try:
            _, m, ev = self.run_fixture("sh -c 'sleep 600 & sleep 600'", {"wall_max_s": 3})
            pgid = self.launched_pid(ev)
            time.sleep(0.2)
            self.assertFalse(group_alive(pgid), "app process group must be empty")
            self.assertTrue(pid_alive(unrelated.pid), "unrelated process must survive")
        finally:
            unrelated.kill()
            unrelated.wait()

    def test_app_exit_is_reported(self):
        _, m, _ = self.run_fixture("sh -c 'sleep 1; exit 3'", {"wall_max_s": 30})
        self.assertEqual(m["stop_reason"], "app_exited returncode=3")
        self.assertEqual(m["app_returncode"], 3)

    def test_operator_stop_file(self):
        def mutate(sup):
            time.sleep(4.5)
            runs = os.listdir(self.run_root)
            open(os.path.join(self.run_root, runs[0], "STOP"), "w").close()
        _, m, _ = self.run_fixture("sleep 600", {"wall_max_s": 30}, mutate=mutate)
        self.assertEqual(m["stop_reason"], "operator_stop_file")

    def test_supervisor_sigint_cleans_up(self):
        def mutate(sup):
            time.sleep(4.5)
            sup.send_signal(signal.SIGINT)
        _, m, ev = self.run_fixture("sleep 600", {"wall_max_s": 30}, mutate=mutate)
        self.assertEqual(m["stop_reason"], "supervisor_signal:SIGINT")
        self.assertFalse(pid_alive(self.launched_pid(ev)))

    def test_log_volume_cap(self):
        _, m, _ = self.run_fixture("sh -c 'head -c 40000000 /dev/zero; sleep 600'", {"wall_max_s": 30})
        self.assertTrue(m["stop_reason"].startswith("log_volume"), m["stop_reason"])

    def test_schema_monotonic_flush_and_unique_dirs(self):
        sup = self.start("sleep 600", {"wall_max_s": 6})
        time.sleep(4.5)
        run_dir = os.path.join(self.run_root, os.listdir(self.run_root)[0])
        with open(os.path.join(run_dir, "samples.tsv")) as f:
            mid_rows = list(csv.DictReader(f, delimiter="\t"))
        self.assertGreaterEqual(len(mid_rows), 3, "rows must be flushed while running")
        run_dir2, m, _ = self.finish(sup)
        self.assertEqual(run_dir, run_dir2)
        with open(os.path.join(run_dir, "samples.tsv")) as f:
            header = f.readline().rstrip("\n").split("\t")
            f.seek(0)
            rows = list(csv.DictReader(f, delimiter="\t"))
        self.assertEqual(header, ms.COLUMNS)
        monos = [float(r["mono_s"]) for r in rows]
        self.assertEqual(monos, sorted(monos))
        self.assertTrue(all(b > a for a, b in zip(monos, monos[1:])))
        phases = [r["phase"] for r in rows]
        self.assertEqual(phases[:3], ["prelaunch"] * 3)
        self.assertIn("running", phases)
        self.assertEqual(phases[-1], "postexit")
        self.assertTrue(all(r["drm_bo_stats"] == "unavailable" for r in rows), "no fixture dri -> unavailable, not 0")
        self.assertTrue(all(r[c] != "" for r in rows for c in ms.COLUMNS))
        self.assertLess(m["samples_written"], 30, "bounded volume: ~1 row/s")
        # second run -> distinct directory
        _, m2, _ = self.run_fixture("sh -c 'exit 0'", {"wall_max_s": 5})
        self.assertNotEqual(m["run_id"], m2["run_id"])
        self.assertEqual(len(os.listdir(self.run_root)), 2)

    def test_fixture_refuses_runtime_binary(self):
        sup = self.start("./bin/experience_runtime")
        _, err = sup.communicate(timeout=30)
        self.assertNotEqual(sup.returncode, 0)
        self.assertIn("refuses to launch the runtime", err)

    def test_real_thresholds_are_the_architecture_values(self):
        self.assertEqual(ms.REAL_THRESHOLDS, {"preflight_cma_free_min_mib": 96.0, "cma_free_floor_mib": 40.0, "cma_max_decline_mib": 48.0,
                                              "mem_available_min_mib": 256.0, "rss_max_mib": 300.0, "wall_max_s": 45.0})

    # -- Linux-only (run on the Pi): real /proc and parent-death protection ----
    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux /proc required (run on the Pi)")
    def test_linux_real_proc_parsing_and_identity(self):
        src = ms.ProcSource()
        st = src.status(os.getpid())
        self.assertIn("VmRSS", st)
        self.assertIn("RssAnon", st)
        mem = src.meminfo()
        self.assertIn("MemAvailable", mem)
        self.assertIsNotNone(src.starttime(os.getpid()))
        child = subprocess.Popen(["sleep", "30"])
        try:
            st1 = src.starttime(child.pid)
            self.assertIsNotNone(st1)
            self.assertEqual(st1, src.starttime(child.pid))
        finally:
            child.kill()
            child.wait()
        print("\n  [linux] CmaFree present: %s; drm: %s" % ("CmaFree" in mem, src.drm()))

    @unittest.skipUnless(sys.platform.startswith("linux"), "PR_SET_PDEATHSIG is Linux-only")
    def test_linux_app_dies_with_killed_supervisor(self):
        sup = self.start("sleep 600", {"wall_max_s": 60})
        time.sleep(4.5)
        run_dir = os.path.join(self.run_root, os.listdir(self.run_root)[0])
        with open(os.path.join(run_dir, "watchdog.tsv")) as f:
            events = list(csv.DictReader(f, delimiter="\t"))
        pid = self.launched_pid(events)
        self.assertTrue(pid_alive(pid))
        sup.kill()  # SIGKILL: no cleanup code runs in the supervisor
        sup.communicate(timeout=10)
        deadline = time.time() + 5
        while pid_alive(pid) and time.time() < deadline:
            time.sleep(0.1)
        self.assertFalse(pid_alive(pid), "PR_SET_PDEATHSIG must kill the orphaned app")


if __name__ == "__main__":
    unittest.main()
