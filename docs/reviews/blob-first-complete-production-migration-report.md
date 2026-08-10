# Blob First Complete Production Migration — Completion Report

Date: 2026-08-09
Scope: Migrate `sketches/blob-region-prototype` into the accepted Ecopunk Runtime
(`ExperienceRuntime`/`SceneManager`/`HudCompositorBridge`) as the first real
production scene implementing `IEcopunkScene`.

## 1. Summary

**Partially complete.** The core migration is done and verified live: Blob now
runs as a real `IEcopunkScene` implementation (`BlobProductionScene`) hosted by
`SceneManager`/`ExperienceRuntime`, drawing into the runtime-owned scene FBO,
compositing through the real HUD (`HudCompositorBridge` →
`hudpresent::HudWireframeRenderer` → `MediaViewportMesh`), consuming the one
canonical `VideoPlaybackService` instance, and responding to `NextMedia`/
`PreviousMedia` commands end-to-end. This was proven with a live, windowed run
(screenshots below), not just a compile check.

Two things named explicit gaps in the plan and were **deliberately not
implemented**, per DEC-015 and this task's own "do not fabricate" rules:

- **`HudFrameData.effects` stays absent for Blob.** No legitimate
  effect-activity owner object exists in Blob's current code (two bare GUI
  effect-name strings, not an owned activity-tracking object). Closing this
  requires an Architecture-owned decision, not a scene-adapter workaround —
  see §8.
- **`Regenerate` is not advertised.** No curated/proven-safe visual-variant
  table exists for Blob yet.

A third gap, discovered during implementation (not anticipated in the plan):
**no automated activate/deactivate/reactivate lifecycle harness was added**
for `BlobProductionScene` (unlike `FakeSceneLifecycleState`'s dependency-free
test suite) — lifecycle correctness was verified by live/manual exercise
only. See §5 and §7.

## 2. Files inspected

`shared/src/scene/SceneContract.h`, `SceneSemanticTypes.h`;
`sketches/experience_runtime/src/{SceneManager,ExperienceRuntime,FakeScene,
FakeSceneLifecycleState,RuntimeServices,InputRouter,SceneRenderGuard,
HudCompositorBridge,GlRestorationHarness,HudVocabularyResolver,
HudPresentationProfile}.{h,cpp}`; `shared/src/video-playback/
{VideoPlaybackService,VideoPlaybackStatus}.h`; `shared/src/video-effects/
knowledge/EffectActivityStatus.h`; `sketches/blob-region-prototype/src/
ofApp.{h,cpp}` (pre-migration); `shared/src/{BlobDetector,BlobTracker,
VideoRegionController,VideoRegionEffectRenderer,Fragment,ShaderLibrary,
VideoRegionMath,VideoRegionMathOf}.{h,cpp}`; `sketches/experience_runtime/
{config.make,addons.make}` and `sketches/blob-region-prototype/config.make`;
`shared/build/test-main-exclusions.mk`; `sketches/experience_runtime/test/
{lifecycle_state_tests,session2_integration_tests}.cpp`; `sketches/
blob-region-prototype/test/videoregion_math_tests.cpp`;
`docs/shared-project-docs/{Scene-HUD-Contract-v1,Decision-Log-v3,
01-architecture-governance,02-cross-domain-handoff-protocol,
HUD-Initiative-Scope-and-Product-Direction-Addendum,
00-shared-project-context,04-project-chat-index}.md`; `docs/reviews/
{hud-wireframe-engineering-session-2-review,
hud-runtime-architecture-closure-review,hud-final-narrow-closure-patch-report,
hud-wireframe-bounds-v1}.md`; `docs/probes/hud-runtime-validation-studio-probe.md`;
`docs/{blob-region-architecture,shared-video-playback-system-implementation-report,
shared-video-playback-engineering-session-2-report,
blob-region-prototype-shared-catalog-migration-status,
shared-effects-final-canonical-activity-producer-seam-patch-report,
hud-double-hud-prevention-and-migration-matrix,
scenemanager-hud-alignment-probe,HUD-Semantic-Slot-Model-v1,
Ecopunk-HUD-System-Master-Roadmap}.md`; `CLAUDE.md`, `.claude/CLAUDE.md`.

