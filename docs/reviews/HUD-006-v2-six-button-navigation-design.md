# HUD-006 v2: six-button navigation (design and contract migration)

**Status:** design proposal, needs Architecture authorization. No geometry has changed. **Date:** 2026-10-22.

**HUD-006 v2 is not compatible with v1.** It moves and resizes the controls row, retires one region and adds two. Anything rendered or computed against v1 control geometry has to be regenerated: the shell, the pressed sprites, fixtures and reference exports.

Target row: `[PREV SCENE] [NEXT SCENE] [MODE] — [PREV MEDIA] [NEXT MEDIA] — [FULLSCREEN]`

## 1. Current implementation (source-verified)

| Concern | Today (v1) | Source |
| --- | --- | --- |
| Canonical regions | 19, normalized on a 1280×720 canvas | `shared/src/hud-compositor/HudRegionCatalog.cpp` |
| Controls row | Five rectangles at y = 648, h = 43.2: `scene_previous` x 25.6, `scene_next` 153.6, `media_previous` 307.2, `media_next` 435.2 (w 102.4); `reseed` 588.8 (w 128) | `HudRegionCatalog.cpp` |
| `controls.reseed` | Since v0.3 hosts the **IMMERSIVE** icon (scene-only presentation, not OS fullscreen). Regenerate moved to key `2`. | `HudPresentationProfile.cpp`, `ExperienceRuntime::handleFocusInput` |
| Mode | Informational **and** focusable: `flexible.secondary_a` NumericValue with an `enabled` role (`control.scene.next_mode.available`). Enter there dispatches `SceneCommand::NextMode`. | `HudPresentationProfile.cpp`, `ExperienceRuntime.cpp` |
| Focus ring | `scene_previous, scene_next, media_previous, media_next, reseed, flexible.secondary_a` | `HudCompositorBridge.h` (`focus_`) |
| Native fullscreen | `RuntimeCommand::ToggleFullscreen` (`f`) and the Installation Controls request. Observable through `InstallationStatus.fullscreen` (`ofGetWindowMode`). RT-007 presentation scales the canonical composition. | `InputRouter.cpp`, `ExperienceRuntime::tickInstallation`, `PresentationTransform.h` |
| Pressed artwork | One optional sprite for `controls.scene_next`: 112×52, unscaled at (149, 644) | `HudReactiveControlArt.h`, `assets/hud/skins/scene_next/placement.json` |
| Media viewport | x 25.6, y 115.2, 768×504; bottom edge y = 619.2 | `HudRegionCatalog.cpp` |
| Space below the viewport | y 619.2 → 720 (100.8 px). The current row uses 648 → 691.2. | derived |

**Duplicated control geometry (a v2 risk).** Control rectangles are currently written down in five places:

1. `HudRegionCatalog.cpp`, the authority.
2. `sketches/experience_runtime/test/skin_fixtures/make_skin_fixtures.py` (`REGIONS`, hand-copied).
3. `HudReactiveControlArt.h`, the scene_next constants and the sprite rectangle.
4. `assets/hud/skins/scene_next/placement.json`, from HUD Art.
5. The Blender scene and layout scripts.

v2 must reduce this to one source (§4).

## 2. Proposed v2 geometry

**Square side 72 px** (0.10 of the canvas height). That allows a two-line runtime label such as "PREV" over "SCENE", at 5 characters × 8 px = 40 px wide, with ≥ 3 px padding, or an icon up to about 48 px. **Gaps:** 12.8 px inside a group, 32 px between groups. **Row:** y = 633.6 to 705.6, centred on the v1 row's centre line (y 669.6). That leaves 14.4 px above (below the viewport's bottom edge at 619.2) and 14.4 px to the canvas bottom.

| Region ID | Status | x | y | w | h | Normalized (x, y, w, h) |
| --- | --- | --- | --- | --- | --- | --- |
| `controls.scene_previous` | kept (moved/resized) | 25.6 | 633.6 | 72 | 72 | 0.02, 0.88, 0.05625, 0.10 |
| `controls.scene_next` | kept (moved/resized) | 110.4 | 633.6 | 72 | 72 | 0.08625, 0.88, 0.05625, 0.10 |
| `controls.mode` | **new** | 195.2 | 633.6 | 72 | 72 | 0.1525, 0.88, 0.05625, 0.10 |
| `controls.media_previous` | kept (moved/resized) | 299.2 | 633.6 | 72 | 72 | 0.23375, 0.88, 0.05625, 0.10 |
| `controls.media_next` | kept (moved/resized) | 384.0 | 633.6 | 72 | 72 | 0.30, 0.88, 0.05625, 0.10 |
| `controls.fullscreen` | **new** | 488.0 | 633.6 | 72 | 72 | 0.38125, 0.88, 0.05625, 0.10 |

