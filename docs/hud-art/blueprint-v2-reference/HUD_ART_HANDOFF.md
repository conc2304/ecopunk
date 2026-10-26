# HUD-006 v2 — HUD Art handoff

**Source of truth:** `regions.json` in this folder. It is generated from `HudRegionCatalog` by `HudArtReferenceExporter` (`EXPERIENCE_RUNTIME_HUD_ART_EXPORT=1`). Don't copy coordinates by hand; read them from that file.

## What changed (v1 → v2)

- **Navigation row:** six 72×72 squares at y 633.6, in groups `[PREV SCENE][NEXT SCENE][MODE]`, `[PREV MEDIA][NEXT MEDIA]`, `[FULLSCREEN]`.

  | Control | x |
  |---|---|
  | PREV SCENE | 25.6 |
  | NEXT SCENE | 110.4 |
  | MODE | 195.2 |
  | PREV MEDIA | 299.2 |
  | NEXT MEDIA | 384.0 |
  | FULLSCREEN | 488.0 |

- **Region IDs:** `controls.reseed` is retired. `controls.mode` and `controls.fullscreen` are new.
- **Region counts:** 20 canonical, 18 production. `crosswalk-v1-v2.json` lists every move, addition and retirement.
- **Mode information:** informational only (not focusable). Its one-line text now lives in `universal.media_title`; `flexible.secondary_a` holds active time (see the content map). The clock keeps its v1 geometry.
- **Media viewport:** unchanged, including its bevel and clipping.

## Region content map (content relocation, 2026-10-23; geometry unchanged)

HUD Art panels should be authored for what each region now holds:

| Region | Contents |
|---|---|
| `universal.scene_title` | scene title (left) + `[Scene x/y]` (right, small) |
| `universal.media_title` | one line: `Mode: <mode name>   x/y` |
| `universal.primary_state` | media title (centred) + playback progress bar and `current / total` |
| `universal.activity` | scene primary-state badge (icon slot, phase, detail) |
| `universal.effect_summary` | activity bar and percentage |
| `universal.timing` | active effect chips, with the effects health right-aligned on the same row |
| `universal.activity_trace`, `universal.secondary_trace` | unchanged sparklines |
| `flexible.primary` | unchanged scene card |
| `flexible.secondary_a` | active time (`ACTIVE TIME   3:42`) |
| `flexible.secondary_b` | clock (unchanged) |
| `overlay.health` | `TRANSITION x%` + bar with the scene health (e.g. `READY`) right-aligned on the same line, and the phase (`IDLE`) beneath |
| `overlay.transition` | recent activity log, the last 1-4 console lines depending on text size |

The transition message (e.g. "NextScene accepted ...") is no longer displayed.

## Shell requirements (`shell.png`)

- 1280×720, RGBA8, straight alpha, sRGB, no colour profile conversion.
- Every region interior stays transparent. That includes the six new squares, and the old `controls.reseed` area is no longer a region. Use `hud_reference_constraints_1280x720.png` as the overlay.
- No baked text or icons. Labels are runtime-owned: stacked two-line text, centred in each 72×72 square ("PREV / SCENE", "NEXT / MODE", "FULL / SCREEN" or "EXIT / FULL").
- Set `"blueprintVersion": 2` in `skin.json`. That value is the only manifest change. **v1 skins are rejected** with `SKIN_BLUEPRINT_VERSION_MISMATCH`; the runtime falls back to the wireframe until a v2 shell exists.

## Pressed sprite (optional, NEXT SCENE only)

- **Path, unchanged:** `controls/scene_next/pressed.png`.
- **Rectangle:** derived from the region, which is expanded by 4 px on every side and snapped outward to whole pixels. For v2 that is **81×81 at canvas (106, 629)**; `regions.json` → `reactiveArtwork` carries the same value.
- **Drawing:** unscaled, binary or straight alpha, edges pre-blended against the v2 shell rail.
- **Behaviour:** interaction and focus bounds stay the 72×72 square.
- **Validation:** any other size is a nonfatal wrong-size diagnostic, and the skin still loads. The current v1 sprite (112×52 @ 149,644) is rejected under v2.
- **`focused.png`:** still not part of the contract and not loaded.

## Shared button faces (delivered 2026-10-25, integrated)

`controls/button/{idle,focused,active}.png`, **80×80** each, one shared set per skin for all six buttons. The size and the centring (the control region plus 4 px bleed per side, snapped to whole pixels) are in `regions.json` → `buttonFace`. Delivered for all three skins (Dark Moss, Abyssal Bioluminescence, Plywood + Marble). For the full contract see `docs/hud-art/hud-004-optional-pressed-artwork-extension.md` (addendum).

Notes for the next iteration:
- The faces now cover the shell's baked button housings, so the shell and the faces should be re-rendered together to avoid seams.
- Label contrast is verified on all nine faces: light text on the dark faces, dark ink on the marble faces.
- `controls/scene_next/pressed.png` (81×81) is now redundant for a skin that supplies `active.png`.

## Blender

Drive the six housings from `regions.json` with one parameterized component, placed by script with no hand-positioned buttons. Render the pressed crop from the derived sprite rectangle, and emit `placement.json` from the same numbers.

## Reference set in this folder

| File | What it is |
|---|---|
| `regions.json` | the geometry contract |
| `hud_reference_schematic_1280x720.png` | labelled regions |
| `hud_reference_production_1280x720.png` | clean production |
| `hud_reference_wireframe_1280x720.png` | wireframe |
| `hud_reference_constraints_1280x720.png` | content-safe overlay |
| `runtime_wireframe_reference_1280x720.png` | real runtime frame |
| `production_membership.md` | which regions production draws |
| `crosswalk-v1-v2.json` | old-to-new region mapping |
| `verification.txt` | exporter checks (22/22) |
