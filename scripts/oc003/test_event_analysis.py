#!/usr/bin/env python3
"""Deterministic fixture tests for the OC-003 sampling-frame event analysis (aggregate_frames.py, reconcile_events.py).
Synthetic frame streams with known event layouts, including rare overlapping combinations that a real run seldom produces.
Run: python3 scripts/oc003/test_event_analysis.py   (exit 0 = all pass)"""
import csv
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import aggregate_frames as agg  # noqa: E402

SEGS = ["update", "preDraw", "draw", "glFinish", "postDraw"]
EVC = ["ev", "ev_fail", "fx_issue", "fx_collect", "raw_issue", "raw_collect", "fx_ifail", "fx_cfail", "raw_ifail", "raw_cfail"]


def header(events=True):
    h = ["frame", "mono", "total_ms"]
    for s in SEGS:
        h += [f"{s}_wall", f"{s}_cpu", f"{s}_vcsw", f"{s}_ivcsw"]
    h += ["scene", "phase", "media", "ex_startup", "ex_media", "ex_scene", "ex_transition", "exclude"]
    return h + (EVC if events else [])


def row(frame, total, mask=0, fail=0, excl=0, draw=2.0, counts=None):
    bits = [("fx_issue", 1), ("fx_collect", 2), ("raw_issue", 4), ("raw_collect", 8)]
    c = {k: (1 if mask & b else 0) for k, b in bits}
    if counts:
        c.update(counts)
    fbits = [("fx_ifail", 1), ("fx_cfail", 2), ("raw_ifail", 4), ("raw_cfail", 8)]
    c.update({k: (1 if fail & b else 0) for k, b in fbits})
    r = {"frame": frame, "mono": frame / 30.0, "total_ms": total, "scene": "s", "phase": 0, "media": "m",
         "ex_startup": excl, "ex_media": 0, "ex_scene": 0, "ex_transition": 0, "exclude": excl, "ev": mask, "ev_fail": fail, **c}
    for s in SEGS:
        r.update({f"{s}_wall": draw if s == "draw" else 1.0, f"{s}_cpu": draw if s == "draw" else 0.5, f"{s}_vcsw": -1, f"{s}_ivcsw": -1})
    return r


def write(path, rows, events=True):
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=header(events), extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)


class Args:
    include_excluded = False
    threshold_ms = 100.0
    long_ms = 40.0


