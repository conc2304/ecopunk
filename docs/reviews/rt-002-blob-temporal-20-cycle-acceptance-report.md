# RT-002 — Blob ↔ Temporal 20-Cycle Production Switching Acceptance: Completion Report

**Date:** 2026-10-05
**Work item:** RT-002 (first real two-scene milestone)
**Recipient:** ExperienceRuntime & SceneManager domain manager → Architecture & Program Coordination; **Temporal Fields Scene Migration** (defect handoff, §7 of this report)
**Produced by:** Coding agent (acceptance/verification session — no production code changed)
**Repository:** `blueprint-emergence-updates` @ `59c24f4` (RT-003, committed, **not pushed**) + RT-002 harness in the working tree
**Authoritative inputs:** RT-002 acceptance prompt; [RT-003 completion report](rt-003-blob-temporal-scenemanager-switching-report.md); [Scene/HUD Contract v1](../shared-project-docs/Scene-HUD-Contract-v1.md); [Blob → Temporal → Blob HUD Acceptance Matrix](../shared-project-docs/blob-temporal-blob-hud-acceptance-matrix.md); [Architecture Authorization — Temporal Scene #2](../shared-project-docs/Architecture-Authorization-Temporal-Production-Scene-2.md); [Governance](../shared-project-docs/01-architecture-governance.md); [Handoff Protocol](../shared-project-docs/02-cross-domain-handoff-protocol.md); [Work Registry](../shared-project-docs/03-work-registry.md); OFF-REPO `~/Downloads/temporal-production-migration-two-scene-runtime-acceptance-requirements.md`, `~/Downloads/blob-temporal-20-cycle-production-switching-acceptance.md`

> The "RT-003 ExperienceRuntime domain acceptance review" and the "Architecture Ruling — Blob + Temporal Stage B / RT-003 Authorization" are not present in the repository or `~/Downloads`. The decisions quoted in the RT-002 prompt (RT-003 ACCEPTED, RT-002 READY, pair-only Stage B gate, in-process switching, no contract change) were treated as authoritative.

---

## 1. Summary

The full acceptance ran on the real production stack, with no stand-ins:
- `BlobProductionScene` + `TemporalProductionScene`, `SceneManager`, `ExperienceRuntime`
- the production HUD (`HudCompositorBridge` → `HudWireframeRenderer`)
- `VideoPlaybackService`, and Temporal's `TimeOffsetPlaybackAdapter` / `TimeOffsetVideoBuffer` path
- `TFEffectPicker` as the canonical effect source.

The run covered **20 complete Blob → Temporal → Blob cycles** plus final validation and clean shutdown. Every switch went through the production route: key → `InputRouter` → `RuntimeCommand::NextScene`/`PreviousScene` → `ExperienceRuntime` → `SceneManager`.

**Final evidence run (run 4, clean Release build): 80,600 checks, 20 failures.** All 20 failures are the same requirement, one per Temporal activation. Everything else passed:

- All switching, lifecycle, ownership, capability, status/HUD-count and effect-snapshot requirements.
- Static-frame transitions, GL/FBO isolation, resource bounds and clean shutdown.
- The mandatory live media change and the mid-`TFFragmentTransition` capture.

**The 20 failures — a Temporal-domain defect.** Whenever Temporal's history buffer is empty, which happens on every activation and after every canonical media change, Temporal still draws its GPU playhead textures. Those textures hold the last frames uploaded **before** the history was cleared.
- **Reactivation:** in all 19 Temporal reactivations, playhead 0 was byte-identical to the previous activation's last frame, and it stayed on screen for 4–16 frames.
- **Changed media:** in 3 of those reactivations, canonical media had changed in between, so old media was shown as current Temporal output.
- **First activation:** the same code path draws never-uploaded textures, producing the bounded `texture is not allocated` warning.

This violates §7 ("old media/history does not remain visible as current state") and §12 ("Temporal history uses current canonical media"). Following §8's decision rule it is reported as a **TEMPORAL DOMAIN DEFECT** and was **not patched**.

The RT-003 switching mechanism itself met every requirement. No frozen contract changed (§8).

## 2. Files inspected

- **`sketches/experience_runtime/src`:** `SceneManager.*`, `SceneSwitchController.h`, `ExperienceRuntime.*`, `ofApp.*`, `HudCompositorBridge.h`, `BlobProductionScene.*`, `TemporalProductionScene.*`, `SceneSwitchHarness.*`
- **`sketches/temporal-fields/src`:** `TemporalSceneCore.{h,cpp}`, `TFComposition.{h,cpp}`, `TFFragmentTransition.h`, `TFEffectPicker.cpp`, `TFSettings.h`
- **`shared/src`:**
  - `TimeOffsetVideoBuffer.{h,cpp}` (root cause, `getPlayheadTexture()` lines 209–226)
  - `video-playback/adapters/TimeOffsetPlaybackAdapter.h`, `video-playback/VideoPlaybackStatus.h`
  - `video-effects/knowledge/EffectActivityStatus.h`
  - `hud-compositor/HudWireframeRenderer.h`, `MediaViewportMesh.h`
  - `scene/SceneSemanticTypes.h`
- **`assets/shared/video-effects/knowledge/effect-knowledge-pack.json`** (authored presets)
- **openFrameworks:** `libs/openFrameworks/utils/ofLog.{h,cpp}` (logger channel), `gl/ofGLRenderer.cpp:337-363` (warning source)
- **Docs:** `docs/reviews/blob-post-acceptance-hardening-report.md` (accepted Blob scratch-FBO state)

## 3. Files changed

| File | Change |
|---|---|
| `sketches/experience_runtime/src/TwoSceneAcceptanceHarness.{h,cpp}` | **New**, test/acceptance tooling only (`EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE`). Adds a log tap that wraps the console channel (counts `texture is not allocated`, records natural preset applications), per-frame invariants, transition and lifecycle checks, media-follow tracking, mid-transition capture, stale-playhead measurement, per-cycle resource rows, and shutdown checks |
| `sketches/experience_runtime/src/ofApp.{h,cpp}` | Wires the harness at lowest priority; always starts in Blob |
| `docs/reviews/logs/rt002-*.log`, `docs/reviews/rt002-captures/*.png` | Evidence |
| this report | — |

**No production, shared, scene or contract file changed.** `git diff 59c24f4` is empty for `shared/`, `SceneManager.*`, `SceneSwitchController.h`, `ExperienceRuntime.*`, both production scene classes, `sketches/temporal-fields`, and `sketches/blob-region-prototype`.

## 4. Tests/builds run

| Command | Result |
|---|---|
| `make clean && make Release -j8` (experience_runtime) | **exit 0, 0 errors**, 24 warnings (none in harness files), 3m31s — [`logs/rt002-clean-build-summary-2026-10-05.log`](logs/rt002-clean-build-summary-2026-10-05.log) |
| ExperienceRuntime unit suites (`make -f Makefile.tests test`) | 78/78 · 42/42 · 68/68 · 34/34 · **switch controller 127/127** |
| Shared Video tests | 38/38 · 113/113 |
| Shared Effects tests | 93/93 |
| HUD tests (`test-all`) | 713/713 · 140/140 · 357/357 · 14/14 |
| `EXPERIENCE_RUNTIME_GL_HARNESS=1` | ALL CHECKS PASSED, 0 warnings |
| `EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS=1` | 895/895, 0 warnings |
| `EXPERIENCE_RUNTIME_TEMPORAL_LIFECYCLE_HARNESS=1` | 295/295 PASS, 0 warnings |
| `EXPERIENCE_RUNTIME_SWITCH_HARNESS=1` (RT-003) | 5,351/5,351 PASS; 4 `texture is not allocated` lines on first Temporal activation |
| **`EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE=1` — run 4 (final evidence)** | **20/20 cycles, 80,600 checks, 20 failures (all: stale playhead, Temporal domain)** |
| Acceptance runs 1–3 (supporting; same binary family) | See §6 — harness corrections and instrumentation additions between runs |

Unit/regression logs: [`logs/rt002-unit-suites-2026-10-05.log`](logs/rt002-unit-suites-2026-10-05.log), `logs/rt002-regression-{gl,blob-lifecycle,temporal-lifecycle,switch}-2026-10-05.log`. Acceptance: [`logs/rt002-acceptance-run4-final-2026-10-05.log`](logs/rt002-acceptance-run4-final-2026-10-05.log) (runs 1–3 alongside).

## 5. Results

### Required acceptance table

| Requirement | Result | Evidence (run 4 unless noted) |
|---|---|---|
| Two real scenes registered | **PASS** | registry `[blob-region-prototype, temporal-fields]`; displayNames "Blob Region Prototype" / "Temporal Fields"; both native 1280×720 |
| Deterministic Blob startup | **PASS** | startup active=Blob, Idle; Blob setup=1, activate=1, capabilityQueries=1; Temporal not set up |
| NextScene | **PASS** | 20/20 via `]` → InputRouter |
| PreviousScene | **PASS** | 20/20 via `[` → InputRouter |
| 20 complete cycles | **PASS** | "complete cycles: 20/20" |
| ≥60 active frames/scene/activation | **PASS** | minimum active frames per activation = 73 (12 FadingIn + 1 + 60 dwell); startup Blob 60 |
| Lifecycle ordering | **PASS** | every leg: deactivate at acceptance → FadingOut 12 → Loading 1 (setup if first use) → activate → FadingIn 12 → Idle |
| No inactive updates | **PASS** | outgoing update/draw/status/effect-pull counters unchanged through transition + dwell, every leg |
| Capability replacement | **PASS** | queries == successful activations (Blob 21/21, Temporal 20/20); outgoing withdrawn at acceptance; incoming = scene's own; Temporal advertises no Regenerate/Reset |
| One SceneHudStatus pull / live frame | **PASS** | 5,627 live frames = 5,627 pulls; 520 static frames = 0 pulls |
| One HudFrameData / presented frame | **PASS** | 6,147 frames = 6,147 assemblies |
| One HUD draw / presented frame | **PASS** | 6,147 runtime HUD draws = 6,147 bridge `drawCallCount()` |
| Blob effects missing | **PASS** | 0 Blob frames with effects; no FakeScene source |
| Temporal present-empty effects | **PASS** | 4,048 frames present with `slots.empty()` (first @ frame 74) |
| Temporal present-active effects | **PASS** | 49 frames present with slots (`threshold`; first @ frame 6003); runs 1–3: ascii_solarpunk, desaturate, recolor, solarize, bioluminescence, chromatic_aberration, heatmap_recolor, hue_rotate, water_refraction |
| No stale Temporal effects on Blob | **PASS** | every first Blob frame `effects == nullopt`; per-frame invariant over all frames |
| Semantic/profile clearing | **PASS** | per frame: status sceneId == activeSceneId == HUD `currentScene()`; semantic IDs carry only the active scene's prefix; Loading publishes none |
| Canonical video continuity | **PASS** | `HudFrameData.video` from `VideoPlaybackService` throughout; Temporal adapter followed canonical media with max 1-frame lag (8 canonical changes) |
| Mandatory live media change | **PASS** (mechanics) | frame 117: `media.auto.7fb8cbf1 → media.auto.5c185454`; adapter followed @117; history cleared @117; refilled (≥10 frames) @134 from the new file |
| Temporal history resync | **FAIL** | CPU history resyncs correctly, **but stale GPU playhead textures are presented while history is empty** (pre-deactivation / pre-change content; old media in 3 reactivations) — §7 of this report |
| Static-frame transitions | **PASS** | retained outgoing frame byte-identical (hash + texture id) at FadingOut 1/12 and Loading; incoming frame replaces it at FadingIn; 520 static frames total |
| GL/FBO isolation | **PASS** | 0 GL-failure frames across 6,147: scissor/stencil off, program 0, no bound tex2D, fbo 0, blend 1/770/771, viewport, model-view matrix and style == baseline |
| Mid-TFFragmentTransition capture | **PASS** | frame 4330, `scene.temporal.state.transitioning` — §5 below |
| Temporal warning instrumented | **PASS** | table in §5 |
| Bounded resources | **PASS** | table in §5 |
| Clean shutdown | **PASS** (runtime) / see Risk 2 | both scenes deactivated + shut down once; commands/switches rejected after; Temporal resources released only at destruction |
| Authored preset natural observation | **NOT OBSERVED — NON-BLOCKING** | 0 `applying canonical eligible preset` log lines in any run |
| Frozen-contract drift | **NONE** | §3 |
| Durable source/artifact state | **WORKTREE_ONLY / COMMITTED (unpushed)** | §5 durability table |

### Lifecycle / transition trace (every one of the 40 legs)

```text
NextScene/PreviousScene accepted (Idle) → outgoing.deactivate() → outgoing capabilities withdrawn
→ FadingOut ×12 (static retained frame, frozen outgoing status/effects, owner=outgoing, pending=incoming)
→ Loading ×1 (owner released; neutral status; incoming setup() only on first use)
→ incoming.activate() → owner=incoming → capabilities queried once
→ FadingIn ×12 (live incoming update / 1 status pull / draw) → Idle
```

Switch commands were sent deliberately during FadingOut, Loading and FadingIn on every leg (120 attempts in total). Every one was rejected with no queued switch.

### Counts (run 4, final)

| Scene | setup | activations | deactivations | shutdown | capability queries | status pulls | updates | draws | effect pulls |
|---|---|---|---|---|---|---|---|---|---|
| Blob | 1 | 21 | 21 (20 at switches + 1 at shutdown) | 1 | 21 | 1,530 | 1,530 | 1,530 | 0 |
| Temporal | 1 | 20 | 20 | 1 | 20 | 4,097 | 4,097 | 4,097 | 4,097 |

Runtime: 6,147 frames = 5,627 live (1,530 Blob + 4,097 Temporal = total status pulls) + 520 static (40 legs × 13). `HudFrameData` assemblies, runtime HUD draws and bridge HUD draws are all 6,147.

### Effect-transition observations
- **Blob → Temporal (20/20):** missing → present on the first Temporal frame.
- **Temporal → Blob (20/20):** present (frozen during FadingOut) → missing from Loading onward and on the first Blob frame. Blob never published effects.
- Temporal present-empty and present-active were both observed.
- `SceneHudStatus::activeEffects` was never used.

### Live Shared Video change evidence

| Event | Frame | Detail |
|---|---|---|
| Intentional NextMedia during Temporal dwell (InputRouter `n` → `SceneCommand::NextMedia` → Temporal → `VideoPlaybackService::next()`) | issued 116 | from `media.auto.7fb8cbf1`, history full |
| Canonical selection changed (`HudFrameData.video.mediaId`) | 117 | → `media.auto.5c185454` |
| Temporal adapter followed (`loadedMediaId`, reloadCount+1) | 117 | — |
| History cleared | 117 | — |
| History refilled (≥10 frames) from the new file | 134 | buffer file changed accordingly |
| Intentional NextMedia during Blob dwell | 375 | `media.auto.5c185454 → media.auto.e20beffd` |
| Next Temporal activation loads that media | 418 | adapter = canonical = `media.auto.e20beffd` |
| Automatic canonical advances (30 s hold) | 1380, 2281, 3182, 4083, 4985, 5886 | Temporal followed each one; maximum adapter/canonical mismatch 1 frame |

Temporal never selected media itself: every adapter reload coincided with a canonical change or an activation.

### Temporal history resync evidence
The CPU history buffer resyncs exactly as designed:
- cleared on every activation (`invalidateForReactivation`) and on every canonical media change;
- refilled only from the newly selected file.

**But** during the 4–16 empty-history frames, the presented playhead textures still hold earlier content (next table). Result: **FAIL**.

### Temporal unallocated-texture warning + stale-playhead table (run 4)

| Activation | Warning? | Warning frames (count) | First valid frame (live idx) | Stale playhead frames | Reused previous activation's last frame | Canonical media changed since previous | Stale Blob visible? | HUD valid? | GL/FBO valid? |
|---|---|---|---|---|---|---|---|---|---|
| 1 (first ever) | **Yes** | 1 frame (7 warnings), live idx 0; history 0, raw video allocated | 10 | 8 | n/a | n/a | No | Yes | Yes |
| 2 | No | 0 | 7 | 7 | **Yes** | No | No | Yes | Yes |
| 3 | No | 0 | 8 | 8 | **Yes** | **Yes — old media shown** | No | Yes | Yes |
| 4–8 | No | 0 | 4 | 4 each | **Yes** | No | No | Yes | Yes |
| 9 | No | 0 | 4 | 4 | **Yes** | **Yes — old media shown** | No | Yes | Yes |
| 10 (extended, 2,710 frames, 6 media changes) | No | 0 | 4 | 16 (activation + after media changes) | **Yes** | No | No | Yes | Yes |
| 11–13, 15–19 | No | 0 | 4 | 4 each | **Yes** | No | No | Yes | Yes |
| 14 | No | 0 | 4 | 8 | **Yes** | No | No | Yes | Yes |
| 20 | No | 0 | 4 | 4 | **Yes** | **Yes — old media shown** | No | Yes | Yes |

"First valid frame" is the first live Temporal frame with history > 0 and no warning.

**Warning across runs:** 1 frame / 3 warnings (run 1), 1 / 2 (run 2), 0 (run 3), 1 / 7 (run 4). It occurs only on the first-ever Temporal activation. It is bounded but not deterministic.

**Decision (§8):**
- The warning by itself meets most of the harmless-warm-up criteria: bounded, no stale Blob, HUD and GL/FBO valid.
- The instrumentation proves the root cause is an **invalid draw**: `TimeOffsetVideoBuffer::getPlayheadTexture()` returns the existing texture unchanged when history is empty, and Temporal draws it.
- The same path presents **stale pre-deactivation / pre-media-change content** on every reactivation and after every media change.

→ **TEMPORAL DOMAIN DEFECT** (not patched).

### Semantic/profile stale-state results
- On the first incoming frame of every leg: status sceneId, `activeSceneId` and HUD `currentScene()` were all the incoming scene.
- On the first incoming frame carrying a payload:
  - state and metric IDs were exclusively the incoming scene's;
  - every published metric carried a real value (`value`, `normalizedValue` or `valueId`);
  - unsupported values were absent.
- Blob payload: `scene.blob.state.*` with `region_count`, `fragment_count`, `occupied_area` (normalized).
- No Blob or Temporal label, metric, effect, control or profile survived a handoff. The per-frame invariant held over all 6,147 frames.

### Mid-TFFragmentTransition evidence

| Field | Value |
|---|---|
| Runtime frame | 4330 (cycle 10 Temporal activation, extended to reach a natural pattern change) |
| Active scene | `temporal-fields` |
| Pattern | BSP (0) → BLOB_GRID (1); semantic state `scene.temporal.state.transitioning` |
| Transition progress | in-transition frame 8 of 9 (`PATTERN_TRANSITION` lasted 9 frames ≈ 0.30 s; elapsed 0.267 s) |
| Scene FBO size | 1280×720 |
| Window viewport | 1280×720 |
| Framebuffer | 0 |
| HUD profile | `temporal-fields`, 19 bindings; HUD geometry `54v [25.6,115.2]-[793.6,619.2]`, identical to baseline |
| GL baseline after the full frame | scissor 0, stencil 0, program 0, tex2D 0, fbo 0, blend 1/770/771, viewport/matrix/style = baseline |
| Production HUD draws | 4330 (= frame number) |
| Next frame | valid (texture, GL, HUD geometry) |

Screenshots: [`rt002-captures/rt002_mid_tffragmenttransition.png`](rt002-captures/rt002_mid_tffragmenttransition.png) and [`rt002-captures/rt002_mid_tffragmenttransition_scene_only.png`](rt002-captures/rt002_mid_tffragmenttransition_scene_only.png). The full capture shows erosion-style fragment blocks inside the canonical viewport, the HUD intact, Temporal state TRANSITIONING, and manager phase Idle.

Note: the production `TemporalSceneCore` never calls `setTransitionParams()`, so it uses `TFComposition` defaults, and it restarts the 90 s pattern cycle on every activation. A pattern transition therefore needs one continuous Temporal activation of more than 90 s. The cycle-10 dwell was extended to 2,697 frames for this. Nothing was forced.

### GL/FBO results
- 0 GL-failure frames in 6,147 (fixed state plus warmup-baseline blend/viewport/model-view/style), checked after every full frame. Scene FBO: 1 allocation, 0 reallocations (same-size scenes).
- **After Temporal → Blob:** Blob scratch state unchanged by the Temporal activation, background scratch unallocated (accepted Blob state), Blob render live.
- **After Blob → Temporal:** history bound to the current canonical media at activation (20/20), playheads = 6.

*Limitation:* the GL baseline is checked after the full frame (scene + HUD), not between scene and HUD; `SceneRenderGuard` provides the post-scene restore.

### Per-cycle resource table (run 4, sampled at the end of each Blob return)

| Cycle | RSS MB | Net live allocs | Scene FBO allocs / switch reallocs | Blob fragment scratch | Temporal history / cap | Playheads | Adapter reloads (cumulative) |
|---|---|---|---|---|---|---|---|
| 1 | 390 | 3,382 | 1 / 0 | 188×100 | 22/72 | 6 | 2 |
| 2 | 406 | 3,374 | 1 / 0 | 188×100 | 66/72 | 6 | 3 |
| 3 | 410 | 3,398 | 1 / 0 | 188×132 | 65/72 | 6 | 4 |
| 4 | 412 | 3,465 | 1 / 0 | 188×132 | 69/72 | 6 | 5 |
| 5 | 413 | 3,444 | 1 / 0 | 188×132 | 69/72 | 6 | 6 |
| 6 | 415 | 3,430 | 1 / 0 | 188×132 | 69/72 | 6 | 7 |
| 7 | 410 | 3,449 | 1 / 0 | 576×448 | 69/72 | 6 | 8 |
| 8 | 416 | 3,529 | 1 / 0 | 1160×660 | 69/72 | 6 | 9 |
| 9 | 405 | 3,490 | 1 / 0 | 1160×660 | 69/72 | 6 | 10 |
| 10 | 432 | 3,732 | 1 / 0 | 1160×660 | 72/72 | 6 | 14 |
| 11 | 416 | 3,523 | 1 / 0 | 1160×660 | 69/72 | 6 | 15 |
| 12 | 416 | 3,518 | 1 / 0 | 1160×660 | 69/72 | 6 | 16 |
| 13 | 416 | 3,581 | 1 / 0 | 1160×660 | 69/72 | 6 | 17 |
| 14 | 391 | 3,560 | 1 / 0 | 1160×660 | 30/72 | 6 | 19 |
| 15 | 415 | 3,593 | 1 / 0 | 1160×660 | 69/72 | 6 | 20 |
| 16 | 420 | 3,647 | 1 / 0 | 1160×660 | 69/72 | 6 | 21 |
| 17 | 420 | 3,600 | 1 / 0 | 1160×660 | 69/72 | 6 | 22 |
| 18 | 420 | 3,591 | 1 / 0 | 1160×660 | 69/72 | 6 | 23 |
| 19 | 418 | 3,596 | 1 / 0 | 1160×660 | 69/72 | 6 | 24 |
| 20 | 420 | 3,600 | 1 / 0 | 1160×660 | 69/72 | 6 | 25 |

Reading the table:
- RSS plateaus at 405–420 MB after warm-up. Cycle 10's 432 MB is the 90 s Temporal activation (full 72-frame history), which returned to 416 by cycle 11.
- Net live allocations plateau at about 3,500–3,650, with no cycle-proportional growth.
- There is one adapter/decoder instance (reload count +1 per activation or media change, not multiplied), a fixed 6 playheads, history capped at 72, and one scene FBO.
- Blob's fragment scratch FBO grows to an on-demand high-water mark (1160×660) and stays there. This is bounded and explained by Blob's on-demand policy.
- SceneManager registers no listeners or timers. No listener/timer counts are exposed by the scenes (not available).
- No Pi conclusions are drawn.

### Clean shutdown evidence
- Final validation (frame 6147): Blob active, Idle, status Ready, live render, effects missing, canonical video health Ready, HUD profile Blob, GL baseline. Screenshot: [`rt002-captures/rt002_final_blob.png`](rt002-captures/rt002_final_blob.png).
- `ExperienceRuntime::exit()`:
  - Blob deactivations 21 = activations 21; Temporal 20 = 20;
  - both scenes shut down exactly once;
  - post-shutdown scene commands and switches rejected;
  - `beginFrame()` reports no live scene;
  - no warnings during exit.
- **Observation (not a switching defect):** after `shutdown()`, Temporal's history (69 frames) and decoder (`hasMedia=1`) are still held, and the runtime scene FBO is still allocated. These are released only by destructors at process teardown, because `TemporalSceneCore::shutdown()` releases nothing ("no separate resource release needed"). Contract §6 says `shutdown()` is "the only step that must actually free FBOs/textures/timers for good" (Risk 2).

### Authored preset observation
**NOT OBSERVED — NON-BLOCKING.** The whitelist holds `preset.dither.20260809_172713_1` and `preset.ascii_solarpunk.20260807_205820_1`. No `applying canonical eligible preset` line appeared in any of the four runs, though the `ascii_solarpunk` effect was selected naturally in run 1. Selector policy was not modified.

### Source / artifact durability

| Artifact group | SOURCE STATE |
|---|---|
| RT-003 source + tests | **COMMITTED** (`59c24f4`), **not pushed** (branch ahead of origin by 1) |
| RT-003 report + logs; Work Registry / governance docs | **COMMITTED** (`59c24f4`), not pushed |
| Architecture RT-003 ruling; RT-003 domain acceptance review | **UNTRACKED / NOT FOUND** — not in repo or `~/Downloads` |
| RT-002 harness source (`TwoSceneAcceptanceHarness.*`, `ofApp.*`) | **WORKTREE_ONLY** |
| RT-002 logs (`docs/reviews/logs/rt002-*`) | **WORKTREE_ONLY** |
| RT-002 screenshots — selected copies in `docs/reviews/rt002-captures/` | **WORKTREE_ONLY** |
| RT-002 screenshots — full set in `bin/data/captures/` | **UNTRACKED** (git-ignored) |
| This report | **WORKTREE_ONLY** |
| Governing specs (20-cycle spec; two-scene requirements) | **UNTRACKED** (`~/Downloads` only) |
| HUD acceptance matrix; Temporal authorization | **COMMITTED** (`d25cc13`) |

Nothing was committed or pushed in this session.

## 6. Deviations

1. **A new acceptance harness was required.** The RT-003 switch harness lacks the RT-002 observations (log tap, media follow, mid-transition, per-cycle resources, stale-playhead measurement). It is test-only, and no production code changed.
2. **Four runs, with harness corrections between them.** Every correction is visible in the logs:
   - **Run 1** (97,516 checks, 41 failures): 40 were two mis-specified harness checks.
     - "Blob background scratch > 0" was wrong; 0×0 is the accepted state.
     - "Every metric has `value`/`valueId`" was wrong; Blob's `occupied_area` uses `normalizedValue`.
     - The 41st was no mid-transition captured within a 4,000-frame cap. Only one natural pattern change fell inside the cap, and it was not observed in-transition.
   - **Run 2** (80,580 checks, **0 failures**): checks corrected; cap raised to about 4 natural transitions. Captured the in-transition frame.
   - **Run 3** (150,832 checks, 1 failure): stale-playhead instrumentation added (observation only). The failure was a third mis-specification: Blob's fragment scratch FBO is legitimately 0×0 until first needed. The check was corrected to "Blob scratch state unchanged across the Temporal activation".
   - **Run 4** (final, clean build; 80,600 checks, 20 failures): the stale-playhead measurement became an acceptance check, and it fails on all 20 Temporal activations.
3. **Mid-transition progress.** The capture is in-transition frame 8 of a 9-frame transition (late, not the midpoint). My progress estimate assumed a 0.8 s transition; 0.30 s was observed.
4. **GL baseline timing.** Checked after the full frame, not between scene draw and HUD draw.
5. **Durations.** The cycle-10 Temporal activation was extended to 2,697 frames, which the "≥60" minimum allows, so a natural `TFFragmentTransition` could occur. The cycle-1 Temporal dwell was extended until the media refill was observed.
6. **Missing governing artifacts.** The RT-003 domain acceptance review and the Architecture RT-003 ruling were not found.

## 7. Newly discovered risks

1. **TEMPORAL DOMAIN DEFECT — stale playhead textures presented while history is empty (blocking for RT-002 §7/§12).**
   - **Root cause:** `shared/src/TimeOffsetVideoBuffer.cpp:209-226`. When `offsetToHistoryIndex()` < 0 (empty history), `getPlayheadTexture()` skips the upload and returns `ph.texture` as-is. That is either never allocated, which triggers the warning, or holding pre-clear content.
   - **Why it shows:** history is cleared on reactivation (`TimeOffsetPlaybackAdapter::invalidateForReactivation()`, TEMP-001) and on media change (`loadExplicit`), but the playhead textures are not.
   - **Effect:** 4–16 frames (≈0.13–0.5 s at 30 fps) of pre-deactivation content after every reactivation, including **old media** when canonical media changed in between (3/19 reactivations), and similar windows after live media changes.
   - **Ownership:** Temporal / Shared Video temporal-history boundary (DEC-014). A fix is likely Temporal-local: skip or hold the playhead draw until history has a frame, or invalidate the playhead textures on clear. It must not change frozen contracts.
   - **Not patched, per instruction.**
2. **Temporal `shutdown()` releases nothing.** History and decoder stay alive until destruction, contrary to contract §6. The runtime scene FBO is also not released in `exit()`. Neither is a switching defect; both are Temporal/ExperienceRuntime hygiene items for Pi memory budgets (PI-003).
3. **The first-activation `texture is not allocated` warning is not deterministic** (0–7 warnings over 1 frame across runs). It is the same root cause as Risk 1.
4. **Production Temporal pattern cadence.** Patterns change only after 90 s of continuous Temporal activation (restarted on each activation). Under any switching cadence shorter than 90 s, production Temporal never shows a pattern transition. This is a product observation for the Temporal domain.
5. **The authored preset was not observed naturally in about 27 minutes of Temporal time across runs.** Non-blocking (FX-004).
6. **Durability.** RT-003 is committed but not pushed. RT-002 artifacts and the governing specs are working-tree only or off-repo.

## 8. Contract changes requested

```text
CONTRACT IMPACT: NONE
```

No stop condition was hit. The defect fix in Risk 1 is expected to be Temporal-internal; if the Temporal domain finds it needs a `TimeOffsetVideoBuffer` API change, it goes through DEC-014's boundary as an additive shared-API proposal.

## 9. Recommended next step

1. **Temporal domain:** a narrow fix for Risk 1 (and optionally the shutdown hygiene in Risk 2), in a separately authorized session, verified by this harness's existing `§7/§12 no stale playhead texture` check, which should then read 0 failures.
2. **Re-run RT-002** unchanged (`EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE=1`) after the fix. Every other requirement already passes.
3. **Durability:** push `59c24f4`; commit the RT-002 harness, logs, captures and this report; bring the RT-003 ruling, the domain review and the two governing specs into `docs/shared-project-docs/` (ARCH-003).

### Work Registry transition recommendation

```markdown
## Work Registry Update
Work Item: RT-002 — Blob ↔ Temporal 20-cycle switching acceptance
Date: 2026-10-05
Owner: ExperienceRuntime & SceneManager
Previous Status: READY
Proposed Status: BLOCKED
Evidence:
- docs/reviews/rt-002-blob-temporal-20-cycle-acceptance-report.md
- docs/reviews/logs/rt002-acceptance-run4-final-2026-10-05.log (80,600 checks; 20 failures, all stale-playhead)
Gate Changes: Implementation PASS (harness); Desktop Verification FAIL (§7/§12 only); Integration Verification PASS for all switching requirements; Architecture Acceptance NOT_STARTED; Pi NOT_STARTED; Documentation PASS; Source Control WORKTREE_ONLY
Source State: WORKTREE_ONLY (RT-002); RT-003 COMMITTED (unpushed)
Dependencies Changed:
- + TEMP-004 [READY] (new): Temporal stale playhead textures while history empty
Next Action: Temporal domain fix (TEMP-004), then re-run RT-002 unchanged
Architecture Review Required: NO (no contract change); Temporal domain handoff YES
```

```markdown
## Work Registry Update
Work Item: TEMP-004 — Temporal presents stale playhead textures while history is empty (NEW)
Proposed Status: READY
Owner: Temporal Fields Scene Migration (DEC-014 boundary with Shared Video)
Evidence: this report §5 warning/stale table, §7 Risk 1; TimeOffsetVideoBuffer.cpp:209-226
Next Action: authorize a narrow Temporal fix; verify with RT-002 harness
Architecture Review Required: NO unless a shared TimeOffsetVideoBuffer API change is proposed
```

Also record RT-003 = ACCEPTED (per the prompt) and RT-003 Source State = COMMITTED (unpushed).

---

### Session-close report

```text
WORK ITEM: RT-002 (new: TEMP-004)
PREVIOUS STATUS: READY
NEW STATUS: BLOCKED (proposed) — by TEMP-004

WHAT CHANGED: Added TwoSceneAcceptanceHarness (test-only) + ofApp wiring; evidence logs/captures/report.
WHAT WAS PROVEN: 20 real cycles; every switching, lifecycle, ownership, capability, status/HUD count,
  effect-snapshot, static-frame, GL/FBO, media-follow, resource and shutdown requirement; mid-TFFragmentTransition
  captured under ExperienceRuntime; no frozen-contract drift.
WHAT WAS NOT PROVEN: absence of stale Temporal imagery (disproven — defect); natural authored-preset application;
  explicit resource release at shutdown(); any Pi behaviour.

NEW EVIDENCE: docs/reviews/logs/rt002-*.log; docs/reviews/rt002-captures/*.png
NEW RISKS: §7 Risks 1-6
NEW BLOCKERS: TEMP-004 (Temporal stale playhead textures)

NEXT ACTION: Temporal domain handoff + narrow fix; re-run RT-002
NEXT GATE: RT-002 re-run PASS -> Architecture first-two-scene acceptance

REGISTRY UPDATE REQUIRED: YES
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: YES (milestone not ready) + Temporal domain handoff
```

**RT-002 FAIL — Temporal history resync (§7/§12): Temporal presents stale pre-deactivation/pre-media-change playhead textures (old canonical media in 3/19 reactivations) while history is empty — TEMPORAL DOMAIN DEFECT; all switching requirements pass**
