#!/usr/bin/env python3
"""OC-003-D tooling tests (synthetic fixtures only; nothing here is, or pretends to be, ROCK Pi data).

  python3 -I -W error test_diag_tooling.py

Covers: planner (P0/P2/P2F compatibility, P2/P2G/P2N arms, deterministic rotation, invalid conditions), the environment audit (exact differences,
invalid control values, missing controls, unexpected variables, settings drift), verify_run's CONFIG control check, stall_analysis (contiguity,
exclusions, no double counting, throughput, fence counters) and summarize_campaign (new comparisons computed correctly, per-round and pooled).
"""
import copy
import csv
import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import plan_interleaved as pi  # noqa: E402
import stall_analysis as sta  # noqa: E402
import verify_run as vr  # noqa: E402

PLAN = os.path.join(HERE, "plan_interleaved.py")
VERIFY = os.path.join(HERE, "verify_run.py")
SUMMARIZE = os.path.join(HERE, "summarize_campaign.py")

HEADER = ("frame,mono,total_ms,update_wall,update_cpu,update_vcsw,update_ivcsw,preDraw_wall,preDraw_cpu,preDraw_vcsw,preDraw_ivcsw,draw_wall,draw_cpu,"
          "draw_vcsw,draw_ivcsw,glFinish_wall,glFinish_cpu,glFinish_vcsw,glFinish_ivcsw,postDraw_wall,postDraw_cpu,postDraw_vcsw,postDraw_ivcsw,scene,phase,"
          "media,ex_startup,ex_media,ex_scene,ex_transition,exclude,ev,ev_fail,fx_issue,fx_collect,raw_issue,raw_collect,fx_ifail,fx_cfail,raw_ifail,raw_cfail,cpu_raw,cpu_raw_gap,cpu_raw_us,pal_us,vid_new").split(",")
MEDIA, SCENE = "media.auto.deadbeef", "blob-region-prototype"


def frame(i, total=33.3, ev=0, excluded=False, **over):
    d = {k: 0 for k in HEADER}
    d.update(frame=i, mono=1000 + i * 0.0333, total_ms=total, update_wall=8.0, update_cpu=8.0, preDraw_wall=16.0, preDraw_cpu=0.6, draw_wall=8.0,
           draw_cpu=8.0, glFinish_wall=0.01, glFinish_cpu=0.01, postDraw_wall=0.5, postDraw_cpu=0.5, scene=SCENE, phase=0, media=MEDIA,
           ex_startup=1 if excluded else 0, exclude=1 if excluded else 0, ev=ev, fx_issue=ev & 1, fx_collect=(ev >> 1) & 1, raw_issue=(ev >> 2) & 1,
           raw_collect=(ev >> 3) & 1)
    d.update(over)
    return d


def write_csv(path, frames):
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=HEADER)
        w.writeheader()
        w.writerows(frames)


def slurp(path):
    with open(path) as fh:
        return fh.read()


def spit(path, text):
    with open(path, "w") as fh:
        fh.write(text)


def jslurp(path):
    with open(path) as fh:
        return json.load(fh)


def run_py(args, **kw):
    return subprocess.run([sys.executable, "-I"] + args, capture_output=True, text=True, **kw)


class Workdir(unittest.TestCase):
    def setUp(self):
        self._td = tempfile.TemporaryDirectory()
        self.d = self._td.name
        self.bin = os.path.join(self.d, "fake_runtime")
        with open(self.bin, "w") as fh:
            fh.write("#!/bin/sh\nexit 0\n")
        os.chmod(self.bin, 0o755)

    def tearDown(self):
        self._td.cleanup()

    def make_plan(self, conds, *extra, rounds=4, name="out"):
        out = os.path.join(self.d, name)
        r = run_py([PLAN, "--bin", self.bin, "--out", out, "--media-id", MEDIA, "--scene", SCENE, "--resolution", "1280x720", "--conditions", conds,
                    "--rounds", str(rounds), "--duration", "300"] + list(extra))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        with open(os.path.join(out, "plan.json")) as fh:
            return os.path.join(out, "plan.json"), json.load(fh)