class EventAnalysis(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix="oc003_evtest_")

    def run_agg(self, rows, events=True, include=False):
        p = os.path.join(self.d, "a.csv")
        write(p, rows, events)
        a = Args()
        a.include_excluded = include
        return agg.summarize("T", [p], a)

    def test_exclusive_vs_independent_with_overlap(self):
        rows = [row(0, 33.0), row(1, 50.0, 1), row(2, 34.0, 2), row(3, 33.0, 4), row(4, 45.0, 8),
                row(5, 60.0, 1 | 2), row(6, 33.0, 1 | 4 | 8), row(7, 33.0), row(8, 70.0, 15)]
        r = self.run_agg(rows)["events"]
        ex = r["exclusive"]
        self.assertEqual(sum(v["n"] for v in ex.values()), 9)
        self.assertTrue(r["exclusive_sum_matches_total"])
        self.assertEqual((ex["none"]["n"], ex["fxIssueOnly"]["n"], ex["fxCollectOnly"]["n"], ex["rawIssueOnly"]["n"], ex["rawCollectOnly"]["n"], ex["multi"]["n"]),
                         (2, 1, 1, 1, 1, 3))
        ind = r["independent"]
        self.assertEqual((ind["fxIssue"]["n"], ind["fxCollect"]["n"], ind["rawIssue"]["n"], ind["rawCollect"]["n"]), (4, 3, 3, 3))
        self.assertGreater(sum(v["n"] for v in ind.values()), 9)  # overlapping populations must exceed the frame count
        self.assertEqual(r["exclusive_multi_breakdown"], {"fxIssue+fxCollect": 1, "fxIssue+rawIssue+rawCollect": 1, "fxIssue+fxCollect+rawIssue+rawCollect": 1})
        self.assertEqual(r["long_frames_total"], 4)  # 50, 45, 60, 70
        self.assertEqual(sum(r["long_frames_by_exclusive_class"].values()), r["long_frames_total"])
        self.assertEqual(r["long_frames_by_independent_class(overlapping)"]["fxIssue"], 3)  # frames 1, 5, 8 (frame 6 = 33 ms is not long)

    def test_exclusions_dropped_and_counted(self):
        rows = [row(0, 400.0, 3, excl=1), row(1, 33.0), row(2, 40.5, 1)]
        r = self.run_agg(rows)
        self.assertEqual((r["frames"], r["excluded_frames"]), (2, 1))
        self.assertEqual(r["events"]["event_counts"]["fx_issue"], 1)  # the excluded frame's events are not counted
        r2 = self.run_agg(rows, include=True)
        self.assertEqual(r2["frames"], 3)
        self.assertEqual(r2["events"]["event_counts"]["fx_issue"], 2)

    def test_lag_contiguity_and_own_event_free(self):
        rows = [row(0, 33.0, 1), row(1, 33.0, 2), row(2, 50.0), row(5, 33.0, 1), row(7, 33.0)]  # frame 6 missing: +1 of frame 5 absent, +2 of 5 = frame 7
        lag = self.run_agg(rows)["events"]["lag"]
        self.assertEqual(lag["fxIssue+1"]["all_lag_frames"]["n"], 1)          # frame 0 -> 1 only (5 -> 6 missing)
        self.assertEqual(lag["fxIssue+1"]["own_event_free_lag_frames"]["n"], 0)  # frame 1 carries fxCollect itself
        self.assertEqual(lag["fxIssue+2"]["all_lag_frames"]["n"], 2)          # 0 -> 2, 5 -> 7
        self.assertEqual(lag["fxIssue+2"]["own_event_free_lag_frames"]["n"], 2)
        self.assertEqual(lag["fxIssue+2"]["all_lag_frames"]["long_frames"], 1)  # frame 2 = 50 ms

    def test_lag_never_uses_excluded_frames(self):
        rows = [row(0, 33.0, 1), row(1, 90.0, excl=1), row(2, 33.0)]
        lag = self.run_agg(rows)["events"]["lag"]
        self.assertEqual(lag["fxIssue+1"]["all_lag_frames"]["n"], 0)
        self.assertEqual(lag["fxIssue+2"]["all_lag_frames"]["n"], 1)

    def test_failed_attempts_are_not_successes(self):
        rows = [row(0, 33.0, 0, fail=2), row(1, 33.0, 1, fail=4), row(2, 33.0)]
        e = self.run_agg(rows)["events"]
        self.assertEqual(e["event_counts"]["fx_collect"], 0)
        self.assertEqual((e["event_counts"]["fx_cfail"], e["event_counts"]["raw_ifail"]), (1, 1))
        self.assertEqual(e["exclusive"]["none"]["n"], 2)  # a failed attempt alone is a no-SUCCESS-event frame
        self.assertEqual(e["frames_with_failed_attempt"], 2)

    def test_legacy_stream_without_event_columns(self):
        r = self.run_agg([row(0, 33.0), row(1, 34.0)], events=False)
        self.assertFalse(r["has_event_columns"])
        self.assertNotIn("events", r)
        self.assertEqual(r["frames"], 2)

    def test_nearest_rank_and_phase_fields(self):
        rows = [row(i, 30.0 + i, 0, draw=2.0 + i) for i in range(100)]
        r = self.run_agg(rows)
        self.assertEqual(r["frame_ms"]["p95"], 30.0 + 94)  # nearest rank: ceil(0.95*100) = 95th value = index 94
        self.assertEqual(r["events"]["exclusive"]["none"]["phase"]["draw_wall"]["p95"], 2.0 + 94)

    def test_reconcile_script_detects_a_lost_event(self):
        csvp, logp = os.path.join(self.d, "r.csv"), os.path.join(self.d, "r.log")
        rows = [row(0, 33.0, 1), row(1, 33.0, 2), row(2, 33.0, 1 | 2)]
        write(csvp, rows)
        good = ("EVENTS mono=1.000 csvFrame=2 | csv: fx.issue=2 fx.ifail=0 fx.collect=2 fx.cfail=0 raw.issue=0 raw.ifail=0 raw.collect=0 raw.cfail=0"
                " | unattributed: fx.issue=0 fx.ifail=0 fx.collect=0 fx.cfail=0 raw.issue=0 raw.ifail=0 raw.collect=0 raw.cfail=0"
                " | abs: fx.issue=2 fx.ifail=0 fx.collect=2 fx.cfail=0 raw.issue=0 raw.ifail=0 raw.collect=0 raw.cfail=0"
                " | base: fx.issue=0 fx.ifail=0 fx.collect=0 fx.cfail=0 raw.issue=0 raw.ifail=0 raw.collect=0 raw.cfail=0\n")
        with open(logp, "w") as fh:
            fh.write(good)
        script = os.path.join(HERE, "reconcile_events.py")
        self.assertEqual(subprocess.run([sys.executable, script, csvp, logp], capture_output=True).returncode, 0)
        with open(logp, "w") as fh:  # the sampler saw an event the CSV does not have
            fh.write(good.replace("abs: fx.issue=2", "abs: fx.issue=3"))
        self.assertNotEqual(subprocess.run([sys.executable, script, csvp, logp], capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main(verbosity=1)
