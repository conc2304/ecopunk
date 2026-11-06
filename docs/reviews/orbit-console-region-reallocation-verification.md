# Orbit Console region reallocation — placement verification (HUD Runtime)

Status: PLACEMENT VERIFIED, PROTOTYPE PRODUCED. No production binding, catalog, geometry, skin or scene
file was edited for this item. Production rebinding is blocked on Architecture (sec. 5). Not staged/committed/pushed.

## 1. Verified region mapping (source: `HudRegionCatalog.cpp`, `HudPresentationProfile.cpp`, `HudWireframeRenderer.cpp`, real captures)

Geometry in the code matches the canonical px table (844.8 x origin, 409.6 wide). Note: `hud-art/blueprint-v2-reference/production_membership.md`
is STALE — it lists #7/#8 as "FullProfile only (not drawn in production)", but System Telemetry v1 enabled both
(`systemTelemetryRegions()`), and they draw the five vitals sparklines today.

| # | Region (rect y,h) | Hosts TODAY | maxBindings / accepted widgets |
|---|---|---|---|
| 4 | universal.activity (165.6, 43.2) | primary-state badge: `scene.state.primary`, detail `scene.state.secondary`, phase icon | 1 / NumericValue, ProgressBar, StatusBadge |
| 5 | universal.effect_summary (216.0, 43.2) | "ACTIVITY" progress bar, `scene.activity.overall` | 2 / EffectChips, StatusBadge, ProgressBar, NumericValue |
| 6 | universal.timing (266.4, 36.0) | active-effect chips `effects.active` + trailing `effects.health` (Hide when no snapshot) | 1 / NumericValue, Label, EffectChips |
| 7 | universal.activity_trace (309.6, 72) | vitals sparklines FPS, FRAME MAX, CPU (3 x 24 px) | 3 / Sparkline |
| 8 | universal.secondary_trace (388.8, 72) | vitals sparklines GPU TEMP, RSS (2 x 36 px) | 2 / Sparkline |
| 15 | flexible.primary (460.8, 115.2) | ONE scene-specific widget (per scene, below) | 1 / MetadataCard, ChannelStrip, EffectChips, ProgressRing, Sparkline, Timeline, AmbientField |
| 16/17 | flexible.secondary_a/b | ACTIVE TIME / wall clock | unchanged |

Scene-specific #15 content today: Blob = MetadataCard (state + region count); Temporal = Sparkline of
`field_activity` (32 samples/8 s history); Fragment = MetadataCard (state, fragment count, movement preset);
Quadrant = ChannelStrip (4 channel states); Blueprint = ProgressRing (`phase_progress`, "CYCLE PROGRESS").
All five production scenes publish `scene.state.primary` / `scene.activity.overall` / `scene.time.active`.

## 2. Displaced-content inventory (target layout vs today)

| Today | Target | Consequence |
|---|---|---|
| #5 activity bar | -> #6 activity gauge | moves, nothing lost (needs ProgressBar accepted in #6) |
| #6 active-effect chips + health | no home in target | DISPLACED: names of active effects and the DEC-015 `effects.health`. Needs a decision (sec. 5-D) |
| #15 per-scene widget (5 kinds) | -> #5 "compact sketch state" | LOSSY: Temporal's 8 s history sparkline, Quadrant's 4-channel strip and Blueprint's ring cannot survive in one 43 px line; reduced to a text/glyph line (e.g. `Q0 ○ Q1 ○ Q2 ○ Q3 ○`, `PHASE 62%`, `REGIONS 7`) |
| #7/#8 five vitals sparklines | -> #15 compact metrics | 5 cells (2x3 grid, number + micro-sparkline, NO DATA preserved) |
| #4 | unchanged | primary/secondary state badge is preserved as is; no information displaced |
| Previous System/Orbit toggle proposal | paused | superseded if dedicated placement is approved |

## 3. Prototype (real region rectangles, real Quadrant capture, Dark Moss)

