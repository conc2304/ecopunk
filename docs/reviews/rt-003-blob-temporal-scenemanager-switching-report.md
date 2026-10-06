# RT-003 — Blob + Temporal In-Process SceneManager Switching: Completion Report

**Date:** 2026-10-05
**Work item:** RT-003 (unblocks RT-002)
**Recipient:** ExperienceRuntime & SceneManager domain manager → Architecture & Program Coordination
**Produced by:** Coding agent
**Repository:** `blueprint-emergence-updates` @ `d25cc13` + uncommitted working-tree changes (listed in §3)
**Authoritative inputs:** RT-003 authorization prompt (Architecture decisions: TEMP-001 ACCEPTED; Stage B collision gate cleared for Blob + Temporal only; in-process switching; contract change NO), [Scene/HUD Contract v1](../shared-project-docs/Scene-HUD-Contract-v1.md) §6/§8/§9/§10/§12, [Blob → Temporal → Blob HUD Acceptance Matrix](../shared-project-docs/blob-temporal-blob-hud-acceptance-matrix.md) §3, [Governance](../shared-project-docs/01-architecture-governance.md), [Handoff Protocol](../shared-project-docs/02-cross-domain-handoff-protocol.md), OFF-REPO `~/Downloads/temporal-production-migration-two-scene-runtime-acceptance-requirements.md`, the 2026-10-05 two-scene reconciliation (returned inline in chat; its file was never saved — see §6)

> **Note on the ruling artifact:** "Architecture Ruling — Blob + Temporal Stage B / RT-003 Authorization" does not exist in the repository or `~/Downloads`. The decisions listed in the RT-003 prompt were treated as that ruling. ARCH-003 should bring the ruling into the repo.

---

## 1. Summary

ExperienceRuntime now hosts **both** real production scenes at once and switches between them in-process:

- `SceneManager` replaces its single `productionScene_` slot with a fixed, ordered two-entry registry: `blob-region-prototype`, then `temporal-fields`.
- `RuntimeCommand::NextScene` / `PreviousScene` work through the existing InputRouter path (`]` / `[`).
- Transitions use only the frozen `SceneTransitionPhase` values (`Idle → FadingOut → Loading → FadingIn → Idle`, plus `Failed`).

All switching logic lives in a new OF-free state machine, `SceneSwitchController.h`, which `SceneManager` drives. It is unit-tested with a bare compiler (127/127 checks). A new real-scene GL harness, `SceneSwitchHarness`, ran **3 complete Blob → Temporal → Blob cycles: 5,351/5,351 checks PASS** on a clean Release build.

All existing suites remain green:
- GL restoration harness: all checks passed.
- Blob lifecycle harness: 895/895.
- Temporal lifecycle harness: 295/295.
- ExperienceRuntime unit suites: 349 checks across 5 binaries.
- Shared Video: 151.
- Shared Effects: 93.
- HUD: 1,224.

**No frozen contract changed.** `git diff` is empty for `shared/src/scene`, `hud-runtime`, `video-playback`, `video-effects`, `hud-compositor`, both production scene classes, and the temporal-fields and blob-region-prototype sketches.

Blob's `HudFrameData.effects = std::nullopt` no longer comes from FakeScene. Blob is registered with no effect source, and SceneManager publishes `nullopt` directly. FakeScene is reached only on the tooling path with no production registry.

## 2. Files inspected

- `shared/src/scene/SceneContract.h`
- `sketches/experience_runtime/src/`:
  - `SceneManager.{h,cpp}`, `ExperienceRuntime.{h,cpp}`, `ofApp.{h,cpp}`, `InputRouter.{h,cpp}`, `RuntimeServices.h`
  - `BlobProductionScene.{h,cpp}`, `TemporalProductionScene.{h,cpp}`, `FakeScene.{h,cpp}`
  - `GlRestorationHarness.cpp` (FBO step 15, GL baseline checks), `TemporalLifecycleHarness.{h,cpp}`, `BlobLifecycleHarness.h`, `AllocationCounter.h`
