#!/usr/bin/env python3
"""OC-003A follow-up: vibrant-colour stabilisation probe. Standard library only.

Usage: analyze_vibrant.py <out_dir> <label>=<run_dir> ...
Reads predominant.csv written by the diagnostic (EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM=<run_dir>) and compares, at 160x90:
  raw vibrant | EMA | winner-bin hysteresis | EMA + hysteresis, plus an OFFLINE "hold last valid" display policy.
An invalid vibrant observation stays invalid in every variant (average/dominant are never substituted). All colours in the
CSV are linear light. Boards use real SceneFBO measurements only.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyze_predominant as ap  # reuse colour/PNG/canvas helpers

PERIOD = 0.5  # seconds per sample (2 Hz)

def build(rows):
    """Per-row dict of series: each entry is a linear rgb tuple or None. gap rows break everything (None + marker)."""
    out = {k: [] for k in ('avg', 'raw', 'ema', 'hyst', 'comb')}
    bins = {'raw': [], 'hyst': []}
    gaps = []
    for r in rows:
        gap = r['valid'] != '1'
        gaps.append(gap)
        if gap:
            for k in out: out[k].append(None)
            bins['raw'].append(None); bins['hyst'].append(None)
            continue
        out['avg'].append(ap.rgb(r, 'a160'))
        ok = r['v160ok'] == '1'
        out['raw'].append(ap.rgb(r, 'v160') if ok else None)
        out['ema'].append(ap.rgb(r, 've160'))
        out['hyst'].append(ap.rgb(r, 'vh160'))
        out['comb'].append(ap.rgb(r, 'vhe160'))
        bins['raw'].append(int(r['v160bin']) if ok and r['v160bin'] not in ('', '-1') else None)
        bins['hyst'].append(int(r['vh160bin']) if r['vh160bin'] not in ('', '-1') else None)
    return out, bins, gaps

def hold_series(raw, gaps, gens):
    """Display policy under test: when vibrant is invalid, keep showing the last valid colour. Resets at gaps/generation changes."""
    out, last, held_flag = [], None, []
    for i, v in enumerate(raw):
        if gaps[i] or (i > 0 and gens[i] != gens[i - 1]): last = None
        if v is not None: last = v; out.append(v); held_flag.append(False)
        elif gaps[i]: out.append(None); held_flag.append(False)
        else: out.append(last); held_flag.append(last is not None)
    return out, held_flag

def jitter(s):
    d = [ap.dE(a, b) for a, b in zip(s, s[1:]) if a and b]
    return (ap.mean(d), ap.pct(d, .95), max(d) if d else float('nan'))

def switches(b):
    pairs = [(x, y) for x, y in zip(b, b[1:]) if x is not None and y is not None]
    return sum(1 for x, y in pairs if x != y) / len(pairs) if pairs else float('nan')

def runs_invalid(raw, gaps):
    runs, n = [], 0
    for v, g in zip(raw, gaps):
        if g:
            if n: runs.append(n)
            n = 0
        elif v is None: n += 1
        else:
            if n: runs.append(n)
            n = 0
    if n: runs.append(n)
    return runs

def steps(raw, thr=20.0, persist=12.0):
    """Persistent colour steps in the RAW vibrant series: a sample >= thr dE from the mean of the previous 3 valid samples
    whose next 3 samples (incl. itself) stay within `persist` dE of each other."""
    ev = []
    for i in range(3, len(raw) - 3):
        prev = raw[i - 3:i]; nxt = raw[i:i + 3]
        if any(v is None for v in prev + nxt): continue
        pm = tuple(sum(c[k] for c in prev) / 3 for k in range(3))
        nm = tuple(sum(c[k] for c in nxt) / 3 for k in range(3))
        if ap.dE(nxt[0], pm) >= thr and all(ap.dE(c, nm) <= persist for c in nxt) and (not ev or i - ev[-1][0] > 6):
            ev.append((i, nm))
    return ev

def latency(var, ev, tol=10.0, cap=10):
    res = []
    for i, tgt in ev:
        k = next((k for k in range(0, cap + 1) if i + k < len(var) and var[i + k] and ap.dE(var[i + k], tgt) <= tol), None)
        res.append(k)
    return res

def analyse(rows, warm=4.0):
    t0 = float(rows[0]['t'])
    rows = [r for r in rows if float(r['t']) - t0 >= warm]
    gens = [r['gen'] for r in rows]
    S, B, gaps = build(rows)
    hold, held_flag = hold_series(S['raw'], gaps, gens)
    n = len(rows)
    res = {'n': n}
    valid = {k: sum(1 for v in S[k] if v is not None) / n for k in ('raw', 'ema', 'hyst', 'comb')}
    res['coverage'] = valid
    ir = runs_invalid(S['raw'], gaps)
    res['invalid_runs'] = (len(ir), ap.mean(ir) * PERIOD if ir else 0.0, (max(ir) * PERIOD) if ir else 0.0)
    res['jitter'] = {k: jitter(S[k]) for k in ('raw', 'ema', 'hyst', 'comb')}
    res['switch'] = {'raw': switches(B['raw']), 'hyst': switches(B['hyst'])}
    ev = steps(S['raw'])
    res['steps'] = len(ev)
    res['latency'] = {}
    for k in ('raw', 'ema', 'hyst', 'comb'):
        lat = latency(S[k], ev)
        reached = [x for x in lat if x is not None]
        res['latency'][k] = (sum(1 for x in lat if x is not None) / len(lat) if lat else float('nan'),
                             (sorted(reached)[len(reached) // 2] * PERIOD) if reached else float('nan'))
    # hold-last: how much of the time would it show a colour that the frame does not currently contain, and how wrong is it?
    holds, cur = [], 0
    stale_err = []
    for i, f in enumerate(held_flag):
        if f: cur += 1
        else:
            if cur and S['raw'][i] is not None and hold[i - 1] is not None:
                stale_err.append((cur, ap.dE(hold[i - 1], S['raw'][i])))
            if cur: holds.append(cur)
            cur = 0
    if cur: holds.append(cur)
    res['hold'] = {'share': sum(held_flag) / n, 'runs': len(holds), 'mean_s': ap.mean(holds) * PERIOD if holds else 0.0,
                   'max_s': max(holds) * PERIOD if holds else 0.0,
                   'stale_err_mean': ap.mean([e for _, e in stale_err]),
                   'long_misleading': (sum(1 for c, e in stale_err if c >= 6 and e > 20), sum(1 for c, e in stale_err if c >= 6)),
                   'misleading': (sum(1 for c, e in stale_err if e > 20), len(stale_err))}
    # cost
    def col(c):
        v = [ap.fl(r[c]) for r in rows if r['valid'] == '1']
        return (ap.pct(v, .5), ap.pct(v, .95), max([x for x in v if x is not None] or [float('nan')]))
    res['cost'] = {c: col(c) for c in ('ms_submit', 'ms_read16', 'ms_read160', 'us_an160', 'us_vib')}
    res['_series'] = (S, B, hold, held_flag, gaps)
    return res

def board(path, label, rows, run):
    t0 = float(rows[0]['t']); T = [float(r['t']) - t0 for r in rows]
    gens = [r['gen'] for r in rows]
    S, B, gaps = build(rows)
    hold, held = hold_series(S['raw'], gaps, gens)
    frames = [(float(r['t']) - t0, r['frame']) for r in rows if r.get('frame')]
    W, H = 1300, 470
    cv = ap.Canvas(W, H)
    cv.text(10, 8, f"{label.upper()}  VIBRANT STABILISATION PROBE - REAL SCENEFBO MEASUREMENTS (NOT SYNTHETIC)  N={len(rows)}", sc=2)
    if frames:
        pick = frames[min(len(frames) - 1, max(0, int(len(frames) * .7)))]
        w, h, ch, d = ap.read_png(os.path.join(run, pick[1]))
        cv.blit(10, 36, 320, 180, w, h, ch, d)
        cv.text(10, 220, f"FX SCENE FRAME T={pick[0]:.0f}S", sc=2)
    rx, rw, rh = 360, 920, 34; ns = len(rows); cw = rw / ns
    specs = [('AVERAGE (REFERENCE ONLY, NEVER A SUBSTITUTE)', 'avg'), ('RAW VIBRANT', 'raw'), ('EMA', 'ema'), ('WINNER-BIN HYSTERESIS', 'hyst'),
             ('EMA + HYSTERESIS', 'comb'), ('HOLD LAST VALID (OFFLINE POLICY)', 'hold')]
    for j, (nm, key) in enumerate(specs):
        y = 44 + j * (rh + 22)
        cv.text(rx, y - 12, nm, sc=1)
        ser = hold if key == 'hold' else S[key]
        for i in range(ns):
            if gaps[i]: c = ap.GAP
            elif ser[i] is None: c = (45, 45, 45)
            else:
                c = ap.e8(ser[i])
                if key == 'hold' and held[i]: c = tuple(int(v * 0.45) for v in c)
            cv.rect(rx + i * cw, y, max(1, cw), rh, c)
            if key == 'hold' and held[i]: cv.rect(rx + i * cw, y + rh - 3, max(1, cw), 3, (230, 60, 60))
        if key in ('raw', 'hyst'):  # winner-bin changes as white ticks above the ribbon
            bb = B['raw' if key == 'raw' else 'hyst']
            for i in range(1, ns):
                if bb[i] is not None and bb[i - 1] is not None and bb[i] != bb[i - 1]: cv.rect(rx + i * cw, y - 5, max(1, cw), 4, (255, 255, 255))
    ya = 44 + len(specs) * (rh + 22)
    for s in range(0, int(T[-1]) + 1, 30):
        x = rx + rw * s / max(1.0, T[-1]); cv.rect(x, ya - 6, 1, 6, (200, 200, 200)); cv.text(x - 6, ya + 2, f"{s}S", (170, 170, 170), 1)
    cv.text(rx, ya + 18, "OLDEST LEFT, NEWEST RIGHT. DARK GREY = INVALID (NO VIBRANT COLOUR), MID GREY = GAP, DIMMED+RED BASE = HELD VALUE, WHITE TICK = WINNER BIN CHANGED", (150, 150, 150), 1)
    cv.save(path)

def main():
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    A = {}
    for arg in sys.argv[2:]:
        label, run = arg.split('=', 1)
        rows = ap.load(run)
        board(os.path.join(out, f"board_vibrant_{label}.png"), label, rows, run)
        A[label] = analyse(rows)
    f = lambda x, n=2: 'n/a' if x is None or (isinstance(x, float) and math.isnan(x)) else f"{x:.{n}f}"
    pc = lambda x: 'n/a' if x is None or (isinstance(x, float) and math.isnan(x)) else f"{100 * x:.0f}%"
    md = []
    md.append("## Coverage, invalid runs and winner switching (steady state, first 4 s dropped)\n")
    md.append("| scene | n | valid | invalid runs (count / mean s / max s) | raw winner switches | hysteresis winner switches |")
    md.append("|---|---|---|---|---|---|")
    for l, a in A.items():
        ir = a['invalid_runs']
        md.append(f"| {l} | {a['n']} | {pc(a['coverage']['raw'])} | {ir[0]} / {f(ir[1],1)} / {f(ir[2],1)} | {pc(a['switch']['raw'])} | {pc(a['switch']['hyst'])} |")
    md.append("\n## Successive-sample colour change (CIE76 dE: mean / p95 / max)\n")
    md.append("| scene | raw | EMA | hysteresis | EMA + hysteresis |")
    md.append("|---|---|---|---|---|")
    for l, a in A.items():
        j = a['jitter']; g = lambda k: f"{f(j[k][0])} / {f(j[k][1])} / {f(j[k][2],1)}"
        md.append(f"| {l} | {g('raw')} | {g('ema')} | {g('hyst')} | {g('comb')} |")
    md.append("\n## Responsiveness to persistent colour steps (a step = raw vibrant moves >= 20 dE and stays; latency = time until the variant is within 10 dE of the new level)\n")
    md.append("| scene | steps | raw reached / median s | EMA reached / median s | hysteresis reached / median s | EMA + hysteresis reached / median s |")
    md.append("|---|---|---|---|---|---|")
    for l, a in A.items():
        L = a['latency']; g = lambda k: f"{pc(L[k][0])} / {f(L[k][1],1)}"
        md.append(f"| {l} | {a['steps']} | {g('raw')} | {g('ema')} | {g('hyst')} | {g('comb')} |")
    md.append("\n## Hold-last-valid policy (offline; what the ribbon would show while vibrant is invalid)\n")
    md.append("| scene | time showing a held colour | hold runs (count / mean s / max s) | mean dE held -> next valid | holds that were wrong (dE>20) | holds >= 3 s that were wrong |")
    md.append("|---|---|---|---|---|---|")
    for l, a in A.items():
        h = a['hold']
        md.append(f"| {l} | {pc(h['share'])} | {h['runs']} / {f(h['mean_s'],1)} / {f(h['max_s'],1)} | {f(h['stale_err_mean'],1)} | {h['misleading'][0]} of {h['misleading'][1]} | {h['long_misleading'][0]} of {h['long_misleading'][1]} |")
    md.append("\n## Incremental cost (desktop; p50 / p95 / max)\n")
    md.append("| scene | submit ms (not needed in production) | 16x9 read ms (already paid by the series) | 160x90 read ms (EXTRA) | 160x90 histogram us | vibrant raw+EMA+hysteresis us |")
    md.append("|---|---|---|---|---|---|")
    for l, a in A.items():
        c = a['cost']; g = lambda k, n=3: f"{f(c[k][0], n)} / {f(c[k][1], n)} / {f(c[k][2], n)}"
        md.append(f"| {l} | {g('ms_submit')} | {g('ms_read16')} | {g('ms_read160')} | {g('us_an160',1)} | {g('us_vib',1)} |")
    open(os.path.join(out, 'results_vibrant.md'), 'w').write('\n'.join(md) + '\n')
    print('\n'.join(md))

if __name__ == '__main__':
    main()
