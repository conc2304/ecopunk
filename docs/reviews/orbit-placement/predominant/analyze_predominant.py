#!/usr/bin/env python3
"""OC-003A: analyse predominant.csv runs (average / dominant / vibrant) and compose evidence boards.

Standard library only (no PIL/numpy). Usage:
    analyze_predominant.py <out_dir> <label>=<run_dir> [<label>=<run_dir> ...]
where each run_dir holds predominant.csv and scene_*.png written by the diagnostic
(EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM=<run_dir>). All colours in the CSV are LINEAR light; they are
display-encoded here only to draw them. Every swatch/ribbon comes from real SceneFBO measurements.
"""
import csv, math, os, struct, sys, zlib

# ------------------------------------------------------------------ colour helpers
def enc(c):
    c = min(1.0, max(0.0, c))
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055

def lab(r, g, b):  # linear sRGB -> CIE Lab (D65)
    x = 0.4124 * r + 0.3576 * g + 0.1805 * b
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = 0.0193 * r + 0.1192 * g + 0.9505 * b
    def f(t): return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    fx, fy, fz = f(x / 0.95047), f(y), f(z / 1.08883)
    return 116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)

def dE(a, b):
    if a is None or b is None: return None
    la, lb = lab(*a), lab(*b)
    return math.dist(la, lb)

def pct(v, p):
    v = sorted(x for x in v if x is not None)
    return v[min(len(v) - 1, int(p * len(v)))] if v else float('nan')

def mean(v):
    v = [x for x in v if x is not None]
    return sum(v) / len(v) if v else float('nan')

# ------------------------------------------------------------------ data
def fl(s):
    try:
        v = float(s)
        return None if math.isnan(v) else v
    except ValueError:
        return None

def rgb(row, p):
    v = [fl(row.get(p + k)) for k in 'rgb']
    return None if any(x is None for x in v) else tuple(v)

def load(run):
    rows = []
    with open(os.path.join(run, 'predominant.csv')) as f:
        for r in csv.DictReader(f):
            if r['valid'] != '1': r['_gap'] = True
            rows.append(r)
    return rows

SERIES = {  # name -> (prefix, bin column or None)
    'avg':        ('a160', None),
    'dom160':     ('d160', 'd160bin'),
    'dom16':      ('d16', 'd16bin'),
    'vib160':     ('v160', 'v160bin'),
    'vib16':      ('v16', 'v16bin'),
    'dom160_ema': ('e160', None),
    'dom160_hyst': ('h160', 'h160bin'),
    'dom16_ema':  ('e16', None),
    'dom16_hyst': ('h16', 'h16bin'),
}

def series(rows, name):
    p, binc = SERIES[name]
    out = []
    for r in rows:
        if r.get('_gap'): out.append((None, None)); continue
        if name.startswith('vib') and r[p.replace('v', 'v') + 'ok'] != '1': out.append((None, None)); continue
        out.append((rgb(r, p), int(r[binc]) if binc and r[binc] not in ('', '-1') else None))
    return out