class PlannerTests(Workdir):
    def test_existing_three_condition_plan_unchanged_and_valid(self):
        _, plan = self.make_plan("P0,P2,P2F")
        self.assertEqual(vr.audit_plan(plan), [])
        self.assertEqual(plan["order"], "fixed")
        self.assertEqual([r["condition"] for r in plan["runs"]], ["P0", "P2", "P2F"] * 4)
        self.assertEqual([r["tag"] for r in plan["runs"]][:4], ["01_P0_r1", "02_P2_r1", "03_P2F_r1", "04_P0_r2"])
        for r in plan["runs"]:
            self.assertEqual(r["expect_glfinish"], "off")
            self.assertEqual(r["expect_vsync"], "on")
            setv, unset, _rest = vr.parse_command(r["command"])
            self.assertNotIn("EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH", setv)
            self.assertIn("EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH", unset)

    def test_diagnostic_arms_validate_and_rotate_deterministically(self):
        _, plan = self.make_plan("P2,P2G,P2N", "--order", "rotate")
        self.assertEqual(vr.audit_plan(plan), [])
        self.assertEqual(plan["round_orders"], {"1": ["P2", "P2G", "P2N"], "2": ["P2G", "P2N", "P2"], "3": ["P2N", "P2", "P2G"], "4": ["P2", "P2G", "P2N"]})
        self.assertEqual([r["condition"] for r in plan["runs"]], ["P2", "P2G", "P2N", "P2G", "P2N", "P2", "P2N", "P2", "P2G", "P2", "P2G", "P2N"])
        self.assertEqual([r["tag"] for r in plan["runs"]], [f"{i:02d}_{r['condition']}_r{r['round']}" for i, r in enumerate(plan["runs"], 1)])
        self.assertEqual([r["position"] for r in plan["runs"]], [1, 2, 3] * 4)
        _, again = self.make_plan("P2,P2G,P2N", "--order", "rotate", name="out2")
        self.assertEqual(again["round_orders"], plan["round_orders"])
        self.assertEqual([r["tag"] for r in again["runs"]], [r["tag"] for r in plan["runs"]])

    def test_fixed_order_is_the_default_for_the_arms(self):
        _, plan = self.make_plan("P2,P2G,P2N")
        self.assertEqual({tuple(v) for v in plan["round_orders"].values()}, {("P2", "P2G", "P2N")})

    def test_arm_environments_differ_by_exactly_one_variable(self):
        _, plan = self.make_plan("P2,P2G,P2N")
        env = {}
        for r in plan["runs"]:
            setv, unset, rest = vr.parse_command(r["command"])
            env.setdefault(r["condition"], (setv, unset))
        keys = lambda c: {k: v for k, v in env[c][0].items() if k not in vr.PER_RUN_KEYS}  # noqa: E731
        self.assertEqual({k for k in set(keys("P2")) | set(keys("P2G")) if keys("P2").get(k) != keys("P2G").get(k)}, {"EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH"})
        self.assertEqual({k for k in set(keys("P2")) | set(keys("P2N")) if keys("P2").get(k) != keys("P2N").get(k)}, {"EXPERIENCE_RUNTIME_VSYNC"})
        self.assertEqual(keys("P2G")["EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH"], "1")
        self.assertEqual(keys("P2N")["EXPERIENCE_RUNTIME_VSYNC"], "off")
        for c in ("P2", "P2G", "P2N"):
            self.assertIn("EXPERIENCE_RUNTIME_FRAME_LIMITER", env[c][1])  # limiter variable explicitly unset => limiter stays on
            self.assertIn("EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH", env[c][1])  # presence-based: unset, never 0
            self.assertNotIn("EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH", env[c][0])
        self.assertIn("EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH", env["P2"][1])
        self.assertIn("EXPERIENCE_RUNTIME_VSYNC", env["P2"][1])
        by = {r["condition"]: r for r in plan["runs"]}
        self.assertEqual((by["P2G"]["expect_glfinish"], by["P2G"]["expect_vsync"], by["P2G"]["expect_limiter"]), ("on", "on", "on"))
        self.assertEqual((by["P2N"]["expect_glfinish"], by["P2N"]["expect_vsync"], by["P2N"]["expect_limiter"]), ("off", "off", "on"))
        self.assertEqual(by["P2G"]["expect_flush_after_fence"], "no")

    def test_relative_out_is_stored_as_absolute_paths(self):
        r = run_py([PLAN, "--bin", self.bin, "--out", "relout", "--media-id", MEDIA, "--scene", SCENE, "--resolution", "1280x720", "--conditions", "P2,P2G",
                    "--rounds", "1"], cwd=self.d)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        plan = jslurp(os.path.join(self.d, "relout", "plan.json"))
        self.assertTrue(os.path.isabs(plan["installation_settings_used"]))
        for run in plan["runs"]:
            setv, _unset, _rest = vr.parse_command(run["command"])
            self.assertTrue(os.path.isabs(setv["EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES"]), run["command"])
            self.assertTrue(os.path.isabs(setv["EXPERIENCE_RUNTIME_FRAME_PHASE_LOG"]))

    def test_planner_rejects_invalid_requests(self):
        for conds in ("P2,P2X", "P2,P2", ""):
            r = run_py([PLAN, "--bin", self.bin, "--out", os.path.join(self.d, "bad"), "--media-id", MEDIA, "--scene", SCENE, "--resolution", "1280x720", "--conditions", conds])
            self.assertNotEqual(r.returncode, 0, conds)
        r = run_py([PLAN, "--bin", self.bin, "--out", os.path.join(self.d, "bad2"), "--media-id", MEDIA, "--scene", SCENE, "--resolution", "1280x720",
                    "--conditions", "P2,P2G", "--defer-flush"])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("DEFER_FLUSH", r.stdout + r.stderr)


