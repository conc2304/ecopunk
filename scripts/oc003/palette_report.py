#!/usr/bin/env python3
"""OC-003-F palette-analysis report for ONE run's EXPERIENCE_RUNTIME_PALETTE_DIAG directory. Diagnostic only; reads files, writes a report.

  palette_report.py PALETTE_DIR --replay PATH/to/palette_replay [--out DIR] [--max-pairs N]

Checks (each is reported as PASS / FAIL / N/A, never silently skipped):
  replay       every stored grid, re-analysed offline by the UNCHANGED production palette::analyze (palette_replay), reproduces the metrics
               the runtime wrote to the CSV row that references it (tolerance 2e-5 = the CSV's 5-decimal rounding)
  determinism  running the replay tool twice over the same grids gives byte-identical output
  cpu_vs_gpu   (only when both raw_cpu and raw_gpu streams have grids) rows of the same generation whose timestamps are within --pair-dt s
               are paired; grids are compared cell by cell in sRGB-ENCODED space (what a viewer sees) and the CSV AVG / palette metrics are
               compared. The thresholds are informational engineering bounds chosen for this diagnostic, NOT a product contract:
               median per-pair mean |dE| <= 0.02 (about 5/255) and median |dAVG| <= 0.01, evaluated on SAME-FRAME pairs only (|dt| <= --same-frame-dt); with fewer than --min-same-frame such pairs the result is INCONCLUSIVE.
Exit 0 when no check FAILED (N/A and INCONCLUSIVE are not failures but are printed). The report never claims hardware results: it describes whatever
directory it is given.
"""
import argparse
import csv
import json
import math
import os
import statistics
import struct
import subprocess
import sys

W, H = 160, 90
METRIC_COLS = ["chromaticFraction", "chromaticEnergy", "complexity", "familiesActive", "highlightOcc", "shadowOcc"]
TOL = 2e-5
MEAN_ABS_LIMIT = 0.02
AVG_LIMIT = 0.01


def load_csv(path):
    if not os.path.isfile(path):
        return []
    with open(path, newline="") as fh:
        return list(csv.DictReader(fh))


def read_grid(path):
    with open(path, "rb") as fh:
        raw = fh.read()
    n = W * H * 3
    if len(raw) != n * 4:
        raise ValueError(f"{path}: {len(raw)} bytes, expected {n * 4}")
    return struct.unpack(f"<{n}f", raw)


def oetf(v):
    v = 0.0 if v < 0 else 1.0 if v > 1 else v
    return 12.92 * v if v <= 0.0031308 else 1.055 * (v ** (1 / 2.4)) - 0.055


def run_replay(replay, files):
    if not files:
        return ""
    r = subprocess.run([replay] + files, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"palette_replay failed: {r.stderr.strip()}")
    return r.stdout


