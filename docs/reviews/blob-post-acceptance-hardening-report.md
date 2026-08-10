# Blob Post-Acceptance Hardening — Completion Report

Date: 2026-08-09
Scope: non-blocking, post-acceptance evidence hardening — scratch-FBO
instrumentation, real-config lifecycle bounds, a desktop soak, and a
controlled same-media parity recapture. Not an acceptance gate.

## 1. Summary

**PARTIAL — NON-BLOCKING FOLLOW-UP REMAINS.**

All four scopes were executed and produced real, measured evidence — but
two of them surfaced genuine, previously-unknown facts about existing Blob
behavior that the original assumptions in this task's own prompt (and the
prior session's harness) got wrong. Neither is a regression or a defect
introduced by this patch; both are pre-existing behavior, now measured and
disclosed rather than assumed:

- **Scope A (instrumentation): done.** Real scratch-FBO width/height/area
  (current + high-water) and real configured caps are now read live through
  new test-only accessors, not estimated.
- **Scope B (tightened bounds): done, with one corrected assumption.**
  `fragment_count <= configured maxActiveFragments` holds everywhere
  measured (20-cycle proof and the full soak). The prompt's other assumed
  bound, `region_count <= configured maxBlobs`, is **not a real invariant**
  — `BlobDetector::Config::maxBlobs` caps new detections reported *per
  frame*, not `BlobTracker`'s accumulated active-track count, which has no
  configured ceiling anywhere. A real 900-second soak observed
  `region_count` transiently reaching 33 against `maxBlobs=12` before
  returning to 0 repeatedly — not a leak, just proof this specific
  comparison was never the right one. The harness assertion was corrected
  to report this value informationally instead of asserting a false bound
  (see §5/§10).
- **Scope C (desktop soak): done.** A full real 900-second (15-minute),
  27,059-frame soak of the real production path completed cleanly: no
  crash, no GL/HUD invalidity, fragment count stayed within its real cap
  the entire time, scratch-FBO high-water stayed well within a generous
  sanity bound, 29 automatic media changes all handled correctly. One
  finding (region_count, above) reported, not hidden.
- **Scope D (controlled parity): partially done.** Both apps were driven to
  the same repeated-`previous()` boundary via a new dev-only `n`/`p`
  keybinding (standalone) and the existing runtime binding — but they
  landed on **different physical media files**, because `VideoPlaybackService`'s
  own `dir.listDir()` call (`shared/src/video-playback/VideoPlaybackService.cpp`)
  never sorts the discovered file list, so catalog index assignment is not
  guaranteed identical across separate process launches. This is a real,
  previously-undocumented Shared Video finding (§10/§11), not a Blob defect
  — the structural comparison (crop-fill, HUD framing, effect rendering)
  is still valid and shows no regression, but the captures are not
  media-matched as intended.

No shared/frozen contract was touched. No Architecture stop condition was
hit. Recommended next step in §14.

## 2. Files inspected

`sketches/experience_runtime/src/{BlobProductionScene,BlobLifecycleHarness,
ExperienceRuntime,RuntimeTelemetryCollector,AllocationCounter,
SceneRenderGuard,HudCompositorBridge,ofApp}.{h,cpp}`;
`sketches/blob-region-prototype/src/{BlobSceneCore,ofApp}.{h,cpp}`;
`shared/src/{BlobDetector,BlobTracker,VideoRegionController,
VideoRegionEffectRenderer}.{h,cpp}`; `shared/src/video-playback/
VideoPlaybackService.cpp` (specifically the media-discovery/catalog-build
path); `shared/src/scene/{SceneContract.h,SceneSemanticTypes.h}`; both prior
completion reports (`docs/reviews/blob-first-complete-production-migration-report.md`,
`docs/reviews/blob-first-production-acceptance-narrow-patch-report.md`) and
their capture artifacts under `docs/reviews/blob-parity-captures/`.

The prompt's other named source documents ("Final Acceptance Handoff to
Architecture", "First Production Scene Acceptance Review") do not exist in
the repo (confirmed via `find`) — same finding as both prior sessions for
their own analogous references; the two reports that do exist were treated
as authoritative for "current accepted state."

