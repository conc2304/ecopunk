#!/usr/bin/env python3
"""Offline tests for pi_validation.py (campaign orchestrator). Dummy
processes and fixture /proc files only -- never the ExperienceRuntime binary.

  python3 scripts/rt004/test_pi_validation.py -v
"""

import glob
import json
import os
import shutil
import signal
import subprocess
import sys
import tarfile
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ORCH = os.path.join(HERE, "pi_validation.py")
sys.path.insert(0, HERE)
import pi_validation as pv  # noqa: E402

MEMINFO = "MemAvailable: %d kB\nSwapFree: 501760 kB\nCmaTotal: 262144 kB\nCmaFree: %d kB\n"
FAKE_TESTS_OK = "python3 -c \"import sys; sys.stderr.write('Ran 21 tests in 1s\\n\\nOK\\n')\""


class OrchestratorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="pv-test-")
        self.proc = os.path.join(self.tmp, "proc")
        self.runs = os.path.join(self.tmp, "runs")
        os.makedirs(self.proc)
        self.set_meminfo(600, 160)
        with open(os.path.join(self.proc, "status"), "w") as f:
            f.write("VmRSS: 122880 kB\nRssAnon: 81920 kB\nRssFile: 30720 kB\nRssShmem: 10240 kB\nVmSwap: 0 kB\n")
        self.binary = os.path.join(self.tmp, "experience_runtime_fixture_binary")
        with open(self.binary, "w") as f:
            f.write("fixture")
        time.sleep(0.05)
        self.prov = os.path.join(self.tmp, "PROVENANCE.json")
        with open(self.prov, "w") as f:
            json.dump({"git_head": "0" * 40, "dirty": False}, f)
        os.utime(self.binary, None)  # binary newer than provenance
        self.media = os.path.join(self.tmp, "media")
        os.makedirs(self.media)
        open(os.path.join(self.media, "a.mp4"), "w").close()
        self.marker = "rt004-pv-fixture-marker-%d" % os.getpid()
        self.escapees = []

    def tearDown(self):
        for p in self.escapees:
            try:
                os.kill(p, signal.SIGKILL)
            except ProcessLookupError:
                pass
        shutil.rmtree(self.tmp, ignore_errors=True)

    def set_meminfo(self, avail_mib, cma_mib):
        with open(os.path.join(self.proc, "meminfo.tmp"), "w") as f:
            f.write(MEMINFO % (avail_mib * 1024, cma_mib * 1024))
        os.replace(os.path.join(self.proc, "meminfo.tmp"), os.path.join(self.proc, "meminfo"))

    def config(self, **over):
        cfg = {"proc_root": self.proc, "thresholds": {"wall_max_s": 20}, "assume_measurements_ok": True,
               "dummy_cmds": {"default": "sh -c 'sleep 1'"}, "process_marker": self.marker,
               "preflight": {"binary": self.binary, "provenance": self.prov, "media_dir": self.media, "display": ":0",
                             "xrandr": "HDMI-1 connected primary 1280x720+0+0\n   1280x720      60.00*+\n",
                             "throttled": "throttled=0x50000", "temp": "temp=48.3'C", "gpu_mem": "gpu=76M",
                             "offline_tests_cmd": FAKE_TESTS_OK}}
        for k, v in over.items():
            if k == "preflight":
                cfg["preflight"].update(v)
            else:
                cfg[k] = v
        path = os.path.join(self.tmp, "fixture-%d.json" % len(os.listdir(self.tmp)))
        with open(path, "w") as f:
            json.dump(cfg, f)
        return path

    def orchestrate(self, cfg, *extra, timeout=240, signal_after=None, stop_after=None):
        p = subprocess.Popen([sys.executable, ORCH, "run", "--fixture", cfg, "--run-root", self.runs] + list(extra),
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        if signal_after or stop_after:
            time.sleep(signal_after or stop_after)
            if signal_after:
                p.send_signal(signal.SIGINT)
            else:
                d = [d for d in glob.glob(os.path.join(self.runs, "campaign-*")) if os.path.isdir(d)][0]
                open(os.path.join(d, "STOP"), "w").close()
        out, err = p.communicate(timeout=timeout)
        lines = out.splitlines()
        self.assertTrue(lines, "no output; stderr=%s" % err)
        cdir = lines[0]
        with open(os.path.join(cdir, "scenario-results.json")) as f:
            res = json.load(f)
        by = {r["id"]: r for r in res["results"]}
        return p.returncode, cdir, res, by, out

    # ------------------------------------------------------------------ tests
    def test_plan_launches_nothing(self):
        out = subprocess.run([sys.executable, ORCH, "plan", "--run-root", self.runs], stdout=subprocess.PIPE, universal_newlines=True).stdout
        self.assertIn("nothing is launched", out)
        self.assertIn("S9b", out)
        self.assertFalse(os.path.exists(self.runs))

    def test_preflight_rejects_active_throttling(self):
        rc, cdir, res, by, _ = self.orchestrate(self.config(preflight={"throttled": "throttled=0x50005"}))
        self.assertEqual(rc, 3)
        self.assertIn("throttled", res["abort_reason"])
        self.assertTrue(all(r["status"] == "NOT RUN" for r in res["results"]))
        self.assertEqual(glob.glob(os.path.join(cdir, "S*")), [], "no scenario may launch")

    def test_preflight_rejects_skipped_or_missing_offline_tests(self):
        cfg = self.config(preflight={"offline_tests_cmd": "python3 -c \"import sys; sys.stderr.write('Ran 21 tests\\n\\nOK (skipped=2)\\n')\""})
        rc, _, res, _, _ = self.orchestrate(cfg)
        self.assertEqual(rc, 3)
        self.assertIn("offline_watchdog_tests", res["abort_reason"])

    def test_preflight_rejects_unknown_binary_identity(self):
        os.remove(self.prov)
        rc, _, res, _, _ = self.orchestrate(self.config())
        self.assertEqual(rc, 3)
        self.assertIn("provenance", res["abort_reason"])

    def test_preflight_rejects_running_runtime(self):
        other = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(300)", self.marker])
        try:
            rc, _, res, _, _ = self.orchestrate(self.config())
            self.assertEqual(rc, 3)
            self.assertIn("no_runtime_running", res["abort_reason"])
            self.assertIsNone(other.poll(), "orchestrator must not signal an unrelated runtime")
        finally:
            other.kill()
            other.wait()

    def test_ordered_supervised_execution_and_outputs(self):
        rc, cdir, res, by, out = self.orchestrate(self.config(), "--only", "S1,S2,S7")
        self.assertEqual(rc, 0, out)
        ids = [r["id"] for r in res["results"]]
        self.assertEqual(ids, [s["id"] for s in pv.SCENARIOS], "results in required order")
        for sid in ("S1", "S2", "S7"):
            self.assertEqual(by[sid]["status"], "PASS")
            self.assertEqual(by[sid]["stop_class"], "completed")
            self.assertTrue(os.path.isfile(os.path.join(by[sid]["run_dir"], "manifest.json")), "launched through the supervisor")
            self.assertIn("recovered", by[sid]["recovery"])
        self.assertEqual(by["S3"]["status"], "NOT RUN")
        for name in ("summary.md", "scenario-results.json", "metrics.csv", "watchdog.tsv", "not-run.md", "manifest.json", "preflight.json", "status.json", "offline-tests.log"):
            self.assertTrue(os.path.isfile(os.path.join(cdir, name)), name)
        with open(os.path.join(cdir, "summary.md")) as f:
            self.assertIn("RT-004 PI FOLLOW-UP REQUIRED", f.read(), "static coverage gaps forbid COMPLETE")
        with tarfile.open(cdir.rstrip("/") + ".tar.gz") as tar:
            self.assertTrue(any(n.endswith("summary.md") for n in tar.getnames()))

    def _safety_case(self, dummy, thresholds=None, extra_cfg=None):
        cfg = {"dummy_cmds": {"default": "sh -c 'sleep 1'", "S2": dummy}}
        if thresholds:
            cfg["thresholds"] = thresholds
        cfg.update(extra_cfg or {})
        rc, cdir, res, by, out = self.orchestrate(self.config(**cfg), "--only", "S1,S2,S3")
        self.assertEqual(rc, 2, out)
        self.assertEqual(by["S1"]["status"], "PASS")
        self.assertEqual(by["S2"]["status"], "FAIL")
        self.assertEqual(by["S3"]["status"], "NOT RUN")
        self.assertIn("campaign aborted", " ".join(by["S3"]["notes"]))
        self.assertEqual(len([d for d in glob.glob(os.path.join(cdir, "S2", "*")) if os.path.isdir(d)]), 1, "no automatic retry")
        self.assertEqual(glob.glob(os.path.join(cdir, "S3")), [], "nothing launched after abort")
        return by["S2"]

    def test_cma_floor_stop_aborts_campaign(self):
        dummy = "sh -c 'sleep 1; printf \"MemAvailable: 614400 kB\\nSwapFree: 1 kB\\nCmaTotal: 262144 kB\\nCmaFree: 30720 kB\\n\" > %s/meminfo; sleep 600'" % self.proc
        r = self._safety_case(dummy)
        self.assertEqual(r["stop_class"], "safety")
        self.assertIn("cma_free<=40MiB_floor", r["stop_reason"])

    def test_rss_stop_aborts_campaign(self):
        dummy = "sh -c 'sleep 1; printf \"VmRSS: 317440 kB\\n\" > %s/status; sleep 600'" % self.proc
        r = self._safety_case(dummy)
        self.assertIn("rss>=300MiB", r["stop_reason"])

    def test_mem_available_stop_aborts_campaign(self):
        dummy = "sh -c 'sleep 1; printf \"MemAvailable: 204800 kB\\nSwapFree: 1 kB\\nCmaTotal: 262144 kB\\nCmaFree: 163840 kB\\n\" > %s/meminfo; sleep 600'" % self.proc
        r = self._safety_case(dummy)
        self.assertIn("mem_available<=256MiB", r["stop_reason"])

    def test_ceiling_stop_aborts_campaign(self):
        r = self._safety_case("sleep 600", thresholds={"wall_max_s": 3})
        self.assertEqual(r["stop_class"], "ceiling")

    def test_log_cap_aborts_campaign(self):
        r = self._safety_case("sh -c 'head -c 40000000 /dev/zero; sleep 600'")
        self.assertIn("log_volume", r["stop_reason"])

    def test_crash_aborts_campaign(self):
        r = self._safety_case("sh -c 'sleep 1; kill -SEGV $$'")
        self.assertEqual(r["stop_class"], "crash")

    def test_missing_measurement_is_inconclusive_and_aborts(self):
        rc, _, res, by, _ = self.orchestrate(self.config(assume_measurements_ok=False), "--only", "S1,S2")
        self.assertEqual(rc, 2)
        self.assertEqual(by["S1"]["status"], "INCONCLUSIVE")
        self.assertTrue(by["S1"]["missing"])
        self.assertEqual(by["S2"]["status"], "NOT RUN")

    def test_unexpected_process_survival_aborts_without_killing(self):
        esc = os.path.join(self.tmp, "escape.py")
        with open(esc, "w") as f:
            f.write("import os, sys, time\nif os.fork() == 0:\n    os.setsid()\n    open(sys.argv[2], 'w').write(str(os.getpid()))\n    time.sleep(120)\n")
        pidfile = os.path.join(self.tmp, "escapee.pid")
        dummy = "%s %s %s %s" % (sys.executable, esc, self.marker, pidfile)
        rc, _, res, by, _ = self.orchestrate(self.config(dummy_cmds={"default": "sh -c 'sleep 1'", "S1": dummy}), "--only", "S1,S2")
        with open(pidfile) as f:
            self.escapees.append(int(f.read()))
        self.assertEqual(rc, 2)
        self.assertIn("unexpected runtime process survival", res["abort_reason"])
        self.assertEqual(by["S2"]["status"], "NOT RUN")
        os.kill(self.escapees[0], 0)  # still alive: orchestrator reports, never kills unrelated pids

    def test_sigint_interrupt_reports_and_cleans(self):
        rc, cdir, res, by, _ = self.orchestrate(self.config(dummy_cmds={"default": "sleep 600"}), "--only", "S1,S2", signal_after=7)
        self.assertEqual(rc, 2)
        self.assertEqual(by["S1"]["stop_class"], "interrupted")
        self.assertEqual(by["S2"]["status"], "NOT RUN")
        with open(os.path.join(cdir, "status.json")) as f:
            self.assertEqual(json.load(f)["state"], "finished")
        with open(os.path.join(by["S1"]["run_dir"], "watchdog.tsv")) as f:
            pid = int([l for l in f if "\tlaunch\t" in l][0].split("pid=")[1].split()[0])
        with self.assertRaises(ProcessLookupError):
            os.kill(pid, 0)

    def test_stop_file_interrupts(self):
        rc, _, res, by, _ = self.orchestrate(self.config(dummy_cmds={"default": "sleep 600"}), "--only", "S1,S2", stop_after=7)
        self.assertEqual(rc, 2)
        self.assertEqual(by["S1"]["stop_reason"], "operator_stop_file")
        self.assertEqual(by["S2"]["status"], "NOT RUN")

    def test_collector_failure_aborts(self):
        cfg = self.config(dummy_cmds={"default": "sleep 600"})
        with open(cfg) as f:
            c = json.load(f)
        c["thresholds"] = {"wall_max_s": 30}
        with open(cfg, "w") as f:
            json.dump(c, f)
        # inject via supervisor fixture flag: emulate through a fixture-only scenario command
        orig = pv.Campaign.supervisor_cmd

        def patched(self_, sc, scen_dir):
            return orig(self_, sc, scen_dir) + ["--collector-fail-after", "2"]
        # run in-process so the patch applies
        pv.Campaign.supervisor_cmd = patched
        saved = {sg: signal.getsignal(sg) for sg in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
        try:
            class A:
                pass
            a = A()
            a.app_dir, a.run_root, a.only, a.continue_on_functional_failure = ".", self.runs, ["S1", "S2"], False
            with open(cfg) as f:
                fx = json.load(f)
            code = pv.Campaign(a, fx).run()
        finally:
            pv.Campaign.supervisor_cmd = orig
            for sg, h in saved.items():
                signal.signal(sg, h)
        self.assertEqual(code, 2)
        cdir = [d for d in glob.glob(os.path.join(self.runs, "campaign-*")) if os.path.isdir(d)][0]
        with open(os.path.join(cdir, "scenario-results.json")) as f:
            by = {r["id"]: r for r in json.load(f)["results"]}
        self.assertIn("collector_stale_or_failed", by["S1"]["stop_reason"])
        self.assertEqual(by["S2"]["status"], "NOT RUN")

    # ---- parsers / evaluation on real line formats --------------------------
    def test_evaluate_real_formats(self):
        d = os.path.join(self.tmp, "run")
        os.makedirs(d)
        with open(os.path.join(d, "driver.log"), "w") as f:
            f.write("UPLOAD-SPLIT 640x360 RGB internalFormat=0x8051 bytes=691200 n=300 meanMs=6.34795 p50Ms=6.1 p95Ms=7.2 p99Ms=9.9 glError=0x0\n"
                    "UPLOAD-SPLIT 640x360 RGBA internalFormat=0x8058 bytes=921600 n=300 meanMs=1.91884 p50Ms=1.8 p95Ms=2.2 p99Ms=3.1 glError=0x0\n"
                    "ACTION t=1.0s probeFrame=0 exit\n")
        status, m, notes, missing = pv.evaluate("S7", d, {"stop_class": "completed"}, None)
        self.assertEqual(status, "PASS", (notes, missing))
        self.assertAlmostEqual(m["rgb_over_rgba_mean"], 3.31, places=2)

        d2 = os.path.join(self.tmp, "run2")
        os.makedirs(d2)
        with open(os.path.join(d2, "app-stdout.log"), "w") as f:
            f.write("[notice ] TFAmbientTextureLayer: residency=bounded pairs=19 tintTextures=19 maskTextures=0 cpuTintMiB=0.0 cpuMaskMiB=0.0 gpuEstMiB=85.5 (rgb-padded 114.0) tintPixelsKept=0 rssBeforeMiB=102 rssAfterMiB=99 glError=0x00000500\n")
        with open(os.path.join(d2, "driver.log"), "w") as f:
            for i in (1, 2, 3):
                f.write("MASK-CYCLE %d masks=0->1->0 loaded=1 released=1 loadMs=20.1 releaseMs=0.02 glErrorLoad=0x0 glErrorRelease=0x0\n" % i)
            f.write("ACTION t=8.0s probeFrame=200 exit\n")
        status, m, notes, missing = pv.evaluate("S4a", d2, {"stop_class": "completed"}, None)
        self.assertEqual(status, "PASS", (notes, missing))
        self.assertEqual(m["residency"]["mask"], "0")

        d3 = os.path.join(self.tmp, "run3")
        os.makedirs(d3)
        with open(os.path.join(d3, "stall-probe.log"), "w") as f:
            f.write("  NOTE frame=3 mono=100.000 t=1ms driver.action: ab-default\n"
                    "\n=== RT-004 SUMMARY frames=900 longFrames=0 thresholdMs=200 ===\n"
                    "window,TimeOffsetVideoBuffer.playheadUpload@ab-default,500,3300,6.6,7.0,8.0,9.0\n"
                    "window,TimeOffsetVideoBuffer.playheadUpload@ab-toggled,520,3432,6.6,7.1,8.1,9.1\n"
                    "windowCounter,TOVB.update.calls@ab-default,90\nwindowCounter,TOVB.playhead.upload@ab-default,480\n"
                    "windowCounter,TOVB.playhead.uploadSkippedRedundant@ab-default,20\nwindowCounter,TOVB.update.calls@ab-toggled,88\n"
                    "windowCounter,TOVB.playhead.upload@ab-toggled,520\n"
                    "frameIntervalMs[owner=temporal-fields/ab-default] n=90 p50=99.1 p95=110.2 p99=130.3 max=140\n"
                    "frameIntervalMs[owner=temporal-fields/ab-toggled] n=88 p50=101.1 p95=112.2 p99=133.3 max=150\n")
        with open(os.path.join(d3, "driver.log"), "w") as f:
            f.write("ACTION t=26.0s probeFrame=900 exit\n")
        status, m, notes, missing = pv.evaluate("S6", d3, {"stop_class": "completed"}, None)
        self.assertEqual(status, "PASS", (notes, missing))
        self.assertEqual(m["uploads_per_frame@ab-default"], 5.33)
        self.assertTrue(any("low confidence" in n for n in notes))


if __name__ == "__main__":
    unittest.main()
