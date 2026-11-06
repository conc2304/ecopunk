#!/usr/bin/env python3
"""Ground truth: full-res gray (BT.601, 8-bit) frame difference of the dumped SceneFBO frames vs probe raw."""
import csv, math, subprocess, sys, glob, os, json, re
GT, OUT = sys.argv[1], sys.argv[2]
res = {}
def pearson(a, b):
    n = len(a); ma, mb = sum(a)/n, sum(b)/n
    c = sum((x-ma)*(y-mb) for x, y in zip(a, b)); d = math.sqrt(sum((x-ma)**2 for x in a)*sum((y-mb)**2 for y in b))
    return c/d if d else float('nan')
def ranks(v):
    o = sorted(range(len(v)), key=lambda i: v[i]); r = [0]*len(v)
    for k, i in enumerate(o): r[i] = k
    return r
for s in "blob temporal blueprint fragment quadrant".split():
    d = f"{GT}/{s}"
    rows = list(csv.DictReader(open(f"{d}/activity.csv")))
    frames = sorted(glob.glob(f"{d}/act_*.bmp"))
    if len(frames) < 10: print(s, "too few frames", len(frames)); continue
    pat = re.sub(r"_\d+\.bmp$", "_%03d.bmp", frames[0])
    txt = f"{d}/gt.txt"
    if os.path.exists(txt): os.remove(txt)
    subprocess.run(["ffmpeg", "-v", "error", "-framerate", "1", "-i", pat, "-framerate", "1", "-start_number", "1", "-i", pat,
                    "-filter_complex", f"[0:v]format=gray[a];[1:v]format=gray[b];[a][b]blend=all_mode=difference,signalstats,metadata=print:key=lavfi.signalstats.YAVG:file={txt}",
                    "-f", "null", "-"], check=True)
    ys = [float(m) / 255 for m in re.findall(r"YAVG=([0-9.]+)", open(txt).read())][:len(frames)-1]  # pair i: frame i -> i+1
    byframe = {r["frame"]: (k, r) for k, r in enumerate(rows) if r["frame"]}
    xs, gt = [], []
    for i, y in enumerate(ys):
        a = os.path.basename(frames[i]); b = os.path.basename(frames[i+1])
        if a in byframe and b in byframe:
            ka, kb = byframe[a][0], byframe[b][0]; rb = byframe[b][1]
            # probe's diff for row kb is against the previous issued sample; require that to be frame a (adjacent rows, no row between)
            if kb == ka + 1 and rb["state"] == "valid" and rb["rawEnc"]:
                xs.append(float(rb["rawEnc"])); gt.append(y)
    n = len(xs)
    rr = ranks(xs), ranks(gt)
    ratio = sorted(x / g for x, g in zip(xs, gt) if g > 1e-5)
    res[s] = dict(pairs=n, pearson=pearson(xs, gt), spearman=pearson(*rr),
                  probeOverGt_median=ratio[len(ratio)//2] if ratio else None,
                  meanProbe=sum(xs)/n if n else None, meanGt=sum(gt)/n if n else None,
                  gtMin=min(gt) if gt else None, gtMax=max(gt) if gt else None)
    print(s, json.dumps(res[s]))
json.dump(res, open(f"{OUT}/ground_truth.json", "w"), indent=1)