def stats(rows, warm=4.0):
    t0 = float(rows[0]['t'])
    rows = [r for r in rows if float(r['t']) - t0 >= warm]  # drop the scene's loading seconds
    res = {'n': len(rows)}
    for name in SERIES:
        s = series(rows, name)
        de = [dE(a[0], b[0]) for a, b in zip(s, s[1:]) if a[0] and b[0]]
        sw = [a[1] != b[1] for a, b in zip(s, s[1:]) if a[1] is not None and b[1] is not None]
        valid = sum(1 for x in s if x[0] is not None) / max(1, len(s))
        res[name] = {'valid': valid, 'jitter_mean': mean(de), 'jitter_p95': pct(de, .95),
                     'switch': (sum(sw) / len(sw)) if sw else float('nan')}
    def diff(a, b):
        sa, sb = series(rows, a), series(rows, b)
        return mean([dE(x[0], y[0]) for x, y in zip(sa, sb) if x[0] and y[0]])
    res['dE_avg_dom160'] = diff('avg', 'dom160')
    res['dE_avg_vib160'] = diff('avg', 'vib160')
    res['dE_dom160_vib160'] = diff('dom160', 'vib160')
    res['dE_dom16_dom160'] = diff('dom16', 'dom160')
    res['dE_vib16_vib160'] = diff('vib16', 'vib160')
    res['dE_avg16_avg160'] = mean([dE(rgb(r, 'a16'), rgb(r, 'a160')) for r in rows if not r.get('_gap')])
    res['mixedFrac'] = mean([fl(r['mixedFrac']) for r in rows if not r.get('_gap')])
    for c in ('ms_submit', 'ms_read16', 'ms_read160', 'us_an16', 'us_an160'):
        v = [fl(r[c]) for r in rows if not r.get('_gap')]
        res[c] = (pct(v, .5), pct(v, .95), max([x for x in v if x is not None] or [float('nan')]))
    return res

# ------------------------------------------------------------------ PNG io (stdlib)
def read_png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    i, idat, w = 8, b'', 0
    while i < len(d):
        n, typ = struct.unpack('>I4s', d[i:i + 8]); body = d[i + 8:i + 8 + n]; i += 12 + n
        if typ == b'IHDR': w, h, bd, ct, _, _, il = struct.unpack('>IIBBBBB', body); assert bd == 8 and il == 0
        elif typ == b'IDAT': idat += body
    ch = {2: 3, 6: 4, 0: 1, 4: 2}[ct]
    raw = zlib.decompress(idat); stride = w * ch; out = bytearray(h * stride); prev = bytearray(stride); p = 0
    for y in range(h):
        f = raw[p]; line = bytearray(raw[p + 1:p + 1 + stride]); p += 1 + stride
        for x in range(stride):
            a = line[x - ch] if x >= ch else 0; b = prev[x]; c = prev[x - ch] if x >= ch else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        out[y * stride:(y + 1) * stride] = line; prev = line
    return w, h, ch, out

def write_png(path, w, h, px):  # px: bytearray RGB
    raw = b''.join(b'\x00' + bytes(px[y * w * 3:(y + 1) * w * 3]) for y in range(h))
    def chunk(t, b): return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))