class AuditNegativeTests(Workdir):
    def setUp(self):
        super().setUp()
        _, self.plan = self.make_plan("P0,P2,P2F,P2G,P2N", rounds=2)

    def mutate(self, cond, fn):
        plan = copy.deepcopy(self.plan)
        for r in plan["runs"]:
            if r["condition"] == cond:
                r["command"] = fn(r["command"])
        return vr.audit_plan(plan)

    def test_baseline_plan_is_clean(self):
        self.assertEqual(vr.audit_plan(self.plan), [])

    def test_invalid_vsync_values_fail(self):
        for bad in ("0", "false", "OFF", "on"):
            errs = self.mutate("P2N", lambda c, bad=bad: c.replace("EXPERIENCE_RUNTIME_VSYNC=off", f"EXPERIENCE_RUNTIME_VSYNC={bad}"))
            self.assertTrue(any("VSYNC" in e and "off" in e for e in errs), (bad, errs))

    def test_invalid_glfinish_values_fail(self):
        for bad in ("0", "true", ""):
            errs = self.mutate("P2G", lambda c, bad=bad: c.replace("EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH=1", f"EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH={bad}"))
            self.assertTrue(any("GLFINISH" in e for e in errs), (bad, errs))

    def test_missing_controls_fail(self):
        errs = self.mutate("P2G", lambda c: c.replace("EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH=1 ", "", 1))
        self.assertTrue(any("GLFINISH" in e for e in errs), errs)
        errs = self.mutate("P2N", lambda c: c.replace("-u EXPERIENCE_RUNTIME_FRAME_LIMITER ", "", 1).replace("EXPERIENCE_RUNTIME_VSYNC=off ", "", 1))
        self.assertTrue(any("VSYNC" in e or "LIMITER" in e for e in errs), errs)
        errs = self.mutate("P2G", lambda c: c.replace("-u EXPERIENCE_RUNTIME_FRAME_PHASE_CONTROL ", "", 1))
        self.assertTrue(errs, "an unset managed control must be reported")

    def test_control_set_in_the_wrong_condition_fails(self):
        errs = self.mutate("P2", lambda c: c.replace("env ", "env EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH=1 ", 1))
        self.assertTrue(any("GLFINISH" in e for e in errs), errs)
        errs = self.mutate("P2G", lambda c: c.replace("env ", "env EXPERIENCE_RUNTIME_VSYNC=off ", 1))
        self.assertTrue(any("VSYNC" in e for e in errs), errs)

    def test_limiter_must_stay_on(self):
        errs = self.mutate("P2N", lambda c: c.replace("env ", "env EXPERIENCE_RUNTIME_FRAME_LIMITER=off ", 1))
        self.assertTrue(any("LIMITER" in e for e in errs), errs)

    def test_defer_flush_must_stay_unset_in_the_arms(self):
        for cond in ("P2", "P2G", "P2N"):
            errs = self.mutate(cond, lambda c: c.replace("env ", "env EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH=1 ", 1))
            self.assertTrue(any("DEFER_FLUSH" in e for e in errs), (cond, errs))
        errs = self.mutate("P2G", lambda c: c.replace("env ", "env EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH=0 ", 1))
        self.assertTrue(any("DEFER_FLUSH" in e for e in errs), errs)

    def test_unexpected_inherited_variable_fails(self):
        errs = self.mutate("P2N", lambda c: c.replace("env ", "env EXPERIENCE_RUNTIME_SOMETHING_ELSE=1 ", 1))
        self.assertTrue(any("unexpected variable" in e for e in errs), errs)

    def test_unexpected_difference_inside_a_condition_fails(self):
        plan = copy.deepcopy(self.plan)
        first = [r for r in plan["runs"] if r["condition"] == "P2G"][0]
        first["command"] = first["command"].replace("EXPERIENCE_RUNTIME_PREDOMINANT_COLOR=1", "EXPERIENCE_RUNTIME_PREDOMINANT_COLOR=0")
        self.assertTrue(any("do not share one environment" in e for e in vr.audit_plan(plan)))

    def test_arm_differing_by_two_variables_fails(self):
        errs = self.mutate("P2N", lambda c: c.replace("EXPERIENCE_RUNTIME_VIDEO_COLOR_STATS=1", "EXPERIENCE_RUNTIME_VIDEO_COLOR_STATS=0"))
        self.assertTrue(any("P2 vs P2N" in e for e in errs), errs)

    def test_unknown_condition_fails(self):
        plan = copy.deepcopy(self.plan)
        plan["runs"][0]["condition"] = "P9"
        self.assertTrue(any("not known to the planner" in e for e in vr.audit_plan(plan)))

    def test_settings_copy_drift_fails(self):
        used = self.plan["installation_settings_used"]
        errs = self.mutate("P2G", lambda c: c.replace(used, used + ".other"))
        self.assertTrue(any("INSTALLATION_SETTINGS" in e for e in errs), errs)
        original = slurp(used)
        try:
            doc = json.loads(original)
            doc["mediaAutoCycle"]["enabled"] = True
            with open(used, "w") as fh:
                json.dump(doc, fh)
            errs = vr.audit_plan(self.plan)
            self.assertTrue(any("changed since planning" in e for e in errs) and any("mediaAutoCycle" in e for e in errs), errs)
        finally:
            spit(used, original)
        self.assertEqual(vr.audit_plan(self.plan), [])


