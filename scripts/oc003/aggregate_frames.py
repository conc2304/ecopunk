#!/usr/bin/env python3
"""OC-003 offline aggregation of the per-frame stream (EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES).

Global percentiles are computed over the POOLED individual frame durations of
every input file given for one condition -- never by averaging window
percentiles. Rows with exclude=1 (startup / media change / scene change /
scene transition) are dropped unless --include-excluded is given.

Percentile method: nearest-rank on the sorted pooled sample (rank = ceil(p*N)),
so p99 of N frames is an actually-observed frame time.

Sampling-event attribution (CSV columns ev, ev_fail, fx_issue, fx_collect, raw_issue, raw_collect, fx_ifail ...; present only
in streams from the attribution build -- older streams still aggregate, the event sections are skipped):
  ev bitmask of SUCCESSFUL events in the frame: 1 fxIssue, 2 fxCollect, 4 rawIssue, 8 rawCollect (0 = no-event frame).
  A frame can carry several events. Two views are therefore reported and must not be mixed:
    INDEPENDENT populations   frames WITH fxIssue / fxCollect / rawIssue / rawCollect. One frame may appear in several of
                              them, so their frame counts do NOT sum to the number of frames.
    EXCLUSIVE categories      no-event / exactly one event (by kind) / multiple events. Each frame is in exactly one; the
                              counts sum to the total (checked).
  Lag views (frame +1, +2 after an event) are separate from the direct views: a lag frame is the frame whose id follows the
  event frame in the same file (contiguous ids only; excluded frames never counted). "own-event-free" lag frames carry no
  event themselves, so the lag effect is not confounded with the direct effect.
  Association is NOT proof of GPU causation: the probe observes wall/CPU time per segment, not GPU timestamps.

Usage:
  aggregate_frames.py P0=run1.csv,run4.csv P2=run3.csv P2F=run5.csv [--json] [--threshold-ms 100] [--long-ms 40]
  aggregate_frames.py run.csv                       # single unnamed condition
Optional: --sampler-log LOG appends the LAST cumulative SceneColor/VideoColor
stat line of each log (attribution; see docs/reviews/OC-003-performance-attribution-procedure.md).
"""
import argparse
import csv
import json
import math
import re
import sys

EVENT_BITS = [("fxIssue", 1), ("fxCollect", 2), ("rawIssue", 4), ("rawCollect", 8)]
EVENT_COLS = ["ev", "ev_fail", "fx_issue", "fx_collect", "raw_issue", "raw_collect", "fx_ifail", "fx_cfail", "raw_ifail", "raw_cfail"]
PHASE_FIELDS = ["total_ms", "update_wall", "preDraw_wall", "draw_wall", "draw_cpu", "postDraw_wall"]


def nearest_rank(sorted_vals, p):
    if not sorted_vals:
        return float("nan")
    k = max(1, math.ceil(p * len(sorted_vals)))
    return sorted_vals[min(k, len(sorted_vals)) - 1]


def dist(vals):
    s = sorted(vals)
    return {"n": len(s), "p50": nearest_rank(s, 0.50), "p95": nearest_rank(s, 0.95), "p99": nearest_rank(s, 0.99),
            "worst": s[-1] if s else float("nan"), "mean": (sum(s) / len(s)) if s else float("nan")}


def popcount(x):
    return bin(x).count("1")


def load(paths, include_excluded):
    """Returns (rows_kept, per_file_sequences, kept, dropped, reasons, has_events). Each row is a dict of floats/ints."""
    kept_rows, files = [], []
    dropped = 0
    reasons = {"startup": 0, "media": 0, "scene": 0, "transition": 0}
    has_events = None
    for path in paths:
        seq = []
        with open(path, newline="") as fh:
            rd = csv.DictReader(fh)
            ev_here = all(c in (rd.fieldnames or []) for c in EVENT_COLS)
            has_events = ev_here if has_events is None else (has_events and ev_here)
            for row in rd:
                excluded = row["exclude"] == "1"
                if excluded:
                    for key, col in (("startup", "ex_startup"), ("media", "ex_media"), ("scene", "ex_scene"), ("transition", "ex_transition")):
                        if row[col] == "1":
                            reasons[key] += 1
                r = {"frame": int(row["frame"]), "excluded": excluded}
                for f in PHASE_FIELDS:
                    r[f] = float(row[f])
                for c in EVENT_COLS:
                    r[c] = int(row[c]) if ev_here else 0
                seq.append(r)
                if excluded and not include_excluded:
                    dropped += 1
                    continue
                kept_rows.append(r)
        files.append(seq)
    return kept_rows, files, len(kept_rows), dropped, reasons, bool(has_events)