## 3. Files changed

**Production code:**
- `sketches/experience_runtime/src/BlobProductionScene.{h,cpp}` — added
  six test-only, read-only instrumentation accessors (fragment/background
  scratch-FBO width/height, configured `maxBlobs`/`maxActiveFragments`).
  No behavior change to any `IEcopunkScene` method.
- `sketches/experience_runtime/src/ExperienceRuntime.{h,cpp}` — added
  `blobSceneForTesting()` (mirrors the existing `sceneManagerForTesting()`
  pattern). No behavior change.
- `sketches/blob-region-prototype/src/ofApp.cpp` — added dev-only `n`/`p`
  Next/Previous-media keybinding (standalone sketch had none before),
  mirroring the runtime's existing `InputRouter` mapping.

**Test/harness code:**
- `sketches/experience_runtime/src/BlobLifecycleHarness.{h,cpp}` — added
  `Mode::Soak` (new opt-in `EXPERIENCE_RUNTIME_BLOB_SOAK_HARNESS` env var,
  same class/pattern as the existing lifecycle mode, not a new harness
  architecture); added scratch-FBO peak tracking to `trackPeaks()`;
  replaced the prior patch's generous hardcoded bound (50) with live reads
  of the real configured caps; corrected the `region_count`-vs-`maxBlobs`
  assertion to be informational after the soak run disproved that
  comparison (see §1/§10).
- `sketches/experience_runtime/src/ofApp.{h,cpp}` — added the soak-mode
  branch alongside the existing three-way (normal/GL-harness/Blob-lifecycle)
  dispatch.

**Capture/report artifacts:**
- `docs/reviews/blob-parity-captures-controlled/{standalone,
  runtime_scene_only,runtime_with_hud}.png` (new).
- This report.

**Not touched by this patch** (pre-existing, unrelated state already
present in the working tree, confirmed via `git status` before and after):
`GlRestorationHarness.{h,cpp}`, several `docs/reviews/*.md`, and the
already-symlinked `.cpp` files under `sketches/experience_runtime/src/`
from earlier sessions' work.

## 4. Resource ownership and instrumentation

