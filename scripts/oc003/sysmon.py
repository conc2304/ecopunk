#!/usr/bin/env python3
"""OC-003-F: lightweight EXTERNAL system monitor (RSS + thermal + CPU frequency) for one runtime process. Standard library only.

Used by run_campaign.py during a run; it never runs inside the runtime, adds nothing to the frame loop and is a separate process-level
observation (so it cannot distort the GPU/CPU acquisition costs it sits beside). What is NOT available is recorded as blank, never as 0.

  sample(pid)                       -> {"rss_kb": int|None, "temps_mC": {zone: int}, "freq_khz": {cpuN: int}}
  SysMonitor(pid, path, interval)   -> .start() / .stop(); appends rows to <path> (CSV: t,rss_kb,temp_<zone>...,freq_<cpu>...)
  summarize(path)                   -> {"samples", "rss_kb": {first,last,max,growth}, "temp_mC": {zone: {max,last}}, "freq_khz": {cpu: {min,max}}, "throttle_hint"}
  sysmon.py summarize FILE.sys.csv  -> prints the JSON

Linux (ROCK Pi): /proc/<pid>/status VmRSS, /sys/class/thermal/thermal_zone*/temp (milli-C), /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq.
macOS (desktop rehearsal): RSS via `ps -o rss=`; no thermal / frequency sources -> blank.
`throttle_hint` is only a HINT: a CPU frequency that dropped below 90% of the maximum seen on that CPU while its zone was warm; the kernel
throttling lines in dmesg (run_campaign.py) remain the stop condition.
"""
import csv
import glob
import json
import os
import subprocess
import sys
import threading
import time


def _read_int(path):
    try:
        with open(path) as fh:
            return int(fh.read().strip())
    except (OSError, ValueError):
        return None


def rss_kb(pid):
    try:
        with open(f"/proc/{pid}/status") as fh:
            for line in fh:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1])
    except (OSError, ValueError, IndexError):
        pass
    try:
        out = subprocess.run(["ps", "-o", "rss=", "-p", str(pid)], capture_output=True, text=True, timeout=5).stdout.strip()
        return int(out) if out else None
    except (OSError, ValueError, subprocess.SubprocessError):
        return None


def sample(pid):
    temps = {}
    for z in sorted(glob.glob("/sys/class/thermal/thermal_zone*")):
        v = _read_int(os.path.join(z, "temp"))
        if v is not None:
            temps[os.path.basename(z).replace("thermal_", "")] = v
    freqs = {}
    for c in sorted(glob.glob("/sys/devices/system/cpu/cpu[0-9]*")):
        v = _read_int(os.path.join(c, "cpufreq", "scaling_cur_freq"))
        if v is not None:
            freqs[os.path.basename(c)] = v
    return {"rss_kb": rss_kb(pid), "temps_mC": temps, "freq_khz": freqs}


class SysMonitor:
    def __init__(self, pid, path, interval=5.0):
        self.pid, self.path, self.interval = pid, path, interval
        self._stop = threading.Event()
        self._th = None
        self._cols = None

    def _row(self, s):
        return [f"{time.time():.3f}", "" if s["rss_kb"] is None else s["rss_kb"]] + [s["temps_mC"].get(z, "") for z in self._cols[0]] + [s["freq_khz"].get(c, "") for c in self._cols[1]]

    def _run(self):
        first = sample(self.pid)
        self._cols = (sorted(first["temps_mC"]), sorted(first["freq_khz"]))
        with open(self.path, "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["t", "rss_kb"] + ["temp_" + z for z in self._cols[0]] + ["freq_" + c for c in self._cols[1]])
            w.writerow(self._row(first))
            fh.flush()
            while not self._stop.wait(self.interval):
                w.writerow(self._row(sample(self.pid)))
                fh.flush()

    def start(self):
        self._th = threading.Thread(target=self._run, daemon=True)
        self._th.start()
        return self

    def stop(self):
        self._stop.set()
        if self._th:
            self._th.join(timeout=10)


def summarize(path):
    rows = []
    try:
        with open(path, newline="") as fh:
            rows = list(csv.DictReader(fh))
    except OSError:
        return {"samples": 0, "available": False}
    out = {"samples": len(rows), "available": bool(rows)}
    rss = [int(r["rss_kb"]) for r in rows if r.get("rss_kb")]
    out["rss_kb"] = {"first": rss[0], "last": rss[-1], "max": max(rss), "growth": rss[-1] - rss[0]} if rss else None
    temps, freqs = {}, {}
    for key in (rows[0].keys() if rows else []):
        if not key.startswith(("temp_", "freq_")):
            continue
        vals = [int(r[key]) for r in rows if r.get(key)]
        if not vals:
            continue
        if key.startswith("temp_"):
            temps[key[5:]] = {"max": max(vals), "last": vals[-1]}
        elif key.startswith("freq_"):
            freqs[key[5:]] = {"min": min(vals), "max": max(vals), "last": vals[-1]}
    out["temp_mC"], out["freq_khz"] = temps, freqs
    hot = max((v["max"] for v in temps.values()), default=0) >= 70000
    out["throttle_hint"] = bool(hot and any(v["min"] < 0.9 * v["max"] for v in freqs.values()))
    out["thermal_available"] = bool(temps)
    return out


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "summarize":
        print(json.dumps(summarize(sys.argv[2]), indent=2))
    else:
        sys.exit(__doc__)
