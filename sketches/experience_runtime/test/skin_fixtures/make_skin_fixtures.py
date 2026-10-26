#!/usr/bin/env python3
"""Deterministic HUD-004 shell-loader test fixtures (Python stdlib only).

TEST FIXTURES — NOT PRODUCTION ART. Writes:

  <repo>/assets/hud/skins/fixture-shell-proof/       valid shell-only package in
                                                       the CANONICAL skin root
  sketches/experience_runtime/test/skin_fixtures/skins/<case>/
                                                       valid copies + one package
                                                       per rejection case

Every package uses the accepted tools/hud-skin-manifest schema exactly
(schemaVersion, skinId, displayName, skinVersion, blueprintId,
blueprintVersion, assets.shell{path, required, coordinateSpace,
expectedWidth, expectedHeight}) — no new keys. The symlink-escape case is NOT
written here (no symlinks in the repo); the harness and unit tests create it
at runtime in a scratch copy.

Shell image: 1280x720, color type 6 (RGBA), bit depth 8, straight alpha,
no gAMA/sRGB/iCCP chunks (sRGB by convention). Every one of the 19 HUD-006
region rectangles is forced to alpha 0, so the shell never covers live
content or the media viewport. A white alpha ramp (alpha = 0..255 across
x = RAMP_X0..RAMP_X1, rows RAMP_Y0..RAMP_Y1) lets the harness check the
composited framebuffer against the straight-alpha source-over formula.

Usage: python3 make_skin_fixtures.py
"""

import json
import pathlib
import shutil
import struct
import zlib

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[3]  # .../EcopunkVideoCollage
CANONICAL_ROOT = REPO / "assets" / "hud" / "skins"
TEST_ROOT = HERE / "skins"

W, H = 1280, 720
RAMP_X0, RAMP_X1, RAMP_Y0, RAMP_Y1 = 300, 1000, 694, 706  # bottom margin, below controls

# HUD-006 canonical regions, 1280x720 px — read from the generated v2 export
# (docs/hud-art/blueprint-v2-reference/regions.json, written by
# HudArtReferenceExporter from HudRegionCatalog). No hand-copied geometry.
REGIONS_JSON = REPO / "docs" / "hud-art" / "blueprint-v2-reference" / "regions.json"
_EXPORT = json.loads(REGIONS_JSON.read_text())
assert _EXPORT.get("blueprintVersion") == 2, "regions.json is not the HUD-006 v2 export"
REGIONS = [tuple(r["px"]) for r in _EXPORT["regions"]]
BLUEPRINT_VERSION = _EXPORT["blueprintVersion"]
# Optional NEXT SCENE pressed sprite size (region + 4 px bleed, snapped outward).
PRESSED_W, PRESSED_H = _EXPORT["reactiveArtwork"]["controls.scene_next"]["spriteRectPx"][2:4]

PALETTES = {
    "a": {"frame": (0, 200, 180, 150), "ring": (0, 230, 210, 220), "hatch": (170, 80, 255, 170),
          "corners": [(255, 0, 200, 230), (255, 220, 0, 230), (0, 200, 255, 230), (255, 120, 0, 230)]},
    "b": {"frame": (255, 170, 0, 160), "ring": (255, 200, 60, 220), "hatch": (40, 220, 90, 170),
          "corners": [(255, 255, 255, 230), (255, 255, 255, 230), (255, 255, 255, 230), (255, 255, 255, 230)]},
}


def in_region(x, y):
    for rx, ry, rw, rh in REGIONS:
        if int(rx) - 1 <= x <= int(rx + rw) + 1 and int(ry) - 1 <= y <= int(ry + rh) + 1:
            return True
    return False