## 3. Files changed

**New:**
- `sketches/blob-region-prototype/src/BlobSceneCore.{h,cpp}` — the reusable,
  host-agnostic pipeline core (ShaderLibrary → BlobDetector → BlobTracker →
  VideoRegionController + a separate background `VideoRegionEffectRenderer`),
  extracted from `ofApp` so both the standalone dev sketch and the production
  adapter drive the same code.
- `sketches/experience_runtime/src/BlobProductionScene.{h,cpp}` — the
  `IEcopunkScene` adapter: lifecycle, `hudStatus()`/`SceneSemanticData`
  mapping, `NextMedia`/`PreviousMedia`/`Reset` command wiring, no
  `EffectActivityStatus` authoring (see §1/§8).
- `sketches/experience_runtime/addons.make` — added (`ofxOpenCv`, required by
  `BlobDetector`); this sketch had no `addons.make` before.
- Symlinks into `sketches/experience_runtime/src/`: `BlobSceneCore.cpp`
  (from `../../blob-region-prototype/src/`), `BlobDetector.cpp`,
  `BlobTracker.cpp`, `Fragment.cpp`, `VideoRegionController.cpp`,
  `VideoRegionEffectRenderer.cpp`, `VideoRegionMath.cpp` (from
  `../../../shared/src/`) — same established pattern already used for
  `ShaderLibrary.cpp`/`TFEffectPicker.cpp`/etc.
- `sketches/experience_runtime/bin/data/{shaders,of_nature_shader_pack_glsl}/`
  — copied from `blob-region-prototype`'s own `bin/data/` (gitignored, local
  build assets, same as every other `ShaderLibrary`-consuming sketch's own
  copy — see §6/§7 "Newly discovered risks").

**Modified:**
- `shared/src/{BlobDetector,BlobTracker,VideoRegionController}.{h,cpp}` —
  added one `reset()` method each (clears transient
  detections/tracks/managed-fragments only; config/params untouched). Small,
  additive lifecycle hooks, not behavior changes to existing methods.
- `sketches/blob-region-prototype/src/ofApp.{h,cpp}` — refactored to host
  `BlobSceneCore` + its own `VideoPlaybackService` instance + the existing
  ofxGui/debug-overlay surface, unchanged in outward behavior.
- `sketches/experience_runtime/config.make` — added `-I../blob-region-prototype/src`.
- `sketches/experience_runtime/src/SceneManager.{h,cpp}` — additive
  `installProductionScene(IEcopunkScene*)` + private `activeScene()`
  indirection (const and non-const overloads); `fakeScene_` and `devScene()`
  unchanged; every existing caller that never installs a production scene is
  unaffected.
- `sketches/experience_runtime/src/ExperienceRuntime.{h,cpp}` — added
  constructor (binds `blobScene_` to `runtimeServices_.video()` at
  construction), `blobScene_` member, `installBlobProductionScene()` public
  method (caller-controlled, not auto-installed inside `setup()` — see §6),
  reordered `setup()` so `RuntimeServices::setup()` runs before scene
  setup/activation, fixed a now-stale `sceneAssetRoot` comment.
- `sketches/experience_runtime/src/InputRouter.cpp` — mapped `n`/`N` →
  `SceneCommand::NextMedia`, `p`/`P` → `SceneCommand::PreviousMedia` (no prior
  binding existed for either).
- `sketches/experience_runtime/src/main.cpp` — updated stale header comment
  ("No production scene is built or referenced here").
- `sketches/experience_runtime/src/ofApp.{h,cpp}` — `setup()` now installs
  `BlobProductionScene` only on the non-harness path (see §6).