The row spans x 25.6 to 560.0, inside the media viewport's column (25.6 to 793.6). That leaves 233.6 px free for the shell's decorative housing. The mode panel (`flexible.secondary_a`) and the clock (`flexible.secondary_b`) keep their v1 geometry.

### Region counts

| Set | v1 | v2 | Change |
| --- | --- | --- | --- |
| Canonical | 19 | **20** | + `controls.mode`, + `controls.fullscreen`, − `controls.reseed` |
| Production | 17 | **18** | same change (`controls.reseed` was in production) |

### Old-to-new crosswalk

| v1 | v2 | Note |
| --- | --- | --- |
| `controls.scene_previous` | `controls.scene_previous` | same ID, new rectangle |
| `controls.scene_next` | `controls.scene_next` | same ID, new rectangle; pressed sprite must be re-rendered (§5) |
| `controls.media_previous` | `controls.media_previous` | same ID, new rectangle |
| `controls.media_next` | `controls.media_next` | same ID, new rectangle |
| `controls.reseed` (IMMERSIVE since v0.3) | **retired** | see decision D1 |
| mode activation via `flexible.secondary_a` | `controls.mode` | the panel becomes informational (no `enabled` role) |
| none | `controls.fullscreen` | native host fullscreen toggle |
| every other region | unchanged | 14 regions, geometry identical |

## 3. Functional migration

| Control | Command path (existing pipeline only) | Availability (`enabled` source) |
| --- | --- | --- |
| PREV / NEXT SCENE | `RuntimeCommand::PreviousScene` / `NextScene` (`[` `]`) | `control.scene.previous/next.available` (≥ 2 scenes, transition idle). Unchanged. |
| MODE | `SceneCommand::NextMode` (`m`), dispatched to the active scene | Existing `control.scene.next_mode.available` (scene advertises > 1 mode). Moves from the mode panel to `controls.mode`. Hidden when the scene has no mode model. |
| PREV / NEXT MEDIA | `SceneCommand::PreviousMedia` / `NextMedia` (`p` `n`) | `control.media.previous/next.available`. Previous now wraps, so it's never disabled with more than one clip (2026-10-22 change). |
| FULLSCREEN | Installation Controls fullscreen request, the same path as `f` / `RuntimeCommand::ToggleFullscreen`. No second state authority. | New `control.runtime.fullscreen.available`: true once the runtime has a window. The label/icon shows the **observed** state from `InstallationStatus.fullscreen`, never the request. |

**Focus ring v2:** `scene_previous → scene_next → mode → media_previous → media_next → fullscreen`. The mode panel leaves the ring. Restoration, disabled/refused feedback and the activation flash work as today, keyed by region. Hidden controls (for example MODE in a scene without modes) are skipped.

**Keyboard shortcuts:** unchanged (`[` `]` `m` `p` `n` `f`; arrows and Enter move and activate focus).

**HUD-004 pressed artwork:** the extension stays limited to `controls.scene_next`. The sprite rectangle must be re-derived from the new region (§4); the v1 sprite (112×52 at 149,644) is invalid under v2.

**Scene and mode authority stay outside the HUD:** the HUD only reads advertised capabilities and dispatches existing commands.

### Decisions needing Architecture

- **D1 — IMMERSIVE.** v2 replaces the IMMERSIVE button with native FULLSCREEN. Options:
  - (a) IMMERSIVE keeps a key binding only (proposed: `i`; currently unbound);
  - (b) FULLSCREEN means IMMERSIVE (scene-only) instead of host fullscreen;
  - (c) a seventh control.

  I recommend (a). It matches the brief's "dedicated fullscreen icon button" and keeps the two meanings distinct, as RT-007 requires.
- **D2 — Labels in 72 px squares.** Text labels need a stacked two-line `button` variant in `LabelWidget` (runtime-owned). The alternative is icons for all six, using the existing reserved-icon slot, with SVG art later. I recommend stacked text now and icons later. MODE could show the mode name as a second line, which is runtime-owned and read from the scene.
- **D3 — Pressed-sprite rule.** Replace the hardcoded 112×52 @ (149,644) with a derived rule: the region rectangle expanded by 4 px on every side, snapped outward to whole pixels. For v2 `scene_next` that gives **81×81 at (106, 629)**. The loader then validates against the derived size, so no second copy of the geometry is needed.
- **D4 — v1 compatibility.** v1 skins and sprites are invalid under v2. Bump `blueprintVersion` to 2. The existing manifest validator then rejects v1 skins cleanly instead of drawing a misaligned shell.

## 4. One authoritative geometry source

