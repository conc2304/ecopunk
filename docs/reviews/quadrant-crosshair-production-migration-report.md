# Quadrant Crosshair — Production Scene Migration: Handoff Report

**Date:** 2026-10-13 · **Base:** `44044c1` (`blueprint-emergence-updates`) · **Worktree:** `apps/myApps/EcopunkVideoCollage-quadrant` (detached; isolated build and capture paths) · **Git:** no stage, commit or push performed.

**Result: QUADRANT PRODUCTION SCENE RUNNING** (desktop). The ring is now [Blob, Temporal, Blueprint, Quadrant]. No shared contract changed. Pi: not tested, so not passed.

## 1. Source findings (live source, `44044c1`)

| | Finding |
|---|---|
| Entrypoint | `ofApp` held the whole frame pipeline (video sampling → motion extraction → crosshair → expansion → triggers → grid → quadrants → HUD layer) |
| Core artwork owner | `QuadrantManager` (4 × `Quadrant`: per-quadrant shader deck, two shader slots, ping-pong erosion FBOs, Ridgeline) + `ExpansionDirector` + `MotionExtraction` |
| Crosshair owner | Quadrant's own `CrosshairSystem` fork. Not in `shared/src`; differs from Fragment's copy; one consumer here. **Decision C:** kept scene-owned (namespaced) |
| Media owner | Local `VideoSystem` (folder scan + shuffle). **Classification:** ordinary Shared Video candidate. The canonical service already exposes `currentTexture()`/`currentPixels()`/`isFrameNew()`; no specialized decoder needed |
| Effects owner | Per-quadrant shader decks over a local `ShaderLibrary` fork, with fixed defaults already sourced from the shared catalog. **Classification:** scene-local visual implementation, not a competing Shared Effects selector |
| Local HUD/debug | `HudManager` = artwork layer (contours, hex grid, node network, reticles, flow field, crosshair scanner, glitch tears) + information layer (per-quadrant telemetry cards, media status light, log scroll, slot-name legend). `DebugMode` (`D`), hud_overlay takeover (`O`), dev keys = developer-only |
| Name collisions | `LFOBank`, `TriggerBus`, `ShaderLibrary`, `MotionExtraction`, `GridState` share global names, with different APIs, with the shared classes the runtime links |
| Host-global state | `ofGetWidth()`/`ofGetHeight()` in 8 production files; `ofSetFrameRate`/`ofHideCursor`/fullscreen only in `ofApp` |
| Asset risks | Every production shader has a canonical source in `shared/assets/video-effects/`. Quadrant's `erosion.glsl` and `motion_effect.glsl` are byte-identical to `erosion_accumulation`/`motion_effect`. `motion_accum`/`motion_extract` are hand-supplied runtime paths (shared with Blueprint) |
| GL/FBO risks | `glScissor` used the window-framebuffer y-flip (mirrored inside the scene FBO). Erosion FBO passes ran *inside* draw (nested FBO). 8 window-sized erosion FBOs |

## 2. Migration result

**QUADRANT PRODUCTION SCENE RUNNING.** The real artwork renders through the runtime's scene FBO under the one production HUD, switches cleanly with Blob, Temporal and Blueprint in both directions, reactivates fresh, and publishes honest §18.5 semantics.

**Defects found and fixed (both scene-local):**
1. **Mirrored quadrant clipping.** The scissor y was flipped for a bottom-up window framebuffer. Under `ofFbo::begin()`'s default matrix flip, canvas row y is FBO row y. It now flips only when framebuffer 0 is bound (the standalone is unchanged).
2. **Upside-down HUD-layer text.** OF's `ofGLProgrammableRenderer::end(const ofFbo&)` always resets the render surface to the window (`ofGLProgrammableRenderer.cpp:1621`). So the erosion pass, nested inside the scene FBO, flipped every later bitmap-string draw that frame, including the reticle labels. The erosion accumulation now runs in `update()`, outside any bound FBO. It still runs once per quadrant per frame, and the standalone behaves the same.