# ------------------------------------------------------------------ canvas + 5x7 font
FONT = {
 'A':["01110","10001","10001","11111","10001","10001","10001"],'B':["11110","10001","10001","11110","10001","10001","11110"],
 'C':["01110","10001","10000","10000","10000","10001","01110"],'D':["11110","10001","10001","10001","10001","10001","11110"],
 'E':["11111","10000","10000","11110","10000","10000","11111"],'F':["11111","10000","10000","11110","10000","10000","10000"],
 'G':["01110","10001","10000","10111","10001","10001","01111"],'H':["10001","10001","10001","11111","10001","10001","10001"],
 'I':["01110","00100","00100","00100","00100","00100","01110"],'J':["00111","00010","00010","00010","00010","10010","01100"],
 'K':["10001","10010","10100","11000","10100","10010","10001"],'L':["10000","10000","10000","10000","10000","10000","11111"],
 'M':["10001","11011","10101","10101","10001","10001","10001"],'N':["10001","11001","10101","10011","10001","10001","10001"],
 'O':["01110","10001","10001","10001","10001","10001","01110"],'P':["11110","10001","10001","11110","10000","10000","10000"],
 'Q':["01110","10001","10001","10001","10101","10010","01101"],'R':["11110","10001","10001","11110","10100","10010","10001"],
 'S':["01111","10000","10000","01110","00001","00001","11110"],'T':["11111","00100","00100","00100","00100","00100","00100"],
 'U':["10001","10001","10001","10001","10001","10001","01110"],'V':["10001","10001","10001","10001","10001","01010","00100"],
 'W':["10001","10001","10001","10101","10101","11011","10001"],'X':["10001","10001","01010","00100","01010","10001","10001"],
 'Y':["10001","10001","01010","00100","00100","00100","00100"],'Z':["11111","00001","00010","00100","01000","10000","11111"],
 '0':["01110","10001","10011","10101","11001","10001","01110"],'1':["00100","01100","00100","00100","00100","00100","01110"],
 '2':["01110","10001","00001","00010","00100","01000","11111"],'3':["11110","00001","00001","01110","00001","00001","11110"],
 '4':["00010","00110","01010","10010","11111","00010","00010"],'5':["11111","10000","11110","00001","00001","10001","01110"],
 '6':["00110","01000","10000","11110","10001","10001","01110"],'7':["11111","00001","00010","00100","01000","01000","01000"],
 '8':["01110","10001","10001","01110","10001","10001","01110"],'9':["01110","10001","10001","01111","00001","00010","01100"],
 ' ':["00000"]*7,'.':["00000","00000","00000","00000","00000","01100","01100"],'-':["00000","00000","00000","11111","00000","00000","00000"],
 ':':["00000","01100","01100","00000","01100","01100","00000"],'/':["00001","00010","00010","00100","01000","01000","10000"],
 '(':["00010","00100","01000","01000","01000","00100","00010"],')':["01000","00100","00010","00010","00010","00100","01000"],
 '=':["00000","00000","11111","00000","11111","00000","00000"],'%':["11001","11010","00010","00100","01000","01011","10011"],
 '_':["00000","00000","00000","00000","00000","00000","11111"],',':["00000","00000","00000","00000","01100","00100","01000"],
 '+':["00000","00100","00100","11111","00100","00100","00000"],'>':["10000","01000","00100","00010","00100","01000","10000"],
}

class Canvas:
    def __init__(self, w, h, bg=(18, 18, 18)):
        self.w, self.h = w, h; self.px = bytearray(bytes(bg) * (w * h))
    def rect(self, x, y, w, h, c):
        x0, y0, x1, y1 = max(0, int(x)), max(0, int(y)), min(self.w, int(x + w)), min(self.h, int(y + h))
        row = bytes(c) * max(0, x1 - x0)
        for yy in range(y0, y1): self.px[(yy * self.w + x0) * 3:(yy * self.w + x1) * 3] = row
    def text(self, x, y, s, c=(230, 230, 230), sc=2):
        for ch in s.upper():
            g = FONT.get(ch, FONT[' '])
            for ry, line in enumerate(g):
                for rx, bit in enumerate(line):
                    if bit == '1': self.rect(x + rx * sc, y + ry * sc, sc, sc, c)
            x += 6 * sc
    def blit(self, x, y, w, h, sw, sh, ch, src):  # nearest-neighbour, composite over black
        for yy in range(h):
            sy = min(sh - 1, int((yy + .5) * sh / h))
            for xx in range(w):
                sx = min(sw - 1, int((xx + .5) * sw / w)); o = (sy * sw + sx) * ch
                a = src[o + 3] / 255 if ch == 4 else 1
                i = ((y + yy) * self.w + x + xx) * 3
                self.px[i:i + 3] = bytes(int(src[o + k] * a) for k in range(3))
    def save(self, p): write_png(p, self.w, self.h, self.px)

def e8(c): return tuple(int(round(enc(v) * 255)) for v in c)
GAP = (70, 70, 70)