def pct(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p * len(xs)))] if xs else float("nan")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dir")
    ap.add_argument("--replay", required=True, help="path to the built palette_replay tool (sketches/experience_runtime/test/palette_replay)")
    ap.add_argument("--out", help="write palette_report.json / palette_report.md here (default: <dir>)")
    ap.add_argument("--pair-dt", type=float, default=0.06, help="max |t_cpu - t_gpu| (s) for a pair (default 0.06 = about two frames at 30 fps)")
    ap.add_argument("--max-pairs", type=int, default=400)
    ap.add_argument("--same-frame-dt", type=float, default=0.017, help="pairs closer than this (about half a 30 fps frame) count as the SAME decoded frame; the cpu_vs_gpu verdict uses only these (default 0.017)")
    ap.add_argument("--min-same-frame", type=int, default=5, help="fewer same-frame pairs than this -> cpu_vs_gpu is INCONCLUSIVE, not PASS (default 5)")
    a = ap.parse_args()
    out = a.out or a.dir
    os.makedirs(out, exist_ok=True)
    res = {"dir": os.path.abspath(a.dir), "checks": {}}
    summary_path = os.path.join(a.dir, "palette_summary.json")
    res["summary"] = json.load(open(summary_path)) if os.path.isfile(summary_path) else None
    # palette_summary.json is written by Sink::close() from ExperienceRuntime::exit(). A run ended by the planned SIGINT (timeout -s INT) is
    # terminated by the default disposition without running exit() (the app installs no handler), so it normally has NO summary; the CSV rows
    # (flushed every 8) and the grid files are already on disk, up to 7 trailing rows per stream are lost. That is recorded, not an error.
    res["clean_close"] = res["summary"] is not None
    if not any(os.path.isfile(os.path.join(a.dir, f"palette_{s}.csv")) for s in ("fx", "raw_gpu", "raw_cpu")):
        print(f"no palette_*.csv in {a.dir}: palette analysis was off or the run produced nothing")
        sys.exit(1)

    streams = {s: load_csv(os.path.join(a.dir, f"palette_{s}.csv")) for s in ("fx", "raw_gpu", "raw_cpu")}
    res["rows"] = {s: len(r) for s, r in streams.items()}
    failures = []

    # ---- replay + determinism ------------------------------------------------------------------------------
    files, expect = [], []
    for s, rows in streams.items():
        for r in rows:
            if r["grid_file"] and r["metrics_valid"] == "1":
                files.append(os.path.join(a.dir, r["grid_file"]))
                expect.append((s, r))
    if not files:
        res["checks"]["replay"] = res["checks"]["determinism"] = "N/A (no stored grids)"
    else:
        o1 = run_replay(a.replay, files)
        o2 = run_replay(a.replay, files)
        res["checks"]["determinism"] = "PASS" if o1 == o2 else "FAIL"
        worst, bad = 0.0, 0
        for line, (s, r) in zip(o1.strip().splitlines(), expect):
            f = line.split(",")
            got = [float(x) for x in f[2:8]]
            want = [float(r[c]) for c in METRIC_COLS]
            for g, w in zip(got, want):
                worst = max(worst, abs(g - w))
            if any(abs(g - w) > TOL for g, w in zip(got, want)):
                bad += 1
        res["checks"]["replay"] = "PASS" if bad == 0 and len(o1.strip().splitlines()) == len(files) else "FAIL"
        res["replay"] = {"grids": len(files), "mismatching_rows": bad, "worst_abs_diff": worst}

    # ---- CPU RAW vs GPU RAW ---------------------------------------------------------------------------------
    cpu = [r for r in streams["raw_cpu"] if r["metrics_valid"] == "1"]
    gpu = [r for r in streams["raw_gpu"] if r["metrics_valid"] == "1"]
    pairs = []
    for c in cpu:
        best = None
        for g in gpu:
            if g["gen"] != c["gen"]:
                continue
            dt = abs(float(g["t"]) - float(c["t"]))
            if dt <= a.pair_dt and (best is None or dt < best[0]):
                best = (dt, g)
        if best:
            pairs.append((c, best[1], best[0]))
    gridpairs = [(c, g, dt) for c, g, dt in pairs if c["grid_file"] and g["grid_file"]][: a.max_pairs]
    res["cpu_vs_gpu"] = {"cpu_rows": len(cpu), "gpu_rows": len(gpu), "pairs": len(pairs), "grid_pairs": len(gridpairs)}
    if not cpu or not gpu:
        res["checks"]["cpu_vs_gpu"] = "N/A (needs both raw_cpu and raw_gpu streams in one run: condition PCMP)"
    elif not gridpairs:
        res["checks"]["cpu_vs_gpu"] = "N/A (no row pair with stored grids on both sides; raise PALETTE_DIAG_GRIDS / lower PALETTE_DIAG_EVERY)"
    else:
        def stats(sub):
            means, maxes, p99s, davg, dmetrics = [], [], [], [], {c: [] for c in METRIC_COLS}
            for c, g, _dt in sub:
                gc, gg = read_grid(os.path.join(a.dir, c["grid_file"])), read_grid(os.path.join(a.dir, g["grid_file"]))
                d = [abs(oetf(x) - oetf(y)) for x, y in zip(gc, gg)]
                means.append(sum(d) / len(d))
                maxes.append(max(d))
                p99s.append(pct(d, 0.99))
                davg.append(max(abs(float(c[k]) - float(g[k])) for k in ("avgR", "avgG", "avgB")))
                for col in METRIC_COLS:
                    dmetrics[col].append(abs(float(c[col]) - float(g[col])))
            if not means:
                return None
            return {"pairs": len(means),
                    "mean_abs_encoded_diff": {"median": statistics.median(means), "p95": pct(means, 0.95), "max": max(means)},
                    "cell_max_abs_encoded_diff": {"median": statistics.median(maxes), "max": max(maxes)},
                    "cell_p99_abs_encoded_diff_median": statistics.median(p99s),
                    "avg_linear_max_channel_diff": {"median": statistics.median(davg), "p95": pct(davg, 0.95), "max": max(davg)},
                    "metric_abs_diff_median": {c: statistics.median(v) for c, v in dmetrics.items()}}
        same = [x for x in gridpairs if x[2] <= a.same_frame_dt]
        res["cpu_vs_gpu"]["dt_s"] = {"median": statistics.median([x[2] for x in gridpairs]), "max": max(x[2] for x in gridpairs)}
        # All pairs include a temporal offset (the two samplers' due times differ by about one video frame), so they are INFORMATIVE only.
        res["cpu_vs_gpu"]["all_pairs_incl_frame_offset"] = stats(gridpairs)
        res["cpu_vs_gpu"]["same_frame_pairs"] = stats(same)
        res["cpu_vs_gpu"]["limits"] = {"median_mean_abs_encoded": MEAN_ABS_LIMIT, "median_avg": AVG_LIMIT, "same_frame_dt_s": a.same_frame_dt,
                                      "min_same_frame_pairs": a.min_same_frame, "status": "informational engineering bounds, not a product contract"}
        sf = res["cpu_vs_gpu"]["same_frame_pairs"]
        if sf is None or sf["pairs"] < a.min_same_frame:
            res["checks"]["cpu_vs_gpu"] = (f"INCONCLUSIVE (only {0 if sf is None else sf['pairs']} same-frame pairs, need {a.min_same_frame}; "
                                           "the all-pairs figures include a ~1-frame temporal offset; run PCMP longer)")
        else:
            ok = sf["mean_abs_encoded_diff"]["median"] <= MEAN_ABS_LIMIT and sf["avg_linear_max_channel_diff"]["median"] <= AVG_LIMIT
            res["checks"]["cpu_vs_gpu"] = "PASS" if ok else "FAIL"

    failures = [k for k, v in res["checks"].items() if v == "FAIL"]
    res["failed"] = failures
    with open(os.path.join(out, "palette_report.json"), "w") as fh:
        json.dump(res, fh, indent=2)
    L = [f"# Palette report: {os.path.basename(os.path.abspath(a.dir))}", "", "| check | result |", "|---|---|"]
    L += [f"| {k} | {v} |" for k, v in res["checks"].items()]
    L += ["", f"rows per stream: {res['rows']}", f"clean sink close (palette_summary.json present): {res['clean_close']}"
          + ("" if res["clean_close"] else " -- run ended by SIGINT; up to 7 trailing CSV rows per stream may be missing")]
    if "replay" in res:
        L.append(f"replay: {res['replay']}")
    if "mean_abs_encoded_diff" in res["cpu_vs_gpu"]:
        L.append(f"cpu_vs_gpu: {json.dumps(res['cpu_vs_gpu'], indent=1)}")
    else:
        L.append(f"cpu_vs_gpu: {res['cpu_vs_gpu']}")
    with open(os.path.join(out, "palette_report.md"), "w") as fh:
        fh.write("\n".join(L) + "\n")
    print("\n".join(L))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
