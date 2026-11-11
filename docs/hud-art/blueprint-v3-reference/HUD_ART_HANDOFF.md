# HUD art reference set v3: HUD Art handoff

**Source of truth:** `regions.json` in this folder. It is generated from `HudRegionCatalog` by `HudArtReferenceExporter` (`EXPERIENCE_RUNTIME_HUD_ART_EXPORT=1`). Don't copy coordinates by hand; read them from that file.

**Naming:** v3 is the third revision of the *reference set* (v1 → v2 → v3). The **blueprint identity is still `ecopunk-hud-blueprint` / `blueprintVersion: 2`**, so `skin.json` keeps `"blueprintVersion": 2`. Nothing here is a manifest change. Enabling regions and adding the MENU square did not bump the blueprint version (see `Architecture-Freeze-HUD-006-Canonical-Blueprint-Identity-v1`). This set supersedes `blueprint-v2-reference`, which stopped at 20 regions and 18 production.

## What changed against the v2 reference set

The v2 PNGs and JSON were exported before these two changes shipped. Geometry of every v2 region is **unchanged**.

| Change | Detail |
|---|---|
| **`controls.menu` added** | 72×72 at **x 619, y 633.6**. It opens Experience Settings. It follows the six nav squares on the same row. Its right edge sits at x 691, which is the nav panel's inner right edge (723) minus the 32 px group gap. It clears FULLSCREEN by 59 px. It uses the shared button faces, so no new art. Its face rect is `[615, 630, 80, 80]`. |
| **`universal.activity_trace` and `universal.secondary_trace` are now production** | They were FullProfile-only ("not drawn") in v2. They now draw the Orbit Console frame metrics. They sit at (844.8, 309.6) and (844.8, 388.8), each 409.6×72. A shell must treat them as live content regions. |
| **Counts** | 21 canonical, **21 production**, 0 FullProfile-only. The v2 figures were 20 and 18, with 2 FullProfile-only. |

Production parts (renderer provenance): historical MinimalSlice 12, Living HUD 2, Clock 1, Mode Navigation 4 (`flexible.secondary_a`, `controls.mode`, `controls.fullscreen`, `controls.menu`), "System Telemetry" 2 (the two trace regions). Details are in `production_membership.md`.

## Numbering and names (changed in this set)

- **Region numbers follow the HUD's reading order**, not catalog order: right column top-down (#1-11), navigation row left to right (#12-18), the two overlays (#19-20), media viewport (#21). v2 numbers do not carry over. Use the region **ID**, never the number, as the stable key.
- `regions.json` carries a **`displayName`** per region (proposed). The **`id` is still the frozen v2 ID** and is what skins and the runtime use. The rename is a pending contract proposal: see `RENAME_PROPOSAL.md`.

| # | ID | Proposed name | What the runtime draws there |
|---|---|---|---|
| 1 | `universal.scene_title` | `scene_title` | scene title (left) + `[EXP x/y]` (right, small) |
| 2 | `universal.media_title` | `scene_mode` | scene mode name + `[Mode x/y]` |
| 3 | `universal.primary_state` | `media_state` | media file title, `[Sample n/N]`, playback progress bar and `current / total` |
| 4 | `universal.activity` | `scene_primary_state` | scene primary-state badge (icon, phase, detail) |
| 5 | `universal.effect_summary` | `scene_secondary_state` | scene status lines (e.g. `REGION FIELD 21`, `FRAGMENTS 10`), or active effect chips + effects health where a scene has them |
| 6 | `universal.timing` | `activity_meter` | segmented measured-visual-activity bar (`--` when unavailable) |
| 7 | `universal.activity_trace` | `frame_metrics_a` | Orbit Console upper half: RGB/luminance swatch, bars, chart |
| 8 | `universal.secondary_trace` | `frame_metrics_b` | Orbit Console lower half: raw-video / luminance history |
| 9 | `flexible.primary` | `runtime_metrics` | five system gauges: FPS, FRAME MAX, CPU, GPU TEMP, RSS |
| 10 | `flexible.secondary_a` | `active_time` | `ACTIVE TIME 3:42` |
| 11 | `flexible.secondary_b` | `date_time` | clock (large `HH:MM`) + date / zone |
| 12-18 | `controls.*` | unchanged | PREV EXP, NEXT EXP, NEXT MODE, PREV MEDIA, NEXT MEDIA, FULL SCREEN, MENU |
| 19 | `overlay.health` | `transition_state` | `TRANSITION x%` + bar, scene health, phase |
| 20 | `overlay.transition` | `runtime_log` | last 1-4 console lines |
| 21 | `media_viewport` | unchanged | media mesh |

