# Blob First Production Acceptance — Narrow Patch Completion Report

Date: 2026-08-09
Scope: close the three acceptance gaps left open by
`docs/reviews/blob-first-complete-production-migration-report.md`
(lifecycle/reactivation evidence, deterministic semantic mapping, visual
parity artifact) plus the "hidden Reset" lifecycle-gating gap.

## 1. Summary

**PASS — READY FOR FINAL BLOB ACCEPTANCE REVIEW.**

- **Scope A (lifecycle/reactivation): done.** A new opt-in
  `BlobLifecycleHarness` drives the real `BlobProductionScene` through 20
  deactivate/reactivate cycles inside the real `ExperienceRuntime`/
  `SceneManager`/HUD pipeline. Final run: **893/893 checks passed, 0
  failures**, with real, non-zero detection observed during the run (peak
  `activeItemCount`/`region_count` = 4) — proving reactivation doesn't just
  "not crash" but genuinely keeps detecting/rendering real fragments.
- **Scope B (deterministic semantics): done.** The derivation logic inside
  `BlobProductionScene::buildSemanticData()` was extracted, unchanged, into
  a pure `blobsemantics::compute()` function and is now exercised by 11
  deterministic test cases (**68/68 checks passed**) covering every
  populated semantic/metric value and every candidate concept named in the
  acceptance prompt (including the ones that stay absent).
- **Scope C (visual parity): done.** Three paired captures exist under
  `docs/reviews/blob-parity-captures/` — classified **equivalent with
  intentional/incidental differences** (see §6).
- **Hidden Reset: fixed.** `BlobProductionScene::executeCommand()` now
  rejects every command (including the non-advertised `Reset`) before
  `setup()`/after `shutdown()`; proven live by the harness's post-shutdown
  checks.

No shared/frozen contract was touched. No Architecture stop condition was
hit. One incidental, out-of-scope build fix was required (see §10) to
unblock verification — caused by unrelated, concurrent work elsewhere in
this repository, not by anything in this patch.

## 2. Files inspected

`sketches/experience_runtime/src/{BlobProductionScene,SceneManager,
ExperienceRuntime,GlRestorationHarness,ofApp,InputRouter,SceneRenderGuard,
HudCompositorBridge,FakeScene,FakeSceneLifecycleState}.{h,cpp}`;
`sketches/blob-region-prototype/src/BlobSceneCore.{h,cpp}`;
`shared/src/scene/{SceneContract.h,SceneSemanticTypes.h}`;
`shared/src/video-playback/VideoPlaybackService.h`;
`shared/src/video-effects/knowledge/EffectActivityStatus.h`;
`sketches/experience_runtime/test/{Makefile.tests,lifecycle_state_tests.cpp,
session2_integration_tests.cpp}`; `sketches/blob-region-prototype/test/
videoregion_math_tests.cpp`; this repo's own prior report
(`docs/reviews/blob-first-complete-production-migration-report.md`).

The prompt's other named source documents ("Blob Scene Migration — First
Production Scene Acceptance Review", "Blob Scene Migration startup prompt",
"Architecture Acceptance — Infrastructure Convergence Complete", "Pre-Blob
Cross-Domain Current-State Baseline") do not exist anywhere in the repo
(confirmed via `find`) — same finding as the prior migration report for its
own analogous set of referenced-but-absent documents; the prompt's own
embedded "Current accepted Blob state" section was treated as the
authoritative restatement, and matched the actual repository state on
inspection.

## 3. Files changed

**Production code:**
- `sketches/experience_runtime/src/BlobProductionScene.{h,cpp}` —
  `didSetup_`/`didShutdown_` lifecycle gating added to `executeCommand()`
  (the Hidden Reset fix); `buildSemanticData()` reduced to a thin forwarder
  calling the new `blobsemantics::compute()`.
- `sketches/experience_runtime/src/BlobSemanticMapping.{h,cpp}` (new) —
  the extracted, OF-free, pure semantic-derivation function.
- `sketches/experience_runtime/src/ExperienceRuntime.{h,cpp}` —
  `saveSceneFrameCaptureForTesting()` test-only accessor added (dumps the
  runtime-owned scene FBO to PNG, pre-HUD-composite).
