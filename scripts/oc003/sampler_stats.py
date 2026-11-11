#!/usr/bin/env python3
"""OC-003: per-stage sampler statistics from the runtime stdout logs (SceneColor / VideoColor stat lines).

For every log given, takes the LAST stat line of each stream (FX, RAW) and prints, one row per stream:
  stage p50/p95/max (ms): submit wait convert push total latency widen predCpu
  counters: issued completed gaps dropGen busy notReady discardTimeout discardWaitFailed invalid
            halfFailures glErrors verifySamples verifyMismatchSamples readbacks readbackBytes
The ring behind p50/p95 holds the latest 256 samples (~2 min at 2 Hz); max and counters are cumulative since start, so
compare runs of equal length. Warm-up is included in cumulative figures (see docs/reviews/OC-003-...-procedure.md).
Usage: sampler_stats.py LOG [LOG ...] [--csv]
"""
import argparse
import re
import sys

TRIPLE = r"([\d.eE+-]+)/([\d.eE+-]+)/([\d.eE+-]+)"
STAGES = [("submit", r"submit=" + TRIPLE), ("wait", r"wait=" + TRIPLE), ("convert", r"convert=" + TRIPLE), ("push", r"push=" + TRIPLE),
          ("total", r"total=" + TRIPLE), ("latency", r"latencyMs\[p50/p95/max\]=" + TRIPLE), ("widen", r"widenMs\[p50/p95/max\]=" + TRIPLE),
          ("predCpu", r"predCpuMs\[p50/p95/max\]=" + TRIPLE)]
# OC-003-F CPU RAW (stream=CPURAW): access / reduce / analyze are measured independently, wall and thread-CPU
for _n in ("access", "reduce", "analyze", "total"):
    for _k in ("Wall", "Cpu"):
        STAGES.append((_n + _k, _n + _k + r"Ms\[p50/p95/max\]=" + TRIPLE))
COUNTERS = ["issued", "done", "completed", "gaps", "dropGen", "droppedGen", "busy", "droppedBusy", "notReady", "discardTimeout", "discardWaitFailed",
            "invalid", "halfFailures", "glErrors", "verifySamples", "verifyMismatchSamples", "verifyMismatchWords", "readbacks", "readbackBytes", "halfReadbacks",
            "due", "samples", "noPixels", "badFormat", "dimMismatch", "domValid", "vibValid", "vibInvalid"]


def last_lines(path):
    last = {}
    with open(path, errors="replace") as fh:
        for line in fh:
            m = re.search(r"stream=(FX|RAW|CPURAW)\b", line)
            if m:
                last[m.group(1)] = line
    return last


def parse(line):
    out = {}
    for name, rx in STAGES:
        m = re.search(rx, line)
        if m:
            out[name] = "/".join(f"{float(x):.3f}" for x in m.groups())
    for k in COUNTERS:
        m = re.search(r"\b" + k + r"=(\d+)", line)
        if m:
            out[k] = m.group(1)
    for k in ("effMode", "rb", "gpuLinear", "defer"):
        m = re.search(r"\b" + k + r"=(\S+)", line)
        if m:
            out[k] = m.group(1)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--csv", action="store_true")
    a = ap.parse_args()
    rows = []
    for path in a.logs:
        for stream, line in sorted(last_lines(path).items()):
            rows.append((path, stream, parse(line)))
    keys = ["effMode", "rb", "defer"] + [s for s, _ in STAGES] + [c for c in COUNTERS]
    if a.csv:
        print(",".join(["log", "stream"] + keys))
        for path, stream, d in rows:
            print(",".join([path, stream] + [d.get(k, "") for k in keys]))
    else:
        for path, stream, d in rows:
            print(f"== {path} [{stream}]")
            for k in keys:
                if k in d:
                    print(f"   {k:<20}{d[k]}")
    if not rows:
        print("no SceneColor/VideoColor stat lines with stream= found (needs the OC-003 attribution build and ..._STATS=1)", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