Verified against `HudPresentationProfile.cpp` bindings and `runtime_wireframe_reference_1280x720.png`. The v2 handoff's content map was wrong for #3, #5, #6, #7-9 (and v3 drafts of this file repeated it). The `scope` strings in `regions.json` ("System Telemetry v1" for #7-8) are renderer provenance labels and are misleading: those two regions carry the Orbit frame metrics, and the system vitals are in #9.

## Navigation row (seven 72×72 squares at y 633.6)

Groups: `[PREV SCENE][NEXT SCENE][MODE]`, `[PREV MEDIA][NEXT MEDIA]`, `[FULLSCREEN]`, then `[MENU]`.

| Control | x | 80×80 face rect (x, y) |
|---|---|---|
| PREV SCENE | 25.6 | 22, 630 |
| NEXT SCENE | 110.4 | 106, 630 |
| MODE | 195.2 | 191, 630 |
| PREV MEDIA | 299.2 | 295, 630 |
| NEXT MEDIA | 384.0 | 380, 630 |
| FULLSCREEN | 488.0 | 484, 630 |
| MENU | 619.0 | 615, 630 |

Focus ring order: scene_previous, scene_next, mode, media_previous, media_next, fullscreen, menu.

## Shell requirements (`shell.png`)

- 1280×720, RGBA8, straight alpha, sRGB, no colour profile conversion.
- **Every region interior stays transparent, including `controls.menu` and both trace regions.** Use `hud_reference_constraints_1280x720.png` as the overlay.
- No baked text or icons. Labels are runtime-owned.
- Keep `"blueprintVersion": 2` in `skin.json`.
- The nav panel's inner right edge (x 723, last panel pixel 722) must stay where it is. MENU's placement derives from it.

## Pressed sprite and shared faces

Unchanged from v2. `controls/scene_next/pressed.png` is optional, 81×81 at canvas (106, 629). `controls/button/{idle,focused,active}.png` are 80×80, one shared set per skin, now used for **all seven** buttons. Rects are in `regions.json` → `buttonFace`. Any skin whose faces were authored only for six housings needs its shell re-rendered with a housing under MENU.

## Reference set in this folder

| File | What it is |
|---|---|
| `regions.json` | the geometry contract (21 regions) |
| `hud_reference_schematic_1280x720.png` | labelled regions |
| `hud_reference_production_1280x720.png` | clean production |
| `hud_reference_wireframe_1280x720.png` | wireframe |
| `hud_reference_constraints_1280x720.png` | content-safe overlay |
| `runtime_wireframe_reference_1280x720.png` | real runtime frame |
| `production_membership.md` | which regions production draws |
| `crosswalk-v1-v2.json` | v1 → current mapping (filename kept by the exporter; it now includes `controls.menu` as added) |
| `verification.txt` | exporter checks (22/22 pass) |

## Regenerating

```bash
cd sketches/experience_runtime && make Release
env -u EXPERIENCE_RUNTIME_HUD_SKIN EXPERIENCE_RUNTIME_HUD_ART_EXPORT=1 \
  EXPERIENCE_RUNTIME_HUD_ART_EXPORT_DIR=$PWD/../../docs/hud-art/blueprint-v3-reference \
  ./bin/experience_runtime.app/Contents/MacOS/experience_runtime
```

Needs an active desktop session. Read `RESULT:` in `verification.txt`.
