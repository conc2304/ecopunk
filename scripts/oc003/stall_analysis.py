#!/usr/bin/env python3
"""OC-003-D: issue-conditioned next-frame STALL analysis of the frame-phase CSV (measurement only; no interpretation in the numbers).

  stall_analysis.py RUN.frames.csv [RUN2.frames.csv ...] [--long-ms 40] [--json]      (several files = one pooled population)

Every statistic is computed from steady-state frames (exclude == 0). A frame is only related to an EARLIER frame when the frame ids are
consecutive AND every frame in between is also steady-state: a warm-up/media/scene exclusion or a gap in the ids breaks the relation, and the
later frame is then classed as `quiet`. Event bits: 1 fxIssue, 2 fxCollect, 4 rawIssue, 8 rawCollect. ISSUE = 1|4, COLLECT = 2|8.

Exclusive frame classes (each steady frame is in exactly one, so the per-class long-frame counts always sum to the total):
  event                  the frame has at least one sampling event itself (direct)
  no_event_after_issue   no event, previous frame contained an issue          (delayed association, lag +1)
  no_event_after_collect no event, previous frame contained a collect and no issue (lag +1)
  no_event_lag2          no event, previous frame no event, the frame before that had an event (lag +2)
  quiet                  everything else
A long frame is never counted in two classes. Overlapping populations are reported separately and never added.

The numbers describe what the probe measured (wall/CPU per frame segment, event bits). They do not say WHY a frame was long.
"""
import argparse
import csv
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import aggregate_frames as agg  # noqa: E402

ISSUE, COLLECT = 1 | 4, 2 | 8
CLASSES = ("event", "no_event_after_issue", "no_event_after_collect", "no_event_lag2", "quiet")
NUM = ("total_ms", "update_wall", "update_cpu", "preDraw_wall", "preDraw_cpu", "draw_wall", "draw_cpu", "glFinish_wall", "glFinish_cpu",
       "postDraw_wall", "postDraw_cpu")


def read_frames(path):
    """All rows of one frame CSV in file order (excluded rows included: they are needed to break relations), typed."""
    rows = []
    with open(path, newline="") as fh:
        rd = csv.DictReader(fh)
        have_ev = "ev" in (rd.fieldnames or [])
        for d in rd:
            r = {"frame": int(d["frame"]), "excluded": d["exclude"] == "1", "ev": int(d["ev"]) if have_ev else 0}
            for k in NUM:
                r[k] = float(d[k])
            rows.append(r)
    return rows


def related(rows, i, j):
    """True when rows i..j (i <= j) are consecutive frame ids and all steady-state."""
    if i < 0 or j >= len(rows):
        return False
    for k in range(i, j + 1):
        if rows[k]["excluded"]:
            return False
        if k > i and rows[k]["frame"] != rows[k - 1]["frame"] + 1:
            return False
    return True


def classify(rows, i):
    """Class of steady-state frame i (see module docstring)."""
    ev = rows[i]["ev"]
    if ev:
        return "event"
    if related(rows, i - 1, i):
        prev = rows[i - 1]["ev"]
        if prev & ISSUE:
            return "no_event_after_issue"
        if prev & COLLECT:
            return "no_event_after_collect"
        if related(rows, i - 2, i) and rows[i - 2]["ev"]:
            return "no_event_lag2"
    return "quiet"


def _dist(vals):
    s = sorted(vals)
    if not s:
        return None
    return {"n": len(s), "p50": agg.nearest_rank(s, 0.5), "p95": agg.nearest_rank(s, 0.95), "p99": agg.nearest_rank(s, 0.99), "max": s[-1]}


def _seg(rows_sel, seg):
    wall = [r[seg + "_wall"] for r in rows_sel]
    cpu = [r[seg + "_cpu"] for r in rows_sel]
    blocked = [max(0.0, w - c) for w, c in zip(wall, cpu)]
    return {"wall": _dist(wall), "cpu": _dist(cpu), "blocked": _dist(blocked)}