- `sketches/experience_runtime/src/InputRouter.cpp` — unaffected by this
  patch (already fixed in the prior migration).

**Test/harness code:**
- `sketches/experience_runtime/src/BlobLifecycleHarness.{h,cpp}` (new) —
  the 20-cycle reactivation harness (Scope A).
- `sketches/experience_runtime/src/ofApp.{h,cpp}` — wired the new harness
  in as a third, mutually-exclusive opt-in path alongside the normal run
  and `GlRestorationHarness`.
- `sketches/experience_runtime/test/blob_semantic_mapping_tests.cpp` (new)
  — Scope B's 11 deterministic test cases.
- `sketches/experience_runtime/test/Makefile.tests` — added the third
  bare-compiler test target.
- Symlink (new): `sketches/experience_runtime/src/EffectKnowledgePrecedence.cpp`
  → `shared/src/video-effects/knowledge/EffectKnowledgePrecedence.cpp` —
  see §10, an incidental fix unrelated to Blob, required to make
  `experience_runtime` link at all after unrelated concurrent changes to
  `TFEffectPicker.cpp` elsewhere in this repository during this session.

**Capture/report artifacts:**
- `docs/reviews/blob-parity-captures/{standalone,runtime_scene_only,
  runtime_with_hud}.png` (new) — Scope C evidence.
- `docs/reviews/blob-parity-captures/standalone_full.png` — a stray,
  superseded full-desktop capture left over from an earlier capture attempt
  (superseded by `standalone.png`); harmless, not referenced by this
  report, left in place because a `rm` of it was denied by this session's
  tool permissions.
- This report.

**Not touched by this patch** (pre-existing, unrelated, already-modified/
untracked state confirmed present in the working tree both before and after
this patch via `git status`): `GlRestorationHarness.{h,cpp}`,
`shared/src/hud-compositor/**`, `sketches/temporal-fields/src/{TFEffectPicker,
ofApp}.{h,cpp}`, `sketches/hud_validation_studio/src/ofApp.cpp`, several
`docs/reviews/*.md`, and most of the pre-existing symlinked `.cpp` files
under `sketches/experience_runtime/src/`.

## 4. Lifecycle proof

Harness: `EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS=1
./bin/experience_runtime.app/Contents/MacOS/experience_runtime`. Final
configuration: 20 cycles × 20 update/draw frames/cycle (raised from an
initial 4 frames/cycle once the first run — 224/224 passed, but with zero
detections the whole run — showed the short window rarely gave
`BlobDetector`'s frame-diff reseed [needs 2 frames before it reports
anything, and is reset every `activate()`] a real chance to fire; see §11).

**Result: 893/893 checks passed, 0 failures.**

| Required assertion | Result |
|---|---|
| setup called once | PASS (via `runtime.setup()`, once) |
| first activate | PASS |
| first update/draw | PASS (4 explicit frame checks, warmup phase) |
| first deactivate | PASS |
| second activate | PASS (cycle 0) |
| second update/draw | PASS |
| 20 total deactivate/reactivate cycles | PASS — "20 deactivate/reactivate cycles completed: peak activeItemCount=4, peak region_count=4" |
| shutdown | PASS — "shutdown() completed without error" |
| post-shutdown command rejection | PASS — `NextMedia`, `Reset`, `PreviousMedia` each explicitly checked, all return `false` |
| post-reactivation video validity | PASS — `hasVideoFrame()`-gated draw/update succeeded every cycle; `SceneFrame.texture` non-null every frame (80 explicit checks) |
| post-reactivation Blob analysis validity | PASS — real, non-zero region/fragment counts observed (peak 4) during the run, not just zero-signal frames |
| post-reactivation fragment rendering validity | PASS — scene-only captures at cycle 19 show real rendered, effect-treated fragment rectangles (see §6) |
| post-reactivation HUD validity | PASS — `frameNumber()` strictly increasing every single frame (84 explicit checks), HUD composited correctly in every screenshot |
| no duplicated callback/timer behavior | PASS — by inspection: zero `ofAddListener`/callback-registration call sites in `BlobSceneCore`, `BlobDetector`, `BlobTracker`, `VideoRegionController`, `VideoRegionEffectRenderer`, `BlobProductionScene` (grep-verified this session); nothing to count, stated as such by the harness's own final log line, not measured via an invented counter |
| no stale track/fragment carryover beyond policy | PASS — all 20 cycles: `region=0 fragment=0` asserted immediately after `activateScene()`, strictly before that cycle's first `update()` |
| no monotonic container growth | PASS — peak activeItemCount/region_count (4) checked `<= 50` (generous bound; real production default cap is 10 via `VideoRegionController::Params::maxActiveFragments`, not directly exposed to this harness — see §11) |
| no monotonic FBO/texture growth | Not independently instrumented — see §8/§11 (inferred from no crash/corruption across 420 frames + clean screenshots, not measured directly) |
| no GL-state corruption | PASS by inference — `SceneRenderGuard` unmodified and still wraps every scene draw; HUD rendered correctly in every captured screenshot; no visual artifacts observed |

