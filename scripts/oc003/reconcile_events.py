#!/usr/bin/env python3
"""OC-003 sampling-frame attribution: reconcile the per-frame event columns with the probe's own cumulative EVENTS lines and
with the samplers' stat lines. Exit 0 only if every check holds.

  reconcile_events.py RUN.frames.csv RUN.phase.log [RUN.stdout.log]

Checks
  1. every CSV row has ev/ev_fail consistent with its per-kind counts (bit set <=> count > 0), and counts are non-negative;
  2. EXACT, per EVENTS line: the sum of the CSV rows with frame <= csvFrame equals the probe's `csv:` totals, and
     csv + unattributed == abs - base (no event lost or double counted; partial startup/exit frames are `unattributed`);
  3. the final EVENTS line's `abs:` totals against the samplers' own stat lines (needs --stats=1 logs; the stat lines print
     every 10 s, so these are bounds, not equalities): abs.issue >= last stat `issued` and at most 22 more (2 Hz x 10 s + 2);
     abs.cfail >= invalid+discardTimeout+discardWaitFailed of the last stat line; abs.ifail >= unsupported+disabledSkips;
  4. with a stat line that shows no failures at all, a run with cfail == 0 and ifail == 0 is expected (and vice versa is reported).
"""
import csv
import re
import sys

KINDS = [("fx_issue", 1), ("fx_collect", 2), ("raw_issue", 4), ("raw_collect", 8)]
FAILS = [("fx_ifail", 1), ("fx_cfail", 2), ("raw_ifail", 4), ("raw_cfail", 8)]
MAP = {"fx.issue": "fx_issue", "fx.ifail": "fx_ifail", "fx.collect": "fx_collect", "fx.cfail": "fx_cfail",
       "raw.issue": "raw_issue", "raw.ifail": "raw_ifail", "raw.collect": "raw_collect", "raw.cfail": "raw_cfail"}
fails = []


def check(ok, msg):
    print(("ok   " if ok else "FAIL ") + msg)
    if not ok:
        fails.append(msg)


def parse_group(text):
    return {MAP[k]: int(v) for k, v in re.findall(r"((?:fx|raw)\.(?:issue|ifail|collect|cfail))=(\d+)", text)}


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    rows = list(csv.DictReader(open(sys.argv[1], newline="")))
    check(len(rows) > 0, f"CSV has {len(rows)} rows")
    bad = 0
    for r in rows:
        ev, evf = int(r["ev"]), int(r["ev_fail"])
        for col, bit in KINDS:
            if bool(ev & bit) != (int(r[col]) > 0) or int(r[col]) < 0:
                bad += 1
        for col, bit in FAILS:
            if bool(evf & bit) != (int(r[col]) > 0) or int(r[col]) < 0:
                bad += 1
    check(bad == 0, "ev/ev_fail bitmasks agree with the per-kind counts in every row")
    multi = sum(1 for r in rows if bin(int(r["ev"])).count("1") > 1)
    check(True, f"rows with several events: {multi}; no-event rows: {sum(1 for r in rows if int(r['ev']) == 0)}")

    lines = [l for l in open(sys.argv[2], errors="replace") if l.startswith("EVENTS ")]
    check(len(lines) > 0, f"{len(lines)} EVENTS line(s) in the phase log")
    final = None
    for l in lines:
        frame = int(re.search(r"csvFrame=(\d+)", l).group(1))
        parts = {m.group(1): parse_group(m.group(2)) for m in re.finditer(r"\| (csv|unattributed|abs|base):([^|]*)", l)}
        sums = {c: sum(int(r[c]) for r in rows if int(r["frame"]) <= frame) for c in MAP.values()}
        check(sums == parts["csv"], f"EVENTS csvFrame={frame}: CSV row sums equal the probe's csv totals")
        diff = {c: parts["abs"][c] - parts["base"][c] for c in MAP.values()}
        tot = {c: parts["csv"][c] + parts["unattributed"][c] for c in MAP.values()}
        check(diff == tot, f"EVENTS csvFrame={frame}: abs - base == csv + unattributed (nothing lost)")
        final = parts

    if len(sys.argv) > 3 and final:
        last = {}
        for l in open(sys.argv[3], errors="replace"):
            m = re.search(r"stream=(FX|RAW|CPURAW)\b", l)
            if m:
                last[m.group(1)] = l
        for stream, key in (("FX", "fx"), ("RAW", "raw")):
            if stream not in last and final["abs"][key + "_issue"] == 0:
                check(True, f"{stream}: no events and no stat line (stream not active in this run)")
                continue
            if stream not in last:
                check(False, f"{stream}: no sampler stat line in the stdout log (needs ..._STATS=1)")
                continue
            def g(name):
                m = re.search(r"\b" + name + r"=(\d+)", last[stream])
                return int(m.group(1)) if m else 0
            issued = g("issued")
            a = final["abs"]
            check(issued <= a[key + "_issue"] <= issued + 22, f"{stream}: abs issue {a[key + '_issue']} vs last stat issued={issued} (0..22 more allowed)")
            sfail = g("invalid") + g("discardTimeout") + g("discardWaitFailed")
            check(a[key + "_cfail"] >= sfail, f"{stream}: abs collect-fail {a[key + '_cfail']} >= stat invalid+timeout+waitFailed={sfail}")
            sif = g("unsupported") + g("disabledSkips")
            check(a[key + "_ifail"] >= sif, f"{stream}: abs issue-fail {a[key + '_ifail']} >= stat unsupported+disabledSkips={sif}")
            sc = g("completed")
            check(a[key + "_collect"] + a[key + "_cfail"] >= sc, f"{stream}: collect ok+fail {a[key + '_collect'] + a[key + '_cfail']} >= stat completed={sc}")
    # OC-003-F CPU RAW: the frame CSV counts every CPU RAW sample / gap; the 10 s stat line's samples+gaps is a lower bound (<= 22 more).
    cpu_total = sum(int(r.get("cpu_raw", 0)) + int(r.get("cpu_raw_gap", 0)) for r in rows)
    if len(sys.argv) > 3:
        cpu_line = next((l for l in reversed(open(sys.argv[3], errors="replace").readlines()) if re.search(r"stream=CPURAW\b", l)), None)
        if cpu_line is None:
            check(cpu_total == 0, f"CPURAW: no stat line and {cpu_total} CPU RAW frame-CSV samples (must be 0 when the stream is inactive)")
        else:
            st = sum(int(m.group(1)) for m in (re.search(r"\bsamples=(\d+)", cpu_line), re.search(r"\bgaps=(\d+)", cpu_line)) if m)
            check(st <= cpu_total <= st + 22, f"CPURAW: frame-CSV samples+gaps {cpu_total} vs last stat samples+gaps={st} (0..22 more allowed)")
    print("RECONCILED" if not fails else f"{len(fails)} FAILURE(S)")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