- `sketches/experience_runtime/test/Makefile.tests`
- `shared/src/hud-compositor/HudWireframeRenderer.cpp` (profile/epoch keyed on `activeSceneId`), `HudRealFrameResolver.cpp` (transition sources; scene-switch control availability)
- `shared/src/scene/SceneSemanticTypes.h`
- Scene/HUD Contract v1 and the HUD acceptance matrix

## 3. Files changed

| File | Change |
|---|---|
| `sketches/experience_runtime/src/SceneSwitchController.h` | **New.** OF-free `sceneswitch::Controller`: ring traversal, request/suppression, per-frame phase advance, ownership (owner/pending), lazy setup tracking, failure handling via `LifecycleTarget` |
| `sketches/experience_runtime/src/SceneManager.{h,cpp}` | Registry (`registerProductionScene`, `setStartupScene`), `beginFrame()`, phase-aware status/effect/capability publication, real `NextScene`/`PreviousScene`, transition-aware `dispatchSceneCommand`, shutdown of every set-up scene, test counters. FakeScene kept only for the no-registry tooling path. Harness-facing methods (`activateScene`, `deactivateScene`, `captureSceneStatus`, `devScene`, effect override) preserved |
| `sketches/experience_runtime/src/ExperienceRuntime.{h,cpp}` | `install*ProductionScene()` registers the pair (startup Blob or Temporal); `update()` calls `beginFrame()` and only updates/pulls on live frames; `draw()` renders live or presents the retained static frame; FBO reallocation only on successful activation with a size change; evidence counters and test accessors; per-scene `sceneAssetRoot` moved to SceneManager (`installedTemporalScene_` removed) |
| `sketches/experience_runtime/src/SceneSwitchHarness.{h,cpp}` | **New.** RT-003 GL harness (`EXPERIENCE_RUNTIME_SWITCH_HARNESS`) |
| `sketches/experience_runtime/src/ofApp.{h,cpp}` | Wires the switch harness (lowest priority; always starts in Blob) |
| `sketches/experience_runtime/test/scene_switch_controller_tests.cpp` + `Makefile.tests` | **New** suite, added to `make test` |
| `docs/reviews/logs/rt003-*.log` | Evidence logs |

**Not changed:** `IEcopunkScene`, `SceneFrame`, `SceneManagerStatus`, `SceneTransitionPhase`, `RuntimeCommand`, `HudFrameData`, `EffectActivityStatus`, `VideoPlaybackService`/`VideoPlaybackStatus`, any HUD code, any semantic slot ID, any scene internals.

Stale comments in `BlobProductionScene.h:12` and `TemporalProductionScene.h:14,83` still mention the removed `installProductionScene()`. They were left untouched, because scene files were out of scope.

## 4. Tests/builds run

| Command | Result |
|---|---|
| `make clean && make Release -j8` (experience_runtime) | **exit 0, 0 errors**, 23 warnings (none in RT-003 files), 7m47s |
| `make -f Makefile.tests clean test` (experience_runtime/test) | lifecycle 78/78 · session2 42/42 · blob semantic 68/68 · temporal semantic 34/34 · **scene switch controller 127/127** |
| `shared/src/video-playback/test` `make test` | 38/38 · 113/113 |
| `shared/src/video-effects/test` `make test` | 93/93 |
| `shared/src/hud-compositor-test` `make test-all` | 713/713 · 140/140 · 357/357 · 14/14 |
| `EXPERIENCE_RUNTIME_GL_HARNESS=1` | "ALL CHECKS PASSED", 0 warning/error lines |
| `EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS=1` | **895/895** (20 Blob reactivation cycles), 0 warning/error lines |
| `EXPERIENCE_RUNTIME_TEMPORAL_LIFECYCLE_HARNESS=1` | **295/295**, 1 `texture is not allocated` warning (see §7) |
| `EXPERIENCE_RUNTIME_SWITCH_HARNESS=1` | **5,351/5,351, RESULT: PASS**, 1 `texture is not allocated` warning (see §7) |
| Not run | Blob 15-minute soak (`EXPERIENCE_RUNTIME_BLOB_SOAK_HARNESS`); the full RT-002 20-cycle acceptance (out of scope by instruction) |