`docs/reviews/orbit-placement/prototype_full_quadrant_1280x720.png` — real 1280x720 runtime capture
(`five-scene-ring/s_quadrant_window.png`) with ONLY #5, #6, #7/#8, #15 repainted (#1-#4, #16/#17, controls,
viewport, overlays are the real pixels). `orbit_states.png` shows Orbit Live / Stale / Unavailable.
Source `.svg` files beside them. Limitations (honest): painted schematically with an approximate Dark Moss
palette and system monospace, NOT the production Argaka/skin renderer, so text metrics/clipping against the
real font are NOT verified; Orbit uses six elements (orbit ring, hue marker/radius=luma, RGB readout, luma
readout, RGB strip, luma strip) as my reading of "six-element" — HUD Art should confirm the list.

## 4. Concrete layout conflicts

1. **Widget acceptance**: #7/#8 accept only Sparkline; Orbit needs a new widget type. #6 does not accept ProgressBar. #15 maxBindings=1 vs five metrics (needs maxBindings 5 + a compact metric variant, or one composite widget). These are catalog edits (widget sets/maxBindings, not geometry) -> Architecture.
2. **Two regions, one widget**: Orbit spans #7+#8 (151.2 px, the 10.8 px gap included). The catalog has no cross-region binding; needs either one binding owning both rects or a combined draw-time slice (precedent: effect_summary/overlay slicing).
3. **#15 per-scene profiles**: `flexibleProfileForScene` and five scene profiles bind #15; moving system vitals there collides with `HudProfileCompiler` occupancy (maxBindings) and the scene-specific assumption that #15 is the scene's flexible slot. `hud_presentation_tests`/`hud_vitals_tests` assert the current region membership and will need updating.
4. **#5 is 43.2 px / #6 36 px**: one text line each; Blueprint/Temporal metrics must be reduced to one string.
5. **Contract**: Orbit needs `runtime.scene_color` (not published; contract proposal pending). Until then Orbit can only render Unavailable.
6. **Stale doc**: `production_membership.md` (see sec. 1) conflicts with the code.

## 5. Decisions needed from Architecture
A. Dedicated placement (#7/#8 Orbit, #15 metrics) replaces the System/Orbit toggle? (assumed yes; toggle work paused)
B. Approve catalog changes in conflict 1 (no geometry change).
C. Approve the `SceneColorSnapshot` additive contract (see scene-sampling handoff, incl. linear-vs-encoded ruling).
D. Where do active-effect chips + health go? Options: (i) drop names, keep health as a trailing StatusBadge in #4 (needs #4 maxBindings 2); (ii) share #16 with ACTIVE TIME; (iii) accept loss. Recommend (i).
E. #5 content rule per scene (one concise string).

## 6. Implementation/test status
- Measurement-independent Orbit work (sampler, tests) continues and is done; see `orbit-console-v0.1-scene-sampling-handoff.md`.
- NOT done (blocked on Architecture): production bindings, Orbit widget, #15 metrics, toggle (paused).
- NOT verifiable yet: Loading/missing-telemetry/stale-sample/skin-switch/keyboard-nav behavior of the NEW layout, enabled-vs-disabled Orbit draw overhead — nothing is drawn, so there is nothing to measure. Existing baseline observed in real captures: Loading shows transition status in overlay.transition; CPU/GPU/RSS show "NO DATA" in the harness; the effects chips row (#6) renders empty when no effect snapshot (Hide policy).
- Sampler overhead (the only Orbit cost that exists): ~3.7-4.1 ms per sample at 2 Hz (sync ~= async) on desktop; see handoff.
- Real screenshots: `sketches/experience_runtime/bin/data/captures/five-scene-ring/` (all five scenes, current HUD, "before"). No "after" screenshots exist beyond the prototype.

## 7. Working tree
Git status shows many modified files from earlier unrelated work; files from THIS item: new `src/SceneColorSeries.h`, `src/SceneColorSampler.h/.cpp`, `test/scene_color_series_tests.cpp`, `docs/reviews/orbit-console-v0.1-scene-sampling-handoff.md`, `docs/reviews/orbit-console-region-reallocation-verification.md`, `docs/reviews/orbit-placement/*`; edited `ExperienceRuntime.h/.cpp`, `test/Makefile.tests`. Nothing staged.