## 3. Key files changed

**`sketches/quadrant-crosshair/`**
- `QuadrantSceneCore.{h,cpp}` (new): the pipeline lifted verbatim from `ofApp`. Video is passed in (`VideoFrame`), media changes are reported (`onMediaChanged`), and `activate()` resets the composition.
- `QCCanvas.h` (new): canvas size, fixed 1280×720 in production; follows the live window in the standalone.
- All Quadrant-local sources are in `namespace quadrantcrosshair`; `ofGetWidth/Height` → `canvasWidth/Height` in the 8 production files.
- `ShaderLibrary`: `AssetLayout {Standalone, CanonicalSync}`; production loads from the canonical delivered paths.
- `MotionExtraction`: configurable shader paths, `reset()`, `const` texture inputs.
- `Quadrant`/`QuadrantManager`: FBO-aware scissor; `accumulateErosion()` split out of draw; `resetState()`/`resetComposition()` (reactivation without reallocating FBOs).
- `HudManager`: `showInformationLayer` (draw-only gate) and a test-only `showElementForTesting()`.
- `ofApp.{h,cpp}`: a thin host over the core. It keeps `VideoSystem`, `DebugMode`, the `O` takeover and all keys.
- `config.make`: excludes `hud-compositor%`, `hud-runtime%`, `scene%`, `video-playback%` and `video-effects/test%` from the sweep. This is a **pre-existing** standalone build break; the standalone builds again.