Logs: [`logs/rt003-scene-switch-harness-2026-10-05.log`](logs/rt003-scene-switch-harness-2026-10-05.log), [`logs/rt003-gl-restoration-harness-2026-10-05.log`](logs/rt003-gl-restoration-harness-2026-10-05.log), [`logs/rt003-blob-lifecycle-harness-2026-10-05.log`](logs/rt003-blob-lifecycle-harness-2026-10-05.log), [`logs/rt003-temporal-lifecycle-harness-2026-10-05.log`](logs/rt003-temporal-lifecycle-harness-2026-10-05.log), [`logs/rt003-unit-suites-2026-10-05.log`](logs/rt003-unit-suites-2026-10-05.log), [`logs/rt003-clean-build-summary-2026-10-05.log`](logs/rt003-clean-build-summary-2026-10-05.log).

## 5. Results — required evidence

### Final SceneManager registry structure
`std::vector<Entry>`, where each entry holds a non-owning `IEcopunkScene*`, an optional `EffectActivitySource`, a lifecycle-active flag, a shutdown flag and counters. `ExperienceRuntime::installProductionScenePair()` registers, in fixed order:

| Index | sceneId | Effect source |
|---|---|---|
| 0 | `blob-region-prototype` | none → `effects = nullopt` |
| 1 | `temporal-fields` | `TemporalProductionScene::currentEffectActivitySnapshot()` |

Duplicate and post-setup registrations are rejected. FakeScene is never registered (harness check: "FakeScene is not a production registry entry" PASS).

### Startup scene behavior
- **Default** (normal run, Blob lifecycle harness, switch harness): Blob.
- `EXPERIENCE_RUNTIME_TEMPORAL_SCENE` or the Temporal lifecycle harness: Temporal.

Only the startup scene is set up at `setup()`; the other is set up lazily, at most once. Switch-harness warmup: Blob `setup=1 activate=1 capabilityQueries=1`, Temporal `setup=0 activate=0`.

### NextScene/PreviousScene trace
Two-entry ring: Blob `]`/`[` → Temporal; Temporal `]`/`[` → Blob. Controller tests check all four directions. The harness issues `]` (even legs) and `[` (odd legs) via `runtime.keyPressed()` → `InputRouter` → `RuntimeCommand` → `ExperienceRuntime::handleRuntimeCommand` → `SceneManager::handleSceneSwitchCommand`. Log example:

```text
SceneManager: NextScene accepted: "blob-region-prototype" -> "temporal-fields"
SceneManager: PreviousScene rejected (transition in progress)
```

### Transition state trace (every leg, all 6)
```text
Idle -> FadingOut (12 frames) -> Loading (1 frame) -> FadingIn (12 frames) -> Idle
```
Per-phase `transitionProgress` runs 0..1.

| Phase | Owner of record | Scene render | Status | Effects | Capabilities |
|---|---|---|---|---|---|
| FadingOut | outgoing (already deactivated) | none — retained outgoing frame | frozen last outgoing | frozen last outgoing | empty |
| Loading | none (`activeSceneId = ""`) | none — retained outgoing frame | neutral (no identity, no semantics) | nullopt | empty |
| FadingIn | incoming | live incoming | live incoming | live incoming | incoming's own |
| Failed | none | none — retained frame | neutral + manager `Failed` + message | nullopt | empty |

### Active/pending ownership trace
- **Acceptance:** `active = outgoing`, `pending = incoming`, outgoing `deactivate()` called once, capabilities cleared.
- **Loading:** `active = ""`, `pending = incoming`.
- **First FadingIn frame:** `active = incoming`, `pending = none`.

The harness checks all of these on every leg, and per frame checks `SceneHudStatus.sceneId == activeSceneId` (no mixing).

### Lifecycle counts (switch harness, final)

