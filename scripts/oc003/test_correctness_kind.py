#!/usr/bin/env python3
"""OC-003-F five-scene correctness workflow: planner --kind correctness / --cycle-scenes, the verify_run.py correctness branch, and the hand-off to
verify_generations.py. Synthetic runs only. Run: python3 -I -W error scripts/oc003/test_correctness_kind.py"""
import csv
import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
PLAN, VERIFY, GENS = (os.path.join(HERE, n) for n in ("plan_interleaved.py", "verify_run.py", "verify_generations.py"))
MEDIA = "media.auto.7021ad87"
SCENES = ["blob-region-prototype", "temporal-fields", "blueprint_emergence", "fragment-trail", "quadrant-crosshair"]
COLS = ["frame", "total_ms", "scene", "media", "exclude", "ev", "fx_issue", "fx_collect", "raw_issue", "raw_collect", "fx_ifail", "fx_cfail", "raw_ifail", "raw_cfail", "cpu_raw", "cpu_raw_gap"]


def jload(path):
    with open(path) as fh:
        return json.load(fh)


def jdump(obj, path):
    with open(path, "w") as fh:
        json.dump(obj, fh)


def put(path, text):
    with open(path, "w") as fh:
        fh.write(text)


def py(args):
    return subprocess.run([sys.executable, "-I"] + args, capture_output=True, text=True)


class Base(unittest.TestCase):
    def setUp(self):
        self._td = tempfile.TemporaryDirectory()
        self.d = self._td.name
        self.bin = os.path.join(self.d, "fake_runtime")
        put(self.bin, "#!/bin/sh\nexit 0\n")
        os.chmod(self.bin, 0o755)

    def tearDown(self):
        self._td.cleanup()

    def plan(self, *extra, name="c", duration="150", conds="P0"):
        out = os.path.join(self.d, name)
        r = py([PLAN, "--bin", self.bin, "--out", out, "--media-id", MEDIA, "--scene", SCENES[0], "--resolution", "1280x720", "--conditions", conds,
                "--rounds", "1", "--duration", duration] + list(extra))
        return r, out


class PlannerKind(Base):
    def test_campaign_default_is_unchanged(self):
        r, out = self.plan(name="camp")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        plan = jload(os.path.join(out, "plan.json"))
        self.assertEqual(plan["kind"], "campaign")
        self.assertIsNone(plan["scene_cycle_seconds"])
        self.assertNotIn("expected_scenes", plan)
        cfg = jload(os.path.join(out, "campaign-installation-settings.json"))
        self.assertIs(cfg["sceneAutoCycle"]["enabled"], False)
        self.assertIs(cfg["mediaAutoCycle"]["enabled"], False)

    def test_correctness_plan_turns_only_the_scene_cycle_on(self):
        r, out = self.plan("--kind", "correctness", "--cycle-scenes", "20")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        plan = jload(os.path.join(out, "plan.json"))
        self.assertEqual((plan["kind"], plan["scene_cycle_seconds"], plan["min_events"]), ("correctness", 20.0, 20))
        self.assertEqual(len(plan["expected_scenes"]), 5)
        cfg = jload(os.path.join(out, "campaign-installation-settings.json"))
        self.assertIs(cfg["sceneAutoCycle"]["enabled"], True)
        self.assertEqual(cfg["sceneAutoCycle"]["durationSeconds"], 20.0)
        self.assertIs(cfg["mediaAutoCycle"]["enabled"], False)
        # the run command is otherwise the campaign command: media pinned, same managed-variable scrub
        cmd = plan["runs"][0]["command"]
        self.assertIn("EXPERIENCE_RUNTIME_VIDEO_STARTUP_MEDIA_ID=" + MEDIA, cmd)

    def test_refusals(self):
        for args, frag in ((["--kind", "correctness"], "--cycle-scenes"),
                           (["--kind", "correctness", "--cycle-scenes", "5"], ">= 10"),
                           (["--kind", "correctness", "--cycle-scenes", "20", "--scene-env", "TEMPORAL"], "--scene-env"),
                           (["--cycle-scenes", "20"], "only applies to --kind correctness")):
            r, _ = self.plan(*args, name="x")
            self.assertNotEqual(r.returncode, 0, args)
            self.assertIn(frag, r.stdout + r.stderr)
        r, _ = self.plan("--kind", "correctness", "--cycle-scenes", "20", duration="90", name="y")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("cannot show five scenes", r.stdout + r.stderr)