def cycle_frames(stall_positions, issue_glfinish=0.01, first=0, count=120, total_stall=45.0):
    """120 frames, frames 0..19 excluded (warm-up). Period 10 starting at frame 20: pos0 fxIssue, pos1 none (after issue), pos2 fxCollect, pos3 none
    (after collect), pos4 rawIssue, pos5 none (after issue), pos6 rawCollect, pos7..9 none. Frames at `stall_positions` are long."""
    ev_at = {0: 1, 2: 2, 4: 4, 6: 8}
    out = []
    for i in range(first, count):
        if i < 20:
            out.append(frame(i, excluded=True, ev=0))
            continue
        pos = (i - 20) % 10
        over = {}
        if pos in (0, 4):
            over["glFinish_wall"] = issue_glfinish
        if pos in stall_positions:
            over["postDraw_wall"] = 12.0
        out.append(frame(i, total=total_stall if pos in stall_positions else 33.3, ev=ev_at.get(pos, 0), **over))
    return out


def typed(frames):
    """Same typing as stall_analysis.read_frames applies to a CSV (the fixtures above are CSV-shaped dicts)."""
    return [dict(frame=int(f["frame"]), excluded=int(f["exclude"]) == 1, ev=int(f["ev"]), **{k: float(f[k]) for k in sta.NUM}) for f in frames]


def A(runs, **kw):
    return sta.analyze([typed(r) for r in runs], **kw)