| Scene | setup | activate | successful | deactivate | shutdown | capability queries |
|---|---|---|---|---|---|---|
| Blob | 1 | 4 (startup + 3) | 4 | 4 | 1 | 4 |
| Temporal | 1 | 3 | 3 | 3 | 1 | 3 |

Capability queries equal successful activations for both scenes, and each incoming activation was asserted to add exactly one query.

### SceneHudStatus pull counts
- Blob: 279 = 60 warmup + 3 × (12 FadingIn + 1 completion + 60 dwell).
- Temporal: 219 = 3 × 73.

Status pulls equal updates equal draws for each scene. The per-frame assertion was "exactly 1 pull on live frames, 0 on transition-only frames", over all 576 frames. The outgoing scene had no pulls, updates, draws or effect pulls after deactivation in any leg.

### HudFrameData assembly / production HUD draw counts
576 frames presented → **576 assemblies, 576 HUD draws** (+1 each per frame, asserted per frame). Live frames 498; static frames 78 (6 switches × 13); neutral status publications 6 (one Loading frame per switch).

### Runtime FBO resize/reallocation evidence
- **Same-size switches** (Blob and Temporal are both 1280×720; 5 legs): no reallocation, same texture.
- **Different-size switch** (leg 5): the harness shrank the runtime FBO to 640×360 after the Loading frame, and the real activation path reallocated it:

  ```text
  ExperienceRuntime: scene FBO reallocated to 1280x720 for incoming scene "temporal-fields"
  PASS — different-size activation: runtime FBO reallocated 640x360 -> incoming native 1280x720 before the incoming draw; SceneFrame reflects the new texture
  ```

  `currentHudFrameData_.sceneFrame` is cleared before `allocate()`, and the new SceneFrame is built only after the incoming draw.
- Reallocation runs **only** on the activation frame. A per-frame auto-resize would undo `GlRestorationHarness` step 15's deliberate forced size; that harness still passes.

### Static outgoing-frame evidence
The runtime-owned `sceneFbo_` keeps the outgoing scene's last live frame, because nothing draws into it on non-live frames. No extra FBO is used, and no pointer to scene-owned resources is held. Per leg, the harness checked:
- the FNV-1a hash of the FBO pixels and the texture id were **identical** at acceptance, FadingOut frame 1, FadingOut frame 12 and the Loading frame;
- `staticSceneFrames` advanced exactly once per transition-only frame;
- the first incoming frame rendered live.

Captures: `bin/data/captures/scene_switch_cycle0_*_{1_fadingout,2_loading,3_first_incoming,4_steady}.png`. The Loading capture shows the retained Blob frame with the HUD at LOADING 100%, the manager message, no title, and ACTIVITY missing.

### Blob effect nullopt production cleanup evidence
- Blob is registered with no effect source (`ExperienceRuntime.cpp`, `installProductionScenePair`).
- `SceneManager::captureEffectActivityStatus()` returns `nullopt` for a live owner without a source.
- The FakeScene fallback is reached only when `entries_.empty()` (GL harness tooling path).
- Harness: warmup "60 live Blob frames, effects nullopt (no FakeScene source)" PASS, plus the per-frame invariant "effects present iff Temporal owns the frame" over 576 frames. Blob `effectPulls = 0`.

### Temporal effect transition evidence
- Blob → Temporal: `nullopt` → present on the first Temporal frame (3/3 legs).
- Temporal → Blob: present (frozen during FadingOut) → `nullopt` from Loading on, including the first Blob frame (3/3 legs).
- Temporal effect source pulled exactly once per live Temporal frame (219).

### Video regression result
- Shared Video suites 38/38 and 113/113.
- The Temporal lifecycle harness (adapter reload/invalidate on reactivation) is unchanged at 295/295.
- No change to `VideoPlaybackService` ownership, status, hold/selection, or the Temporal adapter path.
- Not proven here (RT-002 scope): live media-following across switches.

