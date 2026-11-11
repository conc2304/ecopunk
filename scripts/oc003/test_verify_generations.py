#!/usr/bin/env python3
"""Tests for verify_generations.py and sysmon.py (synthetic runs only; no runtime). Run: python3 -I -W error scripts/oc003/test_verify_generations.py"""
import csv
import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sysmon  # noqa: E402
import verify_generations as vg  # noqa: E402

SCENES = ["blob-region-prototype", "temporal-fields", "blueprint_emergence", "fragment-trail", "quadrant-crosshair"]


def write_run(d, tag, scenes=SCENES, mode="fxGpu=on rawGpu=on cpuRaw=on palette=on", raw_skip=None, drift_bad=False, fx_gens=None):
    with open(os.path.join(d, tag + ".frames.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["frame", "scene", "exclude"])
        n = 0
        for s in scenes:
            for _ in range(5):
                w.writerow([n, s, 0]); n += 1
    lines = [f"[notice ] AcquisitionMode: {mode} predominant=on contract=oc003f-1"]
    gens = fx_gens or list(range(1, len(scenes) + 1))
    for g in gens:
        lines.append(f"[notice ] SceneColor: generation {g} begins: scene={SCENES[(g - 1) % 5]} (previous generation held 3 samples; cleared)")
        if raw_skip != g:
            lines.append(f"[notice ] VideoColor: generation {g} begins: media=media.auto.1 activation=1 scene_gen={g} verdict=1")
            lines.append(f"[notice ] CpuRaw: generation {g} begins: media=media.auto.1 activation=1 scene_gen={g} verdict=1")
    open(os.path.join(d, tag + ".stdout.log"), "w").write("\n".join(lines) + "\n")
    pal = os.path.join(d, tag + ".palette"); os.makedirs(pal, exist_ok=True)
    for stream, fn in vg.STREAM_FILES.items():
        with open(os.path.join(pal, fn), "w", newline="") as fh:
            w = csv.writer(fh); w.writerow(["stream", "gen", "t", "idx", "driftHue"])
            i = 0
            for g in gens:
                for k in range(3):
                    dh = "nan" if (k == 0 and not (drift_bad and g == 2)) else "0.01000"
                    w.writerow([stream, g, i * 0.5, i, dh]); i += 1


class VG(unittest.TestCase):
    def check(self, **kw):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, "r", **kw)
            run = {"expect_fx_events": True, "expect_raw_events": True, "expect_cpu_raw": True, "expect_palette": True}
            return vg.check_run("r", d, run)

    def test_good(self):
        errs, facts = self.check()
        self.assertEqual(errs, [])
        self.assertEqual(facts["scene_changes"], 4)

    def test_one_scene_only_fails(self):
        errs, _ = self.check(scenes=SCENES[:1], fx_gens=[1])
        self.assertTrue(any(e.startswith("scenes:") for e in errs))

    def test_mode_mismatch(self):
        errs, _ = self.check(mode="fxGpu=on rawGpu=off cpuRaw=on palette=on")
        self.assertTrue(any("rawGpu=off but the plan expects on" in e for e in errs))

    def test_missing_mode_line(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, "r")
            p = os.path.join(d, "r.stdout.log")
            open(p, "w").write("nothing\n")
            errs, _ = vg.check_run("r", d, None)
            self.assertTrue(any("no 'AcquisitionMode:'" in e for e in errs))

    def test_stream_misses_a_scene_generation(self):
        errs, _ = self.check(raw_skip=3)
        self.assertTrue(any("never began a generation for scene generation(s) [3]" in e for e in errs))

    def test_drift_must_not_cross_generation(self):
        errs, _ = self.check(drift_bad=True)
        self.assertTrue(any("drift crossed a generation" in e for e in errs))

    def test_unannounced_palette_generation(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, "r")
            p = os.path.join(d, "r.palette", "palette_fx.csv")
            with open(p, "a") as fh:
                fh.write("fx,9,99.0,99,nan\n")
            errs, _ = vg.check_run("r", d, None)
            self.assertTrue(any("never announced" in e for e in errs))

    def test_cli_exit_codes(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, "r")
            ok = subprocess.run([sys.executable, "-I", os.path.join(HERE, "verify_generations.py"), d], capture_output=True, text=True)
            self.assertEqual(ok.returncode, 0, ok.stdout)
            self.assertTrue(ok.stdout.rstrip().endswith("VERIFIED"))
            write_run(d, "r", scenes=SCENES[:2], fx_gens=[1, 2])
            bad = subprocess.run([sys.executable, "-I", os.path.join(HERE, "verify_generations.py"), d], capture_output=True, text=True)
            self.assertEqual(bad.returncode, 1)
            self.assertIn("NOT VERIFIED", bad.stdout)

    def test_plan_with_missing_run_and_require_all(self):
        with tempfile.TemporaryDirectory() as d:
            write_run(d, "a")
            json.dump({"runs": [{"tag": "a"}, {"tag": "b"}]}, open(os.path.join(d, "plan.json"), "w"))
            r = subprocess.run([sys.executable, "-I", os.path.join(HERE, "verify_generations.py"), os.path.join(d, "plan.json"), "--require-all"], capture_output=True, text=True)
            self.assertEqual(r.returncode, 1)
            self.assertIn("MISSING b", r.stdout)


class SM(unittest.TestCase):
    def test_sample_self(self):
        s = sysmon.sample(os.getpid())
        self.assertIsInstance(s["rss_kb"], int)
        self.assertGreater(s["rss_kb"], 1000)

    def test_monitor_writes_and_summarizes(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "x.sys.csv")
            m = sysmon.SysMonitor(os.getpid(), p, interval=0.05).start()
            import time
            time.sleep(0.3)
            m.stop()
            s = sysmon.summarize(p)
            self.assertGreaterEqual(s["samples"], 2)
            self.assertIsNotNone(s["rss_kb"])

    def test_summarize_synthetic_throttle_hint(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "t.csv")
            open(p, "w").write("t,rss_kb,temp_zone0,freq_cpu4\n1,100,60000,1800000\n2,120,75000,1200000\n")
            s = sysmon.summarize(p)
            self.assertEqual(s["rss_kb"]["growth"], 20)
            self.assertTrue(s["throttle_hint"])
            self.assertTrue(s["thermal_available"])

    def test_summarize_unavailable_is_blank_not_zero(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "u.csv")
            open(p, "w").write("t,rss_kb\n1,\n2,\n")
            s = sysmon.summarize(p)
            self.assertIsNone(s["rss_kb"])
            self.assertFalse(s["thermal_available"])
            self.assertEqual(sysmon.summarize(os.path.join(d, "none.csv"))["available"], False)


if __name__ == "__main__":
    unittest.main()