class StallAnalysisTests(unittest.TestCase):
    def test_classes_partition_long_frames_without_double_counting(self):
        res = A([cycle_frames({0, 1, 5, 6})])
        k = res["classes"]
        self.assertEqual(sum(v["long"] for v in k.values()), res["long_frames"])
        self.assertEqual(sum(v["frames"] for v in k.values()), res["frames"])
        self.assertEqual(res["frames"], 100)
        self.assertEqual(k["event"]["long"], 20)  # pos 0 and 6, 10 cycles
        self.assertEqual(k["no_event_after_issue"]["long"], 20)  # pos 1 and 5
        self.assertEqual(res["no_event_after_issue_long_rate"], 1.0)

    def test_after_issue_classification_and_priority(self):
        rows = [frame(0), frame(1, ev=1 | 2), frame(2), frame(3, ev=2), frame(4), frame(5), frame(6, ev=8), frame(7), frame(8), frame(9)]
        cls = [sta.classify(typed(rows), i) for i in range(len(rows))]
        self.assertEqual(cls, ["quiet", "event", "no_event_after_issue", "event", "no_event_after_collect", "no_event_lag2",
                               "event", "no_event_after_collect", "no_event_lag2", "quiet"])

    def test_lag2_needs_an_event_two_frames_earlier_and_none_between(self):
        rows = [frame(0, ev=2), frame(1), frame(2), frame(3)]
        self.assertEqual([sta.classify(typed(rows), i) for i in range(4)], ["event", "no_event_after_collect", "no_event_lag2", "quiet"])

    def test_frame_id_gap_breaks_the_relation(self):
        rows = [frame(0, ev=1), frame(2, total=50.0), frame(3)]  # id 1 is missing: frame 2 is NOT the frame after the issue
        self.assertEqual(sta.classify(typed(rows), 1), "quiet")
        res = A([rows])
        self.assertEqual(res["classes"]["quiet"]["long"], 1)
        self.assertEqual(res["no_event_after_issue_long_rate"], None)

    def test_excluded_frames_break_the_relation_and_are_not_counted(self):
        rows = [frame(0, ev=1, excluded=True), frame(1, total=50.0), frame(2)]
        self.assertEqual(sta.classify(typed(rows), 1), "quiet")
        res = A([rows])
        self.assertEqual(res["frames"], 2)
        rows = [frame(0, ev=1), frame(1, excluded=True), frame(2, total=50.0)]
        self.assertEqual(sta.classify(typed(rows), 2), "quiet")

    def test_relations_do_not_cross_runs(self):
        a = [frame(0, ev=1)]
        b = [frame(1, total=50.0)]
        res = A([a, b])
        self.assertEqual(res["classes"]["quiet"]["long"], 1)
        self.assertEqual(res["after_issue"]["frames"], 0)

    def test_effective_throughput_and_percentiles(self):
        rows = [frame(i, total=40.0) for i in range(100)]
        res = A([rows])
        self.assertAlmostEqual(res["effective_fps"], 25.0, places=3)
        self.assertEqual(res["frame_ms"]["p50"], 40.0)
        self.assertEqual(res["long_frames"], 0)  # exactly 40 is not "> 40"

    def test_segment_distributions_for_populations(self):
        res = A([cycle_frames({1}, issue_glfinish=7.0)])
        self.assertEqual(res["glFinish"]["issue_frame"]["wall"]["p50"], 7.0)
        self.assertEqual(res["glFinish"]["quiet"]["wall"]["max"], 0.01)
        self.assertEqual(res["postDraw"]["no_event_after_issue"]["wall"]["max"], 12.0)
        self.assertAlmostEqual(res["postDraw"]["no_event_after_issue"]["blocked"]["max"], 11.5, places=3)

    def test_fence_not_ready_counters(self):
        with tempfile.TemporaryDirectory() as td:
            p = os.path.join(td, "run.stdout.log")
            with open(p, "w") as fh:
                fh.write("[notice] SceneColor: stream=FX effMode=deferred issued=10 notReady=12 glErrors=0\n")
                fh.write("[notice] SceneColor: stream=FX effMode=deferred issued=20 notReady=27 glErrors=0\n")
                fh.write("[notice] VideoColor: stream=RAW effMode=deferred issued=20 notReady=20 glErrors=0\n")
            res = sta.fence_notready([p])
        self.assertEqual(res["FX"], {"issued": 20, "notReady": 27, "excess": 7})
        self.assertEqual(res["RAW"]["excess"], 0)