def phase_summary(rows):
    return {f: {"p50": nearest_rank(sorted(r[f] for r in rows), 0.5), "p95": nearest_rank(sorted(r[f] for r in rows), 0.95)} for f in PHASE_FIELDS}


def population(rows, long_ms):
    d = dist([r["total_ms"] for r in rows])
    d["long_frames"] = sum(1 for r in rows if r["total_ms"] > long_ms)
    d["phase"] = phase_summary(rows) if rows else {}
    return d


def event_analysis(rows, files, include_excluded, long_ms):
    out = {"long_ms": long_ms}
    # event totals (successful events and failed attempts; counts, not frames)
    out["event_counts"] = {c: sum(r[c] for r in rows) for c in EVENT_COLS[2:]}
    out["frames_with_failed_attempt"] = sum(1 for r in rows if r["ev_fail"] != 0)
    # INDEPENDENT populations (overlap allowed)
    out["independent"] = {name: population([r for r in rows if r["ev"] & bit], long_ms) for name, bit in EVENT_BITS}
    # EXCLUSIVE categories (each frame in exactly one)
    cats = {"none": [], "multi": []}
    for name, _ in EVENT_BITS:
        cats[name + "Only"] = []
    multi_masks = {}
    for r in rows:
        ev = r["ev"]
        if ev == 0:
            cats["none"].append(r)
        elif popcount(ev) == 1:
            cats[next(n for n, b in EVENT_BITS if b == ev) + "Only"].append(r)
        else:
            cats["multi"].append(r)
            label = "+".join(n for n, b in EVENT_BITS if ev & b)
            multi_masks[label] = multi_masks.get(label, 0) + 1
    out["exclusive"] = {k: population(v, long_ms) for k, v in cats.items()}
    out["exclusive_multi_breakdown"] = multi_masks
    out["exclusive_sum_matches_total"] = sum(len(v) for v in cats.values()) == len(rows)
    # long frames and their event classes (exclusive classes sum to the long-frame total; independent classes overlap)
    long_rows = [r for r in rows if r["total_ms"] > long_ms]
    out["long_frames_total"] = len(long_rows)
    out["long_frames_by_exclusive_class"] = {k: sum(1 for r in v if r["total_ms"] > long_ms) for k, v in cats.items()}
    out["long_frames_by_independent_class(overlapping)"] = {name: sum(1 for r in long_rows if r["ev"] & bit) for name, bit in EVENT_BITS}
    # LAG analysis (separate from the direct views). Uses the full per-file sequence so ids can be checked for contiguity.
    lag = {}
    for name, bit in EVENT_BITS:
        for k in (1, 2):
            allrows, freerows = [], []
            for seq in files:
                by_frame = {r["frame"]: r for r in seq}
                for r in seq:
                    if (r["excluded"] and not include_excluded) or not (r["ev"] & bit):
                        continue
                    t = by_frame.get(r["frame"] + k)
                    if t is None or (t["excluded"] and not include_excluded):
                        continue
                    allrows.append(t)
                    if t["ev"] == 0:
                        freerows.append(t)
            lag[f"{name}+{k}"] = {"all_lag_frames": population(allrows, long_ms), "own_event_free_lag_frames": population(freerows, long_ms)}
    out["lag"] = lag
    return out


def summarize(name, paths, args):
    rows, files, kept, dropped, reasons, has_events = load(paths, args.include_excluded)
    total = [r["total_ms"] for r in rows]
    s = sorted(total)
    draw_cpu = [r["draw_cpu"] for r in rows]
    out = {
        "condition": name,
        "files": len(paths),
        "frames": kept,
        "excluded_frames": dropped,
        "excluded_by_reason(overlapping)": reasons,
        "frame_ms": {
            "p50": nearest_rank(s, 0.50),
            "p95": nearest_rank(s, 0.95),
            "p99": nearest_rank(s, 0.99),
            "worst": s[-1] if s else float("nan"),
            "mean": (sum(s) / len(s)) if s else float("nan"),
        },
        "mean_draw_cpu_ms": (sum(draw_cpu) / len(draw_cpu)) if draw_cpu else float("nan"),
        "frames_over_threshold": sum(1 for v in total if v > args.threshold_ms),
        "threshold_ms": args.threshold_ms,
        "has_event_columns": has_events,
    }
    if has_events:
        out["events"] = event_analysis(rows, files, args.include_excluded, args.long_ms)
    return out


STAT_RE = re.compile(r"(SceneColor|VideoColor)\S*\s.*stream=(FX|RAW).*")


def last_sampler_lines(path):
    last = {}
    with open(path, errors="replace") as fh:
        for line in fh:
            m = STAT_RE.search(line)
            if m:
                last[m.group(2)] = line.strip()
    return last