### Semantic/profile stale-state result
- Per frame: any `SceneHudStatus.semantic` state/metric ID carries the active scene's prefix (`scene.blob.` / `scene.temporal.`). Loading publishes no semantic payload.
- The HUD profile and history epoch are keyed on `SceneManagerStatus::activeSceneId` (`HudWireframeRenderer.cpp:233`). The ID sequence outgoing → `""` → incoming makes the existing renderer compile the incoming profile and reset history on each switch, with no HUD change. The captures show the incoming title and profile on the first incoming frame.

### GL regression result
- `GlRestorationHarness`: ALL CHECKS PASSED.
- The switch harness checks the post-draw GL baseline (scissor off, stencil off, program 0, no bound 2D texture, framebuffer 0) at warmup, on every Loading frame and on every first incoming frame (13 checks). All PASS with both real scenes.

### Resource/callback growth observations (macOS, not a Pi claim)

| | After cycle 1 | After cycle 2 | After cycle 3 |
|---|---|---|---|
| Net live allocations | 4,125 | 4,092 | 4,104 |
| Resident memory | 426 MB | 416 MB | 417 MB |

Per-cycle growth was −33 and +12, so there is no cycle-proportional growth. SceneManager registers no callbacks, and lifecycle counts are exact (no duplicate activations). Scene FBO allocations: 3 = setup + 1 forced (test) + 1 switch reallocation.

## 6. Deviations

1. **Activation failure detection.** The frozen `IEcopunkScene::setup()`/`activate()` return `void`, so the only failure observable without a contract change is an exception, which the controller catches. On failure it enters `Failed`, publishes no owner, best-effort deactivates the partially activated incoming scene, keeps the static frame, and suppresses switches. Unit tests cover this; no real scene throws, so it is not GL-proven. A scene that fails silently is not detected as a transition failure.
2. **Failed has no recovery path.** No recovery policy was authorized, so the runtime stays in Failed until restart. Architecture should decide on one.
3. **FadingOut publishes the frozen outgoing status/effects.** This follows the HUD acceptance matrix ("Blob remains current"). Ownership is released at Loading. Capabilities are withdrawn at acceptance, so no control reaches the deactivated scene.
4. **Static frame by retention, not copy.** The runtime FBO keeps the outgoing frame, which avoids an extra 1280×720 RGBA FBO (about 3.7 MB of VRAM).
5. **Scene commands** (NextMedia etc.) are rejected during FadingOut, Loading and Failed, and accepted in FadingIn (incoming is the active owner).
6. **`transitionProgress` is per-phase** (0..1 within each phase).
7. **No different-size scene pair exists.** The different-size path was exercised with the existing test hook plus the real activation path.
8. **Evidence instrumentation.** Test-only counters and accessors were added (`EntryCounters`, `PresentationCounters`, `readSceneFboPixelsForTesting`, `sceneFboTextureIdForTesting`). They are not read by production logic.
9. **Reconciliation report file.** The previous session's report could not be written (permissions at the time), so its link above does not resolve. Its findings were returned inline.

## 7. Newly discovered risks

1. **Intermittent Temporal warm-up warning (pre-existing).** Temporal sometimes logs `ofGLRenderer: draw(): texture is not allocated` for 1–4 frames right after starting. It appeared both after the first lazy setup in the switch harness and in the **unchanged** standalone Temporal lifecycle harness (1 line at frame 21; 0 on an earlier run today), so RT-003 did not introduce it. Most likely Temporal draws before its own decoder/history texture exists. During the first FadingIn the viewport briefly shows only Temporal's background layer. This is Temporal-internal; RT-002's "first valid Temporal render" criterion should account for it, or the Temporal domain should guard that draw.
2. **HUD scene-switch controls stay hidden.** `control.scene.{previous,next}.available` resolves "missing" because no `SceneManagerStatus` field exposes scene count (`HudRealFrameResolver.cpp:279-286`). Showing them needs a new public field (a contract change) or a HUD-side decision. Switching works through InputRouter, as RT-002 requires.
3. **The HUD media buttons ignore the transition phase.** NEXT/PREV MEDIA labels come from `VideoPlaybackStatus`, so they look enabled during Loading even though scene commands are rejected. This is cosmetic and HUD-owned.
4. **Screenshot width varies with the display.** All final-batch harness captures, including the untouched GL harness, are 1080 px wide, versus 1280 earlier today. This is environmental (window size at run time); RT-002 visual evidence should confirm 1280×720 captures.
5. **Lazy Temporal setup happens on the switch path.** About 1 Loading frame plus decoder spin-up. Fine on desktop; Pi cost unmeasured (PI-002).
6. **Exception-only failure detection** (Deviation 1) and **no Failed recovery** (Deviation 2).