**Not touched by this migration** (pre-existing, uncommitted changes already
present in the working tree from earlier sessions — confirmed via `git
status` before/after, not created by this work): `GlRestorationHarness.{h,cpp}`,
`shared/src/hud-compositor/**`, `sketches/temporal-fields/src/{TFEffectPicker,
ofApp}.cpp`, `sketches/hud_validation_studio/src/ofApp.cpp`, several
`docs/reviews/*.md`, and the pre-existing symlinked `.cpp` files in
`sketches/experience_runtime/src/` from the earlier TFEffectPicker seam-proof
session (`DefaultVideoEffectCatalog.cpp`, `EffectKnowledgeBase.cpp`, etc.).

## 4. Tests/builds run

```
cd sketches/blob-region-prototype && make Release -j4
cd sketches/experience_runtime && make Release -j4
cd sketches/experience_runtime/test && make -f Makefile.tests clean && make -f Makefile.tests test
cd sketches/blob-region-prototype/test && make -f Makefile.tests clean && make -f Makefile.tests test
```

Plus two live, windowed runs of the built `experience_runtime` binary
(screenshots captured), and one live run of the standalone
`blob-region-prototype` binary.

## 5. Results

- **Standalone build:** clean, no errors.
- **Runtime build:** clean, no errors, after adding `ofxOpenCv` to
  `addons.make` and the six symlinks (`ofApp`'s pipeline classes weren't
  previously compiled into this sketch at all).
- **Lifecycle test:** no automated harness added for `BlobProductionScene`
  (see §1/§7) — verified only by live exercise: the scene ran continuously
  for ~2 minutes, survived three automatic media changes
  (`holdDurationSeconds=30`, confirmed by "video file loaded" log lines
  ~30s apart) without leaking/crashing, and fragment/scratch-FBO sizes grew
  and shrank normally as blobs appeared/disappeared. `activate()`/
  `deactivate()`/`reset()` themselves were exercised only via direct call
  reasoning (they're trivial `BlobDetector::reset()`/`BlobTracker::reset()`/
  `VideoRegionController::reset()` calls, each independently simple), not a
  scripted repeated-cycle proof.
- **Command tests:** confirmed live via keystrokes routed through the real
  `InputRouter`/`SceneManager`/`ExperienceRuntime` chain —
  `n` → `[notice] ExperienceRuntime: SceneCommand dispatched, accepted=1`
  followed immediately by a new "video file loaded" line (`NextMedia`
  working); `p` → same, confirmed `PreviousMedia` working. `Regenerate`/GUI/
  debug commands are not advertised (`capabilities()` only lists
  `NextMedia`/`PreviousMedia`).
- **Semantic/status tests:** confirmed live via HUD screenshot — the HUD's
  pre-existing `blob-region-prototype` presentation profile/vocabulary
  (`scene.blob.state.fragmenting`, `scene.blob.card.label`="REGION FIELD",
  `scene.blob.metric.region_count`) resolved real data end-to-end: "REGION
  FIELD / FRAGMENTING / 8" and an 80% "ACTIVITY" bar, both driven by real
  `BlobTracker`/`VideoRegionController` counts, not fabricated values. No
  automated fixture-based semantic test was added (see §7).
- **Video data-flow proof:** confirmed live — `media.title` shown in the HUD
  (`media.auto.080b175e`, the un-curated fallback identifier format) and the
  rendered video both changed together on every `NextMedia`/auto-advance
  event, proving `VideoPlaybackService` → `VideoPlaybackStatus` →
  `HudFrameData.video` and Blob's own draw both read the same instance.
- **Render/GL proof:** `SceneRenderGuard` baseline stayed green across the
  full live run (no visible corruption, HUD rendered correctly on every
  frame after Blob's draw, screenshots below show clean composition —
  rounded-corner `MediaViewportMesh`, no bleed). No formal
  begin/end-pair-count instrumentation was added; relied on the
  pre-migration GL/OF-state audit (already balanced, confirmed by direct
  code read) plus this observed clean run.