- `HudRegionCatalog.cpp` remains the only definition.
- The reference exporter (`HudArtReferenceExporter`) writes `docs/hud-art/blueprint-v2-reference/regions.json`, which becomes the machine-readable contract for everything outside C++.
- **Fixture generator:** `make_skin_fixtures.py` reads `regions.json` instead of its hand-copied `REGIONS`.
- **Reactive art:** `HudReactiveControlArt.h` derives the sprite rectangle from the catalog region (D3). The constants go away.
- **Blender:** the layout script reads `regions.json` (§5). `placement.json` becomes a generated output for each sprite, not hand-authored input.

### Reference artifacts to regenerate (exporter, after approval)

- clean production schematic;
- labeled region schematic;
- wireframe;
- constraints and content-safe overlay;
- `regions.json`;
- production membership table (18);
- this crosswalk as `crosswalk-v1-v2.json`.

All of these come from the same exporter run.

## 5. Blender migration specification

The sources named in HUD Art's `placement.json` aren't present on this machine, so this section specifies requirements. It does not inspect actual objects:
- `ecopunk_hud_scaffold12_control_housings.blend`;
- `scripts/hud_button_states.py`;
- `asset_config.json` > `button_states`;
- the `asset_render.render` pipeline (Cycles, 4× supersample, `dark_moss_balanced` compositor, straight sRGB8 output).

1. **Parameterized row.** One reusable button-housing component, a single collection instance or geometry-nodes asset, placed six times by a script reading `regions.json`. Size and position come from the region rectangle; no button is moved by hand in any file.
2. **Square faces.** Six identical 72×72 canvas-px faces. Housing bleed outside the region is the same for all six and bounded by D3 (≤ 4 px).
3. **Shell output unchanged.** 1280×720 RGBA8 straight alpha. The 19 region interiors stay transparent, which becomes 20 under v2: `controls.mode` and `controls.fullscreen` are added and `controls.reseed` is cleared. Labels and icons are never baked.
4. **Pressed sprites.** `hud_button_states.py` renders each state crop from the region-derived sprite rectangle (D3). It emits a `placement.json` with the derived rectangle and the decoded-pixel SHA-256. Edges stay pre-blended against the v2 shell's rail, and alpha stays binary.
5. **Other skin variants.** Each needs re-rendering against v2 geometry, and its `skin.json` gets `blueprintVersion: 2`. v1 variants remain loadable only by v1 runtimes; the D4 validator rejects them under v2.

Don't render final art until the coordinates above are approved.

## 6. Affected C++ files and tests

- **Geometry and profile:**
  - `HudRegionCatalog.cpp`: the six controls, minus `reseed`;
  - `HudPresentationProfile.cpp`: control bindings, the mode panel without `enabled`, the `controls.mode` and `controls.fullscreen` bindings;
  - `HudArtReferenceExporter.cpp`: v2 export, production count 18.
- **Sources and vocabulary:** `HudSourceResolver.cpp` and `HudRealFrameResolver.cpp` (`control.runtime.fullscreen.available` and its label/state; `control.scene.next_mode.available` reused); `HudVocabularyResolver.cpp` (labels and icon IDs).
- **Focus and dispatch:** `HudCompositorBridge.h` (focus ring); `ExperienceRuntime::handleFocusInput` (`controls.mode` → NextMode, `controls.fullscreen` → Installation fullscreen request, `controls.reseed` mapping removed).
- **Widgets:** `LabelWidget.cpp` (stacked two-line `button` variant, D2).
- **Reactive art:** `HudReactiveControlArt.h` (derived sprite rectangle, D3); `HudSkinLoader.cpp` (validate the derived size).
- **Manifest:** the skin manifest validator's policy (`blueprintVersion` 2, D4).
- **Tests and fixtures:** `hud_presentation_tests.cpp` (region counts, allowlist, bindings); `hud_focus_navigator_tests.cpp`; `hud_skin_loader_tests.cpp` and `make_skin_fixtures.py` (read `regions.json`); the HUD-006 reference `verification.txt`.
- **Harnesses:**
  - FocusMenu (ring order and stops);
  - BlueprintHud (IMMERSIVE expectations);
  - ModeNavigation (mode panel no longer focusable; MODE button);
  - v0.3, Reactive and RT-007 (pressed sprite position);
  - Typography (button targets).

## 7. Smallest safe implementation sequence

1. Architecture decides D1–D4 and approves the coordinates in §2.
2. Catalog, exporter and `regions.json` (one change). Regenerate the reference artifacts and review them visually.
3. Switch the fixture generator and the reactive sprite rule to `regions.json` and the derived rectangle. Unit tests go green.
4. Profile, sources, vocabulary, focus ring and dispatch (MODE, FULLSCREEN, mode panel informational). Update the focus and presentation tests, then the harnesses.
5. Stacked button labels (D2). Verify in the real app over the wireframe HUD, because the v1 shell is misaligned under v2.
6. HUD Art re-renders the shell and the pressed sprite from `regions.json`. Bump `blueprintVersion` to 2. Run the full verification over Dark Moss v2.

Do not stage, commit or push.
