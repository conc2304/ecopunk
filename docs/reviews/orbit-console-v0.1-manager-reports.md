# Orbit Console v0.1 / Option B - manager reports (HUD Runtime & Validation Studio agent)

Written for: the Architecture, ExperienceRuntime, HUD Runtime, HUD Art and Fragment-trail managers. Each section is self-contained.
Nothing is staged, committed or pushed. Evidence paths are under `docs/reviews/orbit-placement/` unless stated.

## Shared summary
Done (HUD side): Orbit Console draws in `universal.activity_trace` / `universal.secondary_trace` (#7/#8); System Performance (five vitals) in
`flexible.primary` (#15); ACTIVITY gauge in #6; scene status or effect chips in #5 (visibility-aware, full height). No geometry, region ID, HUD-004
manifest, scene/media/input authority or measurement-semantics change. The System/Orbit toggle was never built, so nothing was retired.
NOT done: Orbit shows NO DATA in production because `HudFrameData` carries no scene-colour snapshot. Valid/gap/stale rendering was verified only
with an env-gated SYNTHETIC fixture (`EXPERIENCE_RUNTIME_ORBIT_FIXTURE`), never with real measurements.

---
## 1. Architecture
Decisions / actions needed (blocking Orbit live data):
1. Ratify the additive contract: `SceneColorSnapshot` (OF-free, `shared/src/hud-runtime/`) + `HudFrameData::sceneColor` (`shared_ptr<const>`, null = none yet)
   + source ID `runtime.scene_color`. Proposal: `orbit-console-v0.1-contract-proposal.md`; ExperienceRuntime amendments: `orbit-console-v0.1-scene-sampling-handoff.md` sec. 4.
   The type is NOT in the tree today, although later prompts describe it as ratified.
2. Rulings still open: (a) linear-light vs sRGB-encoded averaging (sampler: encoded; HUD alignment note asked for linear; needs a colour-space field);
   (b) cadence 2 Hz vs ~1 Hz; (c) scene id per generation in the snapshot; (d) accept or reject measured cost ~3.7-4.1 ms per sample vs the 0.3 ms budget;
   (e) GLES2 fallback for the PBO/fence path (needs GL 3.2) before any Raspberry Pi 3B use.
3. Catalog edits already made under the Option B authorization (acceptedWidgetTypes / maxBindings only, no geometry): #5 +ChannelStrip +Label, max 3;
   #6 +ProgressBar; #7/#8 max 1; #15 max 5. Please confirm for the decision log.
4. Stale doc: `hud-art/blueprint-v2-reference/production_membership.md` still lists #7/#8 as not drawn in production.

## 2. ExperienceRuntime
- Private sampler (`SceneColorSampler.*`, `SceneColorSeries.h`, hooks in `ExperienceRuntime::draw`) exists and was measured on desktop only; Pi unmeasured.
- Needed once Architecture ratifies: add the header + `HudFrameData::sceneColor`; publish an immutable snapshot rebuilt only when the series revision changes
  (refcount bump per frame, like `systemVitals`); keep generation reset and gap semantics (valid black stays valid; failures are gaps, never zeros).
- Perf: ~4 ms CPU per sample at 2 Hz is above budget; suggested fixes in the handoff (raw glBlitFramebuffer chain or 2-pass shader reduction).
- Concurrency note: HUD verification runs and an app rebuild collided with another session's runs; use per-session build and capture dirs.
  One of my broad process kills may have ended another session's harness run around 22:10 (re-run any affected run). No broad kills since.
- HUD side is ready: `HudWireframeRenderer::updateOrbit` takes an `OrbitSeriesView` (samples, state, generation, revision); the adapter is ~20 lines once field names exist.

## 3. HUD Runtime manager
Files changed (new/edited, HUD domain): `shared/src/hud-compositor/` OrbitConsoleModel.cpp/.h, OrbitDraw.h, SegmentedHistory.h (added by a concurrent session),
HudWireframeRenderer.cpp/.h, HudPresentationProfile.cpp, HudRegionCatalog.cpp, HudSourceResolver.cpp, HudRealFrameResolver.cpp, HudVocabularyResolver.cpp,
HudWidgetInput.h, widgets/{SparklineWidget,ChannelStripWidget,NumericValueWidget,ProgressBarWidget,EffectChipsWidget}.cpp(+SparklineWidget.h);
`sketches/experience_runtime/src/OrbitConsoleModel.cpp` (symlink); tests `hud_orbit_tests.cpp`, `hud_vitals_tests.cpp`, `hud_presentation_tests.cpp`, `Makefile.tests`.
Behaviours added: visibility-aware slots in `universal.effect_summary` only (hidden bindings reserve nothing; Placeholder/DimLastValid/Loading-dimmed keep slots);
two-line Blob/Fragment status with an aligned value column (`inline.col`); Quadrant 2x2 (`grid`); Blueprint progress with explicit "--" placeholder (never 0%);
chips vertically centred; env-gated diagnostics: `EXPERIENCE_RUNTIME_ORBIT_FIXTURE`, `EXPERIENCE_RUNTIME_HUD_DRAW_STATS`, `EXPERIENCE_RUNTIME_HUD_LOADING_CAPTURE_DIR` (all off by default).
Results: HUD unit suites pass (presentation 301, vitals+orbit 215, real-frame 357, orbit 34); HUD-006 v2 harness 23/23; focus 21/21; fullscreen/RT-007 30/30; five-scene ring 102/1 (the one failure is the Fragment cap, section 5).
Desktop cost (CPU submission): HUD draw ~1.7 ms/frame of which Orbit ~0.29 ms; no true "disabled" baseline; Pi not measured.
Known defects/limits: Orbit never exercised with real data; Blueprint "--" and DimLastValid-on-screen only unit-tested; Temporal status row blank (field_activity never published);
Fragment preset text and Quadrant detail reformatted (accepted); NEXT MODE button face is a black square during Loading (pre-existing, not changed);
a stray dead-code risk: `SparklineWidget` carries Orbit drawing (Sparkline variants `orbit.upper`/`orbit.lower`) by precedent from `vitals`.
Concurrent-session ownership: another session refined `OrbitConsoleModel`, added `SegmentedHistory.h` and `OrbitInjectHarness.*`; I did not review the latter.

## 4. HUD Art
- Dark Moss + production typography (Calculator "info" face, 15 px for #5) verified in real captures: `option-b/`, `option-b-polish/`. No new shell artwork, no manifest change.
- Orbit uses runtime-drawn fixed measurement colours (R/G/B rim marks and bars) and the skin palette for chrome; please review legibility against Dark Moss
  (`proposal/`, fixture images marked SYNTHETIC) and the 8x13 native text used inside Orbit/vitals vs the skin face.
- Open: whether #7/#8 text should adopt the typography preset (currently native bitmap, like the vitals).

## 5. Fragment-trail owner (separately owned; not touched)
Five-scene ring fails one invariant: "Fragment count above its maxFragments cap" (`ThreeSceneCircuitHarness.cpp:241-243`, default cap 18).
Cause (source evidence): uncommitted `FTFragmentPool.cpp` now evicts only above `maxFragments * kTrailCount` (2 trails = 36), while the harness and
`FragmentSemanticMapping` (`fragmentCount / maxFragments`, clamped) still assume 18. Observed counts 20-32 (pre-dating my HUD edits; reproduced in isolated instances).
The harness does not log the count at the assertion; a one-line log would give exact values. Decision for the owner: update the contract (cap = per-trail x trails) or the pool.

---
## Completion report (per .claude/CLAUDE.md)
1. Summary: above. 2. Files inspected: HUD compositor, runtime sampler/handoff docs, region catalog, profiles, resolvers, five-scene harnesses, Fragment pool diff.
3. Files changed: section 3 + docs under `docs/reviews/` (contract proposal, alignment, occupancy, presentation-correction, this file, `orbit-placement/*`).
4-5. Tests/builds: section 3 (desktop only, isolated app copies). 6. Deviations: Orbit built against an internal series view because the public snapshot does not exist;
env fixtures/diagnostics added to the shared renderer. 7. Risks: sections 1-4. 8. Contract changes requested: `SceneColorSnapshot` / `HudFrameData::sceneColor` / `runtime.scene_color` (section 1).
9. Next step: Architecture ruling + ExperienceRuntime publication, then the HUD adapter and a real-data five-scene verification and Pi measurement.

```text
WORK ITEM: Orbit Console v0.1 / HUD content reallocation (Option B)  [no registry entry yet - new item discovered]
PREVIOUS STATUS: (none)
NEW STATUS: HUD side implemented and desktop-verified without live data; BLOCKED on Architecture contract + ExperienceRuntime publication

WHAT CHANGED: Orbit layout in #7/#8, vitals to #15, gauge to #6, visibility-aware #5 scene status/chips, widget variants, tests, diagnostics
WHAT WAS PROVEN: legible, non-overlapping layout on real Dark Moss frames in five scenes, fullscreen and a real startup Loading frame; slot allocation by unit tests; focus/mode/controls unchanged
WHAT WAS NOT PROVEN: Orbit with real scene measurements; stale/gap behaviour on live data; DimLastValid and Blueprint placeholder on screen; Pi performance or GL sync cost; linear-light correctness

NEW EVIDENCE: orbit-placement/{after,proposal,option-b,option-b-polish}/, harness logs (isolated), unit test outputs
NEW RISKS: sampler cost over budget; encoded-vs-linear ruling; GLES2 path; concurrent-session collisions on shared files and app bundles; Fragment cap contract mismatch
NEW BLOCKERS: SceneColorSnapshot not ratified/published; Fragment maxFragments contract (separate owner)

NEXT ACTION: Architecture rulings -> ExperienceRuntime publishes snapshot -> HUD adapter + real-data verification
NEXT GATE: Architecture ratification of the additive HudFrameData field

REGISTRY UPDATE REQUIRED: YES (new item, dependencies, blockers)
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: YES (catalog acceptedWidgetTypes/maxBindings changes; scene-colour contract once decided)
ARCHITECTURE HANDOFF REQUIRED: YES
```