- **Visual parity:** materially recognizable — same background cover-fit,
  same fragment/shader treatment, same detection/tracking behavior as the
  standalone sketch. No frame-aligned pixel-diff baseline comparison was
  captured (see §7); comparison was visual/live only.
- **Effect-owner/data-flow result:** gap confirmed and reported, not closed
  (see §1/§8) — `HudFrameData.effects` is `std::nullopt` for Blob by design.
- **Allocation/resource observations:** none instrumented beyond what was
  visually/log-observed (steady scratch-FBO reallocation only when fragment
  count/size genuinely changed, no runaway growth over the ~2-minute run).

## 6. Deviations from prompt

- **`SceneManager` required an additive change** (`installProductionScene()`
  + `activeScene()` indirection) that the original prompt's discovery
  material didn't anticipate: `SceneManager` had no scene registry at all
  before this migration (hardcoded `FakeScene fakeScene_`). This is not a
  frozen-contract change (nothing in `IEcopunkScene`/`SceneFrame`/lifecycle
  semantics/command ownership changed), but it is new scope, consistent with
  `01-architecture-governance.md`'s explicit "first real scene" mandatory
  review checkpoint.
- **Scene installation is caller-controlled, not automatic inside
  `ExperienceRuntime::setup()`.** Discovered live: `GlRestorationHarness`
  reuses the same `ExperienceRuntime`/`ofApp` and directly manipulates
  `devScene()` (`FakeScene`) expecting it to be the actively-driven scene.
  Installing Blob unconditionally inside `setup()` would have silently
  broken every harness proof (the harness would keep poking `FakeScene`
  while the pipeline actually drew/reported `BlobProductionScene`). Fixed by
  making installation an explicit `ExperienceRuntime::installBlobProductionScene()`
  call, made only by `ofApp::setup()`'s non-harness branch, before
  `runtime.setup()` runs.
- **Local build assets required copying**, not just source-code work:
  `sketches/experience_runtime/bin/data/` had no `shaders/`/
  `of_nature_shader_pack_glsl/` directories (the sketch never used
  `ShaderLibrary` before), so every shader failed to load at runtime until
  copied from `blob-region-prototype`'s own `bin/data/` — matching the
  per-sketch-copy convention already used by every other `ShaderLibrary`
  consumer (confirmed no cross-sketch symlink convention exists for these
  specific assets, unlike `media`).
- **`InputRouter` had no key bound to `NextMedia`/`PreviousMedia` at all** —
  added `n`/`p` (avoiding `1`–`6`, already used by `SceneCommand::Reset`/
  `Regenerate` and `ExperienceRuntime::keyPressed()`'s `FakeScene`-only dev
  hooks).
- **No automated Blob lifecycle harness was built** (see §1/§7) — the plan
  called for one; given the scope already covered (new adapter, new core,
  `SceneManager` change, build-system wiring, live verification), and that
  `BlobSceneCore`'s pipeline requires a real GL/window context (unlike
  `FakeSceneLifecycleState`'s dependency-free design), building a
  `GlRestorationHarness`-equivalent for Blob was left as follow-up rather
  than done in this patch.
- **`Regenerate`/`Reset` are not advertised** and **`HudFrameData.effects`
  stays absent** — both per the plan's own explicit scope boundary, not
  unplanned deviations, restated here for completeness.

## 7. Newly discovered risks

- **No automated Blob-specific lifecycle test exists.** (Severity: medium.)
  Correctness of repeated `activate()`/`deactivate()`/`reactivate()`/
  `shutdown()` rests on code-reading + one continuous live run, not a
  scripted proof. `BlobDetector::reset()`/`BlobTracker::reset()`/
  `VideoRegionController::reset()` are each simple (`.clear()`-equivalent)
  and were code-reviewed carefully, but no test exercises, e.g., 20 rapid
  activate/deactivate cycles the way `lifecycle_state_tests.cpp` does for
  `FakeSceneLifecycleState`.