class SummaryAndVerifyTests(Workdir):
    def build(self, conds, order, profile, rounds=2):
        plan_path, plan = self.make_plan(conds, "--order", order, rounds=rounds, name="camp")
        out = os.path.dirname(plan_path)
        for r in plan["runs"]:
            frames, fence, cfg = profile(r["condition"], r["round"])
            write_csv(os.path.join(out, r["tag"] + ".frames.csv"), frames)
            with open(os.path.join(out, r["tag"] + ".stdout.log"), "w") as fh:
                fh.write(f"[notice] VideoVerification: startup media ACTIVE id={MEDIA} file=clip.mp4 size=960x540 duration=28.3s\n")
                fh.write("[notice] Installation: applied: sceneAutoCycle off (x) mediaAutoCycle off (y)\n")
                yn = lambda b: "on" if b else "off"  # noqa: E731
                fh.write(f"[notice] AcquisitionMode: fxGpu={yn(r['expect_fx_gpu'])} rawGpu={yn(r['expect_raw_gpu'])} cpuRaw={yn(r['expect_cpu_raw'])} "
                         f"palette={yn(r['expect_palette'])} predominant=on contract=oc003f-1\n")
                fh.write("[notice] SceneColor: flushAfterFence=no defer=fence/2\n")
                fh.write(f"[notice] SceneColor: stream=FX issued=10 notReady={10 + fence} glErrors=0\n")
                fh.write(f"[notice] VideoColor: stream=RAW issued=10 notReady={10 + fence} glErrors=0\n")
            with open(os.path.join(out, r["tag"] + ".phase.log"), "w") as fh:
                fh.write(f"CONFIG t=0 targetFps=30.000000 limiter={cfg.get('limiter', 'on')} vsync={cfg.get('vsync', 'on')} glFinish={cfg.get('glfinish', 'off')} "
                         f"control=none window=1280x720\n")
        return plan_path, plan, out

    @staticmethod
    def profile(cond, rnd):
        if cond == "P2G":
            return cycle_frames(set(), issue_glfinish=12.0), 0, {"glfinish": "on"}
        stalls = {1, 5} if rnd == 1 else {1}
        return cycle_frames(stalls), (3 if rnd == 1 else 2), ({"vsync": "off"} if cond == "P2N" else {})

    def test_verify_run_checks_the_control_state_the_probe_reports(self):
        plan_path, plan, out = self.build("P2,P2G,P2N", "rotate", self.profile)
        r = run_py([VERIFY, plan_path, "--min-events", "1"])
        self.assertEqual(r.returncode, 0, r.stdout)
        tag = [x["tag"] for x in plan["runs"] if x["condition"] == "P2N"][0]
        path = os.path.join(out, tag + ".phase.log")
        spit(path, slurp(path).replace("vsync=off", "vsync=on"))
        r = run_py([VERIFY, plan_path, "--min-events", "1"])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("control: phase log CONFIG vsync=on != planned off", r.stdout)

    def test_summary_new_comparisons_per_round_and_pooled(self):
        plan_path, plan, out = self.build("P2,P2G,P2N", "rotate", self.profile)
        r = run_py([SUMMARIZE, plan_path, "--out", os.path.join(out, "analysis")])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        s = jslurp(os.path.join(out, "analysis", "summary.json"))
        self.assertIn("P2G-P2", s["differences"])
        self.assertIn("P2N-P2", s["differences"])
        self.assertNotIn("P2-P0", s["differences"])  # pairs only appear when both conditions are in the plan
        d = s["stall"]["differences"]["P2G-P2"]
        # P2 r1: 20 long frames of 20 after-issue frames; r2: 10 of 20. P2G: none.
        self.assertEqual(d["long_frames"]["per_round"], [-20, -10])
        self.assertEqual(d["long_frames"]["pooled"], -30)
        self.assertEqual(d["after_issue_long_rate"]["per_round"], [-1.0, -0.5])
        self.assertAlmostEqual(d["after_issue_long_rate"]["pooled"], -0.75, places=4)
        self.assertIs(d["after_issue_long_rate"]["sign_consistent"], True)
        self.assertAlmostEqual(d["glFinish_wall_max"]["pooled"], 12.0 - 0.01, places=3)
        self.assertAlmostEqual(d["glFinish_issue_frame_wall_p50"]["pooled"], 12.0 - 0.01, places=3)
        self.assertIs(d["effective_fps"]["sign_consistent"], True)
        self.assertGreater(d["effective_fps"]["pooled"], 0)
        # fence excess: P2 r1 2 streams x 3 = 6, r2 2 x 2 = 4; P2G 0 -> differences -6, -4
        self.assertEqual(d["fence_not_ready_excess"]["per_round"], [-6, -4])
        n = s["stall"]["differences"]["P2N-P2"]
        self.assertEqual(n["long_frames"]["pooled"], 0)  # P2N is built identical to P2 in stall behaviour
        self.assertIs(n["long_frames"]["sign_consistent"], False)
        exp_fps_r1 = 1000.0 * 100 / (20 * 45.0 + 80 * 33.3)
        self.assertAlmostEqual(s["stall"]["conditions"]["P2"]["per_round"]["1"]["effective_fps"], exp_fps_r1, places=3)
        report = slurp(os.path.join(out, "analysis", "report.md"))
        self.assertIn("OC-003-D stall analysis — MEASURED", report)
        self.assertIn("Interpretation guide (NOT measured", report)
        self.assertLess(report.index("MEASURED"), report.index("Interpretation guide"))
        self.assertIn("### P2G-P2", report)

    def test_existing_three_condition_summary_keeps_its_semantics(self):
        def prof(cond, rnd):
            return cycle_frames({1} if cond != "P0" else set()), 0, {}
        plan_path, plan, out = self.build("P0,P2,P2F", "fixed", prof)
        for r in plan["runs"]:  # P0 has no sampling events: replace its stream by event-free frames
            if r["condition"] == "P0":
                write_csv(os.path.join(out, r["tag"] + ".frames.csv"), [frame(i, excluded=i < 20) for i in range(120)])
        r = run_py([SUMMARIZE, plan_path, "--out", os.path.join(out, "analysis")])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        s = jslurp(os.path.join(out, "analysis", "summary.json"))
        self.assertEqual(sorted(s["differences"]), ["P2-P0", "P2F-P0", "P2F-P2"])
        self.assertEqual(s["differences"]["P2-P0"]["long_frames"]["per_round"], [10, 10])
        report = slurp(os.path.join(out, "analysis", "report.md"))
        self.assertNotIn("OC-003-D", report)  # the new sections appear only for P2G/P2N plans (or --stall)
        r = run_py([SUMMARIZE, plan_path, "--out", os.path.join(out, "analysis2"), "--stall"])
        self.assertIn("OC-003-D stall analysis", slurp(os.path.join(out, "analysis2", "report.md")))