## 8. Contract changes requested

```text
CONTRACT IMPACT: NONE
```

No frozen type, field, enum, command, lifecycle rule, FBO ownership, video or effect authority, or slot ID was changed. Optional future proposals (not requested now): a scene-availability signal for HUD scene controls (Risk 2); a Failed-recovery policy (Deviation 2).

## 9. Recommended next step

1. Domain manager reviews this report and records RT-003 as VERIFIED.
2. Architecture sets RT-002 to READY.
3. Execute the existing RT-002 20-cycle acceptance using `SceneSwitchHarness` as the starting point. RT-002 still needs to add: 20 cycles, a mid-`TFFragmentTransition` capture, live media-following across switches, 1280×720-verified captures, and a decision on the Temporal warm-up warning.

### Work Registry changes recommended

```markdown
## Work Registry Update
Work Item: RT-003 — SceneManager Stage B: in-process Blob + Temporal switching
Date: 2026-10-05
Owner: ExperienceRuntime & SceneManager
Previous Status: AUTHORIZED
Proposed Status: VERIFIED
Evidence:
- docs/reviews/rt-003-blob-temporal-scenemanager-switching-report.md
- docs/reviews/logs/rt003-*.log
Gate Changes: Implementation PASS; Desktop Verification PASS; Integration Verification PASS (3 cycles); Architecture Acceptance PENDING; Pi Verification NOT_STARTED; Documentation PASS; Source Control WORKTREE_ONLY
Source State: WORKTREE_ONLY
Dependencies Changed: none
Next Action: domain/Architecture review; commit
Architecture Review Required: YES
```

```markdown
## Work Registry Update
Work Item: RT-002 — Blob ↔ Temporal 20-cycle switching acceptance
Date: 2026-10-05
Previous Status: BLOCKED (by RT-003)
Proposed Status: READY
Blocked By: none
Next Gate: execute existing 20-cycle acceptance
Architecture Review Required: NO (execution already specified)
```

Also record TEMP-001 = ACCEPTED (per the RT-003 authorization) and the Blob + Temporal Stage B gate = CLEARED FOR THIS PAIR ONLY (DEC log). **Global Stage B collision resolution is not complete.**

---

### Session-close report

```text
WORK ITEM: RT-003
PREVIOUS STATUS: AUTHORIZED
NEW STATUS: VERIFIED (proposed)

WHAT CHANGED: SceneManager registry + in-process switching; ExperienceRuntime frame planning,
  static-frame retention, activation-time FBO reallocation; Blob effects nullopt without FakeScene;
  new controller tests + switch harness.
WHAT WAS PROVEN: 3 real Blob->Temporal->Blob cycles (5,351/5,351); ordering, ownership, suppression,
  capability/status/HUD counts, static frame, FBO realloc, GL baseline; all existing suites green
  on a clean build; no frozen-contract diff.
WHAT WAS NOT PROVEN: 20-cycle acceptance; live media-following across switches; activation-failure
  path with a real scene; any Pi behaviour.

NEW EVIDENCE: docs/reviews/logs/rt003-*.log; bin/data/captures/scene_switch_*.png (untracked)
NEW RISKS: §7 items 1-6
NEW BLOCKERS: none

NEXT ACTION: review; execute RT-002
NEXT GATE: RT-003 acceptance -> RT-002 execution

REGISTRY UPDATE REQUIRED: YES
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: YES (record pair-only Stage B ruling if not already logged)
ARCHITECTURE HANDOFF REQUIRED: YES
```

**RT-003 VERIFIED — RT-002 READY**