| Resource | Owner | Allocation policy | Baseline | Peak / high-water | Final | Bounded? | Evidence |
|---|---|---|---|---|---|---|---|
| ExperienceRuntime scene FBO | `ExperienceRuntime` | Allocated once in `setup()` to `nativeRenderSize()` (1280×720, fixed for Blob); never reallocated in production | 1280×720 | 1280×720 (constant) | 1280×720 | Yes — fixed | Logged at every soak sample and lifecycle summary |
| Blob tracked regions (`BlobTracker`) | `BlobSceneCore` (Blob-owned) | Grows per new detection (up to `BlobDetector::Config::maxBlobs` new ones/frame), shrinks via `maxMissingSeconds` timeout — **no configured ceiling on total count** | 0 | 20-cycle run: 0 (this run); prior run: 7; **900s soak: 33** | 0 (returns to 0 repeatedly, confirmed non-monotonic — see log samples in §6) | **No hard configured bound** — see §1/§10 finding | Soak log: samples oscillate 0→2→12→0→0→25→0→23→0→0→0 across the run |
| Blob active fragments (`VideoRegionController`) | `BlobSceneCore` (Blob-owned) | Explicitly capped at `Params::maxActiveFragments` (real value: 10) by `VideoRegionController::update()` | 0 | 20-cycle run: 0 (this run); prior run: 7; **900s soak: 10** | 0 | **Yes — real, enforced cap**, confirmed never exceeded | `fragment_count <= 10` held in every sample across 27,059 soak frames |
| Fragment-pool scratch FBO width/height (`VideoRegionEffectRenderer`, via `VideoRegionController`) | `BlobSceneCore` (Blob-owned) | Grows to the high-water mark needed by the largest fragment crop rendered so far; reused thereafter (`ensureScratchFbos()`'s own "reallocate only if larger" logic) — does not shrink | 0×0 | **900s soak: 1272×712** (905,664 px area) | 1272×712 (retained) | Yes, within a generous 2×canvas sanity bound; genuinely never observed to shrink | Soak SOAK SUMMARY log |
| Background scratch FBO width/height (separate `VideoRegionEffectRenderer` instance) | `BlobSceneCore` (Blob-owned) | Same policy as above, but never triggered in production — `BackgroundConfig::mode` defaults to 1 ("normal", unshaded), and nothing in the production surface changes it | 0×0 | 0×0 (never allocated) | 0×0 | Yes — trivially, stays unallocated | Every sample this session, both harness modes |

Allocation policy for both scratch-FBO pairs is documented directly from
`VideoRegionEffectRenderer.cpp`'s `ensureScratchFbos(neededW, neededH)`:
reallocates only when the newly-needed size exceeds the current allocation,
never shrinks. This was confirmed by observation (the fragment scratch FBO
only ever grew across the whole 900s run, several distinct step increases,
never a decrease) — matching the code's own logic exactly, not just
asserted.

Memory/RSS: **NOT MEASURED** — no in-repo source exists
(`RuntimeTelemetryCollector::update()` leaves `residentMemoryBytes` at its
default `std::nullopt`; confirmed by direct code read). The existing
`alloccounter` heap new/delete counter (process-wide, already wired for
`GlRestorationHarness`) was reused as a secondary, non-RSS signal — see §6.

## 5. Tightened lifecycle results

Final clean run (after the assertion correction in §10):
**895/895 checks passed, 0 failures.**

- Configured caps read live: `maxBlobs=12`, `maxActiveFragments=10`
  (confirmed identical to the earlier acceptance-patch run's inspection).
- `fragment_count <= maxActiveFragments`: **PASS**, held in every cycle.
- `region_count` vs. `maxBlobs`: now reported **informationally** (not a
  pass/fail bound) — see §1/§10 for why.
- Scratch-FBO high-water this run: fragment 0×0, background 0×0 (this
  particular run's active media happened to have no detectable motion in
  the windows sampled — see §11 for the run-to-run variance this reveals).
  An earlier run in this same session (before the final clean re-run)
  recorded fragment scratch FBO peak 147×70 with real non-zero
  region/fragment peaks (7/7) — both are legitimate, real observations,
  reported together for a fuller picture.
- All other lifecycle assertions unchanged and still passing (20 cycles,
  no stale carryover, `frameNumber()` strictly increasing every frame,
  clean shutdown, all three post-shutdown command-rejection checks).

## 6. Desktop soak results

```
soak duration: 900.0s (15.0 real minutes, target met exactly)
frame count: 27059
media changes: 29
peak region count: 33 (configured maxBlobs: 12 — see §1/§10, not a real bound on this metric)
peak fragment count: 10 (configured maxActiveFragments: 10 — never exceeded)
scratch FBO peak width: fragment=1272 background=0
scratch FBO peak height: fragment=712 background=0
scratch FBO peak area: fragment=905664 background=0
scene FBO size: 1280x720 (constant throughout)
starting RSS: NOT MEASURED
ending RSS: NOT MEASURED
peak RSS: NOT MEASURED
heap alloc delta (existing alloccounter instrumentation): new=7,459,879 delete=7,455,316 (delta 4,563 outstanding after 27,059 frames — ~0.17 net allocations/frame; consistent with normal steady-state churn, not a runaway leak signature)
average FPS/frame time: min fps observed=6.6, max frameTimeMs observed=289.13ms (both transient — coincide with the largest simultaneous-fragment/scratch-FBO-growth moments; fps recovered to 30.0 in every subsequent 30s sample, confirmed in the raw log)
GL/HUD errors: none observed
crashes: none (process reached shutdown cleanly)
```

Soak acceptance checks: no crash (confirmed); no failed media state caused
by Blob (all 29 automatic media changes handled correctly, `HudFrameData.video`
stayed authoritative throughout); no monotonic region/fragment container
growth (region_count oscillated repeatedly, e.g. consecutive 30s samples:
`0 → 2 → 12 → 0 → 0 → 25 → 0 → 23 → 0 → 0 → 0`, never staying pinned high);
no monotonic scratch-FBO growth unrelated to fragment demand (grew in
discrete steps tracking real detected-region size, then held steady, never
grew again after 1272×712 for the remainder of the run); scene FBO fixed at
Blob native size throughout; HUD continued rendering correctly (no
`soakGlHudIssueObserved_` trip, confirmed by the harness's own frame-by-frame
validity check); automatic media changes did not destabilize Blob; no GL
corruption observed; RSS unavailable so no growth claim is made either way.
**5/6 explicit harness checks passed** (one, `region_count <= maxBlobs`,
failed against a comparison later determined to be invalid — see §1 — and
was corrected in code for future runs; the underlying soak run itself
completed with zero crashes/instability).

No Raspberry Pi conclusions are drawn from this desktop-only soak.

## 7. Controlled visual parity

- Standalone controlled capture: `docs/reviews/blob-parity-captures-controlled/standalone.png`
- Runtime scene-only controlled capture: `docs/reviews/blob-parity-captures-controlled/runtime_scene_only.png`
- Runtime + HUD controlled capture: `docs/reviews/blob-parity-captures-controlled/runtime_with_hud.png`

**Same-media proof: did not hold** (see §1 summary and §10/§11 for the root
cause — `VideoPlaybackService`'s file discovery is not sorted). Both apps
were driven identically (repeated `previous()`/`p` far beyond the 26-entry
catalog size, confirmed rejected — `canSelectPrevious`-equivalent boundary
reached in both), but landed on different physical files: standalone shows
an ocean-waves clip; runtime shows a distinct "crystalline flower" clip.
This is disclosed, not hidden.

Recorded settings (verified identical by direct code comparison, not
assumed):
- Blob detector config: `BlobDetector::Config{}` defaults in both apps
  (`enabled=true, analysisWidth=320, analysisHeight=180,
  processEveryNFrames=2, threshold=40, minBlobAreaNormalized=0.0008,
  maxBlobAreaNormalized=0.35, maxBlobs=12, erodeIterations=1,
  dilateIterations=2`).
- Blob tracker config: `BlobTracker::Config{}` defaults in both
  (`maxAssociationDistanceNormalized=0.18, minIoU=0.08,
  maxMissingSeconds=0.5, positionSmoothing=0.35, sizeSmoothing=0.25`).
- Background mode: `1` ("normal", unshaded) in both — `BlobSceneCore::
  BackgroundConfig{}`'s default matches the standalone GUI's `pBackgroundMode`
  default exactly.
- Fragment effect: `"desaturate"` in both (`VideoRegionController::Params{}`'s
  default matches the standalone GUI's `pEffectIndex=0` default).
- Fragment scale/crop: `fragmentScale=1.0, cropPadding=0.0,
  maxActiveFragments=10, drawMode=0` — struct defaults, identical in both.
- Native render size: 1280×720 both (standalone's `ofCreateWindow(1280,720)`
  == `BlobProductionScene::kNativeWidth/kNativeHeight`).
- Capture dimensions: standalone is a full-desktop screenshot with the
  1280×720 app window visible among other windows (clean window-only
  cropping was attempted but unreliable this session — same limitation
  noted in the prior acceptance-patch report); runtime captures are exact
  1280×720 (scene-only, via `saveSceneFrameCaptureForTesting()`) and a
  full-desktop screenshot for the HUD capture.

Structural comparison (valid regardless of the media mismatch, since these
are pipeline-mechanics checks, not content checks):
- Background cover-fit/aspect handling: consistent — both use
  `VideoRegionMath::computeCropFillSourceRect()`, unchanged code path.
- HUD viewport crop/rounded corners/bevel: correct in
  `runtime_with_hud.png` — `MediaViewportMesh` renders cleanly, no bleed.
- Coordinate alignment/color treatment: no anomalies observed in either
  capture; the runtime's scene-only and HUD-composited captures show
  identical underlying content (confirmed by eye), proving internal
  consistency of the runtime's own pipeline at minimum.
- Fragment crop/scale/blending: not comparable in this specific capture
  pair (neither capture happened to show an active fragment at the
  captured instant — both read `region count 0/1`-ish states); the prior
  acceptance-patch report's captures already demonstrated real fragment
  rendering, and this session's soak/lifecycle screenshots (§4, §6) show
  further real fragment instances.

**Classification: equivalent with intentional/incidental differences.**
The intentional difference is the same one the prior report already
documented (automatic media advance under the runtime vs. none standalone).
The incidental difference is new to this session: the two captures show
different media content due to the newly-discovered catalog-ordering
non-determinism (§10/§11) — an incidental capture-methodology limitation,
not a rendering regression. No noticeable regression was found in any
structural comparison point that was actually checkable.

## 8. Builds/tests

```
cd sketches/blob-region-prototype && make Release -j4                         # OK
cd ../experience_runtime && make Release -j4                                  # OK
cd test && make -f Makefile.tests clean && make -f Makefile.tests test        # lifecycle_state_tests 78/78, session2_integration_tests 42/42, blob_semantic_mapping_tests 68/68
cd ../../blob-region-prototype/test && make -f Makefile.tests clean && make -f Makefile.tests test   # videoregion_math_tests 62/62
EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS=1 ./bin/experience_runtime.app/Contents/MacOS/experience_runtime   # run 1 (before assertion fix): 895/895, informational data gathered
EXPERIENCE_RUNTIME_BLOB_SOAK_HARNESS=1 ./bin/experience_runtime.app/Contents/MacOS/experience_runtime         # 900.0s, 27059 frames, 5/6 checks (1 disproved assumption, corrected — see §10)
EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS=1 ./bin/experience_runtime.app/Contents/MacOS/experience_runtime   # final clean re-run after fix: 895/895, 0 failures
./bin/experience_runtime.app/Contents/MacOS/experience_runtime               # normal-path controlled-capture run
./bin/blob-region-prototype.app/Contents/MacOS/blob-region-prototype         # standalone controlled-capture run
```

All required tests ran; none were skipped. Exact check counts reported
above for every harness that supports counting.

## 9. Shared Video / Shared Effects verification

- canonical VideoPlaybackService preserved: **yes**
- canonical media root preserved: **yes**
- NextMedia/PreviousMedia semantics preserved: **yes** — `next()`/`previous()`
  used exactly as documented (reject cleanly at catalog boundaries); the new
  standalone `n`/`p` keybinding calls these same existing methods directly,
  nothing new added to `VideoPlaybackService`'s public surface.
- HudFrameData.video remains authoritative: **yes**
- standalone-only media-control helper affects production semantics: **no**
  — the new standalone keybinding is confined to `sketches/blob-region-prototype/src/ofApp.cpp`
  (the dev-only sketch), calls only the existing public `next()/previous()`,
  and touches nothing under `sketches/experience_runtime/` or
  `shared/src/video-playback/`.

Blob canonical EffectActivityStatus producer: **none** (unchanged from both
prior reports — not re-investigated in depth this session, no new evidence
found or sought that would change this).
HudFrameData.effects: **std::nullopt** (unchanged).
Effect reconstruction introduced: **no**.

## 10. Deviations

- **`region_count <= configured maxBlobs` was never a valid assertion** —
  discovered via the real 900s soak, not anticipated by this task's own
  prompt (which assumed `maxBlobs` bounds `region_count`). Corrected the
  harness code (both lifecycle and soak paths) to report this value
  informationally instead of asserting a false bound, per the prompt's own
  "report the pattern rather than tuning around it" instruction (applied
  here to a false assumption, not just RSS growth). No Blob production
  code was changed to enforce a new cap — that would be a real behavior
  change (redesigning `BlobTracker`), out of this patch's explicit scope.
- **The soak's initial launch was investigated as a possible early crash**
  before being confirmed to actually be alive and progressing normally —
  a background-process monitoring artifact in this session's tooling (a
  `pgrep -f` pattern that didn't match the process's actual relative-path
  argv), not a real process failure. Documented here for transparency since
  it briefly looked like an acceptance-relevant finding before being ruled
  out.
- **Scope D's same-media control did not hold**, revealing that
  `VideoPlaybackService`'s file discovery (`ofDirectory::listDir()` in
  `VideoPlaybackService.cpp`) is not sorted — confirmed by direct code read
  (no `.sort()` call exists on the `ofDirectory` before iterating
  `getFiles()`). This means catalog index assignment is not guaranteed
  stable across separate process launches, even scanning the identical
  directory. Not fixed in this patch (would be a `shared/src/video-playback/`
  change, cross-domain, and the prompt explicitly forbids "redesign
  VideoPlaybackService" here) — reported as a cross-domain finding (§13).
- **Standalone capture cropping remained unreliable this session** (same
  limitation as the prior acceptance-patch report) — full-desktop
  screenshots were used again rather than a clean window-only crop.
- **The lifecycle harness's final "clean" re-run happened to sample media
  with zero detected activity** (all peaks 0) — the earlier, pre-fix run in
  this same session captured real non-zero peaks (7/7, scratch 147×70),
  which are reported alongside the clean run's numbers in §5 for a fuller
  picture rather than only reporting the least informative run.

## 11. Newly discovered risks

**Blob-owned:**
- `BlobTracker` has no configured ceiling on accumulated active-track
  count (only `BlobDetector::Config::maxBlobs` bounds new detections per
  frame). Observed peak 33 during a 900s soak — not a leak (returns to 0
  repeatedly), but worth a deliberate design decision (does Blob want a
  track-count cap independent of the per-frame detection cap?) before any
  future Pi-performance profiling work, since `BlobTracker::update()`'s
  O(tracks × detections) matching cost scales with whatever this
  unbounded number turns out to be under real gallery conditions.
  (Severity: low/non-blocking — informational for future work, not
  observed to cause any problem in this soak.)
- Minimum observed FPS during the soak (6.6, vs. steady-state 30.0)
  coincided with the largest simultaneous scratch-FBO/fragment-count
  moments — expected given `BlobDetector`'s per-frame cost scales with
  activity, but not previously quantified. Worth keeping in mind for
  future Pi profiling (without drawing a Pi conclusion from this desktop
  number).

**Cross-domain (Shared Video):**
- `VideoPlaybackService`'s media-catalog file discovery
  (`shared/src/video-playback/VideoPlaybackService.cpp`, the
  `dir.listDir(mediaRoot_)` call) does not sort the discovered file list
  before building the catalog — meaning catalog index assignment (and
  therefore which physical file "index 0" refers to) is not guaranteed
  identical across separate process launches scanning the same directory.
  This blocked a fully controlled same-media parity capture in this
  session (§7/§10). A one-line fix (sort `discoveredRelativePaths` before
  `MediaCatalog::build()`) would make catalog ordering deterministic
  without changing `VideoPlaybackService`'s public API or `MediaCatalog`'s
  shape — flagged for the Shared Video domain to evaluate and decide,
  not implemented here (out of this patch's scope per its own non-goals).

**Pi-only:** none discovered this session (desktop-only work, as required).

**Product enhancements:** none proposed — out of scope for this hardening
patch.

## 12. Contract changes requested

None. No Architecture stop condition was hit.

## 13. Cross-domain handoffs recommended

- **Shared Video domain**: evaluate sorting `VideoPlaybackService`'s
  discovered media file list for deterministic catalog ordering across
  process launches (§11). Low cost, would unblock fully-controlled
  cross-app visual-parity captures for Blob and any future scene.
- **Raspberry Pi Runtime domain**: `BlobTracker`'s unbounded active-track
  count (§11) is worth a specific measurement once real Pi hardware
  profiling begins, since its per-frame association cost scales with it.

## 14. Recommended next step

**ADDITIONAL NON-BLOCKING BLOB HARDENING RECOMMENDED** — specifically: (a)
a deliberate design decision on whether `BlobTracker` should gain a
track-count cap (currently none), and (b) once the Shared Video domain
addresses catalog-ordering determinism, a follow-up controlled parity
recapture that actually achieves same-media matching. Neither blocks
Blob's already-accepted production status; both are narrow, well-scoped
follow-ups discovered by this session's real measurement work, not new
scope invented here.