def write_run(out, run, scenes=SCENES, scene_cycle="on", media=MEDIA, rows_per_scene=40):
    tag = run["tag"]
    with open(os.path.join(out, tag + ".frames.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=COLS)
        w.writeheader()
        n = 0
        for s in scenes:
            for k in range(rows_per_scene):
                w.writerow({"frame": n, "total_ms": 33.3, "scene": s, "media": media, "exclude": 1 if k < 5 else 0, **{c: 0 for c in COLS[5:]}})
                n += 1
    with open(os.path.join(out, tag + ".stdout.log"), "w") as fh:
        fh.write(f"[notice] VideoVerification: startup media ACTIVE id={MEDIA} file=clip.mp4 size=960x540 duration=28.3s\n")
        fh.write(f"[notice] Installation: applied: sceneAutoCycle {scene_cycle} (x) mediaAutoCycle off (y)\n")
        fh.write("[notice] AcquisitionMode: fxGpu=off rawGpu=off cpuRaw=off palette=off predominant=off contract=oc003f-1\n")
    put(os.path.join(out, tag + ".phase.log"), "CONFIG t=0 targetFps=30.000000 limiter=on vsync=on glFinish=off control=none window=1280x720\n")


class VerifyKind(Base):
    def make(self, **kw):
        r, out = self.plan("--kind", "correctness", "--cycle-scenes", "20")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        plan = jload(os.path.join(out, "plan.json"))
        write_run(out, plan["runs"][0], **kw)
        return os.path.join(out, "plan.json"), plan, out

    def test_five_scenes_verify(self):
        plan_path, _, _ = self.make()
        r = py([VERIFY, plan_path])  # default --min-events 100 is overridden by the plan's floor (P0 expects no events anyway)
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertTrue(r.stdout.rstrip().endswith("VERIFIED"))

    def test_four_scenes_fail(self):
        plan_path, _, _ = self.make(scenes=SCENES[:4])
        r = py([VERIFY, plan_path])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("only 4 of 5 production scenes", r.stdout)

    def test_unknown_scene_fails(self):
        plan_path, _, _ = self.make(scenes=SCENES[:4] + ["fake-scene"])
        r = py([VERIFY, plan_path])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("are not production scenes", r.stdout)

    def test_scene_cycle_must_be_on_in_the_log(self):
        plan_path, _, _ = self.make(scene_cycle="off")
        r = py([VERIFY, plan_path])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("lacks 'Installation: applied: sceneAutoCycle on", r.stdout)

    def test_settings_copy_tamper_is_caught(self):
        plan_path, _, out = self.make()
        p = os.path.join(out, "campaign-installation-settings.json")
        cfg = jload(p)
        cfg["mediaAutoCycle"]["enabled"] = True
        jdump(cfg, p)
        r = py([VERIFY, plan_path])
        self.assertNotEqual(r.returncode, 0)
        self.assertTrue("sha256 differs" in r.stdout or "mediaAutoCycle.enabled is not false" in r.stdout)

    def test_campaign_plans_still_require_the_single_pinned_scene(self):
        r, out = self.plan(name="camp")
        plan = jload(os.path.join(out, "plan.json"))
        write_run(out, plan["runs"][0], scene_cycle="off")  # five scenes in a CAMPAIGN is a failure
        r = py([VERIFY, os.path.join(out, "plan.json")])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("scene: steady-state CSV scene", r.stdout)

    def test_handoff_to_verify_generations(self):
        plan_path, _, out = self.make()
        r = py([GENS, plan_path, "--require-all"])
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertTrue(r.stdout.rstrip().endswith("VERIFIED"))


if __name__ == "__main__":
    unittest.main()