- **No pixel-level visual-parity baseline was captured.** (Severity: low.)
  Comparison between the standalone sketch and the runtime-hosted scene was
  visual/live only, not the screenshot-diff protocol
  `hud-runtime-architecture-closure-review.md` used for HUD geometry. The
  ARB-texture-UV bug that review flagged for "re-confirm against a real
  opaque scene" appears fine (video renders correctly, no black/garbled
  frames observed), but this is an observation, not a captured proof
  artifact.
- **`sketches/experience_runtime/bin/data/{shaders,of_nature_shader_pack_glsl}/`
  is now a fourth uncoordinated copy** of shader assets already duplicated
  across `blob-region-prototype`/`temporal-fields`/`blueprint_emergence`/
  `quadrant-crosshair` (per `CLAUDE.md`'s own already-known
  `check-video-effect-drift.py` risk category) — no single source of truth
  enforces these stay in sync; a future shader edit made in one sketch's
  `bin/data/` will silently not propagate to the others, including this new
  one.
- **`BlobProductionScene::executeCommand(Reset)` has no lifecycle-state
  enforcement** (severity: low) — unlike `FakeScene`'s
  `FakeSceneLifecycleState`-backed rejection of commands issued
  before-setup/after-shutdown, Blob's `executeCommand()` always returns
  `true` for `Reset` regardless of scene state. Not observed to cause a
  problem (no such state exists in production dispatch order today), but
  it's a real gap relative to the more rigorous `FakeScene` pattern.
- **The working tree already contained substantial unrelated uncommitted
  changes** from earlier sessions (see §3, "Not touched by this migration")
  — this migration's diff is entangled in the same working tree as that
  pre-existing state; a clean commit of just this migration would need
  careful `git add` scoping, not `git add -A`.

## 8. Contract changes requested

**None required to complete this patch.** One pre-existing, already-documented
gap remains genuinely open and is *not* proposed to be closed here:

- **Current limitation:** Blob has no object that owns canonical effect
  execution/selection state. Its "current effect" is two bare
  `ofParameter<int>` GUI indices, read directly in `ofApp`/now
  `BlobSceneCore`'s config accessors — there is no `TFEffectPicker`-shaped
  owner that could honestly author a `videoeffects::EffectActivityStatus`
  (canonical IDs, transition/evolution state) without reconstructing it from
  display-name strings, which DEC-015 and this task's own Step 8 forbid.
- **Proposed change:** none proposed by this report — deciding whether Blob
  gets a real effect-execution owner object (and what it looks like) is
  Architecture-owned scope, not a scene-adapter decision.
- **Alternatives considered:** (a) forward a synthesized snapshot built from
  the two GUI strings — rejected, this is exactly the "reconstruct canonical
  effect status from display names" anti-pattern the contract forbids; (b)
  leave `HudFrameData.effects` absent for Blob — **adopted**, matches the
  "missing snapshot" semantics the contract already defines for exactly this
  situation.
- **Affected domains:** Shared Effects / Blob Scene Migration.
- **Migration cost:** unknown until an owner design is proposed; likely a
  small new Blob-local class (not a shared-contract change) once Architecture
  decides what "effect ownership" should mean for a scene whose effect
  selection today has no per-blob variety (`VideoRegionController::
  assignEffectForId()`'s own comment already marks this as an intentional,
  unbuilt seam).

## 9. Recommended next step

**Requires one narrow Blob patch** — add an automated `BlobProductionScene`
lifecycle harness (§7's top risk) before requesting acceptance review, given
"first real scene" is a named mandatory Architecture checkpoint
(`01-architecture-governance.md`) and the current lifecycle proof is
live-run-only.

Once that patch lands, this migration should move to **Ready for Blob
migration acceptance review** for everything except effects, and separately
**Requires Shared Effects ownership follow-up** for the `HudFrameData.effects`
gap (§8), which is out of scope for a scene-adapter patch regardless of how
much of the lifecycle work is finished first.