class IsolationArmTests(Workdir):
    """OC-003-F P0..P5 acquisition-isolation arms (synthetic fixtures only)."""

    def test_p3_p4_p5_plan_is_valid_and_exact(self):
        _, plan = self.make_plan("P0,P1,P2,P3,P4,P5", "--order", "rotate", rounds=2)
        self.assertEqual(vr.audit_plan(plan), [])
        by = {}
        for r in plan["runs"]:
            setv, unset, _ = vr.parse_command(r["command"])
            by.setdefault(r["condition"], (setv, unset, r))
        e = lambda c, k: by[c][0].get(k)  # noqa: E731
        # P3 = GPU RAW only; P4 = CPU RAW only; P5 = CPU RAW + FX GPU
        self.assertEqual((e("P3", "EXPERIENCE_RUNTIME_SCENE_COLOR"), e("P3", "EXPERIENCE_RUNTIME_VIDEO_COLOR"), e("P3", "EXPERIENCE_RUNTIME_CPU_RAW")), ("0", "1", None))
        self.assertEqual((e("P4", "EXPERIENCE_RUNTIME_SCENE_COLOR"), e("P4", "EXPERIENCE_RUNTIME_VIDEO_COLOR"), e("P4", "EXPERIENCE_RUNTIME_CPU_RAW")), ("0", None, "1"))
        self.assertEqual((e("P5", "EXPERIENCE_RUNTIME_SCENE_COLOR"), e("P5", "EXPERIENCE_RUNTIME_VIDEO_COLOR"), e("P5", "EXPERIENCE_RUNTIME_CPU_RAW")), ("1", None, "1"))
        for c in ("P0", "P1", "P2"):  # the earlier arms never turn CPU RAW on and always explicitly unset it
            self.assertIn("EXPERIENCE_RUNTIME_CPU_RAW", by[c][1])
            self.assertIsNone(e(c, "EXPERIENCE_RUNTIME_CPU_RAW"))
        for c in ("P0", "P1", "P2", "P3", "P4", "P5"):  # palette analysis is off unless --palette-diag
            self.assertIn("EXPERIENCE_RUNTIME_PALETTE_DIAG", by[c][1])
        flags = {c: (by[c][2]["expect_fx_gpu"], by[c][2]["expect_raw_gpu"], by[c][2]["expect_cpu_raw"]) for c in by}
        self.assertEqual(flags, {"P0": (False, False, False), "P1": (True, False, False), "P2": (True, True, False),
                                 "P3": (False, True, False), "P4": (False, False, True), "P5": (True, False, True)})

    def test_palette_diag_is_per_run_and_never_on_p0(self):
        _, plan = self.make_plan("P0,P4,P5", "--palette-diag", rounds=1)
        self.assertEqual(vr.audit_plan(plan), [])
        for r in plan["runs"]:
            setv, _u, _ = vr.parse_command(r["command"])
            self.assertEqual("EXPERIENCE_RUNTIME_PALETTE_DIAG" in setv, r["condition"] != "P0")
            self.assertEqual(r["expect_palette"], r["condition"] != "P0")

    def test_pcmp_is_gpu_raw_plus_cpu_raw_without_fx(self):
        _, plan = self.make_plan("P0,PCMP", rounds=1)
        self.assertEqual(vr.audit_plan(plan), [])
        r = [x for x in plan["runs"] if x["condition"] == "PCMP"][0]
        self.assertEqual((r["expect_fx_gpu"], r["expect_raw_gpu"], r["expect_cpu_raw"]), (False, True, True))

    def test_defer_flush_rejected_for_fx_off_arms(self):
        r = run_py([PLAN, "--bin", self.bin, "--out", os.path.join(self.d, "x"), "--media-id", MEDIA, "--scene", SCENE, "--resolution", "1280x720",
                    "--conditions", "P3", "--defer-flush"])
        self.assertNotEqual(r.returncode, 0)