def board(path, label, rows, run):
    """One scene: real SceneFBO frame, three swatches, three synchronised colour-history ribbons."""
    t0 = float(rows[0]['t']); T = [float(r['t']) - t0 for r in rows]
    frames = [(float(r['t']) - t0, r['frame']) for r in rows if r.get('frame')]
    W, H = 1180, 330
    cv = Canvas(W, H)
    cv.text(10, 8, f"{label.upper()}  ACTUAL SCENEFBO MEASUREMENTS (REAL, NOT SYNTHETIC)  N={len(rows)} SAMPLES", sc=2)
    pick = frames[min(len(frames) - 1, max(0, int(len(frames) * .7)))] if frames else None
    pt = pick[0] if pick else None
    if pick:
        w, h, ch, d = read_png(os.path.join(run, pick[1]))
        cv.blit(10, 36, 320, 180, w, h, ch, d)
        cv.text(10, 220, f"FX SCENE FRAME T={pt:.0f}S", sc=2)
    k = min(range(len(T)), key=lambda i: abs(T[i] - (pt or 0)))
    r = rows[k]
    sw = [('AVERAGE', rgb(r, 'a160')), ('DOMINANT', rgb(r, 'd160')), ('VIBRANT', rgb(r, 'v160') if r['v160ok'] == '1' else None)]
    for i, (n, c) in enumerate(sw):
        x = 345 + i * 100
        if c: cv.rect(x, 36, 90, 90, e8(c))
        else:
            cv.rect(x, 36, 90, 90, GAP); cv.text(x + 10, 74, "NONE", (255, 200, 80), 2)
        cv.text(x, 130, n, sc=1)
        if c: cv.text(x, 142, "%d %d %d" % e8(c), (170, 170, 170), 1)
    cv.text(345, 160, f"MIXED 16X9 CELLS {100 * float(r['mixedFrac']):.0f}%", (170, 170, 170), 1)
    # ribbons: one column per sample, newest on the right, same x axis for all three
    rx, rw, rh = 660, 500, 44
    ns = len(rows); cw = rw / ns
    for j, (n, nm, key) in enumerate([('AVERAGE', 'avg', 'a160'), ('DOMINANT 160X90', 'dom', 'd160'), ('VIBRANT 160X90', 'vib', 'v160')]):
        y = 36 + j * (rh + 26)
        cv.text(rx, y - 12, n, sc=1)
        for i, rr in enumerate(rows):
            if rr.get('_gap'): c = GAP
            elif key == 'v160' and rr['v160ok'] != '1': c = (45, 45, 45)
            else:
                v = rgb(rr, key); c = e8(v) if v else GAP
            cv.rect(rx + i * cw, y, max(1, cw), rh, c)
        if pt is not None: cv.rect(rx + k * cw, y - 3, max(1, cw), 3, (255, 255, 255))
    cv.text(rx, 36 + 3 * (rh + 26) - 6, f"0S -> {T[-1]:.0f}S (OLDEST LEFT, NEWEST RIGHT)  TICK = FRAME SHOWN  DARK GREY = NO VIBRANT COLOUR", (150, 150, 150), 1)
    cv.save(path)

def compare_board(path, label, rows, warm=4.0):
    """160x90 vs 16x9 and the temporal variants, as ribbons on one shared time axis."""
    t0 = float(rows[0]['t']); rows = [r for r in rows if float(r['t']) - t0 >= 0]
    W, H = 1180, 470
    cv = Canvas(W, H); cv.text(10, 8, f"{label.upper()}  RESOLUTION AND TEMPORAL COMPARISON (REAL)", sc=2)
    ns = len(rows); rx, rw, rh = 280, 880, 30; cw = rw / ns
    specs = [('AVG 160X90', 'a160'), ('DOM 160X90', 'd160'), ('DOM 16X9', 'd16'), ('DOM 160 EMA', 'e160'), ('DOM 160 HYSTERESIS', 'h160'),
             ('VIB 160X90', 'v160'), ('VIB 16X9', 'v16')]
    for j, (nm, key) in enumerate(specs):
        y = 40 + j * (rh + 8)
        cv.text(10, y + 9, nm, sc=2)
        for i, rr in enumerate(rows):
            if rr.get('_gap'): c = GAP
            elif key.startswith('v') and rr[key + 'ok'] != '1': c = (45, 45, 45)
            else:
                v = rgb(rr, key); c = e8(v) if v else GAP
            cv.rect(rx + i * cw, y, max(1, cw), rh, c)
    cv.text(10, 40 + len(specs) * (rh + 8) + 4, "ALL ROWS SHARE ONE TIME AXIS (OLDEST LEFT). DARK GREY = NO VIBRANT COLOUR, MID GREY = GAP", (150, 150, 150), 1)
    cv.save(path)

