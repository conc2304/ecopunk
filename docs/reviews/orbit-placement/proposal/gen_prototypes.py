import base64, sys, os
S = sys.argv[1]
CAP = S + "/iso2-dm/"
OUT = S + "/proto/"
BG, FR, CAPC, PRI, GRN, GLD = "#161912", "#3f716d", "#9aa88c", "#EDE6CF", "#B8D99A", "#cdd96a"
X, W = 844.8, 409.6
R = {"5": (216.0, 43.2), "6": (266.4, 36.0), "15": (460.8, 115.2)}
FONT = 'font-family="CalcProd, monospace" font-size="15"'
FONT_VITALS = 'font-family="Courier New, monospace" font-size="13.33"'
FACE = '<style>@font-face{font-family:CalcProd;src:url(file:///Users/joseconchello/openFrameworks/apps/myApps/EcopunkVideoCollage/assets/hud/dev-typography/fonts/Calculator.ttf)}</style>'

def t(x, y, s, c=PRI, extra=""):
    return f'<text x="{x:.1f}" y="{y:.1f}" {FONT} fill="{c}" {extra}>{s}</text>'

def cover(k, pad=0):
    y, h = R[k]
    return f'<rect x="{X-2:.1f}" y="{y-1:.1f}" width="{W+4:.1f}" height="{h+2:.1f}" fill="{BG}" fill-opacity=".93"/>'

def scene_lines(scene, y):
    """Two native 8x13 lines, baselines y+17 / y+31 (region h 43.2 or a 35.2 px section)."""
    b1, b2 = y + 17, y + 35
    x0 = X + 4
    o = ""
    if scene == "fragment":
        o += t(x0, b1, "TRAIL STATUS", CAPC) + t(x0 + 17 * 8, b1, "17", PRI)
        o += t(x0, b2, "PRESET", CAPC) + t(x0 + 17 * 8, b2, "HUNT", PRI)
    elif scene == "quadrant":
        for i, ((cx, cy), st) in enumerate(zip([(0, b1), (1, b1), (0, b2), (1, b2)], ["ONLINE", "QUIET PHASE", "QUIET PHASE", "QUIET PHASE"])):
            xx = x0 + cx * 204.8
            o += t(xx, cy, f"Q{i}", CAPC) + t(xx + 26, cy, st, GRN)
    elif scene == "blueprint":
        o += t(x0, b1, "CYCLE PROGRESS", CAPC) + t(x0 + 17 * 8, b1, "62%", PRI)
        o += f'<rect x="{x0:.1f}" y="{b2-6:.1f}" width="{W-8:.1f}" height="6" fill="none" stroke="{FR}"/><rect x="{x0:.1f}" y="{b2-6:.1f}" width="{(W-8)*0.62:.1f}" height="6" fill="{GLD}"/>'
    return o

def micro(x, y, w, seed):
    import math
    pts = " ".join(f"{x+i*w/40:.1f},{y+4+3*math.sin(i*0.9+seed)+1.5*math.sin(i*2.3):.1f}" for i in range(41))
    return f'<polyline points="{pts}" fill="none" stroke="#5fbf8f" stroke-width="1"/>'

def optionA(scene):
    o = cover("5") + cover("15")
    y0 = R["15"][0]
    o += scene_lines(scene, y0 - 4)  # 35.2 px scene section
    o += f'<line x1="{X}" y1="{y0+35.2}" x2="{X+W}" y2="{y0+35.2}" stroke="{FR}" stroke-opacity=".6"/>'
    rows = [("FPS", "30.0 Hz"), ("FRAME MAX", "64.4 ms"), ("CPU", "--"), ("GPU TEMP", "--"), ("RSS", "--")]
    for i, (lab, val) in enumerate(rows):
        top = y0 + 36 + i * 15.8
        b = top + 12
        o += t(X + 4, b, lab, CAPC, "").replace(FONT, FONT_VITALS) + t(X + 4 + 80, b, val, PRI if val != "--" else CAPC).replace(FONT, FONT_VITALS)
        if val != "--":
            o += micro(X + 164, top + 1, 240, i)
        else:
            o += f'<line x1="{X+164}" y1="{top+14}" x2="{X+W-4}" y2="{top+14}" stroke="{FR}" stroke-opacity=".6"/>' + t(X + 170, b, "NO DATA", CAPC, 'fill-opacity=".7"').replace(FONT, FONT_VITALS)
    return o

def optionB(scene):
    return cover("5") + scene_lines(scene, R["5"][0])

def label(x, y, s):
    return f'<text x="{x}" y="{y}" font-family="Helvetica, Arial" font-size="15" font-weight="bold" fill="#fff">{s}</text>'

for scene in ["fragment", "quadrant", "blueprint"]:
    png = base64.b64encode(open(CAP + f"s_{scene}_window.png", "rb").read()).decode()
    img = f'<image x="0" y="0" width="1280" height="720" href="data:image/png;base64,{png}"/>'
    # full-HUD, preferred option B'
    open(OUT + f"full_{scene}_optionB.svg", "w").write(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="720">{FACE}{img}{optionB(scene)}</svg>')
    open(OUT + f"full_{scene}_optionA.svg", "w").write(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="720">{FACE}{img}{optionA(scene)}</svg>')
    # side-by-side crops of the right column: current | B | A
    pw, ph, cx, cy = 440, 440, 830, 150
    def panel(ox, over):
        return (f'<svg x="{ox}" y="30" width="{pw}" height="{ph}" viewBox="{cx} {cy} {pw} {ph}">{img}{over}</svg>')
    cmp_ = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{3*pw+40}" height="{ph+40}">{FACE}<rect width="100%" height="100%" fill="#000"/>'
            + label(10, 20, "CURRENT (real capture)") + label(pw + 20, 20, "B: scene state in #5 (preferred)") + label(2 * pw + 30, 20, "A: shared #15")
            + panel(0, "") + panel(pw + 10, optionB(scene)) + panel(2 * pw + 20, optionA(scene)) + '</svg>')
    open(OUT + f"compare_{scene}.svg", "w").write(cmp_)
print("ok")