class IsolationVerifyTests(SummaryAndVerifyTests):
    def frames_for(self, cond):
        ev = {"P0": 0, "P1": 0b0011, "P2": 0b1111, "P3": 0b1100, "P4": 0, "P5": 0b0011, "PCMP": 0b1100}[cond]
        fr = []
        for i in range(120):
            f = frame(i, ev=ev if i % 2 == 0 else 0)
            if cond in ("P4", "P5", "PCMP") and i % 2 == 0:
                f["cpu_raw"] = 1
            fr.append(f)
        return fr

    def build_arms(self, conds):
        return self.build(conds, "fixed", lambda c, r: (self.frames_for(c), 0, {}), rounds=1)

    def test_all_six_arms_verify(self):
        plan_path, _plan, _out = self.build_arms("P0,P1,P2,P3,P4,P5,PCMP")
        r = run_py([VERIFY, plan_path, "--min-events", "10"])
        self.assertEqual(r.returncode, 0, r.stdout)

    def test_cpu_raw_without_events_fails(self):
        plan_path, plan, out = self.build_arms("P4")
        write_csv(os.path.join(out, plan["runs"][0]["tag"] + ".frames.csv"), [frame(i) for i in range(120)])
        r = run_py([VERIFY, plan_path, "--min-events", "10"])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("cpu_raw=0 <", r.stdout)

    def test_cpu_raw_leaking_into_p2_fails(self):
        plan_path, plan, out = self.build_arms("P2")
        fr = self.frames_for("P2")
        fr[2]["cpu_raw"] = 1
        write_csv(os.path.join(out, plan["runs"][0]["tag"] + ".frames.csv"), fr)
        r = run_py([VERIFY, plan_path, "--min-events", "10"])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("CPU RAW samples recorded in a condition without CPU RAW", r.stdout)

    def test_acquisition_mode_mismatch_fails(self):
        plan_path, plan, out = self.build_arms("P4")
        path = os.path.join(out, plan["runs"][0]["tag"] + ".stdout.log")
        spit(path, slurp(path).replace("cpuRaw=on", "cpuRaw=off"))
        r = run_py([VERIFY, plan_path, "--min-events", "10"])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("acquisition: runtime reports", r.stdout)

    def test_fx_events_in_p4_fail(self):
        plan_path, plan, out = self.build_arms("P4")
        fr = self.frames_for("P4")
        fr[0]["fx_issue"] = 1
        write_csv(os.path.join(out, plan["runs"][0]["tag"] + ".frames.csv"), fr)
        r = run_py([VERIFY, plan_path, "--min-events", "10"])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("FX events recorded", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=1)
