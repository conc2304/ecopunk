# Temporal Production Scene #2 Migration — Coding-Agent Completion Report

**Scope executed:** Section 21 of the manager migration package (Temporal Fields as Ecopunk Runtime production scene #2), against the live `apps/myApps/EcopunkVideoCollage` repository.

## Summary

Implemented a real, complete `TemporalProductionScene : IEcopunkScene`, hosted by the existing `SceneManager`/`ExperienceRuntime` pipeline, driving the exact real Temporal artwork (`TFComposition`, all nine `TFPattern*`, `TFBackgroundLayer`→`TFEffectPicker`, `TFAmbientTextureLayer`) through a new reusable `TemporalSceneCore`. Shared Video ownership is unchanged (`VideoPlaybackService` remains sole canonical authority; `TimeOffsetPlaybackAdapter`/`TimeOffsetVideoBuffer` remain the Temporal-specialized decoder/history/six-playhead pipeline). `TFEffectPicker::activityStatus()` now reaches `HudFrameData.effects` through a new **generic, non-test** production seam added to `SceneManager` (not a promotion of the existing test-only override, not a scene-ID branch, not an `IEcopunkScene` change). A narrow reactivation-history fix (`TimeOffsetPlaybackAdapter::invalidateForReactivation()`) prevents stale pre-deactivation history from surviving a reactivate. Both Release builds are green, all pre-existing test suites remain green, a new pure semantic-mapping test suite passes (34/34), and a real 2-cycle/60-frame-per-cycle lifecycle/reactivation harness passed 295/295 checks against the real `ExperienceRuntime` pipeline with zero errors/warnings, real video decode, and real captured production screenshots showing the artwork + one production HUD only.

No frozen shared contract (`IEcopunkScene`, `SceneFrame`, `HudFrameData`, `SceneSemanticTypes.h`, `EffectActivityStatus.h`) was changed. No Architecture stop condition was crossed.

**This work builds on a pre-existing dirty working tree** (Blob production migration + Shared Video/Shared Effects seam work from prior sessions, per the manager's own "Blob production migration... as an implementation precedent" instruction) — per explicit direction, that work was treated as the accepted starting state and inspected, not re-implemented.

## Files inspected

- `shared/src/scene/SceneContract.h`, `SceneSemanticTypes.h` (frozen contracts — confirmed unchanged)
- `shared/src/hud-runtime/HudFrameData.h`, `shared/src/hud-compositor/HudDataTypes.h`
- `shared/src/video-effects/knowledge/EffectActivityStatus.h`
- `shared/src/video-playback/VideoPlaybackService.h`, `VideoPlaybackStatus.h`, `adapters/TimeOffsetPlaybackAdapter.h/.cpp`
- `shared/src/TimeOffsetVideoBuffer.h/.cpp` (incl. its already-uncommitted `configure()`/`loadExplicit()` additions)
- `sketches/experience_runtime/src/SceneManager.h/.cpp`, `ExperienceRuntime.h/.cpp`, `RuntimeServices.h`, `ofApp.h/.cpp`, `config.make`
- `sketches/experience_runtime/src/BlobProductionScene.h/.cpp`, `BlobSemanticMapping.h`, `BlobLifecycleHarness.h/.cpp` (precedent pattern)
- `sketches/temporal-fields/src/ofApp.h/.cpp`, `TFComposition.h/.cpp`, `TFBackgroundLayer.h/.cpp`, `TFEffectPicker.h`, `TFAmbientTextureLayer.h`, `TFPattern.h`, all nine `TFPattern*.h`, `TFPatternType.h`, `TFParameterPanel.h/.cpp` (default-value source of truth), `TFSettings.h`, `main.cpp`
- Full-tree `grep` for camera APIs (`ofVideoGrabber`/`videoDevice`/`camera`) across `sketches/temporal-fields/src/` — **none found**.
- `shared/src/hud-compositor/HudVocabularyResolver.cpp`, `HudPresentationProfile.cpp`, `fake/FakeTemporalScenario.cpp`, `fake/FakeHudScenarioRegistry.cpp` (canonical `sceneId`/semantic-ID source of truth)
- `.claude/CLAUDE.md` (project-level agent instructions), `CLAUDE.md` (shared-effects/video-effect conventions)

## Files changed

**New:**
- `sketches/temporal-fields/src/TemporalSceneCore.h/.cpp` — reusable core
- `sketches/experience_runtime/src/TemporalProductionScene.h/.cpp` — the `IEcopunkScene` adapter
- `sketches/experience_runtime/src/TemporalSemanticMapping.h/.cpp` — pure semantic mapping
- `sketches/experience_runtime/src/TemporalLifecycleHarness.h/.cpp` — reactivation proof harness
- `sketches/experience_runtime/test/temporal_semantic_mapping_tests.cpp` — pure unit tests (34 checks)
- Symlinks (matching the exact pre-existing per-file convention, e.g. `TFEffectPicker.cpp`/`BlobSceneCore.cpp`): `sketches/experience_runtime/src/{TemporalSceneCore,TFComposition,TFBackgroundLayer,TFAmbientTextureLayer,TFFragmentTransition,TFImageCycler,TFShapeFragmentRenderer,TFPatternBSP,TFPatternBlobGrid,TFPatternBands,TFPatternColumnGrid,TFPatternTelescopingFrames,TFPatternParticleField,TFPatternEcologicalSuccession,TFPatternNetworkGrowth,TFPatternTemporalTides,TimeOffsetVideoBuffer}.cpp`
- Asset sync (plain-file copies, matching that directory's existing plain-file — not symlink — convention): `sketches/experience_runtime/bin/data/shaders/{fragmentDissolve,particleExistenceFade,textureBlendFade}.{vert,frag}`
- Asset symlink: `sketches/experience_runtime/bin/data/backgrounds -> ../../../temporal-fields/bin/data/backgrounds` (ambient textures + `TFImageCycler` images — genuinely missing before this session; without it Temporal's FULL_IMAGE/SPLIT background modes and ambient underlay/overlay would silently degrade to nothing in production)

**Modified:**
- `sketches/temporal-fields/src/TFBackgroundLayer.h` — added `effectActivityStatus()` one-line forward to `TFEffectPicker::activityStatus()`, plus an `#include`. No behavior change to any existing method.
- `shared/src/video-playback/adapters/TimeOffsetPlaybackAdapter.h` — added `invalidateForReactivation()` (clears only this adapter's own `loadedMediaId_` cache; touches nothing else).
- `sketches/experience_runtime/src/SceneManager.h/.cpp` — `installProductionScene()` gained an optional `EffectActivitySource effectSource` parameter; `captureEffectActivityStatus()` gained one new priority tier between the existing test override and the `fakeScene_` fallback. Blob's call site is unchanged (passes no source) — verified byte-identical fallback behavior for Blob.
- `sketches/experience_runtime/src/ExperienceRuntime.h/.cpp` — added `temporalScene_` member, `installTemporalProductionScene()`, `temporalSceneForTesting()`; `sceneAssetRoot` derivation now branches on which scene was installed (still an inert, unread value either way).
- `sketches/experience_runtime/src/ofApp.h/.cpp` — added `EXPERIENCE_RUNTIME_TEMPORAL_SCENE` env var (selects Temporal instead of Blob for a normal run; unset = today's default, Blob, unchanged) and wired `TemporalLifecycleHarness` into the existing mutually-exclusive harness-selection chain.
- `sketches/experience_runtime/test/Makefile.tests` — added the `temporal_semantic_mapping_tests` target, mirroring `blob_semantic_mapping_tests` exactly.

**Not changed:** `config.make` (the existing `-I` set already covered every new header), `IEcopunkScene`, `SceneFrame`, `HudFrameData`, `SceneSemanticTypes.h`, `EffectActivityStatus.h`, any command enum, any semantic slot ID, HUD layout/geometry, canonical Shared Video ownership/status/hold/Previous-Next semantics, the dual-decoder boundary.

## Tests/builds run

| Command | Result |
|---|---|
| `cd sketches/temporal-fields && make Release -j4` | ✅ exit 0 — links `TemporalSceneCore.o` (new, unused in that binary — harmless) alongside all existing sources |
| `cd sketches/experience_runtime && make Release -j4` | ✅ exit 0 — links all new Temporal object files |
| `cd sketches/experience_runtime/test && make -f Makefile.tests test` | ✅ `lifecycle_state_tests` 78/78, `session2_integration_tests` 42/42, `blob_semantic_mapping_tests` 68/68 (all pre-existing, unaffected), `temporal_semantic_mapping_tests` **34/34 (new)** |
| `shared/src/video-playback/test` → `make -f Makefile.tests test` | ✅ `media_catalog_tests` 38/38, `video_selection_policy_tests` 113/113 |
| `shared/src/video-effects/test` → `make -f Makefile.tests test` | ✅ `effect_knowledge_extension_tests` 93/93 |
| `shared/src/hud-compositor-test` → `make -f Makefile.tests test-all` | ✅ `hud_presentation_tests` 676/676, `hud_real_frame_tests` 140/140, `hud_media_viewport_tests` 357/357, `hud_typography_tests` 14/14 |
| Real launch: `EXPERIENCE_RUNTIME_TEMPORAL_LIFECYCLE_HARNESS=1 ./experience_runtime` | ✅ **295/295 checks, 0 failures**, 0 errors/warnings in the log (after the shader/asset fixes below) |

No pre-edit baseline build was captured separately: the working tree was already dirty with prior-session work before this session began (per the "build on it" direction), so a clean "before" snapshot would have required reverting that accepted work first — the post-edit results above are the ones that matter for this migration's own acceptance.

## Results

**Real bugs found and fixed during verification (not anticipated from static inspection alone):**
1. `experience_runtime/bin/data/shaders/` was missing `fragmentDissolve.{vert,frag}` (pattern-transition FBO shader), `particleExistenceFade.{vert,frag}` (Particle Field pattern), and `textureBlendFade.{vert,frag}` (ambient SCREEN/MULTIPLY blend) — the first surfaced immediately as a real `ofShader` load error in the first harness run; the other two were found proactively by diffing the two sketches' `bin/data/shaders/` trees before they could surface pattern-by-pattern. All three copied in (plain files, matching that directory's existing convention).
2. `experience_runtime/bin/data/` had no `backgrounds/` folder at all (ambient textures + `TFImageCycler` images) — symlinked from `temporal-fields/bin/data/backgrounds`. Confirmed working: log shows `TFAmbientTextureLayer: loaded 19 background texture(s) from backgrounds`.
3. My first harness draft never called `ofExit()` on completion (BlobLifecycleHarness's own documented convention) — fixed; the harness now exits the process cleanly after logging its PASS/FAIL summary.

After those three fixes, a full harness run produced **zero** `[ error ]`/`[ warning ]` log lines and a real captured production frame (`captures/temporal_lifecycle_cycle0_end.png`) showing: the real decoded video, the real BSP pattern's time-offset fragment seams (visible low-opacity tiling — the actual specialized visual behavior, not flattened current-frame playback), and exactly one production HUD (title "Temporal Fields", `RUNNING` state, `ACTIVITY` label, real media title, `PREV MEDIA`/`NEXT MEDIA` commands) — no `TFHudLayer`, no debug text, no GUI panel anywhere in the frame.

**Exact ownership after migration:**
```
standalone temporal-fields ofApp (UNCHANGED — see Deviations)
    owns: ShaderLibrary, TFParameterPanel, TFComposition, 9 patterns,
          TFBackgroundLayer, TFAmbientTextureLayer, TFHudLayer, hud_overlay,
          its own VideoPlaybackService + TimeOffsetPlaybackAdapter

TemporalProductionScene : IEcopunkScene
    owns: TimeOffsetPlaybackAdapter (Temporal-specialized decoder/history)
          TemporalSceneCore
              owns: ShaderLibrary, TFComposition, 9 patterns,
                    TFBackgroundLayer (-> TFEffectPicker -> TFImageCycler),
                    TFAmbientTextureLayer
    references (not owned): RuntimeServices::VideoPlaybackService&
```

**Exact update order** (inside `TemporalProductionScene::update(dt)`, called by `SceneManager::updateActiveScene(dt)`):
```
syncVideoAdapter()                     // video_.status()/currentAbsolutePath() -> adapter
temporalVideoAdapter_.update(dt)       // dedicated decoder + history/playhead pump
core_.update(dt)
    -> backgroundLayer_.update(dt)     // -> effectPicker.update(dt)  [frame's effect state finalized here]
    -> composition_.update(dt)         // active pattern + any in-flight transition
```
Runtime canonical video update (`RuntimeServices::update(dt)`) happens later in `ExperienceRuntime::update()`'s own numbered sequence (step 7, after `SceneManager::updateActiveScene()` at step 3) — **unchanged from Blob's existing order**; Temporal's `syncVideoAdapter()` therefore observes the canonical selection up to one frame later than the theoretical earliest point, converging the very next frame. Reordering `ExperienceRuntime`'s own numbered, Blob-shared sequence for Temporal's benefit alone was judged out of scope (see Deviations).

**Exact draw order** (`TemporalProductionScene::drawToCurrentTarget()` → `TemporalSceneCore::draw()`):
```
backgroundLayer_.draw()
ambientTextures_.drawUnderlay()
composition_.draw()
ambientTextures_.drawOverlay()
```
No `TFHudLayer`, no `hud_overlay`, no `TFParameterPanel::draw()`, no debug text — confirmed by `grep`: none of those three classes/functions are referenced anywhere in `TemporalProductionScene.*` or `TemporalSceneCore.*`.

**Host-global calls:** `ofDisableArbTex()`/`ofSetFrameRate()`/`ofSetVerticalSync()` remain solely in `ExperienceRuntime::establishGlobalRenderingBaseline()` (pre-existing, shared with Blob) and in the standalone `temporal-fields` `ofApp::setup()` (unchanged). `TemporalProductionScene`/`TemporalSceneCore` call none of these.

**Shared Video path:** `RuntimeServices::VideoPlaybackService` (one instance, referenced by `TemporalProductionScene` exactly as `BlobProductionScene` already does) → `TemporalProductionScene::syncVideoAdapter()` → `TimeOffsetPlaybackAdapter::synchronizeSelectedMedia()` → `TimeOffsetVideoBuffer`. No second `VideoPlaybackService`, no restored local scan/shuffle/selection.

**Activate/deactivate history policy:** `deactivate()` does nothing beyond `core_.deactivate()` — the dedicated decoder simply stops being pumped (no `TimeOffsetPlaybackAdapter::update()` call reaches it while inactive, confirmed by the harness: history frame count held at 0 across 5 idle frames with the scene deactivated). `activate()` calls `TimeOffsetPlaybackAdapter::invalidateForReactivation()` on every activation *after* the first, forcing `synchronizeSelectedMedia()`'s next call to reload+clear history unconditionally even against an unchanged media identity — confirmed by the harness: history frame count immediately after `activate()` never exceeded its value at the preceding `deactivate()`.

**Real effect-snapshot forwarding path:**
```
TFEffectPicker::update()  [inside TFBackgroundLayer::update(), inside TemporalSceneCore::update()]
  -> TFBackgroundLayer::effectActivityStatus() -> TFEffectPicker::activityStatus()   [new 1-line forward]
  -> TemporalProductionScene::currentEffectActivitySnapshot()
  -> SceneManager::productionEffectActivitySource_ (installed by ExperienceRuntime::installTemporalProductionScene())
  -> SceneManager::captureEffectActivityStatus() / currentEffectActivityStatus()
  -> ExperienceRuntime::draw(): currentHudFrameData_.effects = sceneManager_.currentEffectActivityStatus();
  -> HudFrameData.effects
```
`activityStatus()` is called exactly once per completed Temporal frame (read once, inside `currentEffectActivitySnapshot()`, itself called exactly once per frame by `SceneManager::captureEffectActivityStatus()` — same once-per-frame discipline already enforced for Blob/FakeScene). Confirmed present-empty and present-active behavior structurally (same `TFEffectPicker::activityStatus()` code Blob's own seam-proof session already validated); confirmed **present** (not `nullopt`) in the real harness run via `HudFrameData.effects.has_value()` checks at warmup and after each reactivation cycle. A live authored-preset selection was not specifically observed in this session's short (~130-frame) run — non-blocking per the migration prompt.

**Semantic mapping table** (all formulas, sources, and classes documented in `TemporalSemanticMapping.cpp`'s own header comment; reproduced here):

| Semantic ID | Source | Formula | Class | Missing when |
|---|---|---|---|---|
| `scene.temporal.state.running` / `.transitioning` | `TFComposition::getPhase()` | `RUNNING`→running, `PATTERN_TRANSITION`→transitioning | Literal | never (always present) |
| `timing.stateElapsedSeconds` | `TFComposition::getPhaseElapsed()` | direct | Literal | never |
| `timing.stateProgress` | — | — | — | **always** (no exposed cycle/transition-duration getter on `TFComposition`) |
| `scene.temporal.metric.pattern` | `TFComposition::getActivePatternType()`, lowered | 9-way switch to snake_case id (`bsp`, `blob_grid`, …) | Identifier/Literal | never |
| `scene.temporal.metric.temporal_depth` | — | — | — | **always** (§10.5: no settled formula found) |
| `scene.temporal.metric.history_fill` | `TimeOffsetVideoBuffer::getHistoryFrameCount()`/`getHistoryCapacityFrames()` | `clamp01(count/capacity)` | Ratio/Normalized | capacity ≤ 0 |
| `scene.temporal.metric.field_activity` | — | — | — | **always** (§10.6: no honest cheap signal) |
| `scene.temporal.metric.evolution_state` | — | — | — | **always** (§10.7: `TFPresetTimeline` not driven in production — see Deviations) |
| `scene.temporal.metric.playhead_count` | `TimeOffsetVideoBuffer::getNumPlayheads()` | direct cast to float, real current count (not hard-coded 6) | Count/Literal | never |

Verified in `temporal_semantic_mapping_tests.cpp` (34/34 pass): every populated field's exact value/type/class, every absent field's absence, boundary conditions (zero-capacity, empty, partial, over-100%-clamped history), and "exactly 3 metrics populated, no more" for a representative input.

**Capabilities/commands:** `NextMedia`/`PreviousMedia` advertised, routed through `video_.next()/previous()` only. `Regenerate` **not** advertised — inspection found no existing curated, bounded, production-safe variation action (`TFComposition::forceNextPattern()` is a raw debug key, explicitly disqualified by the migration prompt's own language). `Reset` not advertised but still honored directly by `executeCommand()`, matching `BlobProductionScene`'s identical split. `SceneManager::activateScene()` caches capabilities exactly once per activation — unchanged, generic mechanism, exercised identically for Temporal.

**Lifecycle proof (real, via `TemporalLifecycleHarness`, against the real `ExperienceRuntime`):**
```
setup (once, at runtime.setup())
-> activate -> 10 warmup frames -> deactivate
-> 5 idle frames (deactivated) — history held flat, proving no scene work while inactive
-> activate (cycle 0) -> 60 real update/draw frames -> deactivate
-> activate (cycle 1) -> 60 real update/draw frames -> deactivate
-> activate (final) -> 10 frames
-> deactivate -> shutdown -> post-shutdown command rejected
```
295/295 checks passed: `SceneFrame` texture non-null + `frameNumber()` strictly increasing every real frame; `HudFrameData.effects` present after the first completed frame and after every reactivation; history never carries stale content across a deactivate→reactivate boundary; playhead pool valid (`numPlayheads() > 0`) after every reactivation; `shutdown()` clean; commands rejected after `shutdown()`.

**Render/GL:** `SceneRenderGuard` (the same generic, scene-agnostic guard already wrapping Blob/FakeScene) wraps `TemporalProductionScene::drawToCurrentTarget()` identically — no Temporal-specific GL code was added anywhere in this session. Real captured frames show the composited scene + intact production HUD with no corruption/black-frame/stale-texture artifacts, both immediately after warmup and after two full deactivate/reactivate cycles.

**Camera:** confirmed absent — a full-tree `grep` for `ofVideoGrabber`/`videoDevice`/`camera` (case-insensitive) across `sketches/temporal-fields/src/` returned zero matches.

**Contract changes:** none requested. `IEcopunkScene`, `SceneFrame`, `HudFrameData`, `SceneSemanticTypes.h`, `EffectActivityStatus.h` are byte-for-byte unchanged (confirmed no edits made to any of those files).

## Deviations from prompt

1. **Standalone `ofApp` not refactored onto `TemporalSceneCore`.** Unlike Blob (where `blob-region-prototype`'s own `ofApp` was refactored to construct `BlobSceneCore` directly), the standalone `temporal-fields` `ofApp` is untouched — it still owns its pipeline inline. `TemporalSceneCore` drives the exact same real classes (not a copy/fork), just as a second real instance. Judged materially lower-risk than refactoring `ofApp`'s much larger live-tunable `TFParameterPanel` surface (dozens of GUI-bound param pushes/frame) and three startup self-tests in this session. Recommended as explicit future work, not attempted here.
2. **Preset timeline / evolution state not driven in production.** `TFPresetTimeline`'s machinery is entangled inside `TFParameterPanel` (a GUI class) in the current tree, not a standalone driver. Production Temporal therefore runs `TFComposition`'s own ~90s auto-cycle with every pattern's real in-class default `Params` (verified identical to what `TFParameterPanel`'s own `ofParameter`s are seeded with at standalone startup — confirmed against `TFSettings.h` constants) rather than any authored preset/evolution sequence. `scene.temporal.metric.evolution_state` is left absent accordingly (per §10.7's own instruction: absent when no explicit source exists).
3. **`scene.temporal.metric.pattern`'s identifier vocabulary is unresolved.** `HudVocabularyResolver.cpp` has no display-text mapping yet for `bsp`/`blob_grid`/etc. (only the fake scenario's placeholder `particle_field` example exists). The real value now reaches the slot correctly; a missing vocabulary entry is expected to fall back to the raw identifier in display, not a rendering failure — flagged for the HUD Runtime domain, not fixed here (out of scope: "no scene-ID branch in shared HUD renderer," and this isn't one — it's a missing vocabulary *entry*, not code).
4. **Runtime frame-order left unchanged for Temporal's benefit.** See "Exact update order" above — `RuntimeServices::update()` (canonical video tick) runs after `SceneManager::updateActiveScene()` in `ExperienceRuntime`'s existing, Blob-shared, numbered sequence. Not reordered for Temporal; the one-frame-latest-selection lag this can cause is harmless and self-correcting.
5. **No real pattern-transition (`TFFragmentTransition` internal-FBO) frame was specifically captured** within this session's short (~130-frame / ~2 real-seconds-per-cycle) harness run — `TFComposition`'s auto-cycle interval (90s) is far longer than the harness's per-cycle window. The transition code itself is unmodified, already proven in the standalone sketch, and the harness's captured frames show no GL/HUD corruption after two full deactivate/reactivate cycles, but a frame mid-transition specifically was not captured as evidence. Recommended as a specific follow-up check (either a longer real-time soak or a dev-only forced-transition hook) before or during the Blob↔Temporal switching acceptance.

## Newly discovered risks

- `experience_runtime`'s `bin/data/` was missing three shader pairs and the entire `backgrounds/` asset folder for Temporal — silent visual degradation risk for any future Temporal-consuming build that doesn't inherit this session's fixes from a clean checkout of `bin/data/` (these are `bin/` contents, commonly `.gitignore`d — verify `.gitignore` coverage before assuming this survives a fresh clone).
- `TFComposition` exposes no `getTransitionDuration()`/cycle-duration getter, so `SceneTimingStatus::stateProgress` can never be honestly populated for Temporal without either adding that getter (small, `TFComposition`-local, not attempted here to keep this session's touched-file set minimal) or leaving the slot permanently absent (current state).
- The generic `SceneManager` effect-activity-source seam (this session's own addition) is currently exercised by exactly one real caller (Temporal). It is generic by construction, but has only one real-world data point; worth a second real production scene adopting it before treating the shape as fully proven.

## Contract changes requested

None.

## Recommended next step

**Ready for Temporal production-scene acceptance review.**

Suggested before/alongside the full 20-cycle Blob↔Temporal switching acceptance: (a) capture one real mid-transition frame under the runtime FBO (Deviation 5), (b) decide whether to invest in the standalone-`ofApp`↔`TemporalSceneCore` unification (Deviation 1) before or after Pi profiling, (c) hand the current desktop evidence (Release builds green, 295/295 real lifecycle checks, real captured frames) to Raspberry Pi Runtime for dual-decoder profiling per the manager package's own readiness criterion.