**`sketches/experience_runtime/`**
- `QuadrantProductionScene.{h,cpp}` (new), `kSceneId = "quadrant-crosshair"` (matches the frozen profile).
- `QuadrantSemanticMapping.{h,cpp}` (new) + `test/quadrant_semantic_mapping_tests.cpp`.
- `QuadrantCrosshair_*.cpp` (11 one-line wrapper translation units; they keep the forks' own includes resolving to their own directory).
- `effect-manifest.json` (new) + `config.make` hook for `sync-video-effect-assets.py`; `-DQC_PRODUCTION_RUNTIME`; include paths (`quadrant-crosshair/src` last).
- `ExperienceRuntime.{h,cpp}`: `ProductionRoster {BlobTemporal, ThreeScene, Full}` replaces the `includeBlueprint` flag; Quadrant is registered 4th under `Full`.
- `ofApp.cpp`: existing harnesses keep their accepted rings (two- or three-scene, including the RT-004 stall driver); the normal run and the four-scene proof get `Full`. New `EXPERIENCE_RUNTIME_QUADRANT_SCENE`.
- `ThreeSceneCircuitHarness`: generalised to the registered ring; `EXPERIENCE_RUNTIME_FOUR_SCENE_HARNESS` adds Quadrant checks plus a per-frame scissor/FBO-leak check.

## 4. Parity matrix (standalone vs production)

| Feature | Standalone | Production | Status |
|---|---|---|---|
| Crosshair behaviour (noise path, presets, bloom, motion attractor, colour sampling) | `CrosshairSystem` | Same class via the core | MIGRATED |
| Quadrant layout / per-quadrant decks / erosion / morph / scale | `QuadrantManager` | Same, canvas-fixed | MIGRATED (scissor + erosion fixes) |
| Timing / triggers / silences | `TriggerBus` → `QuadrantManager` | Same | MIGRATED |
| Expansion (auto, 120–300 s) | `ExpansionDirector` | Same; verified to HOLD | MIGRATED |
| Motion extraction underlay + output-mode switching | fork | Same fork, same shaders | MIGRATED |
| Media | `VideoSystem` scan/shuffle | Canonical `VideoPlaybackService` (texture, pixels, frame-new, identity) | MIGRATED (canonical) |
| Media-change response (erosion reset, nature copy) | `fileChanged()` | Canonical identity change | MIGRATED |
| Shaders / effects | local paths | Canonical delivery, byte-identical | MIGRATED |
| Artwork HUD layer | `HudManager` | Drawn | MIGRATED |
| Information HUD layer | `HudManager` | Not drawn; the production HUD's channel strip + media slot carry it | DEVELOPER-ONLY / superseded |
| `DebugMode`, `O` takeover, preset/overlay/output-mode keys, `E`/`F1–F4` | keys | Not compiled/exposed | DEVELOPER-ONLY |
| Reseed | none (expansion keys are "for testing") | none | n/a |
| Cursor/frame-rate/fullscreen | `ofApp` | Runtime-owned | Not scene-owned (correct) |

## 5. Semantics / commands

- **State** (`primaryStateId`): `expanding` (travel-out/hold), `reconfiguring` (travel-back), otherwise `online` / `quiet` / `standby` from the liveliest channel.
- **Metrics:**
  - `channel_0–3_state` → `scene.quadrant.state.*` (drives the frozen four-channel strip);
  - `channel_N_effect` (active shader; absent when idle);
  - `crosshair_preset` (shared preset vocabulary, as `FakeQuadrantScenario`);
  - `expansion_state`;
  - `expansion_progress` (travel-out/hold only; travel-back progress is internal, so absent).
- **Activity:** overall/density = online ÷ 4; motion = extraction energy ÷ 0.3; transition = travel-out progress.
- **Not exposed (absent rather than invented):** variation and `stateProgress`.
- **Commands advertised:** NextMedia, PreviousMedia (canonical Shared Video).
- **Commands not advertised:**
  - Regenerate: there's no curated reseed; the expansion keys are test keys.
  - Reset: honoured but not advertised, per product direction.
- **Effects:** `HudFrameData.effects = nullopt` (no canonical producer).

## 6. Verification

| Item | Result |
|---|---|
| ExperienceRuntime Release / standalone `quadrant-crosshair` Release | Both clean |
| **Four-scene harness (final)** | **85/85.** 10 circuits / 40 real switches, alternating direction. Forward: Blob → Temporal → Blueprint → Quadrant → Blob; backward: the reverse. Consecutive legs cover the brief's Quadrant → Blob → Temporal → Blueprint → Quadrant, and each forward circuit's Quadrant → Blob is followed by the next circuit's Blob → Quadrant. 80 mid-transition switch requests and 40 scene commands rejected. Per scene: 1 setup, activations = visits, 1 capability query per activation, status pulls = updates = draws. 4985 scene updates = 4985 live frames (no inactive updates). 5505 HudFrameData = 5505 HUD draws. 4505 Idle frames with 0 stale-state violations. 1182 Quadrant frames observing the canonical media. Forced expansion reached HOLD with `expanding` + `expansion_progress`, and the next activation came back with expansion idle (switched away mid-expansion). 0 GL errors. **0 scissor/FBO leaks across 5505 frames.** Scene FBO allocated once. RSS from circuit 2 to the end: 158.1 → 167.0 MiB (peak 180.7). Switch frames 33–112 ms |
| Unit suites (9) | All pass: 523 checks, including the new Quadrant mapping at 46/46 |
| `check-video-effect-drift.py` | ExperienceRuntime "up to date (21 assets)"; no duplicate IDs. Its overall FAIL comes only from `shader-effect-debugger`/`temporal-fields` data not synced in the fresh worktree |
| Regressions on the new binary (RT-005 three-scene, RT-003, RT-002, Blob/Temporal lifecycle, Blueprint roster/HUD, parity soak) | **Running when this report was written. Results to follow.** |

Captures (`docs/reviews/quadrant-captures/`, all 1280×720, in-process GL readback):
- `s_quadrant_scene.png`: four quadrants with distinct effects, crosshair, upright reticle labels ("WIND", "FRONT").
- `s_quadrant_window.png`: production HUD with the QUADRANTS channel strip.
- `s_quadrant_expanded_scene.png`: one quadrant expanded full-canvas, UI layer faded.
- `t_*`: transitions into and out of Quadrant.

Logs: `docs/reviews/logs/quadrant-*-2026-10-13.log`.

## 7. Resource / GL state

- **FBOs:** 8 erosion FBOs (1280×720 RGBA, about 29 MB) plus motion-extraction FBOs, allocated once at first setup and reused. Reactivation clears them and never reallocates (FBO allocation count was stable across circuits).
- **GL state:** no FBO is bound inside draw any more, and scissor is disabled after every quadrant draw (verified per frame).
- **Assets:** shader assets are delivered reproducibly by the canonical manifest sync at build time.

## 8. Remaining limitations

- **`motion_accum`/`motion_extract`** load from runtime paths that are still hand-supplied. Blueprint shares this gap. Another session's uncommitted `scripts/sync-shared-shader-runtime-assets.py` (main checkout) covers them; it's not part of this work.
- **Standalone visual parity** was established by code identity (same core and order) and builds, not by a side-by-side standalone capture.
- **First expansion timing:** a natural expansion takes 120–300 s, so most switch dwells won't show one. Verification forced it from the harness.
- **One instance per process:** `QCCanvas` is process-wide.
- **Merge with RT-006 (Fragment, separate worktree):** both add the same `ProductionRoster` design and generalise the same harness. Merging needs a manual reconcile of `ExperienceRuntime.{h,cpp}`, `ofApp.cpp`, `config.make` and `ThreeSceneCircuitHarness.*`, deciding the five-scene ring order.

## 9. Pi follow-up (structural risks only, not measured)

- 8 full-canvas RGBA erosion FBOs plus motion FBOs (about 30+ MB GPU) resident after first use.
- Per-quadrant erosion pass + up to two shader passes per quadrant per frame, plus Ridgeline CPU geometry.
- CPU pixel sampling of every new frame (brightness + crosshair colour).
- 20 shaders compiled at first setup (first-activation cost).

## 10. Next scene

Fragment Trail is already integrated as a production scene in the separate RT-006 worktree (desktop four-scene 82/82). The next step is merging RT-006 and this work into one five-scene ring, not another migration.

## 11. Git

No stage, commit or push performed. All changes are uncommitted in the worktree `EcopunkVideoCollage-quadrant`. The main checkout's uncommitted RT-004 work was not touched.

```text
WORK ITEM: Quadrant Crosshair production scene migration
PREVIOUS STATUS: not started (standalone only)
NEW STATUS: WORKTREE_ONLY — desktop verified (four-scene 85/85); Pi NOT_TESTED
WHAT CHANGED: QuadrantSceneCore shared by standalone + QuadrantProductionScene; canonical Shared Video consumption; namespaced forks; canonical shader delivery (effect manifest); scissor + nested-FBO text fixes; HUD information layer suppressed; roster enum; four-scene harness mode; standalone build repaired
WHAT WAS PROVEN: real artwork in one binary with Blob/Temporal/Blueprint; 40 bidirectional switches; lifecycle/caps/HUD/no-inactive-update invariants; canonical media observation; clean reactivation incl. mid-expansion exit; 0 GL errors; 0 scissor/FBO leaks; bounded RSS; honest §18.5 semantics
WHAT WAS NOT PROVEN: Pi; side-by-side standalone visual parity; regression harness results (pending at report time)
NEW EVIDENCE: docs/reviews/quadrant-captures/; docs/reviews/logs/quadrant-*-2026-10-13.log
NEW RISKS: GPU residency of 8 erosion FBOs on Pi; RT-006/Quadrant merge reconcile
NEW BLOCKERS: none
NEXT ACTION: record regression results; merge RT-006 + Quadrant into one five-scene ring; Pi run
NEXT GATE: Quadrant scene-domain acceptance
REGISTRY UPDATE REQUIRED: YES (new migration item + evidence)
ROADMAP UPDATE REQUIRED: NO
DECISION LOG UPDATE REQUIRED: NO
ARCHITECTURE HANDOFF REQUIRED: NO
```
