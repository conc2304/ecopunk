#!/usr/bin/env python3
"""OC-004 analysis: stdlib only. usage: analyze.py <evdir> <outdir>"""
import csv, math, sys, os, json
from collections import Counter

EV, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
SCENES = ["blob", "temporal", "blueprint", "fragment", "quadrant"]
N = 12

def load(s):
    rows = list(csv.DictReader(open(f"{EV}/{s}/activity.csv")))
    for r in rows:
        r["t"] = float(r["t"])
    return rows

def f(x):
    return float(x) if x != "" else float("nan")

def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(p * len(v)))] if v else float("nan")

# ---- Python mirror of the C++ normalisers/smoother (validated against the CSV columns) ----
class Norm:
    def __init__(s, ref=0.04, gamma=0.5, floor=0.002, win=60.0, lo=0.5, hi=2.0):
        s.ref, s.g, s.floor, s.win, s.lo, s.hi = ref, gamma, floor, win, lo, hi
        s.q = []
    def curve(s, raw, ref):
        return min(1.0, (raw / ref) ** s.g) if ref > 0 and raw >= 0 else 0.0
    def push(s, t, raw):
        while s.q and s.q[-1][1] <= raw: s.q.pop()
        s.q.append((t, raw))
        while s.q and s.q[0][0] < t - s.win: s.q.pop(0)
        m = s.q[0][1]
        rr = max(m, s.floor); br = min(max(m, s.ref * s.lo), s.ref * s.hi)
        return (s.curve(raw, s.ref), s.curve(raw, rr), s.curve(raw, br))

class Smooth:
    def __init__(s, a=0.08, d=0.9): s.a, s.d, s.v, s.have = a, d, 0.0, False
    def push(s, x, dt):
        if not s.have: s.v, s.have = x, True; return s.v
        tau = s.a if x > s.v else s.d
        s.v += (1 - math.exp(-max(dt, 0) / tau)) * (x - s.v); return s.v

def seg(level):
    return 0 if not level > 0 else min(N, max(1, math.ceil(level * N - 1e-9)))

def run(rows, **kw):
    """returns list of dict(t, raw, lv[3], sm[3], seg[3], segRaw[3]) over raw-bearing rows"""
    nm = Norm(**kw); sm = [Smooth() for _ in range(3)]; out = []; last = None
    for r in rows:
        if r["state"] == "scenechange":
            nm = Norm(**kw); sm = [Smooth() for _ in range(3)]; last = None; continue
        raw = f(r["rawEnc"]); has = r["state"] in ("valid", "wide")
        lv = nm.push(r["t"], raw) if has else (0, 0, 0)
        dt = 0.0 if last is None else r["t"] - last; last = r["t"]
        s = [sm[i].push(lv[i], dt) for i in range(3)]
        out.append(dict(t=r["t"], raw=raw if has else float("nan"), state=r["state"], lv=lv, sm=s,
                        seg=[seg(x) for x in s], segRaw=[seg(x) for x in lv]))
    return out

def flicker(series, key, idx):
    ch = sum(1 for a, b in zip(series, series[1:]) if a[key][idx] != b[key][idx])
    span = series[-1]["t"] - series[0]["t"]
    return ch / span if span > 0 else 0

if __name__ == "__main__":
    res = {}
    # 1. mirror validation + per-scene summary
    for s in SCENES:
        rows = load(s)
        mir = run(rows)
        cs = [r for r in rows if r["state"] in ("valid", "wide")]
        mx = 0.0
        for r, m in zip([r for r in rows if r["state"] != "scenechange"], mir):
            if r["state"] in ("valid", "wide"):
                mx = max(mx, abs(m["lv"][0] - f(r["fixed"])), abs(m["lv"][1] - f(r["rolling"])), abs(m["lv"][2] - f(r["bounded"])))
        raw = [f(r["rawEnc"]) for r in cs if r["state"] == "valid"]
        rawlin = [f(r["rawLin"]) for r in cs if r["state"] == "valid"]
        delta = max((f(r["gpuCpuDelta"]) for r in cs if r["gpuCpuDelta"] != ""), default=float("nan"))
        valid = [m for m in mir if m["state"] == "valid"]
        stat = dict(counts=dict(Counter(r["state"] for r in rows)),
                    mirrorMaxErr=mx, gpuCpuMaxDelta=delta,
                    raw={k: pct(raw, p) for k, p in (("p05", .05), ("p25", .25), ("p50", .5), ("p75", .75), ("p95", .95), ("p99", .99), ("max", 1.0))},
                    rawZeroFrac=sum(1 for x in raw if x < 1e-5) / max(1, len(raw)),
                    corrEncLin=None)
        # pearson enc vs lin
        n = len(raw); ma, mb = sum(raw) / n, sum(rawlin) / n
        cov = sum((a - ma) * (b - mb) for a, b in zip(raw, rawlin))
        stat["corrEncLin"] = cov / math.sqrt(sum((a - ma) ** 2 for a in raw) * sum((b - mb) ** 2 for b in rawlin))
        names = ["fixed", "rolling", "bounded"]
        for i, nme in enumerate(names):
            sg = [m["seg"][i] for m in valid]
            stat[nme] = dict(meanSeg=sum(sg) / len(sg), p10=pct(sg, .1), p50=pct(sg, .5), p90=pct(sg, .9),
                             atMaxFrac=sum(1 for x in sg if x == N) / len(sg),
                             atOrBelow1Frac=sum(1 for x in sg if x <= 1) / len(sg),
                             flickerRaw=flicker(valid, "segRaw", i), flickerSmoothed=flicker(valid, "seg", i))
        res[s] = stat

    # 2. synthetic stress (behaviour ONLY; not used to pick thresholds)
    def synth():
        t = 0.0; seq = []
        def add(name, n, fn):
            nonlocal t
            for k in range(n):
                seq.append((name, t, fn(k))); t += 0.1
        add("stillness", 100, lambda k: 0.0001)
        add("subtle", 100, lambda k: 0.004)
        add("continuous", 100, lambda k: 0.02)
        add("rapid", 100, lambda k: 0.08)
        add("flash", 1, lambda k: 0.4)
        add("post-flash-still", 100, lambda k: 0.0001)
        add("cut", 1, lambda k: 0.35)
        add("post-cut-subtle", 100, lambda k: 0.004)
        return seq
    syn = {}
    for label, kw in (("default", {}),):
        nm = Norm(**kw); sm = [Smooth() for _ in range(3)]; agg = {}; last = None
        for name, t, raw in synth():
            lv = nm.push(t, raw); dt = 0 if last is None else t - last; last = t
            sv = [sm[i].push(lv[i], dt) for i in range(3)]
            agg.setdefault(name, []).append((lv, sv))
        for name, v in agg.items():
            syn[name] = {k: round(sum(x[1][i] for x in v[len(v)//2:]) / len(v[len(v)//2:]), 3) for i, k in enumerate(["fixed", "rolling", "bounded"])}
            syn[name]["peakSmoothedSeg"] = [max(seg(x[1][i]) for x in v) for i in range(3)]
    res["_synthetic"] = syn
    json.dump(res, open(f"{OUT}/analysis.json", "w"), indent=1)
    print(json.dumps(res, indent=1))