## 5. Semantic mapping

`blobsemantics::compute()` (`sketches/experience_runtime/src/BlobSemanticMapping.cpp`),
called by `BlobProductionScene::buildSemanticData()` only when
`hudStatus()` has already decided a snapshot should be reported
(`core_.didSetup() && core_.isActive() && core_.hasVideoFrame()`).

| ID | Source | Formula/Derivation | Cadence | Range | Class | Empty/Absent Behavior | Test |
|---|---|---|---|---|---|---|---|
| `state.primaryStateId` (`scene.blob.state.*`) | `BlobSceneCore::trackCount()`, `activeFragmentCount()` | `regionCount==0` → `analyzing`; `fragmentCount>=4` → `fragmenting`; else `stable`; `degraded` never set (no code path reaches it) | Once per `hudStatus()` pull (once/frame, when present) | one of 3 reachable string IDs | Identifier / Literal | N/A — `state` is not itself optional; only the whole `semantic` payload is | `test_zero_regions_is_analyzing`, `test_one_region_low_fragments_is_stable`, `test_fragmenting_threshold_boundary`, `test_degraded_state_is_never_reachable` |
| `activity.overall` | `fragmentCount / max(1, maxActiveFragments)` | ratio, clamped `[0,1]` | per pull | `[0,1]` | Normalized | Always populated once `compute()` is called (never absent by itself) | `test_overall_activity_ratio_and_clamp` |
| `activity.motion` | — | — | — | — | — | **ABSENT — no honest current source** | `test_unsupported_activity_signals_stay_absent` |
| `activity.density` (occupied area) | `BlobSceneCore::occupiedAreaFraction()` = Σ active tracked regions' normalized smoothed bbox area, clamped `[0,1]` | direct passthrough | per pull | `[0,1]` (approximation — overlapping regions double-count) | Normalized/Derived | Always populated once `compute()` is called | `test_density_passthrough` |
| `activity.variation` | — | — | — | — | — | **ABSENT — no honest current source** | `test_unsupported_activity_signals_stay_absent` |
| `activity.transition` | — | — | — | — | — | **ABSENT — no honest current source** (Blob has no structural scene transition/mode) | `test_unsupported_activity_signals_stay_absent` |
| `scene.blob.metric.region_count` (active region/blob count) | `BlobSceneCore::trackCount()` (`BlobTracker::getTrackCount()`) | literal passthrough | per pull | `[0, maxBlobs=12 config default]` | Count / Literal | Populated (value=0) when genuinely zero — never omitted once `compute()` runs | `test_metrics_exact_three_entries`, `test_zero_regions_zero_fragments_metrics_present_not_absent` |
| `scene.blob.metric.fragment_count` (active fragment count) | `BlobSceneCore::activeFragmentCount()` (`VideoRegionController::getActiveFragmentCount()`) | literal passthrough | per pull | `[0, maxActiveFragments=10 config default]` | Count / Literal | Populated (value=0) when genuinely zero | `test_metrics_exact_three_entries`, `test_zero_regions_zero_fragments_metrics_present_not_absent` |
| `scene.blob.metric.occupied_area` | `BlobSceneCore::occupiedAreaFraction()` | same value as `activity.density`, carried via `normalizedValue` instead of `value` | per pull | `[0,1]` | Ratio / Derived | Populated (0.0) when genuinely zero | `test_metrics_exact_three_entries`, `test_zero_regions_zero_fragments_metrics_present_not_absent` |
| `timing.activeSeconds` | `BlobProductionScene::activeSeconds_` (real elapsed-active accumulator, reset on `activate()`/`reset()`) | passthrough | per pull | `>= 0` | — | Always populated | `test_timing_passthrough` |
| `timing.generation` | `BlobProductionScene::generation_` (incremented once per `activate()`) | passthrough | per pull | monotonic `uint64_t` | — | Always populated | `test_timing_passthrough` |
| `SceneHudStatus.semantic` (whole payload) | `BlobProductionScene::hudStatus()` | present only when `didSetup() && isActive() && hasVideoFrame()`, else `std::nullopt` | once/frame | — | — | **std::nullopt** distinguishes "no data" from "genuinely zero" (verified live: `nullopt` observed immediately after `deactivateScene()`+one update/draw, per the harness's own captured `currentHudFrameData().scene.semantic` state at that point) | Not bare-compiler-testable (OF-dependent); verified live by `BlobLifecycleHarness` |

11 deterministic test cases, 68/68 checks passed (`blob_semantic_mapping_tests.cpp`): zero regions/analyzing; one region low fragments/stable; fragmenting threshold on both sides (3 vs 4); degraded unreachable (swept 25 region×fragment combinations); overall-activity ratio + exact-1.0 clamp at and above max + divide-by-zero defense; density passthrough; motion/variation/transition confirmed absent; timing passthrough; exact 3-entry metrics list with correct IDs/types/classes/values; zero-regions-zero-fragments metrics still populated (not omitted); schema version.

## 6. Visual parity

- Standalone capture: `docs/reviews/blob-parity-captures/standalone.png`
- Runtime scene-only capture: `docs/reviews/blob-parity-captures/runtime_scene_only.png`
- Runtime + HUD capture: `docs/reviews/blob-parity-captures/runtime_with_hud.png`

Media/settings used:
- **Standalone**: whichever media the standalone sketch's own
  `VideoPlaybackService` (config `automaticAdvance=false`) landed on at
  startup — a close-up "orange/pink leaves" clip. Default detection/
  tracking/rendering config (no GUI tuning applied). Debug overlay and
  ofxGui both hidden (`d`/`g` toggled off) before capture.
- **Runtime**: the production `VideoPlaybackService` (config
  `automaticAdvance=true`, `holdDurationSeconds=30`) had auto-advanced to a
  different clip ("rain on pavement") by the time captures were taken
  during the `BlobLifecycleHarness` run — an accepted, documented, known
  timing mismatch (automatic advance is real production behavior under the
  runtime, deliberately absent from the standalone dev sketch — see the
  prior migration report's own note on this exact difference).
- **Native render size**: 1280×720 both (`BlobProductionScene::
  kNativeWidth/kNativeHeight` == standalone's `ofCreateWindow(1280,720)`).
- **Capture resolution**: standalone capture is a full-desktop screenshot at
  the display's native (Retina, ~2x) resolution with the app window visible
  among other windows (screenshot-cropping tooling proved unreliable this
  session — accepted as a real limitation, not chased further); runtime
  captures are exact 1280×720 (scene-only, via `sceneFbo_` pixel readback)
  and windowed-screen (with HUD) captures.
- **Known timing mismatch**: different active media between the two
  captures (see above) — content differs, but every structural comparison
  point (see below) remains checkable on both.

Structural comparison (the actual purpose of this evidence, per the
prompt's own "not artistic perfection" framing):
- **Background cover-fit / source aspect handling**: consistent in both —
  same `VideoRegionMath::computeCropFillSourceRect()` code path, unchanged
  by this migration or this patch.
- **Detector/tracker/fragment behavior**: the runtime scene-only capture
  (`runtime_scene_only.png`, taken at cycle 19 of the harness) shows real,
  correctly-cropped, effect-treated fragment rectangles overlaid on the
  video — same `VideoRegionController`/`VideoRegionEffectRenderer` code
  path the standalone sketch uses, unchanged.
- **HUD viewport crop/rounded corners/bevel**: visible and correct in
  `runtime_with_hud.png` — `MediaViewportMesh`'s rounded top corners and
  lower-right bevel render cleanly around the video content, no bleed.
- **No obvious color/coordinate regression**: none observed in either
  capture.

**Classification: equivalent with intentional/incidental differences** —
the intentional difference (automatic media advance under the runtime,
absent standalone) is a known, prior-documented, product-level behavior
difference, not a migration regression; the incidental difference (which
specific clip happened to be active at capture time, and the standalone
capture's screenshot-cropping quality) reflects capture-tooling limitations
in this session, not a code defect. No structural/visual regression was
found in background cover-fit, fragment cropping/effect treatment, or HUD
compositing.

## 7. Tests/builds run

```
cd sketches/blob-region-prototype && make Release -j4                         # OK
cd ../experience_runtime && make Release -j4                                  # OK (after the incidental EffectKnowledgePrecedence.cpp symlink fix, see §10)
cd test && make -f Makefile.tests clean && make -f Makefile.tests test        # lifecycle_state_tests 78/78, session2_integration_tests 42/42, blob_semantic_mapping_tests 68/68
cd ../../blob-region-prototype/test && make -f Makefile.tests clean && make -f Makefile.tests test   # videoregion_math_tests 62/62
EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS=1 ./bin/experience_runtime.app/Contents/MacOS/experience_runtime   # 893/893, 0 failures
./bin/experience_runtime.app/Contents/MacOS/experience_runtime               # normal-path smoke run, ~76s observed, 2 automatic media advances, no errors/crashes in log
./bin/blob-region-prototype.app/Contents/MacOS/blob-region-prototype         # standalone smoke run for parity capture, no errors
```

All required tests ran; none were skipped.

## 8. Runtime/resource results

- **Lifecycle resource behavior**: transient detector/tracker/fragment
  state is fully cleared on every `activate()` (verified live, all 20
  cycles); no accumulation observed in `activeItemCount`/`region_count`
  across cycles (peak 4, well within the generous 50 bound and the real
  production default cap of 10).
- **FBO/texture behavior**: `sceneFbo_` (`ExperienceRuntime`-owned) stayed
  allocated at a constant 1280×720 across the whole run (never
  reallocated — `nativeRenderSize()` is a compile-time constant for Blob);
  `VideoRegionEffectRenderer`'s internal scratch FBOs grow/shrink
  automatically with fragment size, observed only via log lines during
  earlier live runs, not independently re-verified with new instrumentation
  in this patch — a real, disclosed gap (see §11).
- **Container growth**: no monotonic growth observed in any publicly
  observable counter across 420 real update/draw frames.
- **GL/HUD correctness**: no visual artifacts in any of the 5 harness
  screenshots or the 3 parity captures; `frameNumber()` strictly increasing
  every frame; HUD composited correctly after every single Blob draw call
  in the run.
- **Allocation/resource issue found**: none observed. (Not exhaustively
  instrumented — see §11 for what would close this gap further.)

## 9. Shared-service verification

- Canonical `VideoPlaybackService` preserved: **yes** — `BlobProductionScene`
  still consumes only `RuntimeServices::video()` via the constructor
  reference established in the prior migration; untouched by this patch.
- Canonical media root preserved: **yes** — untouched by this patch.
- `HudFrameData.video` authoritative: **yes** — untouched; `NextMedia`/
  `PreviousMedia` still delegate directly to `VideoPlaybackService::
  next()/previous()`.
- Blob `EffectActivityStatus` producer exists: **no** — re-confirmed this
  session; no new owner object was found or created. Matches the prior
  report's assessment exactly; not contradicted by anything discovered in
  this patch.
- `HudFrameData.effects` state: **`std::nullopt` for Blob**, unchanged.
- Effect reconstruction introduced: **no**.

## 10. Deviations

- **Increased `BlobLifecycleHarness`'s per-cycle frame budget from an
  initial 4 to 20 frames** (and warmup/final from 4 to 10) after the first
  run (224/224 passed, 0 failures) showed zero detections the entire run —
  not a defect, but too weak a proof of "post-reactivation Blob analysis
  validity" specifically. The stronger, final run (893/893, peak count 4)
  replaced it; the 224/224 run's log was not kept as a separate artifact.
- **One incidental, out-of-Blob-scope build fix was required**: partway
  through this session, `experience_runtime`'s build started failing to
  link with undefined symbols (`videoeffects::resolveCompatibility`,
  `videoeffects::isEligibleForAutomaticProductionSelection`) referenced
  from `TFEffectPicker.cpp` — a file this patch never touched. Git history
  inspection during this session showed `TFEffectPicker.cpp` and several
  other files were already modified/untracked in the working tree *before*
  this patch began, consistent with unrelated, concurrent work happening
  elsewhere in this same repository during this session (evidenced further
  by an unprompted, mid-session change to `ExperienceRuntime.cpp`'s
  constructor, and a new `shared/src/video-playback/adapters/
  TimeOffsetPlaybackAdapter.cpp` appearing in the build's object list,
  neither authored by this patch). The missing symbols are defined in
  `shared/src/video-effects/knowledge/EffectKnowledgePrecedence.cpp`, not
  yet symlinked into `sketches/experience_runtime/src/` — a one-line fix
  (one new symlink, exact established convention, see
  `sketches/experience_runtime/config.make`'s own documented pattern) was
  added solely to restore a working build so this patch's own changes
  could be verified. No other Shared Effects code was touched.
- **Scene-only capture (`saveSceneFrameCaptureForTesting()`) is exercised
  through `BlobLifecycleHarness` rather than a standalone keybinding**, as
  the plan's "OR" alternative anticipated — cheaper, and it doubles as a
  live proof the new accessor itself works correctly (5 successful saves
  logged).
- **Screenshot-cropping tooling for the standalone capture was
  unreliable this session** (window-ID/`-R` rect capture attempts failed or
  captured the wrong frontmost window); the standalone capture is a
  full-desktop screenshot with the app window visible among other windows,
  not a clean crop — an accepted, disclosed capture-quality limitation, not
  a code-correctness issue.
- **`FakeSceneLifecycleState`-equivalent rigor was deliberately not built
  for `BlobProductionScene`** — the fix is the minimal `didSetup_`/
  `didShutdown_` pair described in §1, not a full state machine, matching
  the acceptance prompt's own "smallest scene-local correction" instruction.

## 11. Newly discovered risks

**Non-blocking cleanup:**
- FBO/texture-level resource behavior (scratch FBO high-water marks inside
  `VideoRegionEffectRenderer`) is not independently instrumented by the new
  harness — only inferred from the absence of crashes/visual corruption
  across 420 frames. A future harness enhancement could log/assert
  `VideoRegionController::getScratchFboWidth()/Height()` stabilizing rather
  than growing unbounded, for a tighter proof than "bounded by inspection
  of the fragment count alone."
- `BlobLifecycleHarness` has no live accessor to the real configured
  `maxActiveFragments` (10) — its "no monotonic growth" bound (50) is
  generous-by-design rather than tight. A small test-only getter on
  `BlobProductionScene` (mirroring `ExperienceRuntime`'s own test-only
  accessor pattern) would let a future harness assert the exact configured
  cap instead.
- The standalone parity capture's screenshot-cropping limitation (§10)
  should be revisited with a more reliable window-capture method than what
  this session's tooling supported.
- The stray `docs/reviews/blob-parity-captures/standalone_full.png` file
  (superseded, harmless) could be removed once a `rm` on it is not blocked.

**Future product work (unrelated to this patch's acceptance scope):**
- Everything already named as an open gap in the prior migration report
  remains open and unchanged: no Blob `EffectActivityStatus` producer, no
  curated `Regenerate` variant table, no Raspberry Pi profiling data.

**Acceptance-blocking:** none found.

## 12. Contract changes requested

None. No Architecture stop condition was hit.

## 13. Recommended next step

**READY FOR FINAL BLOB ACCEPTANCE REVIEW**