def shell_pixels(variant, w=W, h=H):
    pal = PALETTES[variant]
    rows = []
    vx0, vy0, vx1, vy1 = 25, 115, 794, 620  # viewport rect, rounded outward
    for y in range(h):
        row = bytearray()
        for x in range(w):
            px = (0, 0, 0, 0)
            if x < 12 or y < 12 or x >= w - 12 or y >= h - 12:
                px = pal["frame"]
            if (vx0 - 8 <= x < vx1 + 8 and vy0 - 8 <= y < vy1 + 8) and not (vx0 <= x < vx1 and vy0 <= y < vy1):
                px = pal["ring"]
            if 808 <= x < 836 and 30 <= y < 620 and ((x + y) // 6) % 2 == 0:
                px = pal["hatch"]
            for i, (cx, cy) in enumerate([(0, 0), (w - 20, 0), (0, h - 20), (w - 20, h - 20)]):
                if cx <= x < cx + 20 and cy <= y < cy + 20:
                    px = pal["corners"][i]
            if RAMP_X0 <= x < RAMP_X1 and RAMP_Y0 <= y < RAMP_Y1:
                px = (255, 255, 255, round(255 * (x - RAMP_X0) / (RAMP_X1 - RAMP_X0 - 1)))
            if w == W and h == H and in_region(x, y):
                px = (0, 0, 0, 0)
            row += bytes(px)
        rows.append(bytes(row))
    return rows


def chunk(ctype, data):
    return struct.pack(">I", len(data)) + ctype + data + struct.pack(">I", zlib.crc32(ctype + data) & 0xFFFFFFFF)


def png(rows, w, h, bit_depth=8, color_type=6, extra_chunks=()):
    raw = b"".join(b"\x00" + r for r in rows)  # filter type 0 per row
    ihdr = struct.pack(">IIBBBBB", w, h, bit_depth, color_type, 0, 0, 0)
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
    for ctype, data in extra_chunks:
        out += chunk(ctype, data)
    return out + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def manifest(skin_id, display, version="0.0.1", blueprint_id="ecopunk-hud-blueprint", blueprint_version=None,
             shell_path="shell.png", extra_assets=None, include_shell=True):
    if blueprint_version is None:
        blueprint_version = BLUEPRINT_VERSION
    assets = {}
    if include_shell:
        assets["shell"] = {"path": shell_path, "required": True, "coordinateSpace": "canvas",
                           "expectedWidth": 1280, "expectedHeight": 720}
    for name, path in (extra_assets or {}).items():
        assets[name] = {"path": path, "required": False, "coordinateSpace": "canvas",
                        "expectedWidth": 1280, "expectedHeight": 720}
    return {"schemaVersion": 1, "skinId": skin_id, "displayName": display, "skinVersion": version,
            "blueprintId": blueprint_id, "blueprintVersion": blueprint_version, "assets": assets}


def write_package(root, dirname, man, files, dirs=()):
    pkg = root / dirname
    pkg.mkdir(parents=True, exist_ok=True)
    for old in pkg.iterdir():
        shutil.rmtree(old) if old.is_dir() and not old.is_symlink() else old.unlink()
    (pkg / "skin.json").write_text(json.dumps(man, indent=2) + "\n")
    for name, data in files.items():  # names may be nested ("controls/scene_next/pressed.png")
        (pkg / name).parent.mkdir(parents=True, exist_ok=True)
        (pkg / name).write_bytes(data)
    for d in dirs:  # empty directories (e.g. a directory where a file is expected)
        (pkg / d).mkdir(parents=True, exist_ok=True)


# Reactive Control Artwork v0.1 — optional controls/scene_next/pressed.png.
# TEST ART ONLY: a stand-in pressed sprite at the v2 derived size
# (controls.scene_next region + 4 px bleed, snapped outward; see regions.json). Straight alpha: a translucent green face (alpha 150)
# inside an opaque-ish rim (alpha 235), transparent outside a 2 px margin.
PRESSED = "controls/scene_next/pressed.png"


def pressed_pixels(w, h):
    k = 2
    rows = []
    for y in range(h):
        row = bytearray()
        for x in range(w):
            m = min(x, y, w - 1 - x, h - 1 - y)
            if m < k:
                px = (0, 0, 0, 0)
            elif m < 3 * k:
                px = (120, 255, 170, 235)
            else:
                px = (30, 150, 90, 150)
            row += bytes(px)
        rows.append(bytes(row))
    return rows


def main():
    valid_a = png(shell_pixels("a"), W, H)
    valid_b = png(shell_pixels("b"), W, H)

    # Corrupt IDAT byte, CRC left stale -> structural CRC failure.
    idat = valid_a.index(b"IDAT")
    corrupt_crc = bytearray(valid_a); corrupt_crc[idat + 40] ^= 0xFF
    # Corrupt IDAT byte with CRC recomputed -> structurally valid, zlib stream broken -> decoder failure.
    length = struct.unpack(">I", valid_a[idat - 4:idat])[0]
    body = bytearray(valid_a[idat + 4: idat + 4 + length]); body[len(body) // 2: len(body) // 2 + 64] = b"\x00" * 64
    corrupt_stream = valid_a[:idat - 4] + chunk(b"IDAT", bytes(body)) + valid_a[idat + 4 + length + 4:]

    rgb_rows = [bytes(b for i, b in enumerate(r) if i % 4 != 3) for r in shell_pixels("a")]
    ga_rows = [bytes(v for p in range(W) for v in (r[p * 4], r[p * 4 + 3])) for r in shell_pixels("a")]
    rgba16_rows = [bytes(v for b in r for v in (b, b)) for r in shell_pixels("a")]
    idx_rows = [bytes(0 if r[p * 4 + 3] == 0 else 1 for p in range(W)) for r in shell_pixels("a")]
    indexed = png(idx_rows, W, H, 8, 3, extra_chunks=[(b"PLTE", bytes([0, 0, 0, 0, 200, 180])), (b"tRNS", bytes([0, 150]))])

    CANONICAL_ROOT.mkdir(parents=True, exist_ok=True)
    write_package(CANONICAL_ROOT, "fixture-shell-proof",
                  manifest("fixture-shell-proof", "Shell Loader Proof Fixture (TEST ONLY - not production art)"),
                  {"shell.png": valid_a})

    TEST_ROOT.mkdir(parents=True, exist_ok=True)
    cases = {
        "fixture-shell-proof": (manifest("fixture-shell-proof", "Shell Loader Proof Fixture A (test)"), {"shell.png": valid_a}),
        "fixture-shell-proof-b": (manifest("fixture-shell-proof-b", "Shell Loader Proof Fixture B (test)", "0.0.2"), {"shell.png": valid_b}),
        "wrong-blueprint": (manifest("wrong-blueprint", "x", blueprint_id="hud-blueprint-v1"), {"shell.png": valid_a}),
        # A v1 skin under the v2 runtime: rejected (HUD-006 v2 D4).
        "wrong-blueprint-version": (manifest("wrong-blueprint-version", "x", blueprint_version=1), {"shell.png": valid_a}),
        "path-escape": (manifest("path-escape", "x", shell_path="../fixture-shell-proof/shell.png"), {}),
        "missing-shell": (manifest("missing-shell", "x"), {}),
        "shell-undeclared": (manifest("shell-undeclared", "x", include_shell=False), {"shell.png": valid_a}),
        "not-png": (manifest("not-png", "x"), {"shell.png": b"GIF89a this is not a png\n"}),
        "corrupt-png": (manifest("corrupt-png", "x"), {"shell.png": bytes(corrupt_crc)}),
        "corrupt-stream": (manifest("corrupt-stream", "x"), {"shell.png": corrupt_stream}),
        "truncated-png": (manifest("truncated-png", "x"), {"shell.png": valid_a[: len(valid_a) * 6 // 10]}),
        "rgb-png": (manifest("rgb-png", "x"), {"shell.png": png(rgb_rows, W, H, 8, 2)}),
        "gray-alpha-png": (manifest("gray-alpha-png", "x"), {"shell.png": png(ga_rows, W, H, 8, 4)}),
        "indexed-png": (manifest("indexed-png", "x"), {"shell.png": indexed}),
        "sixteen-bit-png": (manifest("sixteen-bit-png", "x"), {"shell.png": png(rgba16_rows, W, H, 16, 6)}),
        "wrong-size": (manifest("wrong-size", "x"), {"shell.png": png(shell_pixels("a", 640, 360), 640, 360)}),
        "decoration-declared": (manifest("decoration-declared", "x", extra_assets={"decoration": "decoration.png"}),
                                {"shell.png": valid_a, "decoration.png": valid_b}),
        "emissive-declared": (manifest("emissive-declared", "x", extra_assets={"emissive": "emissive.png"}),
                              {"shell.png": valid_a, "emissive.png": valid_b}),
        "id-mismatch": (manifest("some-other-id", "x"), {"shell.png": valid_a}),
    }
    for dirname, (man, files) in cases.items():
        write_package(TEST_ROOT, dirname, man, files)

    # -- Reactive Control Artwork v0.1 cases (valid shell + optional asset) --
    pressed_ok = png(pressed_pixels(PRESSED_W, PRESSED_H), PRESSED_W, PRESSED_H)
    pressed_rgb = png([bytes(b for i, b in enumerate(r) if i % 4 != 3) for r in pressed_pixels(PRESSED_W, PRESSED_H)], PRESSED_W, PRESSED_H, 8, 2)
    # Shared button faces (controls/button/{idle,focused,active}.png): control region + 4 px bleed per side.
    FACE_W = FACE_H = _EXPORT["buttonFace"]["size"][0]
    def face(r, g, b):
        rows = []
        for y in range(FACE_H):
            row = bytearray()
            for x in range(FACE_W):
                m = min(x, y, FACE_W - 1 - x, FACE_H - 1 - y)
                row += bytes((0, 0, 0, 0) if m < 1 else (r, g, b, 252))
            rows.append(bytes(row))
        return png(rows, FACE_W, FACE_H)
    face_idle, face_focused, face_active = face(40, 90, 70), face(70, 170, 110), face(200, 230, 120)
    wrong_face = png([bytes(FACE_W * 4 - 4) for _ in range(FACE_H - 8)], FACE_W - 2, FACE_H - 8)  # wrong size
    FACES = {"controls/button/idle.png": face_idle, "controls/button/focused.png": face_focused, "controls/button/active.png": face_active}
    pi = pressed_ok.index(b"IDAT")
    pressed_corrupt = bytearray(pressed_ok); pressed_corrupt[pi + 20] ^= 0xFF  # stale CRC
    optional_cases = {
        "pressed-valid": ({"shell.png": valid_a, PRESSED: pressed_ok}, ()),
        # The v1 HUD Art sprite size (112x52 @ 149,644): wrong under v2.
        "pressed-legacy-v1-112x52": ({"shell.png": valid_a, PRESSED: png(pressed_pixels(112, 52), 112, 52)}, ()),
        "pressed-rgb": ({"shell.png": valid_a, PRESSED: pressed_rgb}, ()),
        "pressed-wrong-size": ({"shell.png": valid_a, PRESSED: png(pressed_pixels(100, 40), 100, 40)}, ()),
        "pressed-corrupt": ({"shell.png": valid_a, PRESSED: bytes(pressed_corrupt)}, ()),
        "pressed-not-file": ({"shell.png": valid_a}, (PRESSED,)),
        # Shell broken, artwork valid: the skin must still be rejected, and
        # the optional asset must never be staged.
        "pressed-bad-shell": ({"shell.png": bytes(corrupt_crc), PRESSED: pressed_ok}, ()),
        "faces-valid": ({"shell.png": valid_a, **FACES}, ()),
        "faces-partial": ({"shell.png": valid_a, "controls/button/idle.png": face_idle}, ()),
        "faces-wrong-size": ({"shell.png": valid_a, "controls/button/idle.png": face_idle, "controls/button/focused.png": wrong_face,
                              "controls/button/active.png": face_active}, ()),
        "faces-and-pressed": ({"shell.png": valid_a, PRESSED: pressed_ok, **FACES}, ()),
    }
    for dirname, (files, dirs) in optional_cases.items():
        write_package(TEST_ROOT, dirname, manifest(dirname, "Reactive artwork fixture (test)"), files, dirs)
    cases.update(optional_cases)

    print(f"canonical: {CANONICAL_ROOT / 'fixture-shell-proof'} ({len(valid_a)} B shell.png)")
    print(f"test root: {TEST_ROOT} ({len(cases)} packages)")


if __name__ == "__main__":
    main()