def fmt_pop(label, d):
    if d["n"] == 0:
        return f"  {label:<26}{0:>8}   (no frames)"
    ph = d["phase"]
    return (f"  {label:<26}{d['n']:>8}{d['p50']:>8.2f}{d['p95']:>8.2f}{d['p99']:>8.2f}{d['worst']:>9.2f}{d['long_frames']:>7}"
            f"   draw wall {ph['draw_wall']['p50']:.2f}/{ph['draw_wall']['p95']:.2f} cpu {ph['draw_cpu']['p50']:.2f}/{ph['draw_cpu']['p95']:.2f}"
            f"  upd {ph['update_wall']['p50']:.2f}/{ph['update_wall']['p95']:.2f}  pre {ph['preDraw_wall']['p50']:.2f}/{ph['preDraw_wall']['p95']:.2f}"
            f"  post {ph['postDraw_wall']['p50']:.2f}/{ph['postDraw_wall']['p95']:.2f}")


def print_events(r):
    e = r["events"]
    print(f"\n== {r['condition']}: sampling-event attribution (ms; long frame = > {e['long_ms']:g} ms; phase = p50/p95 wall unless 'cpu')")
    c = e["event_counts"]
    print("  events (counts): " + "  ".join(f"{k}={v}" for k, v in c.items()) + f"   frames with a failed attempt: {e['frames_with_failed_attempt']}")
    hdr = f"  {'population':<26}{'frames':>8}{'p50':>8}{'p95':>8}{'p99':>8}{'worst':>9}{'long':>7}"
    print("\n  INDEPENDENT populations (a frame may be in several; counts do not sum)")
    print(hdr)
    for k, d in e["independent"].items():
        print(fmt_pop("with " + k, d))
    print("\n  EXCLUSIVE categories (each frame in exactly one; sums to the total: " + ("yes" if e["exclusive_sum_matches_total"] else "NO - BUG") + ")")
    print(hdr)
    for k, d in e["exclusive"].items():
        print(fmt_pop(k, d))
    if e["exclusive_multi_breakdown"]:
        print("  multi-event combinations: " + ", ".join(f"{k}: {v}" for k, v in sorted(e["exclusive_multi_breakdown"].items())))
    print(f"\n  long frames: {e['long_frames_total']} total; by EXCLUSIVE class (sums to total): " + ", ".join(f"{k}={v}" for k, v in e["long_frames_by_exclusive_class"].items()))
    print("               by INDEPENDENT class (overlapping): " + ", ".join(f"{k}={v}" for k, v in e["long_frames_by_independent_class(overlapping)"].items()))
    print("\n  LAG (separate from the direct views; frame +k after an event frame, contiguous ids only)")
    print(hdr)
    for k, v in e["lag"].items():
        print(fmt_pop(k + " all", v["all_lag_frames"]))
        print(fmt_pop(k + " own-event-free", v["own_event_free_lag_frames"]))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="NAME=a.csv,b.csv or a bare path")
    ap.add_argument("--threshold-ms", type=float, default=100.0)
    ap.add_argument("--long-ms", type=float, default=40.0, help="long-frame threshold for the event analysis (default 40 ms)")
    ap.add_argument("--include-excluded", action="store_true", help="do not drop exclude=1 rows")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--sampler-log", action="append", default=[], help="runtime stdout/stderr log with sampler stat lines")
    args = ap.parse_args()

    results = []
    for item in args.inputs:
        name, _, rest = item.partition("=") if "=" in item else ("run", "", item)
        results.append(summarize(name, [p for p in rest.split(",") if p], args))

    if args.json:
        print(json.dumps(results, indent=2))
    else:
        hdr = f"{'cond':<8}{'frames':>9}{'excl':>8}{'p50':>9}{'p95':>9}{'p99':>9}{'worst':>10}{'mean':>9}{'drawCPU':>9}{'>' + format(args.threshold_ms, 'g') + 'ms':>9}"
        print(hdr)
        for r in results:
            f = r["frame_ms"]
            print(f"{r['condition']:<8}{r['frames']:>9}{r['excluded_frames']:>8}{f['p50']:>9.2f}{f['p95']:>9.2f}{f['p99']:>9.2f}{f['worst']:>10.2f}{f['mean']:>9.2f}{r['mean_draw_cpu_ms']:>9.2f}{r['frames_over_threshold']:>9}")
        print("(all ms; percentiles are nearest-rank over pooled individual frames)")
        for r in results:
            if r["has_event_columns"]:
                print_events(r)
            else:
                print(f"\n== {r['condition']}: no event columns in this stream (recorded before the attribution build); event sections skipped")
    for log in args.sampler_log:
        for stream, line in sorted(last_sampler_lines(log).items()):
            print(f"[{log}] last {stream} stats: {line}", file=sys.stderr)


if __name__ == "__main__":
    main()
