#!/usr/bin/env python3
"""Global frame-time distribution from RAW frame records (frames.csv), pooled per config across repeats.

usage: global_frame_stats.py <root>      root contains r*/<config>/frames.csv (and optionally phase.log)

Percentiles are taken over every frame interval of every repeat of a config, never as an average of
one-second WINDOW percentiles (a mean of window p95s is not the p95 of the run, and hides rare long frames).
For comparison the script also prints the mean of the WINDOW p95s from phase.log, when present.
"""
import csv, glob, os, re, sys
from collections import defaultdict

root = sys.argv[1]
SKIP_S = 3.0  # drop startup


def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(p * len(v)))] if v else float("nan")


frames = defaultdict(list)
winp95 = defaultdict(list)
n_runs = defaultdict(int)
for f in sorted(glob.glob(os.path.join(root, "r*", "*", "frames.csv"))):
    cfg = f.split(os.sep)[-2]
    rows = list(csv.DictReader(open(f)))
    if not rows:
        continue
    t0 = float(rows[0]["t"])
    frames[cfg] += [float(r["frameDtMs"]) for r in rows if float(r["t"]) - t0 >= SKIP_S]
    n_runs[cfg] += 1
    ph = os.path.join(os.path.dirname(f), "phase.log")
    if os.path.exists(ph):
        for l in open(ph):
            m = l.startswith("WINDOW") and re.search(r"frame p50=([\d.]+) p95=([\d.]+)", l)
            if m:
                winp95[cfg].append(float(m.group(2)))

print(f"{'config':22s} runs frames   p50     p95     p99     p99.9   max    >50ms  >100ms | mean(window p95)")
for cfg in sorted(frames):
    v = frames[cfg]
    w = winp95.get(cfg)
    print(f"{cfg:22s} {n_runs[cfg]:4d} {len(v):6d} {pct(v,.5):7.2f} {pct(v,.95):7.2f} {pct(v,.99):7.2f} {pct(v,.999):7.2f} {max(v):7.1f}"
          f" {sum(1 for x in v if x>50):6d} {sum(1 for x in v if x>100):6d} | {sum(w)/len(w):.2f}" if w else
          f"{cfg:22s} {n_runs[cfg]:4d} {len(v):6d} {pct(v,.5):7.2f} {pct(v,.95):7.2f} {pct(v,.99):7.2f} {pct(v,.999):7.2f} {max(v):7.1f}"
          f" {sum(1 for x in v if x>50):6d} {sum(1 for x in v if x>100):6d} |")