# ------------------------------------------------------------------ main
def main():
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    md = []
    allstats = {}
    for arg in sys.argv[2:]:
        label, run = arg.split('=', 1)
        rows = load(run)
        st = stats(rows); allstats[label] = st
        board(os.path.join(out, f"board_{label}.png"), label, rows, run)
        compare_board(os.path.join(out, f"compare_{label}.png"), label, rows)
    def f(x, n=2): return 'n/a' if x is None or (isinstance(x, float) and math.isnan(x)) else f"{x:.{n}f}"
    L = list(allstats)
    md.append("## Stability (steady state; the first 4 s of each run are dropped)\n")
    md.append("jitter = mean CIE76 dE between successive samples; switch = share of successive samples whose winning 8x8x8 bin changed.\n")
    md.append("| scene | n | avg jitter | dom160 jitter / switch | dom160 EMA jitter | dom160 hyst jitter / switch | dom16 jitter / switch | vib160 valid / jitter / switch | vib16 valid / jitter |")
    md.append("|---|---|---|---|---|---|---|---|---|")
    for l in L:
        s = allstats[l]
        md.append(f"| {l} | {s['n']} | {f(s['avg']['jitter_mean'])} | {f(s['dom160']['jitter_mean'])} / {f(100*s['dom160']['switch'],0)}% | {f(s['dom160_ema']['jitter_mean'])} | "
                  f"{f(s['dom160_hyst']['jitter_mean'])} / {f(100*s['dom160_hyst']['switch'],0)}% | {f(s['dom16']['jitter_mean'])} / {f(100*s['dom16']['switch'],0)}% | "
                  f"{f(100*s['vib160']['valid'],0)}% / {f(s['vib160']['jitter_mean'])} / {f(100*s['vib160']['switch'],0)}% | {f(100*s['vib16']['valid'],0)}% / {f(s['vib16']['jitter_mean'])} |")
    md.append("\n## Disagreement between measures (mean CIE76 dE)\n")
    md.append("| scene | avg vs dom160 | avg vs vib160 | dom160 vs vib160 | dom16 vs dom160 | vib16 vs vib160 | avg16 vs avg160 | mixed 16x9 cells |")
    md.append("|---|---|---|---|---|---|---|---|")
    for l in L:
        s = allstats[l]
        md.append(f"| {l} | {f(s['dE_avg_dom160'])} | {f(s['dE_avg_vib160'])} | {f(s['dE_dom160_vib160'])} | {f(s['dE_dom16_dom160'])} | {f(s['dE_vib16_vib160'])} | {f(s['dE_avg16_avg160'],3)} | {f(100*s['mixedFrac'],0)}% |")
    md.append("\n## Incremental cost (this machine; p50 / p95 / max)\n")
    md.append("| scene | submit ms | 16x9 read ms | 160x90 read ms (extra) | analysis 16x9 us | analysis 160x90 us |")
    md.append("|---|---|---|---|---|---|")
    for l in L:
        s = allstats[l]; g = lambda k, n=3: f"{f(s[k][0], n)} / {f(s[k][1], n)} / {f(s[k][2], n)}"
        md.append(f"| {l} | {g('ms_submit')} | {g('ms_read16')} | {g('ms_read160')} | {g('us_an16',1)} | {g('us_an160',1)} |")
    open(os.path.join(out, 'results.md'), 'w').write('\n'.join(md) + '\n')
    print('\n'.join(md))

if __name__ == '__main__':
    main()
