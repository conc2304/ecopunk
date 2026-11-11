# Region ID rename: change proposal (NOT APPLIED)

**Status:** PROPOSED. Region IDs are a frozen shared contract (`Architecture-Freeze-HUD-006-Canonical-Blueprint-Identity-v1`). Nothing in the runtime has been renamed. The v3 reference set shows the proposed names as `displayName` beside the frozen `id` only. Names and order were specified by the operator on 2026-11-10 from the running HUD and verified against `HudPresentationProfile.cpp` bindings.

## Why

Several legacy IDs no longer describe what their region holds, and some actively mislead:

- `universal.media_title` holds the **scene mode**. The media title lives in `universal.primary_state`.
- `universal.activity` holds the **scene primary-state badge**. The **activity meter** lives in `universal.timing`.
- `universal.effect_summary` holds the **scene secondary state** (and effect chips where present).
- `universal.activity_trace` / `universal.secondary_trace` hold the **Orbit frame metrics**.
- `flexible.primary` holds the **runtime (OS) metrics**.
- `overlay.health` is the **transition state**; `overlay.transition` is the **runtime log**.

## Mapping (numbering = HUD reading order)

| # | Current ID | Proposed ID | Holds now |
|---|---|---|---|
| 1 | `universal.scene_title` | `universal.scene_title` | scene title + `[EXP x/y]` |
| 2 | `universal.media_title` | `universal.scene_mode` | scene mode name + `[Mode x/y]` |
| 3 | `universal.primary_state` | `universal.media_state` | media title, sample, progress, time |
| 4 | `universal.activity` | `universal.scene_primary_state` | primary-state badge |
| 5 | `universal.effect_summary` | `universal.scene_secondary_state` | scene status lines / effect chips |
| 6 | `universal.timing` | `universal.activity_meter` | measured visual activity bar |
| 7 | `universal.activity_trace` | `universal.frame_metrics_a` | Orbit upper (swatch, RGB bars, chart) |
| 8 | `universal.secondary_trace` | `universal.frame_metrics_b` | Orbit lower (raw video / luminance) |
| 9 | `flexible.primary` | `flexible.runtime_metrics` | FPS, frame max, CPU, GPU temp, RSS |
| 10 | `flexible.secondary_a` | `flexible.active_time` | active time |
| 11 | `flexible.secondary_b` | `flexible.date_time` | clock + date |
| 12-18 | `controls.*` | unchanged | seven nav squares, left to right |
| 19 | `overlay.health` | `overlay.transition_state` | transition %, health, phase |
| 20 | `overlay.transition` | `overlay.runtime_log` | console lines |
| 21 | `media_viewport` | unchanged | media mesh |

Open points for the reviewer:
- The role prefixes (`universal`, `flexible`, `overlay`) are kept because they drive the role enum. `flexible.active_time` / `date_time` are arguably universal now; that is a separate question.
- Whether the **catalog order** should also become this order. At present only the *export* is ordered. Catalog order is what `controls.menu` was appended to ("existing numbering untouched"), and focus-ring and slot logic may depend on it. Check before reordering.
- Renderer provenance labels ("System Telemetry v1" on the two trace regions) are stale relative to content (see `HUD_ART_HANDOFF.md`).
- The runtime button labels say EXP (`PREV EXP`, `NEXT EXP`) while IDs say `scene_*`. Not addressed here.

## Blast radius (files containing the old IDs as string literals)

- `shared/src/hud-compositor/`: `HudRegionCatalog.cpp`, `HudPresentationProfile.{h,cpp}`, `HudWireframeRenderer.cpp`
- `shared/src/hud-compositor-test/`: `hud_presentation_tests.cpp`, `hud_vitals_tests.cpp`
- `sketches/experience_runtime/`: `src/Hud006v2Harness.cpp`, `src/ModeNavigationHarness.cpp`, `src/TypographyPlayground.cpp`, `src/HudArtReferenceExporter.cpp`, `test/hud_focus_navigator_tests.cpp`
- `assets/hud/dev-typography/preset.json`
- Skin manifests and art shipped against the v2 IDs (not scanned; skins are rejected by `blueprintVersion`, not by region ID, so this needs an explicit check).

## Compatibility questions the decision owner must answer

1. Is this a `blueprintVersion` bump (3) or an alias layer (old IDs accepted, new IDs canonical)? The freeze doc says enabling regions leaves bounds and IDs untouched; it does not say whether renaming is in scope for v2.
2. If it is a bump, `SKIN_BLUEPRINT_VERSION_MISMATCH` rejects every v2 skin until a v3 shell exists (same fallback-to-wireframe as v1 to v2).
3. Does anything persisted (settings, presets, logs, telemetry) key on region IDs? `preset.json` does for the typography playground.

## Recommendation

Prefer an **alias layer** with no `blueprintVersion` bump: the catalog is keyed by new IDs, and the old IDs resolve through a fixed table for one release, so shipped skins and presets keep loading. Do not implement until the freeze owner accepts or rejects this.
