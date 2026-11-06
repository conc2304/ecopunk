#!/usr/bin/env python3
"""OC-004 evidence: per-scene SVG timeline + frame contact sheet. usage: evidence.py <evdir> <outdir>"""
import csv, math, os, subprocess, sys
sys.path.insert(0, os.path.dirname(__file__))
EV, OUT = sys.argv[1], sys.argv[2]
SCENES = ["blob", "temporal", "blueprint", "fragment", "quadrant"]
os.makedirs(OUT, exist_ok=True)
N = 12

def f(x): return float(x) if x != "" else float("nan")

def svg_timeline(scene, rows):
    t0 = rows[0]["t"]; T = rows[-1]["t"] - t0
    W, H, L, R = 900, 330, 46, 10
    px = lambda t: L + (t - t0) / T * (W - L - R)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="sans-serif" font-size="10">',
           '<rect width="100%" height="100%" fill="#fff"/>']
    # panel 1: raw on log axis 3e-4 .. 1
    top, bot = 20, 150
    ly = lambda v: bot - (math.log10(max(v, 3e-4)) - math.log10(3e-4)) / (0 - math.log10(3e-4)) * (bot - top)
    out.append(f'<text x="{L}" y="12" font-weight="bold">{scene}: raw mean |Δluma| (log) — states: ▲ static/first/scenechange, ■ missing</text>')
    for v in (1e-3, 1e-2, 1e-1, 1):
        out.append(f'<line x1="{L}" x2="{W-R}" y1="{ly(v):.1f}" y2="{ly(v):.1f}" stroke="#ddd"/><text x="4" y="{ly(v)+3:.1f}">{v:g}</text>')
    pts = [(px(r["t"]), ly(f(r["rawEnc"]))) for r in rows if r["rawEnc"] != ""]
    out.append('<polyline fill="none" stroke="#1565c0" stroke-width="1" points="' + " ".join(f"{x:.1f},{y:.1f}" for x, y in pts) + '"/>')
    for r in rows:
        if r["state"] in ("static", "first", "scenechange"):
            out.append(f'<text x="{px(r["t"]):.1f}" y="{bot+12}" fill="#c62828" text-anchor="middle">▲</text>')
        if r["state"] == "missing":
            out.append(f'<rect x="{px(r["t"])-2:.1f}" y="{bot+4}" width="4" height="4" fill="#ef6c00"/>')
    # panel 2: smoothed segment counts
    top2, bot2 = 190, 310
    sy = lambda s: bot2 - s / N * (bot2 - top2)
    out.append(f'<text x="{L}" y="{top2-8}" font-weight="bold">illuminated segments of {N} (fast-attack/slow-decay): fixed · rolling-60s · bounded-adaptive</text>')
    for s in (0, 3, 6, 9, 12):
        out.append(f'<line x1="{L}" x2="{W-R}" y1="{sy(s):.1f}" y2="{sy(s):.1f}" stroke="#ddd"/><text x="14" y="{sy(s)+3:.1f}">{s}</text>')
    for col, k in (("#1565c0", "segFixed"), ("#2e7d32", "segRolling"), ("#ef6c00", "segBounded")):
        out.append(f'<polyline fill="none" stroke="{col}" stroke-width="1.2" points="' + " ".join(f"{px(r['t']):.1f},{sy(int(r[k])):.1f}" for r in rows if r["state"] in ("valid", "wide")) + '"/>')
    out.append('</svg>')
    open(f"{OUT}/timeline_{scene}.svg", "w").write("\n".join(out))

def contact(scene, rows, n=8):
    fr = [r for r in rows if r["frame"] and r["state"] in ("valid", "wide")]
    pick = [fr[int((i + 0.5) * len(fr) / n)] for i in range(n)]
    tiles = []
    for r in pick:
        cap = f't={r["t"]-rows[0]["t"]:.0f}s raw={f(r["rawEnc"]):.4f} seg F{r["segFixed"]}/R{r["segRolling"]}/B{r["segBounded"]}'
        tile = f"{OUT}/_tile_{len(tiles)}.png"
        subprocess.run(["magick", f"{EV}/{scene}/{r['frame']}", "-resize", "320x180", "-background", "black", "-fill", "white",
                        "-gravity", "south", "-splice", "0x16", "-font", "/System/Library/Fonts/Supplemental/Arial.ttf", "-pointsize", "11", "-annotate", "+0+1", cap, tile], check=True)
        tiles.append(tile)
    subprocess.run(["magick", "montage", "-font", "/System/Library/Fonts/Supplemental/Arial.ttf", *tiles, "-tile", "4x2", "-geometry", "+3+3", "-background", "#222", f"{OUT}/sheet_{scene}.jpg"], check=True)
    for t in tiles: os.remove(t)

if __name__ == "__main__":
    for s in SCENES:
        rows = list(csv.DictReader(open(f"{EV}/{s}/activity.csv")))
        for r in rows: r["t"] = float(r["t"])
        svg_timeline(s, rows)
        contact(s, rows)
        print("done", s)