def analyze(runs, long_ms=40.0):
    """runs = list of per-file row lists (one run each). Returns a JSON-able dict for the pooled population of those runs."""
    steady, cls_of, after_issue_own = [], [], {"none": [0, 0], "collect": [0, 0], "issue_only": [0, 0]}
    cls_count = {c: 0 for c in CLASSES}
    cls_long = {c: 0 for c in CLASSES}
    for rows in runs:
        for i, r in enumerate(rows):
            if r["excluded"]:
                continue
            c = classify(rows, i)
            steady.append(r)
            cls_of.append(c)
            cls_count[c] += 1
            if r["total_ms"] > long_ms:
                cls_long[c] += 1
            if related(rows, i - 1, i) and rows[i - 1]["ev"] & ISSUE:
                own = "none" if not r["ev"] else ("collect" if r["ev"] & COLLECT else "issue_only")
                after_issue_own[own][0] += 1
                after_issue_own[own][1] += 1 if r["total_ms"] > long_ms else 0
    n = len(steady)
    out = {"long_ms": long_ms, "frames": n}
    if n == 0:
        return out
    totals = [r["total_ms"] for r in steady]
    d = _dist(totals)
    out["frame_ms"] = {"p50": d["p50"], "p95": d["p95"], "p99": d["p99"], "worst": d["max"], "mean": sum(totals) / n}
    out["effective_fps"] = round(1000.0 * n / sum(totals), 4)  # steady-state frames / steady-state frame time: throughput, not the display rate
    long_total = sum(1 for t in totals if t > long_ms)
    out["long_frames"] = long_total
    assert sum(cls_long.values()) == long_total, "classes must partition the long frames"
    out["classes"] = {c: {"frames": cls_count[c], "long": cls_long[c]} for c in CLASSES}
    n_ai = sum(v[0] for v in after_issue_own.values())
    l_ai = sum(v[1] for v in after_issue_own.values())
    out["after_issue"] = {"frames": n_ai, "long": l_ai, "long_rate": (l_ai / n_ai) if n_ai else None,
                          "by_own_event": {k: {"frames": v[0], "long": v[1], "long_rate": (v[1] / v[0]) if v[0] else None} for k, v in after_issue_own.items()}}
    nai = out["classes"]["no_event_after_issue"]
    out["no_event_after_issue_long_rate"] = (nai["long"] / nai["frames"]) if nai["frames"] else None
    sel = {"all": steady,
           "quiet": [r for r, c in zip(steady, cls_of) if c == "quiet"],
           "issue_frame": [r for r in steady if r["ev"] & ISSUE],
           "collect_frame": [r for r in steady if r["ev"] & COLLECT],
           "no_event_after_issue": [r for r, c in zip(steady, cls_of) if c == "no_event_after_issue"]}
    out["postDraw"] = {k: _seg(v, "postDraw") for k, v in sel.items() if v}
    out["glFinish"] = {k: _seg(v, "glFinish") for k, v in sel.items() if v}
    out["draw"] = {k: _seg(v, "draw") for k, v in sel.items() if v}
    return out


def fence_notready(stdout_paths):
    """Cumulative `notReady` / `issued` of the LAST stat line per stream per log (derived: excess = notReady - issued; each sample is polled
    unsignalled once by construction, so the excess counts the additional polls that found the fence still unsignalled). Per-run totals only."""
    import sampler_stats as sst
    res = {}
    for p in stdout_paths:
        if not os.path.isfile(p):
            continue
        for stream, line in sst.last_lines(p).items():
            d = sst.parse(line)
            if "notReady" in d and "issued" in d:
                e = res.setdefault(stream, {"issued": 0, "notReady": 0})
                e["issued"] += int(d["issued"])
                e["notReady"] += int(d["notReady"])
    for e in res.values():
        e["excess"] = e["notReady"] - e["issued"]
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+")
    ap.add_argument("--long-ms", type=float, default=40.0)
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    res = analyze([read_frames(p) for p in a.csv], a.long_ms)
    if a.json:
        print(json.dumps(res, indent=2))
        return
    print(f"frames {res['frames']}  effective fps {res.get('effective_fps')}  long>{a.long_ms:g}ms {res.get('long_frames')}")
    for c, v in res.get("classes", {}).items():
        print(f"  {c:24s} frames {v['frames']:6d}  long {v['long']:5d}")
    ai = res.get("after_issue")
    if ai:
        print(f"  frames after an issue frame: {ai['frames']}, long {ai['long']} ({100 * (ai['long_rate'] or 0):.1f}%)")
        for k, v in ai["by_own_event"].items():
            print(f"    own {k:11s} frames {v['frames']:6d} long {v['long']:5d}")


if __name__ == "__main__":
    main()
