# Scene/SceneManager Consolidation — Codebase Exploratory Probe

**Purpose of this document**: a factual inventory of every sketch in this monorepo (11 total, including Blueprint Emergence and its siblings), plus the shared infrastructure they draw on, gathered as scaffolding for a follow-up engineering prompt to consolidate them into a single Scene/SceneManager binary. This is **not** a proposal, review, or recommendation document — no refactor suggestions appear anywhere below. Where something doesn't apply to a given sketch, that is stated explicitly.

**Method**: repo root inspected directly (git history, `shared/src/`, `shared/assets/`, `scripts/`, `tools/`, `docs/`); each of the 11 sketches inventoried in detail (member variables, `setup()`/`update()`/`draw()` order, build config, video/media handling, shared-code usage, state machines, performance notes, input surface), including an actual `make Release -j4` build attempt for every sketch on the macOS dev machine, on 2026-08-01.

---

## 0. Repo-Level Facts (apply to all sketches unless noted otherwise)

- **Location**: `/Users/joseconchello/openFrameworks/apps/myApps/EcopunkVideoCollage`
- **Git**: a single git repository (not per-sketch repos) at the `EcopunkVideoCollage` root. Current branch: `blueprint-emergence-updates`. Other branches: `main`, `initial-draft`. Full history is shared across every sketch — confirmed no nested `.git` exists anywhere under `sketches/` or `shared/`. Recent commit history (newest first): `add a bunh of crap`, `save`, `good enough for temporal fields`, `add timeline cycles for presets`, `save a bunch of shit to the temporal feilds, new widget`, `missed saving files`, `save sketches`, `fix blueprint fragments not loading, make more organic, update composition to be more organic`, `get a better blueprint emergence sketch`, `wrap up the quadrant sketch`, `add themed copy to ui based on video, add new shaders to library, update looks`, `greate stable design`, `update hud looks and feels`, `add graph elements`, `quadrant crosshair clean up effects add debugging for effects`, `fix video now playing, slowdown fragments, update light colog`, `get some video in the blueprint sketch working`, `add fragment fill ins`, `create app structure and initial blueprint grid sketch`, `initial commit of scaffolded ecopunk video collage project`.
- **openFrameworks version**: 0.12.1 for every sketch (`OF_VERSION_MAJOR/MINOR/PATCH = 0/12/1` in `/Users/joseconchello/openFrameworks/libs/openFrameworks/utils/ofConstants.h`). No sketch pins a different version — all point `OF_ROOT` at the same shared checkout via a relative path (`../../../../..`), so there is exactly one OF install in play, not per-sketch copies.
- **Deployment target** (per `README.md`/`CLAUDE.md`): macOS for development, **Raspberry Pi 3B** (quad-core Cortex-A53, 1GB RAM, VideoCore IV GPU) for deployment. No sketch currently has an actual cross-compile toolchain, Pi-specific `config.make`, deploy script, or systemd unit anywhere in the repo (confirmed by repo-wide search — see Section 10). `PLATFORM_PI` is referenced via `#ifdef` in several sketches' code and in `shared/src/MotionExtraction.h/.cpp`, but it is **never actually defined** by any Makefile/config.make/build script in this repo today — it is unset ifdef scaffolding, implying sketches are meant to be built natively on the Pi (once that path exists), not cross-compiled from macOS.
- **Repo layout** (per `CLAUDE.md`):
  ```text
  shared/src/             # code shared by every sketch, pulled in via PROJECT_EXTERNAL_SOURCE_PATHS
  shared/assets/          # canonical data assets shared by every sketch (currently: video-effects/)
  sketches/<name>/        # one openFrameworks project per sketch, generated via projectGenerator
  scripts/                # repo-wide tooling (asset sync, drift checks)
  docs/                   # architecture docs and engineering plans
  ```
  Each sketch was generated via OF's `projectGenerator` CLI with `-s ../../shared/src`, so its `config.make` computes relative paths to both the OF root and `shared/src` correctly from that generation step.
- **11 sketch directories exist under `sketches/`**: `blob-region-prototype`, `blueprint_emergence`, `contour-portrait`, `fragment-trail`, `hud_elements` (**not a real sketch** — see its section below), `hud_validation_harness`, `quadrant-crosshair`, `radar-effects-gallery`, `radar-pulse`, `shader-effect-debugger`, `temporal-fields`.
- **Existing docs relevant to this probe** (all in `docs/`, read for grounding and cited inline below where used): `blueprint_emergence_engineering_plan.md`, `blob-region-architecture.md`, `blob-fragment-and-foreground-mask-architecture-analysis.md`, `shared-video-effect-architecture.md`, `shader-effect-system-probe.md`, `video-effect-promotion-inventory.md`, `video-effect-second-wave-evaluation.md`, `implement-shader-effect-debugger-and-service.md`, `quadrant-crosshair-reuse-analysis.md` (confirmed **stale** — see quadrant-crosshair §5 below), `temporal-fields-context-questions.md`, `temporal-fields-implementation-brief.md`, `temporal-fields-preset-parameter-reference.md`, `temporal-fields-preset-timeline-format.md`.
- **`scripts/`** (repo-wide tooling): `check-hud-library-uniqueness.sh` (fails non-zero if more than one copy of any canonical `shared/src/hud/` widget `.cpp` exists anywhere in the repo — a past incident: the HUD library was copy-pasted into two sketches independently and drifted before consolidation); `check-video-effect-drift.py` (read-only; flags duplicate-ID effect registrations, stale/drifted synced shader assets, and advisory per-effect uniform cascades outside `shared/src/video-effects/` that look like un-migrated duplication); `sync-video-effect-assets.py` (writes synced shader assets into a sketch's `bin/data/shared-video-effects/` from the canonical `shared/assets/video-effects/`; `--check` mode is read-only).
- **`tools/`**: `convert_media.sh` — standalone ffmpeg-based footage-prep script (not invoked by any sketch at runtime), converts source video to the target spec (MP4, H.264 Baseline/Main, 540p preferred/720p max, 24-30fps, audio stripped, sRGB/no HDR, yuv420p), defaulting its output to `sketches/quadrant-crosshair/bin/data/media/` (overridable via `OUTPUT_DIR`).

---

## Shared Infrastructure Reference

Every sketch's Section 5 below refers back to this. `shared/src/` totals **~11,561 LOC** across `.cpp`/`.h` files (not counting `shared/src/video-effects/`, counted separately below).

### `shared/src/` top-level classes
| Class | File(s) | What it owns |
|---|---|---|
| `GridSystem` | `GridSystem.h/.cpp` (127 lines) | Dynamic, content-derived grid: structural lines generated per cycle (golden-ratio/weighted-random subdivision), fragment-contributed lines, occupancy tracking by actual rectangles (not fixed cells), snap-position queries, dissolve choreography. Originated in Blueprint Emergence's "Grid System Handoff." |
| `AnnotationRenderer` | `AnnotationRenderer.h/.cpp` (115 lines) | Draws grid lines, zone divider, text labels, measurement lines between fragments, and an autonomous code-fragment text overlay. Timing/tuning values are passed in by the sketch, not hardcoded. |
| `Fragment` | `Fragment.h/.cpp` (153 lines) | Base state machine for one placed video crop: `enum class State { ARRIVING, STABLE, DRIFTING, DISSOLVING, GHOST, DEAD }`. Owns timing/opacity/drift/a non-owned shared video-texture pointer/fragment shader effects. Subclassed by `BEFragment` (blueprint_emergence). |
| `CompositionBase` | `CompositionBase.h/.cpp` (157 lines) | Owns the four-stage composition cycle arc: `enum class CyclePhase { BLANK, PLACEMENT, DENSITY, DISSOLVE, RESET_HOLD }` plus the fragment list. Placement itself (`attemptPlacement()`) is a pure-virtual hook implemented by a sketch subclass (e.g. `BEComposition`). All numeric tuning lives in a `Timing` struct passed to `setup()`, not hardcoded. Subclassed by `BEComposition`; explicitly **not** subclassed by `TFComposition` (temporal-fields) — see that sketch's §5. |
| `VideoSampler` | `VideoSampler.h/.cpp` (65 lines) | Wraps the single `ofVideoPlayer` instance every sketch is constrained to. Implements the repo's canonical **seek-and-freeze** pattern: caller queues a normalized offset + callback (max 2 in-flight), `update()` calls `player.setPosition(offset)`, enters a `SEEKING` state with a settle-frame countdown (60 frames, ~2.5s at 24fps), polls `player.isFrameNew()`, then grabs pixels and fires the callback — with P50/P95 latency logging after 20 samples. |
| `TimeOffsetVideoBuffer` | `TimeOffsetVideoBuffer.h/.cpp` (166 lines) | A deliberately different shape from `VideoSampler`: plays one video continuously (never seeks), keeps a rolling CPU-RAM (`ofPixels`, not GPU-resident) history ring buffer of downscaled frames, so several independent "playheads" can each read a different point in the recent past with zero extra decode/seek cost. Default 6 playheads, 12-16 quantize bands, up to 10s of history. Explicit Pi 3B RAM-budget rationale in the header (a naive texture-per-history-frame approach would burn ~220MB of the Pi's 1GB shared RAM). |
| `BlobDetector` / `BlobTracker` | `BlobDetector.h/.cpp`, `BlobTracker.h/.cpp` | Frame-difference blob detection (via `ofxCvContourFinder`) producing normalized-space detections, rate-limited by `processEveryNFrames`; `BlobTracker` associates detections into persistent, smoothed tracks (distance/IoU matching, exponential smoothing, missing-detection grace period) and emits `VideoRegion` objects. Strict ownership boundary documented in `docs/blob-region-architecture.md`: detector owns detections, tracker owns identity/smoothing, region controller owns lifecycle, Fragment owns drawing, video source owns decode, shader library owns shaders. |
| `VideoRegion` / `VideoRegionController` | `VideoRegion.h`, `VideoRegionController.h/.cpp` | `VideoRegion` is a canonical, ownership-free data struct (normalized bounds, smoothed bounds, confidence, age, assigned effect). `VideoRegionController` owns the active-region-to-`Fragment` lifecycle: one `Fragment` per tracked region, enforces an active-fragment cap, assigns a stable effect per persistent region id, draws via `VideoRegionEffectRenderer`. Explicitly does not decode video, does not own a second video player, does not run OpenCV itself, and is "not a general scene manager." |
| `ShaderLibrary` | `ShaderLibrary.h/.cpp` | Named registry of fragment-shader effects sharing one passthrough vertex shader — a shader-*loading* layer only. Promoted unchanged from `quadrant-crosshair/src/ShaderLibrary.h` (already sketch-agnostic). Explicitly **not** where uniform-binding logic belongs — see the video-effects service below. |
| `LFOBank` | `LFOBank.h/.cpp` | A bank of independent sinusoidal oscillators (CPU-only, no GL, no per-frame allocation), generalized from `quadrant-crosshair/src/LFOBank.h` — lane count/meaning is caller-owned here vs. a fixed enum in the original. |
| `TriggerBus` | `TriggerBus.h/.cpp` | Generic edge-triggered event bus (rising-edge detection + per-trigger cooldown + listener dispatch), adapted from `quadrant-crosshair/src/TriggerBus.h` which hardcoded its condition checks against `CrosshairState` — the shared version is mechanics-only. |
| `GridState` | `GridState.h/.cpp` | A float grid that accumulates per-cell activity and decays every frame, uploaded as a texture. Adapted from `quadrant-crosshair/src/GridState.h` (which was fixed at 24×18 under a cursor); this version takes arbitrary dimensions and accumulates by cell index (for grid-placed fragments rather than a cursor). |
| `MotionExtraction` | `MotionExtraction.h/.cpp` | Low-res motion-difference extraction: an accumulation buffer (slow-decaying memory) and a delayed ring-buffer history, diffed against the current frame for a stylized "motion glow" texture. Ported unchanged from `quadrant-crosshair/src/MotionExtraction.h/.cpp`. History depth is `#ifdef PLATFORM_PI`-gated: 15 frames on Pi vs. 60 on desktop. |
| `ErosionFBO` | `ErosionFBO.h/.cpp` | Ping-pong decay-and-blend FBO wrapping a whole scene's draw call (captures current frame, blends against running history via a normalized `mix`, presents the composite) — generalized from `quadrant-crosshair/src/Quadrant.cpp`'s embedded per-quadrant `fbo_read`/`fbo_write` pattern into a standalone reusable class. |
| `RidgelineRenderer` | `ridgeline/RidgelineRenderer.h/.cpp` | Brightness-driven horizontal ridgeline effect (Joy Division/pulsar style): CPU-side luminance sampling + polyline construction in `update()`, GL-calls-only `draw()`. Designed explicitly for Pi 3B/GLSL ES 1.0 constraints (no per-pixel fragment shader, no FBO ping-pong). |
| `Settings.h` | `Settings.h` (17 lines) | Repo-wide color tokens shared by every sketch: `GROUND_DARK` (`0x0D0D0D`), `GROUND_LIGHT` (`0xB2AC88`, with a commented-out warm-off-white alternative), `RULE_WHITE`, `RULE_ORANGE`, `TEXT_CODE`, `TEXT_DIM`, `HATCH_WARNING`. Sketch-level `Settings.h`-style files (e.g. `BESettings.h`, `TFSettings.h`) may override these for sketch-specific variants. |

### `shared/src/hud/` — canonical HUD widget library
Per its own README: "the only copy of this library in the repository" — a past incident saw it copy-pasted into `sketches/hud_elements/` and `sketches/quadrant-crosshair/src/hud_elements/` independently, where all three drifted before being consolidated. `scripts/check-hud-library-uniqueness.sh` now fails the build if a second copy of any widget `.cpp` reappears anywhere. 19 widgets total (`ScannerWidget`, `PulseEmitterWidget`, `NodeNetworkWidget`, `FlowFieldWidget`, `ContourWidget`, `GaugeWidget`, `DataCardWidget`, `HexGridWidget`, `ReticleWidget`, `GlitchTearWidget`, `StatusLightWidget`, `LogScrollWidget`, `TickBurstWidget`, `RadarStationWidget`, `BreathingTickClusterWidget`, `TelemetryReadoutWidget`, `HalftonePatchWidget`, `TextCalloutWidget`, `DashedLineWidget`), all deriving from a single `HudWidget` base with no further inheritance, drawing only immediate-mode vector primitives (no shaders/FBOs/textures) for Raspberry Pi friendliness. A sketch adds `shared/src/hud` (or the broader `shared/src`) to its `config.make`'s `PROJECT_EXTERNAL_SOURCE_PATHS`. Pi runtime FPS validation is explicitly noted as "currently outstanding... no Pi-specific build target exists in this repo yet."

### `shared/src/hud_overlay/` — HUD Glitch Overlay System
A standalone ambient overlay layer (Lock Sequence, Fault Cascade, Radar Ping, Handshake composite "organisms" plus always-on ambient atoms — scanline, radar station, breathing tick clusters, telemetry readouts), driven by 4 master dials. Per its own README, it has a strict isolation boundary: nothing in this module knows about (or should reference) the Composition Manager's cycle lifecycle, `Fragment` placement events, `VideoSampler`, or the composition's own random seed — `HudOverlayLayer` only ever draws over a solid near-black background. Each of `quadrant-crosshair`, `blueprint_emergence`, and `temporal-fields` gates a `HudOverlayLayer` + `HudOverlayDialPanel` behind the `o` key as a standalone full-screen-replace test harness (**note**: `fragment-trail`'s `'o'` key does something different — see that sketch's §3).

### `shared/src/video-effects/` — the canonical video-effect service
Per top-level `CLAUDE.md`, this exists specifically because the monorepo "spent a long time with every sketch independently reinventing the same handful of effects... with hand-duplicated literal uniform values" — see `docs/shader-effect-system-probe.md` for the original duplication investigation and `docs/shared-video-effect-architecture.md` for the full design. Directory map:
```text
shared/src/video-effects/
├── core/        VideoEffectService, VideoEffectRegistry, VideoEffectDefinition, VideoEffectTypes,
│                VideoEffectContext, VideoEffectInstance, VideoEffectParameterSchema,
│                VideoEffectParameters, VideoEffectAssetRegistry, VideoEffectLoadReport, VideoEffectCapabilities
├── effects/     One VideoEffectInstance subclass per effect *kind*: SinglePassShaderEffect alone
│                backs all 19 single-pass effects (desaturate, recolor, heatmap_recolor, ...);
│                MotionExtractionEffect, MotionCompositeEffect, ErosionEffect, RidgelineEffect,
│                TemporalTrailsEffect, ReactionDiffusionEffect each back one stateful family.
├── knowledge/   EffectKnowledgeBase (whitelist/blacklist persistence), EffectRandomizer
├── evolution/   EffectEvolutionController (current->target transitions), PatternDriftController (continuous drift)
└── catalog/     DefaultVideoEffectCatalog.{h,cpp} — every effect gets registered here
```
Shader assets live separately, in `shared/assets/video-effects/` (`common/vert.glsl`; `erosion/`; `motion-extraction/`; `reaction-diffusion/`; `single-pass/` — 18 `.glsl` files: `ascii_threshold_solarpunk`, `bioluminescence`, `caustics`, `channelshift`, `chromatic_aberration`, `desaturate`, `dither`, `edge_glow`, `heatmap_recolor`, `hue_rotate`, `ink_outlines`, `invert`, `pixel_drift`, `pixel_sorting`, `recolor`, `scanlines`, `solarize`, `threshold`, `water_refraction`; `temporal-trails/`; `utility/video_adjust.glsl`), synced per-sketch via `scripts/sync-video-effect-assets.py` into that sketch's `bin/data/shared-video-effects/`.

A consuming sketch needs: (1) `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src` in `config.make`; (2) an `effect-manifest.json` at the sketch root listing which canonical effect ids it wants enabled; (3) synced shader assets via the sync script. Two shader uniform conventions ("contracts") exist: **Contract A** (`tex`/`vTexCoord`, the default, used by every migrated sketch) and **Contract B** (`videoTex`/`maskTex`/`texCoordVarying`, used only by `radar-effects-gallery`'s `GalleryCompositor`). `VideoEffectDefinition::contract` already reserves a `B` value for this.

**Migration status per sketch** (from top-level `CLAUDE.md`, cross-checked against each sketch's own inventory below):

| Sketch | Status |
|---|---|
| `blob-region-prototype`, `blueprint_emergence`, `temporal-fields` | Migrated — fixed-default uniforms source from the shared catalog |
| `quadrant-crosshair` (production path) | Migrated (production path only) — reads default uniform values from the shared catalog for its own locally-owned `ShaderLibrary` shaders |
| `quadrant-crosshair`'s `DebugMode` | **Deliberately not migrated** — every parameter is live-tunable via keyboard-driven GUI state, not a duplicate of fixed literals; real migration path is retirement in favor of `shader-effect-debugger`, not a mechanical refactor |
| `shader-effect-debugger` | Built directly against the full service — the reference example for schema-driven GUI consumption |
| `fragment-trail` | **Deliberately not migrated** — local `ShaderLibrary`/`LFOBank`/`TriggerBus`/`TimeOffsetVideoBuffer` forks collide by name with `shared/src`'s top-level classes; `config.make` excludes the whole `shared/src` top level plus all of `video-effects%` (see fragment-trail §2 for the exact wildcard mechanics) |
| `radar-effects-gallery`, `radar-pulse`, `contour-portrait` | **Not migrated, second-wave candidates** — each has a specific, documented reason (see `docs/video-effect-second-wave-evaluation.md` and each sketch's own §5 below) |

Drift is checked with `python3 scripts/check-video-effect-drift.py` (advisory, read-only) and `python3 scripts/sync-video-effect-assets.py --check` (asset drift).

---

## Per-Sketch Inventories

### blueprint_emergence

**1. Project Inventory**
- Path: `sketches/blueprint_emergence/`. Also has its own `blueprint_emergence.xcodeproj` (in addition to the Makefile path) — `Project.xcconfig` sets `CLANG_CXX_LANGUAGE_STANDARD = c++23`, `MACOSX_DEPLOYMENT_TARGET = 11.5`.
- OF 0.12.1 (repo-wide; no per-sketch override).
- **Builds cleanly.** `make Release -j4` succeeds. One warning: `src/BEFragment.cpp:16:8: warning: unused function 'sampleAverageBrightness'`.
- LOC in `src/`: **3,773** across 13 files (`BEComposition.cpp` 1,435; `BEFragment.cpp` 598; `BESettings.h` 508; `ofApp.cpp` 371; `BESettings_presets.h` 348; `BEComposition.h` 260; `BEFragment.h` 107; `ofApp.h` 71; `BECycleMode.h` 12; `BECompositionState.h` 16; `BETriggers.h` 17; `BELFOLanes.h` 13; `main.cpp` 17).
- Git: clean working tree on `blueprint-emergence-updates`; no nested `.git`.

**2. Build Configuration**
- Makefile: unmodified OF template.
- `addons.make`: `ofxGui`, `ofxOpenCv` — neither used directly by this sketch's own `src/`; both are pulled in transitively because `PROJECT_EXTERNAL_SOURCE_PATHS` sweeps all of `shared/src` unconditionally (`ofxOpenCv` for `BlobDetector.h`'s `ofxCvContourFinder`; `ofxGui` for `HudOverlayDialPanel.h`'s `ofParameter`/`ofxGui` usage).
- `config.make`: `OF_ROOT = ../../../../..`; `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src`, no `PROJECT_EXCLUSIONS` set. No custom `PROJECT_LDFLAGS`/`PROJECT_DEFINES`/`PROJECT_CFLAGS`.
- All paths relative except one conditional: `MEDIA_PATH` in `BESettings.h:392-396` is a **hardcoded absolute path on Pi**: `"/home/pi/blueprint/media/"` under `#ifdef PLATFORM_PI`, else the relative `"media/"`.
- No Pi cross-compile toolchain/config anywhere in the repo. `PLATFORM_PI` is referenced but never defined by any build file. `docs/blueprint_emergence_engineering_plan.md` describes a planned `tools/deploy.sh` (rsync + systemd restart) and systemd unit as a Phase 5 deliverable — **neither file exists yet**.

**3. ofApp Structure**
- Members: `GridSystem grid`; `AnnotationRenderer annotations`; `VideoSampler videoSampler`; `BEComposition composition`; `LFOBank lfoBank`; `TriggerBus triggerBus`; `GridState gridState`; `ShaderLibrary shaderLib`; `ErosionFBO erosionFBO`; `MotionExtraction motionEx`; `float gridPulseBoost`; `float motionOverlayOpacity`; `bool showOccupancyDebug`; `bool bypassErosion` (default `true`, labeled "TEMP DIAGNOSTIC"); `bool hadHudWidget`; `hudoverlay::HudOverlayLayer hudOverlay`; `hudoverlay::HudOverlayDialPanel hudOverlayPanel`; `hudoverlay::HudOverlayDialState hudOverlayDials`; `bool hudOverlayActive`.
- `setup()` order: `ofDisableArbTex()` (comment: "DO NOT REMOVE" — must precede all texture/FBO allocation) → `ofSetFrameRate(24)` → `grid.setup(...)`/`annotations.setup(...)` → `Fragment::loadFragmentShader(...)` (static, shared across all `Fragment` instances) → `videoSampler.setup(MEDIA_PATH)` → `shaderLib.setup()`/`gridState.setup()`/`erosionFBO.setup()` → `motionEx.setup()` (mode `LUMA_GLOW`, reference `REFERENCE_ACCUMULATION`) → `lfoBank.setup()` (6 named lanes, randomized phase offsets) → `triggerBus.setup()` (per-trigger cooldowns) → `composition.setupBE(...)` then wire LFOBank/TriggerBus/GridState/ShaderLibrary/MotionExtraction pointers into it → `wireTriggerResponses()` (LONG_SILENCE/ZONE_IMBALANCE → code-text spawn; CIRCLE_PLACED → measurement-line pulse) → register `onFragmentPlaced`/`onFragmentRemoved`/`onCycleStart`/`onPhaseChanged` callbacks → `composition.startCycle()` → `hudOverlay.setup(...)`/`hudOverlayPanel.setup()`.
- `update()`: early-return path when `hudOverlayActive` (updates only the overlay). Otherwise: `videoSampler.update()` → `lfoBank.update(dt)` → `composition.update(dt)` → `triggerBus.update(dt)` → `annotations.update(dt)` → set `MEASUREMENT_HUB` trigger from `annotations.getMaxConnectionCount()` → update code-text LFO weight → clear code text under an active HUD widget on its first frame → exponential decay of `gridPulseBoost` (`*= exp(-dt/0.8f)`) → if media present, update `motionEx` and smooth `motionOverlayOpacity` toward a phase-gated target.
- `draw()`: early-return path for `hudOverlayActive` (near-black background + overlay only). Otherwise: scene drawn either straight or through `ErosionFBO::beginCapture/endCapture/update/draw` (gated by `bypassErosion`, default `true`) → fragment shader-effect overlays drawn in a **separate pass after** the erosion FBO capture closes (documented reason: `ofFbo::end()` unconditionally resets viewport/matrix, so nesting corrupts the parent FBO) → divider drawn last, outside any FBO → optional occupancy debug → fps/status bitmap string.
- `exit()`: present but empty body — no teardown.
- Input: `keyPressed` handles `o`/`O` (toggle HUD overlay), `g` (occupancy debug), `r` (restart cycle), `e` (toggle erosion bypass), `f` (force divider axis flip). All mouse handlers are declared/overridden but with **empty bodies** — no mouse interaction implemented. `windowResized` forwards only to `hudOverlay.windowResized(...)`. No GPIO.
- Window: `1280×720`, `OF_WINDOW` (not fullscreen), no multi-monitor logic.

**4. Video / Media Handling**
- Player: `VideoSampler` (shared), wrapping exactly **one** `ofVideoPlayer`.
- Max concurrent players: **1**. Every `Fragment`'s `videoTexture` points at the same shared texture with a different crop rectangle — not separate decoders.
- Media path: config-driven `MEDIA_PATH` constant, `PLATFORM_PI`-conditional (see §2).
- Texture format: default `ofVideoPlayer`/`ofPixels` (AVFoundation-backed on macOS); FBOs elsewhere are `GL_RGBA`.
- **Seek-and-freeze**: `VideoSampler` architecturally implements the full pattern (see Shared Infrastructure Reference above), but **blueprint_emergence's own code never calls `requestCapture()` anywhere** — confirmed absent by grep. `BEComposition::requestVideoTexture()` instead binds each fragment directly to the single shared **live**, continuously-playing texture with a randomly-chosen aspect-matched crop rect. So while the capability exists in the wrapper class, this sketch's actual runtime behavior is: one video plays forward continuously, and every fragment is a live, different crop of the same continuously-advancing texture — not a frozen still.

**5. Shared/Duplicated Infrastructure**
- Includes used: `AnnotationRenderer.h`, `ErosionFBO.h`, `GridState.h`, `GridSystem.h`, `LFOBank.h`, `MotionExtraction.h`, `ShaderLibrary.h`, `TriggerBus.h`, `HudOverlayLayer.h`, `HudOverlayDialPanel.h`, `HudOverlayDialState.h`, `CompositionBase.h`, `VideoSampler.h`, `Fragment.h`, `RidgelineRenderer.h`, `Settings.h`, plus `hud/HudElements.h` (12 widgets: DataCard, Gauge, Contour, HexGrid, FlowField, NodeNetwork, Reticle, StatusLight, LogScroll, GlitchTear, Scanner) and `video-effects/` (`DefaultVideoEffectCatalog.h`, `VideoEffectRegistry.h`, `VideoEffectTypes.h`, consumed in `BEFragment.cpp`).
- Compile-time reality: of **69 total `.o` files** in `obj/osx/Release/`, only 4 are sketch-local (`BEComposition.o`, `BEFragment.o`, `ofApp.o`, `main.o`) — the remaining ~65 are the *entire* `shared/src` tree (including `BlobDetector`, `TimeOffsetVideoBuffer`, `VideoRegionController`, all `hud/`+`hud_overlay/organisms/` widgets, the whole `video-effects/` service), most never called from this sketch's own code, because `PROJECT_EXCLUSIONS` is unset.
- No local forks by name — this sketch's own files are all `BE`-prefixed and subclass/extend shared base classes (`BEFragment : public Fragment`, `BEComposition : public CompositionBase`) rather than reimplementing them.
- **Orphaned file**: `src/BESettings_presets.h` (348 lines) exists but is **not `#include`d anywhere** — an earlier-looking variant of the same preset struct found in the actually-used `BESettings.h` (pixel-unit `CIRCLE_REGION_CENTER_X`/`Y`/`SIZE` vs. the used file's fraction-of-canvas `_FRAC` variants). Compiles as a header-only file (no `.cpp`), so it produces no object and has zero runtime effect.
- Constants: `BESettings.h` includes `shared/src/Settings.h` directly and consumes its tokens (`GROUND_DARK`, `GROUND_LIGHT`, `RULE_WHITE`, `TEXT_DIM`) verbatim by reference — no divergence.

**6. Scene/State Lifecycle**
Multiple layered, class-based enum state machines (no strings, no config-driven state):
- `CompositionBase::CyclePhase` (base): `enum class { BLANK, PLACEMENT, DENSITY, DISSOLVE, RESET_HOLD }`. `BEComposition` overrides `usesAutomaticPlacementTimer()` → `false` (drives 4 slots itself) and `usesAutomaticPhaseTimer()` → `currentMode != CycleMode::PERPETUAL`.
- `CycleMode` (`src/BECycleMode.h`): `enum class { GHOST_LAYERS, PERPETUAL }`, picked 50/50. `BEComposition::onDissolveComplete()` intercepts the base class's dissolve-completion hook and calls `enterMode(selectNextMode())` instead of transitioning to `RESET_HOLD` — the sketch runs continuously between these two "meta-modes" rather than ever fully resetting.
- `BEComposition::SlotPhase`: `enum class { EMPTY, ARRIVING, HOLD, DISSOLVING }` across `NUM_SLOTS = 4` independently-timed slots.
- `Fragment::State` (shared base): `enum class { ARRIVING, STABLE, DRIFTING, DISSOLVING, GHOST, DEAD }` per-fragment.
- `BEComposition::HudPhase`: `enum class { SILENCE, ACTIVE }` for the single at-most-one-at-a-time HUD widget.
- `BEComposition::RotationPhase`: `enum class { INACTIVE, ROTATING_AROUND_A, ROTATING_AROUND_B }` — divider axis-flip sub-machine.
- `BEFragment::EffectSlot::State`: `enum class { IDLE, FADE_IN, ACTIVE, FADE_OUT }` — per-fragment autonomous shader-effect cycling.
"Scene"/mode switching exists at the `CycleMode` level (alternating GHOST_LAYERS/PERPETUAL continuously), but there is no single top-level "Scene" abstraction — one continuous, self-perpetuating loop driven by the nested machines above.

**7. Performance & Resource Notes**
- `TARGET_FPS = 24`, enforced via `ofSetFrameRate`.
- `docs/blueprint_emergence_engineering_plan.md` documents several **planned-but-not-yet-executed** Pi 3B gates: 24fps sustained at 720p (fallback to 960×540 + HDMI upscale if not sustained), seek-latency P95 &lt; 400ms for `seekAndCapture()` (note: as established in §4, this sketch's shipped code doesn't actually exercise the seek path), peak VRAM logged via `vcgencmd get_mem gpu`, ~10MB memory estimate for 12 average-size fragments.
- README states the deploy-scope confirmation: "1-2 simultaneous video layers, so plain `ofVideoPlayer` (GStreamer-backed on Linux) should be sufficient without needing OMX/MMAL hardware-decode plumbing."
- No in-code FPS counter beyond the on-screen `ofGetFrameRate()` bitmap string.
- Shaders: `shaders/fragmentEffects.vert/.frag` (`#version 120`, `PLATFORM_PI` branch present); `shaders/erosion.vert/.frag` (via `ErosionFBO`, `#version 120`, `PLATFORM_PI` branch present); `shaders/effects/*.glsl` (17 effects loaded by `ShaderLibrary::setup()`) + `of_nature_shader_pack_glsl/*.glsl` (7 more) — all `#version 120`, no `PLATFORM_PI` branch (Contract-A single-pass shaders).
- FBOs: `ErosionFBO` is a documented ping-pong decay-and-blend FBO (decay rate `0.92`). Each `BEFragment` **additionally** allocates its own pair of scratch FBOs (`effectSourceFbo`/`effectResultFbo`) for its per-fragment shader-effect overlay — one pair per fragment (not pooled), lazily allocated on first use.

**8. Input / Control Surface**
- No shell script, systemd unit, or kiosk boot config anywhere in the repo references this sketch. `README.md` documents only manual launch (`make Release -j4 && make RunRelease`, or the Xcode project). The planned `tools/deploy.sh`/systemd service (Phase 5 of the engineering plan) do not exist yet.
- No GPIO/MQTT/OSC/network trigger code found.
- No crossfade/transition tied to process restart — all cycle/mode state resets fresh each launch; RNG reseeded from `ofRandom` at various points, not loaded from a saved seed.

---

### blob-region-prototype

**1. Project Inventory**
- Path: `sketches/blob-region-prototype/`.
- OF 0.12.1.
- **Builds cleanly.** Zero compiler warnings or errors (only the harmless `-jN forced in submake` make-tool notice).
- LOC in `src/`: **491** total (`ofApp.cpp` 354, `ofApp.h` 126, `main.cpp` 11) — no sketch-local classes beyond `ofApp`; everything else is consumed from `shared/src/`.
- Git: clean, no nested `.git`.
- `test/` subfolder: `videoregion_math_tests.cpp` (295 lines) + `Makefile.tests`, a standalone unit-test suite for `shared/src/VideoRegionMath.h/.cpp` — deliberately OF-free (pure-float geometry/association math; even `ofRectangle.h` alone would pull in `GL/glew.h`), built with plain `c++ -std=c++17` outside the OF Makefile system, kept as a sibling of `src/` (not nested inside it) specifically to avoid a `main()` collision with the sketch's own `main.cpp`. Running it: **62/62 checks passed**.

**2. Build Configuration**
- Makefile: unmodified OF template, identical structure to blueprint_emergence's.
- `addons.make`: `ofxGui`, `ofxOpenCv` — **both actually used directly** here (unlike blueprint_emergence): `ofxGui`/`ofxPanel`/`ofParameter<T>` for an extensive on-screen tuning GUI; `ofxOpenCv` transitively via `BlobDetector.h`.
- `config.make`: `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src`, with an explanatory comment: "This is the blob-driven video-region prototype... It reuses the canonical shared/src systems... rather than re-implementing them sketch-locally... same convention as blueprint_emergence and temporal-fields." `PROJECT_EXCLUSIONS =` present but empty. No `.xcodeproj` (Makefile-only).
- Media path is a **hardcoded relative literal `"media"`** passed directly to `TimeOffsetVideoBuffer::setup()` — no `PLATFORM_PI` branch (contrast with blueprint_emergence's conditional `MEDIA_PATH`).
- No Pi cross-compile evidence; no `PLATFORM_PI` reference anywhere in this sketch's own `src/`.

**3. ofApp Structure**
- Members: `TimeOffsetVideoBuffer videoBuffer`; `ShaderLibrary shaderLib`; `BlobDetector blobDetector`; `BlobTracker blobTracker`; `VideoRegionController regionController`; `VideoRegionEffectRenderer backgroundEffectRenderer` (a **separate** scratch-FBO instance from `regionController`'s own, explicitly to avoid the fragment-crop FBO's high-water mark growing to full-frame size just because the background effect always needs full-frame); `ofxPanel gui` + 5 `ofParameterGroup`s (detection: 10 params; tracking: 5; background: 3; rendering: 7; perf: 2); a `RollingAverage` struct + 5 rolling timers (`detectorMsAvg`, `trackerMsAvg`, `regionUpdateMsAvg`, `drawMsAvg`, `totalFrameMsAvg`). Per its own header comment, `ofApp` here is "deliberately thin — almost everything it does is call into shared/src" — there is no sketch-local composition/state class at all.
- `setup()` order: `ofDisableArbTex()` → window title/background/**`ofSetVerticalSync(true)` with no `ofSetFrameRate()` call anywhere** (frame rate is vsync-limited only, unlike every other sketch in this probe) → `shaderLib.setup()` → `videoBuffer.setup("media", {bufferWidth=640, bufferHeight=360, maxHistorySeconds=2.0f})` (explicitly kept short since "this prototype doesn't use temporal offsets") → `blobDetector.setup({})`/`blobTracker.setup({})`/`regionController.setup(&shaderLib)` → logs an 18-entry effect-index table to console → builds the 5-group GUI panel.
- `update()`: capture `lastFrameStartMicros` → clamp `dt` to `1/60` if invalid → `videoBuffer.update(dt)` → `syncParamsToSystems()` (copies every `ofParameter` into its backing `Config`/`Params` struct each frame) → if media present: `blobDetector.update()` on `videoBuffer.getRawVideoPixels()` (CPU path, no GPU readback), timed → `blobTracker.update(detections, dt)`, timed → `regionController.update(...)`, timed. Every 2 seconds, logs a `PerfStats` line to console (fps, detector/tracker/region-update/draw/total ms, tracked-region count, active-fragment count, scratch FBO dims) — explicitly there "so performance numbers can be captured from stdout... on a headless Pi run."
- `draw()`: background clear → if media loaded, `drawBackground()` (mode-gated: off/normal-cover-fit/shader-effect) then `regionController.draw()`, timed; else a "Loading media..." placeholder → optional debug overlay → optional `gui.draw()`.
- `exit()`: **not overridden at all** (no declaration in `ofApp.h`).
- Input: only `keyPressed` (`g` toggles GUI visibility, `d` toggles debug visualization param). **No mouse handlers declared at all** (unlike blueprint_emergence, which declares all of them as no-ops). No `windowResized`/`gotMessage`/`dragEvent`.
- Window: `1280×720` hardcoded directly in `main()` (not via a named constant), `OF_WINDOW`, no multi-monitor logic.

**4. Video / Media Handling**
- Player: `TimeOffsetVideoBuffer` (shared), itself wrapping one `ofVideoPlayer`.
- Max concurrent players: **1**. This sketch does **not** exercise `TimeOffsetVideoBuffer`'s playhead/history-ring machinery at all — confirmed no `Playhead`/`jumpPlayhead`/`rampPlayheadTo`/`holdPlayhead` calls anywhere. It only ever calls `getRawVideoTexture()`/`getRawVideoPixels()` — the live, un-delayed decoder output, bypassing the history buffer entirely.
- Media path: hardcoded relative literal `"media"`, no config/PLATFORM_PI branching. 3 `.mp4` files present in `bin/data/media/`, **all symlinks into blueprint_emergence's media folder** (which itself is a real directory of 26 clips, not symlinked).
- Texture format: default `ofPixels`/`ofTexture`; `BlobDetector` reads source pixels only through `ofPixels::getColor()` so RGB/RGBA/BGRA are handled uniformly.
- **Seek-and-freeze**: **not used** — `TimeOffsetVideoBuffer` never seeks by design; video plays forward continuously with no frozen-texture-copy timing anywhere in this sketch's execution path.

**5. Shared/Duplicated Infrastructure**
- Includes used (the sketch's *entire* dependency surface): `BlobDetector.h`, `BlobTracker.h`, `ShaderLibrary.h`, `TimeOffsetVideoBuffer.h`, `VideoRegionController.h`, `VideoRegionEffectRenderer.h`, `VideoRegionMathOf.h`. No `AnnotationRenderer`, `GridSystem`, `LFOBank`, `TriggerBus`, `MotionExtraction`, `ErosionFBO`, `CompositionBase`, `Fragment` (used only transitively through `VideoRegionController`), or `hud`/`hud_overlay` widgets are referenced directly.
- Compile-time reality: same ~65 shared-object-file sweep as blueprint_emergence (confirmed identical file list minus blueprint_emergence's `BE*.o` files) — this sketch also compiles `AnnotationRenderer`, `GridSystem`, `LFOBank`, `TriggerBus`, `MotionExtraction`, `ErosionFBO`, `CompositionBase`, all `hud/` widgets, and the full `video-effects/` tree, none of which its own code calls.
- No local forks — `src/` contains only `ofApp.h/.cpp` + `main.cpp`.
- This sketch does **not** include `shared/src/Settings.h` and has no color-token overlap to diverge from. Its only "constants" are inline `ofParameter<T>` defaults and a local 18-entry `kEffectChoices` array.

**6. Scene/State Lifecycle**
No enum-based phase/cycle machine at the top level (this sketch does not use `CompositionBase` at all). State that does exist: `BlobTracker::Track`'s implicit "active while `missingSeconds < maxMissingSeconds`" lifecycle (plain struct fields, not an enum); `VideoRegionController::ManagedRegion`'s `presentThisUpdate` boolean per frame. GUI-parameter mode switches (`pBackgroundMode` 0/1/2, `pDrawMode` 0/1/2) are live-tunable `ofParameter<int>` values, not compiled states. No notion of "scene" or discrete mode-switching at the top level — one continuous behavior (detect → track → render), parameterized live via GUI.

**7. Performance & Resource Notes**
- **Actually measured** (per `docs/blob-region-architecture.md`, cross-checked against the live `PerfStats` log format, which matches): source video 960×540 @ ~24fps; **total frame time ~11ms (≈75-80fps, vsync-limited) on the macOS dev machine**; scratch FBO high-water mark stabilized at **103×60** and stopped reallocating for the rest of the run.
- **Documented Pi 3B risk** (same doc): "the Pi 3B+'s Cortex-A53 cores are roughly an order of magnitude slower per-core than this development machine for scalar CPU work... A naive linear scaling estimate would put the detector alone at tens of milliseconds... this must be profiled on actual Pi 3B+ hardware before the default analysisWidth/analysisHeight/processEveryNFrames values are considered final."
- **Known limitation documented**: the tracker's associator is "a single-pass greedy match, not a globally-optimal assignment... can occasionally swap track IDs when two similar-sized blobs cross paths at close range — not exercised under real crossing-paths conditions in this test run."
- No GPU-memory-budget number logged in code.
- Shaders: same 17+7 `ShaderLibrary`-registered set as blueprint_emergence, all `#version 120`, no `PLATFORM_PI` branch. Does **not** use `Fragment::loadFragmentShader`/`fragmentEffects.vert/.frag` or `ErosionFBO`/`erosion.vert/.frag` at all (confirmed zero references), even though those files are present in `bin/data/shaders/` (copied there regardless of use).
- FBOs: `VideoRegionEffectRenderer` used twice — once inside `regionController` (region fragments), once as the separate standalone `backgroundEffectRenderer` (full-frame background effect) — each owns one scratch source/result FBO pair reused sequentially across all regions in a frame, generalized from `BEFragment`'s overlay-FBO pattern.

**8. Input / Control Surface**
- No launch script/systemd/kiosk config references this sketch. Manual launch only (`make Release -j4` / `make RunRelease`).
- No GPIO/MQTT/OSC/network code.
- No crossfade/restart-tied transition — no persisted state; all `ofParameter` values reset to declared defaults on every launch.

---

### contour-portrait

**1. Project Inventory**
- Path: `sketches/contour-portrait/`.
- OF 0.12.1. `Project.xcconfig`: `CLANG_CXX_LANGUAGE_STANDARD = c++23`, `MACOSX_DEPLOYMENT_TARGET = 11.5`.
- **Builds cleanly.** Only 2 warnings, both from the `ofxOpenCv` addon itself (`ofxCvFloatImage::setRoiFromPixels hides overloaded virtual function`, `ofxCvContourFinder::draw hides overloaded virtual function`) — not from this sketch's own code.
- LOC in `src/`: **2,149**.
- Git: clean; no nested `.git`.

**2. Build Configuration**
- Makefile: unmodified OF template.
- `addons.make`: `ofxGui`, `ofxOpenCv`.
- `config.make`: `OF_ROOT = ../../../../..`. **`PROJECT_EXTERNAL_SOURCE_PATHS` is present but commented out** — this sketch does **not** pull in `shared/src` at all:
  ```make
  # This sketch is self-contained (own ContourSource/ContourDisplacementEffect,
  # no shared/src reuse) — the effect's video/camera/image input handling and
  # preprocessing pipeline are bespoke enough that pulling in shared/src would
  # add coupling without saving code, same call radar-effects-gallery made for
  # its own throwaway video sampler.
  # PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src
  ```
  `PROJECT_EXCLUSIONS =` (empty, moot since nothing is included).
- All paths relative. No Pi cross-compile config; only runtime `#ifdef PLATFORM_PI` conditionals inside `.cpp`/shader source, implying native-on-Pi build assumption once that path exists.

**3. ofApp Structure**
- Members: `ContourSource source`; `ContourDisplacementEffect effect`; `ofImage maskImage` + `bool haveMaskImage`; `ContourMaskSource maskPolySource`; `int lastAppliedMaskSource`; `ofxPanel panel`; 9 `ofxButton`s (7 presets + save/load custom); `bool showOverlay`; `int lastAppliedInputMode`.
- `setup()`: 30fps, black background, `ofDisableArbTex()` → `source.setup()` → `effect.setup(1280,720)` → optionally loads `mask.png` → `maskPolySource.setup(160,90)` → wires `ofxPanel` to `effect.parameters()` + 9 buttons → applies "Clean Portrait" preset → forces input mode to VIDEO.
- `update()`: checks GUI-driven input-mode parameter, re-applies if changed → `source.update()` → `updateMaskPolygon()` (background-subtraction re-arm, fixed constants `thresholdValue=40`, `minAreaPct=1`, `maxAreaPct=90` — not exposed as GUI params) → `effect.update(source.getTexture(), dt)`.
- `draw()`: clears background → `effect.draw(...)` scaled to window → optional 4-panel debug view + fps → overlay text (fps/vertex count/source availability/key legend) → conditional GUI panel.
- `exit()`: not overridden — no explicit teardown (`ContourSource`'s own destructor does a best-effort camera-teardown exception guard on macOS).
- Input: `i` cycles input mode, `m` toggles mask-enabled, `b` captures background reference, `g` toggles overlay, `f` toggles fps-only overlay. No mouse/GPIO. `windowResized` is a no-op — canvas stays fixed at 1280×720, `effect.draw()` scales into whatever bounds are passed.
- Window: `OF_WINDOW`, 1280×720, no fullscreen, no multi-monitor.

**4. Video / Media Handling**
- Player: `ofVideoPlayer`, wrapped inside sketch-local `ContourSource`, which multiplexes video/image/camera via `Mode { INPUT_IMAGE, INPUT_VIDEO, INPUT_CAMERA }`.
- Max concurrent players: **1** (`closeAll()` tears down whichever was active before switching modes).
- Media path: video mode scans `bin/data/media/` if it has clips, else `bin/data/sharedMedia/` (a symlink to `../../../blueprint_emergence/bin/data/media`), picking a random `.mp4`. Camera mode uses `ofVideoGrabber` with a macOS `@try/@catch` around `setup()` to survive AVFoundation permission-throw crashes, latching `cameraKnownBroken` so a failed camera isn't retried mid-run.
- Texture format: whichever underlying source's texture is active, or a 4×4 black `blankTex` if none available. Effect's own FBOs (`processedFbo`, `colorFbo`) are `GL_RGBA`.
- **No seek-and-freeze pattern.** Video plays continuously looped (`OF_LOOP_NORMAL`). (`ContourDisplacementEffect` has an unrelated `freezeFrame` debug parameter that just pauses calling `preprocessSource()`/`computeDisplacement()` — a debug pause, not a video seek-and-freeze pattern.)

**5. Shared/Duplicated Infrastructure**
- **Zero `shared/src/` includes** — confirmed both by grep and by `PROJECT_EXTERNAL_SOURCE_PATHS` being commented out.
- Local classes with no `shared/src` counterpart: `ContourSource` (video/image/camera multiplexer — its own header comment notes it follows "the same one-decoder-at-a-time discipline as shared/src/VideoSampler" without inheriting from or wrapping it — an independent parallel implementation, not a fork of a specific file); `ContourMaskSource` (own OpenCV background-subtraction + contour pipeline via `ofxCvContourFinder`, independent of `BlobDetector`/`BlobTracker`); `ContourDisplacementEffect` (single fixed preprocess shader + CPU displacement — no shader registry of any kind, consistent with `docs/video-effect-second-wave-evaluation.md`'s characterization).
- No local shader/effect registry, no recolor/desaturate/heatmap-style effect implemented here (the one shader, `contour_preprocess.frag`, bakes luminance/edge/threshold/mask into RGBA channels for CPU-side line displacement — not a stylize/recolor pass).
- No literal constants here duplicate a `Settings.h` value (this sketch never includes `Settings.h`). Defines its own local `kCanvasWidth=1280`/`kCanvasHeight=720` and its own fixed mask constants.

**6. Scene/State Lifecycle**
- Internal state is enum-based, entirely inside `ContourDisplacementEffect`: `Orientation`, `SourceFit`, `DisplacementSource`, `BreakupDirection`, `RenderMode`, `BlendMode`, `QualityPreset`, `MaskSource`, `DensityMode`, `DensitySource`, `ColorBlendMode` — all are parameter-value enums (rendering/behavior toggles), not app-level scenes.
- "Scene"/mode switching: 7 named presets (`ContourPresets::apply`) that write a batch of `ofParameter`s directly, switched via GUI buttons — no state machine, no timed/automatic transitions, no history/back-stack. Custom presets can be saved/loaded to XML via `ofxPanel::save/loadToFile`. Otherwise one continuous behavior (single effect, single canvas), with input-mode (image/video/camera) as the only other mode concept, also GUI/keyboard-driven.

**7. Performance & Resource Notes**
- `ofSetFrameRate(30)`. No on-screen FPS-vs-target validation beyond the optional debug overlay.
- Extensive hardware notes in `ContourDisplacementEffect.h` and `README.md`: the real deploy target (Pi 3B/VideoCore IV/GLES2) "does not reliably support sampling textures from a vertex shader" — the explicit reason this effect does one small GPU preprocess pass (`processedFbo`, default 192×108, smaller at lower quality presets: 96×54/64×36) with **one CPU readback per frame** rather than per-vertex GPU texture fetch.
- Known bottleneck documented in README: `ofSetLineWidth()`-based line rendering may be clamped to 1px on core GL profiles; README flags expanding each segment into a camera-facing quad as the fix, explicitly "Not done here."
- No explicit GPU memory budget numbers beyond the FBO resolution figures above.
- Shaders: GLSL `#version 120` for `contour_preprocess.{vert,frag}` and `contour_color.{vert,frag}` (companion source-color pass, only runs when `sourceColorEnabled`). No ping-pong FBO pattern — each FBO is single-buffer, re-rendered fresh each frame.

**8. Input / Control Surface**
- No shell script, systemd unit, or kiosk boot config anywhere in the repo references this sketch.
- No GPIO/MQTT/OSC/network trigger code.
- No crossfade/transition tied to process restart; presets and input mode switch live via keyboard/GUI only.

---

### radar-effects-gallery

**1. Project Inventory**
- Path: `sketches/radar-effects-gallery/`.
- OF 0.12.1, same `Project.xcconfig` settings as contour-portrait.
- **Builds cleanly**, with **zero warnings** in the build log (the only prior sketch to achieve this cleanly along with shader-effect-debugger).
- LOC in `src/`: **769**.
- Git: clean; no nested `.git`.

**2. Build Configuration**
- Makefile: unmodified OF template.
- `addons.make`: `ofxGui`, `ofxOpenCv`. **Note**: grepping this sketch's own `src/` for `ofxCv`/`ofxOpenCv` returns **no matches** — the addon is declared but not actually used by this sketch's own source (present only because `shared/src`'s object set needs it).
- `config.make`: `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src` (active), `PROJECT_EXCLUSIONS =` (empty), with a comment explaining why this sketch carries its own local copies of `RPVideoSampler`/`RPRevealMask` instead of pointing at `radar-pulse/src` directly (would pull in radar-pulse's own `main.cpp`/`ofApp.cpp` and collide at link time).
- Build-log evidence: because the whole `shared/src` tree is pulled in unfiltered, the Makefile compiles files this sketch never `#include`s at all (e.g. `VideoRegionEffectRenderer.cpp`, `ShaderLibrary.cpp`, `VideoRegionController.cpp` all appear in the build log even though `GalleryCompositor`/`ofApp` never reference `ShaderLibrary` or `VideoRegionController`).
- All paths relative. No Pi cross-compile setup, only runtime `#ifdef PLATFORM_PI` branches.

**3. ofApp Structure**
- Members: `RPVideoSampler videoSampler`; `RPRevealMask revealMask`; `GalleryCompositor compositor`; `hud::PulseEmitterWidget pulseEmitter`; `bool showOverlay`.
- `setup()`: 24fps (comment: chosen as "the handoff's target," "a sanity-check number, not a Pi-3B-validated figure"), black bg, `ofDisableArbTex()` → `videoSampler.setup(...)` (`/home/pi/blueprint/media` under `PLATFORM_PI`, else `bin/data/sharedMedia` → symlink to `quadrant-crosshair/bin/data/media`) → `revealMask.setup(1280,720, 0.965f)` + `setMaxOpacity(1.0f)` → `compositor.setup(1280,720)` → `hud::PulseEmitterWidget` themed/bounded/configured (`frequencyMs=900`, `speed=90`, `range=260`, `concurrency=4`, `bandWidth=60`) → 3 fixed emitters at `(0.25,0.3)`, `(0.75,0.3)`, `(0.5,0.75)` of canvas size.
- `update()`: `videoSampler.update()` → `pulseEmitter.update(dt)` → builds a pulse vector (with per-pulse `tint` from `compositor.getEmitterTint()`) and a parallel gallery-pulse-info vector → `revealMask.beginStamp()/stampPulses()/endStamp()/update()` → `compositor.update(dt, pulses)` (drives Mode 7's ghost-text spawn/decay regardless of active mode).
- `draw()`: rebuild pulse-info vector → if video ready, `compositor.draw(videoTex, maskTex, pulses)`, else draw the raw mask texture as fallback → `pulseEmitter.draw()` → overlay text (mode index/name, fps, active pulse count, stamp-blend mode, clip filename, key legend) when `showOverlay`.
- `exit()`: not overridden.
- Input: `n`/`p` next/prev mode (`compositor.nextMode()/prevMode()`), `d` toggles overlay. No mouse/GPIO.
- Window: `OF_WINDOW`, 1280×720, no fullscreen, no multi-monitor.

**4. Video / Media Handling**
- Player: `ofVideoPlayer`, wrapped in sketch-local `RPVideoSampler` — a **byte-for-byte-identical copy** (confirmed via `diff`) of `radar-pulse/src/RPVideoSampler.cpp`; the `.h` differs only in header comments explaining it's a "temporary copy... tied to Radar Reveal-Mask Engineering Handoff v2."
- Max concurrent players: **1**.
- Media convention: scans a folder for every `.mp4`, shuffles a playlist, advances after a *minimum wall-clock duration* (`minPlaySeconds = 30.0f`) at the next natural loop boundary — distinct from `shared/src/VideoSampler`'s seek-and-freeze design, per the class's own header comment.
- Texture format: `GL_RGBA` FBOs throughout (`captureFbo`/`fboA`/`fboB` in `RPRevealMask`).
- **No seek-and-freeze pattern.** Loop-and-rotate playback (`OF_LOOP_NONE` + manual `setPosition(0.0f)`/`play()` re-loop when under `minPlaySeconds`), not seek-then-freeze.

**5. Shared/Duplicated Infrastructure**
- Includes: `"HudElements.h"` → `shared/src/hud/HudElements.h`, using `hud::PulseEmitterWidget` — **this sketch does consume `shared/src/hud`** even though it's excluded from the shared *video-effect* service.
- Local classes that are explicit, documented forks of *sibling-sketch* code (not `shared/src`): `RPVideoSampler.{h,cpp}` (copy of radar-pulse's, header comment: "Temporary copy... this sketch gets deleted alongside the rest of this sketch once a mode wins... Not a shared/ promotion"; `.cpp` identical, confirmed by diff); `RPRevealMask.{h,cpp}` (copy of radar-pulse's, with one deliberate addition: `RPPulseStamp::tint` for Mode 2's per-emitter hue, plus a corresponding 2-line diff in `drawStampMesh()`).
- Local shader/effect registry: `GalleryCompositor` owns a flat `std::array<ofShader, 11>` indexed by mode number, loading `gallery_mode0_passthrough.frag` through `gallery_mode10_heatmaprecolor.frag` — not a name-keyed registry like `ShaderLibrary`. Per `docs/video-effect-second-wave-evaluation.md`, uses a genuinely different uniform contract (`videoTex`/`maskTex`/`texCoordVarying`, "Contract B") vs. the migrated sketches' Contract A.
- Mode 7 ("Telemetry Ghosting") is a local, minimal reimplementation of ghost-text overlay rather than using `AnnotationRenderer`.
- Mode 10 (`gallery_mode10_heatmaprecolor.frag`) is explicitly a hand-port of `shared/src/shaders/effects/heatmap_recolor.glsl`'s luminance→palette-gradient logic, adapted to this sketch's Contract-B uniform convention, with palette/alpha/gamma/etc. set as **hardcoded C++ literals** in `GalleryCompositor::applyModeUniforms()` (`alpha=1.0`, `intensity=1.0`, `gamma=0.9`, `minLuminance=0.05`, `maxLuminance=0.95`, `palette=2` "Solarpunk Botanical", `reverse=0`) rather than exposed `ofParameter`s.
- Local constants: `kCanvasWidth=1280`/`kCanvasHeight=720` — matches contour-portrait's and radar-pulse's identical local constants (no shared `Settings.h` canvas-size constant exists to diverge from). `PulseEmitterConfig` values are set identically in radar-pulse's `ofApp.cpp` too — same literals, independently declared in each `ofApp::setup()`.

**6. Scene/State Lifecycle**
- State is a single `int modeIndex` cycling through `NUM_MODES=11` (`nextMode()`/`prevMode()`, modulo arithmetic, no enum names — modes identified by index into parallel arrays). One continuous video+pulse-emitter+reveal-mask pipeline, with the *rendering mode* as the only cycled state, driven by keyboard (`n`/`p`), not automatic/timed. No broader scene manager.

**7. Performance & Resource Notes**
- `ofSetFrameRate(24)`, with an explicit comment that this is "a sanity check that the harness itself isn't pathologically expensive, not as a stand-in for the Pi 3B numbers."
- `GalleryCompositor`'s class comment states the one-shader-per-mode design (vs. a single uber-shader with a mode uniform) follows "the doc's own recommendation... to avoid GLSL ES dynamic-branch instruction-count risk on VideoCore IV-class GPUs," while noting "whether that risk is real on this specific GPU/driver is an open question the doc explicitly asks to confirm on-device... not something this desktop build can settle."
- No explicit GPU memory budget figures for this sketch specifically.
- Shaders: all `#version 120`. 11 mode fragments + shared `gallery_passthrough.vert`, plus `reveal_mask.{vert,frag}` for `RPRevealMask`. `RPRevealMask` implements ping-pong FBOs (`fboA`/`fboB`, flipped each `update()`) with GL_MAX blend-equation compositing when available (falls back to sorted alpha-over otherwise); the mask shader does decay-then-max blending, not ping-pong-blended mix.

**8. Input / Control Surface**
- No launch script/systemd/kiosk config references this sketch.
- No GPIO/MQTT/OSC/network code.
- No crossfade/restart-tied transition logic; mode switching is live keyboard only.

---

### radar-pulse

**1. Project Inventory**
- Path: `sketches/radar-pulse/`.
- OF 0.12.1, same `Project.xcconfig` settings as the other two.
- **Does NOT build cleanly in Release.** `make Release -j4` fails with a genuine compile error:
  ```
  Compiling ../../shared/src//BlobDetector.cpp
  In file included from ../../shared/src//BlobDetector.cpp:1:
  ../../shared/src/BlobDetector.h:5:10: fatal error: 'ofxCvContourFinder.h' file not found
      5 | #include "ofxCvContourFinder.h"
        |          ^~~~~~~~~~~~~~~~~~~~~~
  1 error generated.
  make[1]: *** [obj/osx/Release//BlobDetector.o] Error 1
  make[1]: *** Waiting for unfinished jobs....
  make: *** [Release] Error 2
  ```
  Root cause, directly traceable in this sketch's own config: `config.make` sets `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src` with empty `PROJECT_EXCLUSIONS` — the whole `shared/src` tree, including `BlobDetector.cpp` (which needs `ofxOpenCv`'s `ofxCvContourFinder.h`), is compiled unconditionally. `addons.make` lists only `ofxGui` — **no `ofxOpenCv`** — unlike `radar-effects-gallery` and `contour-portrait`, both of which declare `ofxOpenCv`. No `bin/radar-pulse` Release binary/app exists on disk (only a pre-existing `bin/radar-pulse_debug.app` from an earlier Debug build, dated Jul 31 19:02, predating this session).
- LOC in `src/`: **561**.
- Git: clean; no nested `.git`.

**2. Build Configuration**
- Makefile: unmodified OF template.
- `addons.make`: `ofxGui` only — the direct cause of the build failure above.
- `config.make`: `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src`, `PROJECT_EXCLUSIONS =` (empty), with a comment reasoning that no local source file collides by name with anything under `shared/src`, "so it stays unexcluded" — this comment addresses *name-collision* exclusion, not the addon-dependency gap that actually breaks the build.
- All paths relative; no Pi cross-compile setup, only runtime `#ifdef PLATFORM_PI` branches (e.g. for the media path).

**3. ofApp Structure**
- Members: `RPVideoSampler videoSampler`; `RPRevealMask revealMask`; `RPCompositor compositor`; `hud::PulseEmitterWidget pulseEmitter`; `bool showDebugOverlay`; three per-stage timing floats (`msVideoUpdate`, `msMaskStampAndDecay`, `msComposite`), described in a comment as "Milestone 4/5/9 instrumentation... so the Pi 3B go/no-go measurements the engineering handoff calls for can actually be read off on target hardware"; private helper `addEmitterAtRandomPosition()`.
- `setup()`: 60fps (the highest target of the three radar/contour sketches), black bg, `ofDisableArbTex()` → `videoSampler.setup(...)` → `revealMask.setup(1280,720,0.965f)` + `setMaxOpacity(1.0f)` → `compositor.setup()` + `setColorThreshold(0.55f)` → `pulseEmitter` configured with the *same literal values* as radar-effects-gallery; 3 fixed emitters at the same normalized positions, with a comment flagging "the engineering handoff leaves 'placement strategy with no pointer input on a headless kiosk' as an open question... This is a placeholder."
- `update()`: times each stage (`ofGetElapsedTimeMicros()`): `videoSampler.update()` → `msVideoUpdate`; `pulseEmitter.update(dt)`; builds an `RPPulseStamp` vector (no `tint` field — radar-pulse's copy of the stamp struct doesn't have RG's tint addition); `revealMask.beginStamp()/stampPulses()/endStamp()/update()` → `msMaskStampAndDecay`.
- `draw()`: times `compositor.draw(...)` (or mask-only fallback) → `msComposite`; `pulseEmitter.draw()`; debug overlay shows fps + all three stage timings + active pulse count + stamp-blend mode + clip filename + key legend.
- `exit()`: not overridden.
- Input: `a` adds an emitter at a random position, `d` toggles debug overlay. No mouse/GPIO.
- Window: `OF_WINDOW`, 1280×720, no fullscreen, no multi-monitor.

**4. Video / Media Handling**
- Player: `ofVideoPlayer`, wrapped in `RPVideoSampler` — the **original** that radar-effects-gallery's copy is derived from.
- Max concurrent players: **1**.
- Media convention: identical shuffle-playlist + minimum-duration rotation as RG's copy; path `bin/data/sharedMedia` (symlink to `quadrant-crosshair/bin/data/media`) or `/home/pi/blueprint/media` on Pi. `bin/data/media/` here contains only a `.gitkeep`.
- Texture format: `GL_RGBA` FBOs.
- **No seek-and-freeze pattern** — same continuous loop-and-rotate playback as RG.

**5. Shared/Duplicated Infrastructure**
- Includes: `"HudElements.h"` → `hud::PulseEmitterWidget`, same pattern as RG.
- `RPVideoSampler`/`RPRevealMask`/`RPCompositor` here are the **originals** RG copies (RG's own headers name this sketch as the source). `RPRevealMask.h`'s own header explicitly documents why it is *not* built on `shared/src/ErosionFBO`: "Structured like shared/src/ErosionFBO.h/.cpp's proven ping-pong lifecycle... but deliberately NOT that class: ErosionFBO's shader does a normalized mix(current, history, decayRate), which would dilute an overlapping bright pulse... This class's shader (reveal_mask.frag) does decay-then-max instead... Kept local to this sketch per the engineering handoff — this risk... shouldn't leak into shared/src/hud and silently affect sketches that never touch this feature."
- `RPCompositor` is a single fixed shader wrapper (`color_reveal.{vert,frag}`) — no registry, no swappable effects. `color_reveal.frag` implements its own luma-based binary desaturate↔color-reveal recolor driven by `maskValue` (matching `desaturate.frag`/`erosion.frag`'s luma formula elsewhere in the repo "for visual/behavioral consistency," per its own comment, but not routed through any shared effect class).
- Local constants: `kCanvasWidth=1280`/`kCanvasHeight=720` (identical literal to both siblings); `colorThreshold=0.55f`; `trailPersistence=0.965f`/`maxOpacity=1.0f` — all independently declared, no shared `Settings.h` equivalent.

**6. Scene/State Lifecycle**
No mode/state enum, no scene switching at all — per `docs/video-effect-second-wave-evaluation.md`'s characterization, a single fixed two-stage pipeline (`RPRevealMask` → `RPCompositor`) that always runs, with only emitter count (`a` key) and overlay visibility (`d` key) as runtime-adjustable state.

**7. Performance & Resource Notes**
- `ofSetFrameRate(60)` — the highest target of the three.
- Explicit per-stage instrumentation exists specifically, per the code comment, because "this sketch can't make that [go/no-go] call itself; it can only make the numbers visible" for the Pi 3B — desktop-measured proxies, not on-device Pi numbers.
- Same `ofDisableArbTex()` rationale as the other two (GLES2/desktop-GL sampler-type mismatch note).
- Shaders: `#version 120` for `color_reveal.{vert,frag}` and `reveal_mask.{vert,frag}`; same ping-pong FBO / GL_MAX-blend-with-fallback pattern as radar-effects-gallery (same class family).
- No explicit GPU memory budget figures.

**8. Input / Control Surface**
- No launch script/systemd/kiosk config references this sketch (only doc mentions).
- No GPIO/MQTT/OSC/network code (`addons.make` lists only `ofxGui`).
- No crossfade/restart-tied transition logic; only runtime controls are `a`/`d`.

**Cross-cutting note (RG/RP/CP)**: all three declare identical local `kCanvasWidth=1280`/`kCanvasHeight=720` constants independently, and all three windows are `OF_WINDOW` at 1280×720, never fullscreen, single monitor. All shaders across the three declare `#version 120` and guard `precision mediump float` behind `#ifdef PLATFORM_PI`. RG and RP share `hud::PulseEmitterWidget` with identical config literals and identical 3-emitter placement, independently declared in each `ofApp::setup()`. Of the three, **only radar-pulse currently fails a clean `make Release -j4` build**, reproducibly, from its own `addons.make`/`config.make` combination.

---

### fragment-trail

**1. Project Inventory**
- Path: `sketches/fragment-trail/`.
- OF 0.12.1.
- **Builds cleanly.** Zero warnings/errors on the sketch's own files (only benign `-jN forced in submake` make-tool noise).
- LOC in `src/`: **2,520**.
- Git: clean, no nested `.git`. A `HANDOFF.md` exists at the sketch root (not one of the 8 requested sections, but directly relevant to §4/§6 below).

**2. Build Configuration**
- Makefile: unmodified OF template.
- `addons.make`: `ofxGui`.
- `config.make` (full, with its extensive comment block quoted since the exclusion mechanics are directly relevant to future consolidation work):
  ```make
  OF_ROOT = ../../../../..

  PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src
  PROJECT_EXCLUSIONS = ../../shared/src ../../shared/src/video-effects%
  ```
  The comment explains: `shared/src` is included (recursively contributing `hud/`/`ridgeline/` subdirectories), but its own top level is excluded wholesale because `CrosshairSystem`, `TriggerBus`, `ShaderLibrary`, and `LFOBank` are **not** reused from `shared/src` — those names collide with unrelated blueprint_emergence-owned classes of the same name at `shared/src`'s top level. `TimeOffsetVideoBuffer` (needed here) is *also* kept as a local copy despite having no name collision, because an attempted narrower exclusion (individual colliding files only) was tried first and **failed**: a real Debug build confirmed excluding individual files within an already-included directory doesn't reliably work in this Makefile system (`LFOBank.cpp`/`ShaderLibrary.cpp` were compiled/linked in anyway, producing duplicate-symbol link errors). The actual, working mechanism (per `config.project.mk`'s `OF_PROJECT_SOURCE_PATHS = $(filter-out $(OF_PROJECT_EXCLUSIONS), ...)`): each `PROJECT_EXCLUSIONS` entry is matched against the full list of *individually-discovered subdirectories* under `PROJECT_EXTERNAL_SOURCE_PATHS`, **not** matched recursively against a directory tree. A bare `../../shared/src` entry excludes *only* files sitting directly in that top-level directory; subdirectories like `hud/`, `ridgeline/`, and `video-effects/` are each their own separate discovered entries and need their own exclusion pattern to be dropped. `../../shared/src/video-effects%` (Make's `%` wildcard) matches every nested `video-effects/*` subdirectory as a suffix pattern — confirmed this is what actually excludes the whole subtree (a bare second entry without the wildcard was confirmed to leave `shared/src/video-effects/effects/` still compiled). `video-effects%` was added because pulling in `video-effects/` transitively needs `MotionExtraction.h`, unreachable once the top-level exclusion drops it.
  - **Verified against the actual build's include-path list**: compile invocation includes `-I../../shared/src/shaders`, `-I../../shared/src/shaders/effects`, `-I../../shared/src/ridgeline`, `-I../../shared/src/hud_overlay`, `-I../../shared/src/hud_overlay/organisms`, `-I../../shared/src/hud`, plus 28 individual widget dirs — and does **not** include bare `-I../../shared/src` or any `video-effects*` path, matching the comment's claimed mechanism exactly.
- No `PLATFORM_PI` conditionals anywhere in this sketch's `src/` — no cross-compile branches exist today, despite the stated Pi 3B target. `ofSetFrameRate(24)` in `main.cpp` is commented `// Pi 3B render target — see brief Section 7`.
- No hardcoded absolute paths; media path is relative `"media"`.

**3. ofApp Structure**
- Members: `TimeOffsetVideoBuffer videoBuffer`; `CrosshairSystem crosshair`; `TriggerBus triggerBus`; `LFOBank lfoBank`; `ShaderLibrary shaderLib`; `FTFragmentPool pool`; `FTModeController modeController`; `FTParameterPanel paramPanel`; `FTBackgroundLayer backgroundLayer`; `hudoverlay::HudOverlayLayer hudOverlay`; `hudoverlay::HudOverlayDialPanel hudOverlayPanel`; `hudoverlay::HudOverlayDialState hudOverlayDials`; `FTOverlayDirector overlayDirector`; `static constexpr int kNumPlayheads = 6`.
- `setup()`: `ofSetVerticalSync(true)` → `ofDisableArbTex()` → `crosshair.setup()` then `crosshair.setPreset(2)` ("HUNT") → `triggerBus.setup()`/`lfoBank.setup()`/`shaderLib.setup()`/`modeController.setup()`/`paramPanel.setup()` → wire `triggerBus.addListener(...)` → `modeController.onTriggerEvent` → `videoBuffer.setup("media", {numPlayheads=6, ...})` → `backgroundLayer.setup(&videoBuffer, "backgrounds", ...)` → `pool.setup(...)` with panel-derived spawn/decay params → `hudOverlay.setup(...)` → set initial `HudOverlayDialState` (intensity 0.18, discipline 0.85, eventCoupling 0.15, with inline rationale comments) → `hudOverlay.setSchedulerEnabled(false)` → `hudOverlay.setFaultCascadeHalftoneEnabled(false)` → `overlayDirector.setup(&pool, &hudOverlay)`.
- `update()`: bail if `dt <= 0` → `lfoBank.update(dt)` → `crosshair.applyLiveSettings(...)` (pulls every crosshair dial from `paramPanel`) → `crosshair.update(dt, lfoBank)` → `triggerBus.update(crosshair.getState(), dt)` → `videoBuffer.update(dt)` → `backgroundLayer.update(dt)` → `modeController.setDwellDuration(...)` + `modeController.update(dt)` → `pool.applyLiveSettings(...)` → compute `energyScale` from crosshair speed vs. duplicated `kSpeedHighThresh=4.0f`/`kSpeedLowThresh=0.8f` constants (a file-local namespace block, commented as duplicated because `TriggerBus`'s real thresholds are private) → `pool.update(...)` with speed-scaled spawn thresholds → `hudOverlayPanel.update(...)` → `overlayDirector.update(...)` → `hudOverlay.update(...)` (deliberately last).
- `draw()`: `ofBackground(8,10,9)` → `backgroundLayer.draw()` → `hudOverlay.drawAmbient()` → `pool.draw(videoBuffer, shaderLib)` → `hudOverlay.drawOrganisms()` → `crosshair.draw()` → `hudOverlayPanel.draw()` → `paramPanel.draw()`.
- `exit()`: not overridden.
- Input: `keyPressed()` forwards every key to `modeController.onKeyPressed(key)` unconditionally, then handles `'h'/'H'` (toggle `paramPanel` visibility), `'p'/'P'` (`crosshair.nextPreset()`), **`'o'/'O'` (toggle only the `hudOverlayPanel` visibility — explicit comment noting this differs from the other three sketches where `'o'` swaps the whole scene)**. `FTModeController::onKeyPressed` additionally handles `'1'/'2'/'3'` (direct content-mode select) and `'m'/'M'` (cycle mode-switch strategy). GUI: `ofxGui`-based `FTParameterPanel` (4 `ofParameterGroup`s), values polled every frame, no `addListener` callbacks. A second panel (`hudOverlayPanel`) drives the overlay's 3 dials.
- Window: fixed 1280×720 `OF_WINDOW`, not fullscreen, `ofSetFrameRate(24)`. `windowResized` only forwards to `hudOverlay.windowResized(...)`. No multi-monitor logic.

**4. Video / Media Handling**
- Player: one `ofVideoPlayer` inside a **local fork** of `TimeOffsetVideoBuffer` (`src/TimeOffsetVideoBuffer.h/.cpp`). Wrapped, not inlined.
- Max concurrent players: **1**. The "6 playheads" are texture slots fed from a CPU-side history ring buffer, not extra decoders.
- Media path: folder scan of `bin/data/media/`. Per `HANDOFF.md`, populated with **per-file symlinks** (not a single directory symlink like temporal-fields) into blueprint_emergence's 26 clips — a workaround because `rmdir` on a pre-existing empty `media/` dir was denied.
- Texture format: `ofDisableArbTex()` called before any allocation — forces `GL_TEXTURE_2D` with normalized `[0,1]` texcoords.
- **No seek-and-freeze pattern** — same "never seeks" design as `shared/src/TimeOffsetVideoBuffer`.
- **Notable bug-history detail**: `HANDOFF.md` documents an unresolved "fragments render as a flat, detail-free colored fill instead of video" bug with several ruled-out hypotheses (scissor/shader/zoom). The **current code** contains a comment describing the actual root cause as since found: `theme.colors.background = ofColor(0, 20, 16, 36)` — "Background alpha must stay low... This was the actual cause of the 'flat, detail-free fill' bug: 160 (63% opacity) drowned the video in a near-opaque wash after every fragment's content draw." This is a different mechanism than `HANDOFF.md`'s "recommended next steps" (scissor bisection, coordinate-scale mismatch) were chasing — the two documents describe different stages/theories of the same investigation, and the doc has not been updated to reflect the resolution.

**5. Shared/Duplicated Infrastructure**
- Shared/src actually used (confirmed against the build's `-I` list): `hud/HudElements.h` and its widgets (`hud::WindowChrome`, `hud::HexGridWidget`, `hud::HudTheme`), `hud_overlay/` (`HudOverlayLayer.h`, `HudOverlayDialPanel.h`, `HudOverlayDialState.h`), and transitively `ridgeline/RidgelineRenderer` (linked but not directly included by any FT file). **Not used at all**: `Settings.h`, `GridSystem`, `AnnotationRenderer`, `Fragment`, `BlobDetector`/`BlobTracker`, `VideoSampler`, any `VideoRegion*`, `MotionExtraction`, `ErosionFBO`, `CompositionBase`.
- **The critical fragment-trail-specific item — four local forks of shared/src classes**, diffed directly against `shared/src`:
  - **`ShaderLibrary`** — diverged. Shared version loads 18 effects from `shaders/effects/*.glsl`; FT's local copy loads 9 effects from a flat `shaders/*.glsl` path with different formatting/logging, and additionally `shaders.erase(name)`s on load failure (shared's version does not).
  - **`LFOBank`** — diverged. Shared version is fully generalized (`setup(int numLanes)`, caller-owned lane semantics, has `getUnipolar()`/`setFrequency()`/`setPhaseOffset()`). FT's local copy is "a trimmed copy of quadrant-crosshair's LFOBank" (per its own header comment) — fixed single-lane `std::array`, no unipolar/frequency-setter API.
  - **`TriggerBus`** — diverged from `shared/src` (fully generic, no `CrosshairState` knowledge), **but byte-identical to quadrant-crosshair's own `TriggerBus.h`/`.cpp`** (confirmed via `cmp`, no diff) — FT's copy is not an independent fork, it's a verbatim copy of quadrant-crosshair's sketch-specific version, both diverged from the shared generic one together.
  - **`TimeOffsetVideoBuffer`** — `.cpp` is **byte-identical** to `shared/src/TimeOffsetVideoBuffer.cpp` (confirmed via `diff`, no output). `.h` differs only in that `shared/src`'s header has one extra method FT's copy lacks: `getRawVideoPixels()` (added for a `BlobDetector`-style CPU consumer) — everything else matches.
  - **`CrosshairSystem`** has **no shared/src counterpart at all** — exists only as independent copies inside `fragment-trail/src/` and `quadrant-crosshair/src/`, and those two copies are themselves **not identical**: FT's version adds an entire `applyLiveSettings(...)` live-tuning API that quadrant-crosshair's base version doesn't have.
- Local constants: no `Settings.h` include, no grid — sketch-local values (`kZoom=1.8f`, HUD theme colors, `kVideoAlpha=0.28f`/`kVideoSaturation=0.5f`) have no shared equivalent to diverge from.

**6. Scene/State Lifecycle**
- Explicit enum-based content-mode system: `FTContentMode` — `TIME_SLICE` (Mode A, playhead-pool driven), `EFFECT_VARIED` (Mode B, one shader per fragment off the live feed), `EFFECT_PARAM_VARIANT` (Mode C, one shared shader with jittered per-fragment params). A second enum, `FTModeSwitchStrategy` — `TIMER` (auto-rotates every `dwellDuration`, default 12s, cycling A→B→C→A), `TRIGGER_BUS` (`VELOCITY_HIGH`→EFFECT_VARIED, `VELOCITY_LOW`→TIME_SLICE, wired through `TriggerBus::addListener`), `MANUAL_KEY` (default; `'1'/'2'/'3'` direct-select, `'m'/'M'` cycles which strategy is active) — a plain `enum class` + small state machine in `FTModeController`, not a class hierarchy or string-based system.
- Each spawned `FTFragment` has its own independent age/decay lifecycle (`age`, `sustain`, `decay`, `isExpired()`) — a per-object particle lifecycle, not a scene-level phase system.
- No scene-level phase/state machine beyond the content-mode switch — one continuous behavior (crosshair-driven spawning) with the *content mode* of newly-spawned fragments switchable at runtime.

**7. Performance & Resource Notes**
- `ofSetFrameRate(24)` with an inline "Pi 3B render target" comment.
- Explicit GPU memory budget note in the local `TimeOffsetVideoBuffer.h` (identical rationale to shared's): Pi 3B has 1GB shared RAM, a naive texture-per-history-frame approach would burn ~220MB, hence CPU-resident `ofPixels` history. Also flags "real on-device memory measurement is still outstanding."
- `FTFragment.cpp` and `FTFragmentPool` note zero-width/height scissor guards, "same guard quadrant-crosshair's Quadrant::draw() uses" — explicit VideoCore IV GL-error avoidance.
- `FTFragmentPool.h` caps the playhead pool at 6 independent of `maxFragments` (default 18) — "kept small... not raised to match maxFragments."
- Shaders: all `#version 120`, legacy fixed-function attributes (`gl_Vertex`, `gl_ModelViewProjectionMatrix`, `gl_MultiTexCoord0`). 10 shader files loaded through the local `ShaderLibrary` + a separate `bg_dim.glsl` used directly by `FTBackgroundLayer`. No FBOs/ping-pong anywhere — all effects are single-pass, per-fragment `glScissor` + direct texture draw.
- No measured FPS figures anywhere beyond the target-rate statement.

**8. Input / Control Surface**
- No shell script, systemd unit, or kiosk boot config anywhere references this sketch. Manual launch only (`make RunRelease` / `open bin/fragment-trail.app`), per `HANDOFF.md`.
- No GPIO/MQTT/OSC/network code.
- No crossfade/restart-tied transition logic; media advancement and mode switching are all in-process.

---

### quadrant-crosshair

**1. Project Inventory**
- Path: `sketches/quadrant-crosshair/`.
- OF 0.12.1.
- **Builds cleanly.** 3 real compiler warnings (not errors): `Quadrant.cpp:8:8: warning: unused function 'sampleAverageBrightness'` (dead code — this sketch separately builds its own lazily-initialized `videoeffects::VideoEffectRegistry` for default uniforms, and this older brightness helper is no longer called); three `DebugMode.cpp` deprecation warnings (`'ofClear' is deprecated: Use ofClear(brightness, alpha)`, at 3 call sites).
- LOC: **4,491** total.
- Git: clean, no nested `.git`.
- **Extra directories unique to this sketch**: a top-level `docs/` folder (`keybindings.md`, `update-docs.md` — a very detailed "system reference" spec of the whole sketch) plus a top-level `quadrant-crosshair-design-to-engineering-handoff.md` (34KB); and **both** a `data/` directory and a `bin/data/` directory — two separate, non-symlinked copies of the same shader-asset tree (`data/shaders/*.glsl`, `data/of_nature_shader_pack_glsl/*.glsl` mirrored under `bin/data/`), confirmed via `file`/`ls` (both plain directories, not symlinks).

**2. Build Configuration**
- Makefile: unmodified OF template, identical structure to fragment-trail's.
- `addons.make`: `ofxGui`.
- `config.make` (full, comment quoted in full since it directly parallels fragment-trail's exclusion problem):
  ```make
  # A single external source path is required here: this OF version's
  # Makefile system uses PROJECT_EXTERNAL_SOURCE_PATHS in a plain $(subst)
  # (and in a pattern-rule prerequisite) that only works correctly with one
  # value, not a space-separated list. Point at the shared/src/ parent (which
  # recursively contributes its hud/ and ridgeline/ subdirectories as source
  # paths) and exclude the parent's own top-level sources, which are
  # blueprint_emergence-specific classes (GridState, LFOBank, MotionExtraction,
  # ShaderLibrary, TriggerBus, ...) that collide by name with quadrant-crosshair's
  # own independent implementations of the same classes in src/.
  PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src
  PROJECT_EXCLUSIONS = ../../shared/src
  ```
  A **single** bare exclusion entry (no `%` wildcard, no second `video-effects%` entry) — unlike fragment-trail's two-entry exclusion. This works here specifically because quadrant-crosshair's production path *wants* `shared/src/video-effects/` compiled in (see §5/§6) — it only needs to drop the top-level colliding files, not the whole `video-effects` subtree.
  - Confirmed via the actual build's `-I` list: includes `-I../../shared/src/video-effects` and all its subdirs (`core`, `catalog`, `knowledge`, `evolution`, `effects`) — unlike fragment-trail — plus the same `hud/`, `hud_overlay/`, `ridgeline/` set.
- **`PLATFORM_PI` conditionals present** (unlike fragment-trail/shader-effect-debugger): in `VideoSystem.cpp`, `ofApp.cpp`, `MotionExtraction.h/.cpp`. Per `docs/update-docs.md`, these gate: media path, whether a desktop-only brightness/contrast/saturation FBO pass runs, CPU sampling stride (every 64px on Pi vs 16px desktop), and motion-history ring buffer depth (15 vs 60 frames).
- No hardcoded absolute paths for bin/data.

**3. ofApp Structure**
- Members: `ShaderLibrary shaders`; `VideoSystem video`; `LFOBank lfo`; `GridState grid`; `CrosshairSystem crosshair`; `TriggerBus triggerBus`; `QuadrantManager quadrants`; `MotionExtraction motionEx`; `ExpansionDirector expansionDirector`; `float steeringBrightness`; `int motionOverlayAlpha`; `bool showHUD`; `DebugMode debug`; `float motionStillTimer`; `float motionRampAlpha`; `HudManager hud`; `hudoverlay::HudOverlayLayer hudOverlay`; `hudoverlay::HudOverlayDialPanel hudOverlayPanel`; `hudoverlay::HudOverlayDialState hudOverlayDials`; `bool hudOverlayActive`; `static float sampleBrightness(...)`.
- `setup()`: `ofSetFrameRate(24)`, `ofSetVerticalSync(true)`, `ofBackground(13,13,13)`, `ofHideCursor()`, `ofDisableArbTex()` → `shaders.setup()` → `video.setup(...)` (Pi vs desktop media path via `#ifdef PLATFORM_PI`) → `lfo.setup()`/`grid.setup()`/`crosshair.setup()`/`triggerBus.setup()` → `quadrants.setup(&shaders)` → `motionEx.setup()` → `debug.setup(&shaders, &video, &motionEx)` → two `triggerBus.addListener(...)` registrations (→ `quadrants.onTrigger`, → crosshair bloom/fill) → `hud.setup(triggerBus, crosshair, lfo, quadrants, motionEx)` → `expansionDirector.setup()` → `hudOverlay.setup(...)`/`hudOverlayPanel.setup()`.
- `update()`: early-returns if `hudOverlayActive` (only updates overlay+panel) or `debug.active` (only updates `debug`). Otherwise: `video.update()` (reset erosion FBOs + notify HUD on file change) → `lfo.update(dt)` → if `video.isFrameNew()`: sample crosshair color/`steeringBrightness`, run `motionEx.update(...)` with LFO-driven decay/sensitivity, feed motion centroid to crosshair attractor, switch `MotionExtraction` output mode on energy thresholds, ramp `motionOverlayAlpha` → `crosshair.update(dt, lfo)` → `expansionDirector.update(...)` + react to state transitions (hand crosshair control, `triggerBus.clearAll()`, `quadrants.beginContraction(...)`, `crosshair.beginResume()`) → `triggerBus.update(...)` (suppressed while expansion active) → `grid.update(...)` + `grid.uploadTexture()` → `quadrants.update(...)` → `hud.update(dt)`.
- `draw()`: same early-return pattern. Otherwise: background clear → motion overlay texture full-screen if alpha>0 (behind quadrants) → `quadrants.draw(video.getTexture(), ...)` → `hud.draw(uiAlpha)` if `showHUD` → `crosshair.draw(uiAlpha)` (always last, on top).
- `exit()`: not overridden.
- Input: `keyPressed` handles `'o'/'O'` (toggle `hudOverlayActive`, full-replace), `'d'/'D'` (toggle `debug.active`, and if active, delegates to `debug.keyPressed(key)` first), `1`-`5` (crosshair preset), `TAB` (next preset), `n/N` (next video), `f/F` (fullscreen toggle), `[`/`]` (motion overlay alpha), `m/M` (motion output mode), `h/H` (HUD toggle), `ESC` (quit), `e/E` + `F1`-`F4` (expansion trigger, random/specific quadrant). **`DebugMode`'s parameter panel is NOT `ofxGui`** — a hand-rolled system: a `DebugParam{label, float* value, step, min, max}` struct and `buildParams()` returning a vector of raw pointers into ~50 individual `float pXxx` member fields (one per shader's tunable uniforms), navigated via `↑/↓` (select) and `=`/`-`/`+`/`_` (fine/coarse step) — entirely keyboard-driven, no GUI widgets. `DebugMode` also owns 6 `TriggerSim` structs letting each `TriggerBus` trigger be manually simulated (`E/V/L/C/K/W`) without the crosshair actually producing it.
- **Doc-vs-code discrepancy found**: `docs/keybindings.md`/`docs/update-docs.md` describe `'o'/'O'` as "Toggle motion overlay depth: behind quadrants ↔ in front of quadrants." The **actual code** has `'o'/'O'` toggle `hudOverlayActive` — a full-replace standalone `HudOverlayLayer` mode — not a motion-overlay depth toggle. No motion-overlay-depth-toggle code exists anywhere in current `src/`. This is a genuine documentation/code mismatch.
- **Second doc-vs-code discrepancy**: `docs/update-docs.md` §01 and `docs/quadrant-crosshair-reuse-analysis.md` both describe a `src/hud_elements/` directory as "a self-contained, Pi-friendly vector widget library... checked into src/." **No `src/hud_elements/` directory exists** in this sketch. `HudManager.h` instead `#include`s `"HudElements.h"`, which resolves to `shared/src/hud/HudElements.h`. Both docs predate this sketch's adoption of the shared `hud/` library.
- Window: no explicit size-setting differs materially from the other sketches; fullscreen toggled at runtime via `f/F`; no multi-monitor logic.

**4. Video / Media Handling**
- Player: `VideoSystem` wraps a single `ofVideoPlayer` — not inlined in `ofApp`. Per `docs/update-docs.md` §06: "Single `ofVideoPlayer` instance, always — a hard Pi 3B constraint, never relaxed."
- Max concurrent players: **1**.
- Media path: scans `.mp4` files, builds a shuffled playlist (avoiding repeating the seam file between cycles); each file loops a random `loopMin`(3)-`loopMax`(5) times before advancing. Path: `/home/pi/blueprint/media` on Pi, `ofToDataPath("media", true)` elsewhere.
- Texture format: `ofDisableArbTex()`; on desktop (`#ifndef PLATFORM_PI`) a brightness/contrast/saturation correction pass runs through `video_adjust.glsl` into an FBO; on Pi, `getTexture()` returns the raw player texture directly.
- **No seek-and-freeze pattern** — no `setPaused`/`freeze`/`seekToFrame`/`setFrame` calls anywhere. Video always plays forward and loops; the only "hold" concepts are `ExpansionDirector`'s HOLD state (holds the *crosshair position/quadrant layout*, not the video) and `Quadrant`'s scale/morph — the underlying video texture keeps advancing throughout.

**5. Shared/Duplicated Infrastructure**
- Shared/src actually used (confirmed via build `-I` list): `hud/` (all widgets via `HudElements.h`), `hud_overlay/` (`HudOverlayLayer`, `HudOverlayDialPanel`, `HudOverlayDialState`), `ridgeline/RidgelineRenderer` (used directly — `DebugMode.h` owns a `ridgelineRenderer` member; also used inside `Quadrant`'s "ridgeline" effect slot), and **`video-effects/`** — this sketch's most significant shared consumption: `Quadrant.cpp` builds a lazily-initialized, process-lifetime `videoeffects::VideoEffectRegistry` (comment: "Mirrors shared/src/VideoRegionEffectRenderer.cpp's identical helper") that its fallback path reads `VideoEffectDefinition::params` from, to set each uniform's canonical default value. This is a **lighter-weight integration** than shader-effect-debugger's — it only reads default values for its own locally-owned `ShaderLibrary` shaders, it does not use `VideoEffectService`/`VideoEffectInstance::render()` — matching CLAUDE.md's characterization of this being the "production path... fixed-default uniforms source from the shared catalog."
- Local forks of shared/src classes, **all diverged** (confirmed via `diff -q`, no byte-identical pairs): `ShaderLibrary.h/.cpp`, `LFOBank.h/.cpp`, `TriggerBus.h/.cpp`, `MotionExtraction.h/.cpp`, `GridState.h/.cpp`. `docs/update-docs.md` documents the full internal shape of each (e.g. 12-lane `LFOBank` vs. shared's variable-lane generic version; 6-trigger hardcoded `TriggerBus` vs. shared's generic index-based bus; 24×18 `GridState` vs. shared's own, which per `docs/quadrant-crosshair-reuse-analysis.md` "serves a different purpose than blueprint_emergence's existing 6×8 occupancy GridSystem").
  - `TriggerBus.h`/`.cpp` are **byte-identical to fragment-trail's copies** (confirmed via `cmp`) — the fork lineage is quadrant-crosshair → copied verbatim into fragment-trail, both independently diverged from `shared/src/TriggerBus`.
  - `CrosshairSystem` has no `shared/src` equivalent at all; fragment-trail's copy is a superset fork of this sketch's version.
- **`docs/quadrant-crosshair-reuse-analysis.md` is confirmed a stale historical snapshot**: it states "quadrant-crosshair is fully self-contained inside its own `src/`" and "does not use [shared/src] at all" — directly contradicted by the current build, which pulls in `hud/`, `hud_overlay/`, `ridgeline/`, and `video-effects/` per the config.make/`-I` evidence above. This doc predates the sketch's shared-library/video-effect-service migration.
- **Duplicate asset noted in that same stale doc, still true today**: "`desaturate.glsl` currently exists as two independent copies — one in `shared/src/shaders/`, one in `quadrant-crosshair/data/shaders/`." Confirmed still the case.
- Local constants: `GridState::COLS=24, ROWS=18` has no `Settings.h` equivalent (never includes `Settings.h`). `TriggerBus`'s local constants (`EDGE_ZONE=100`, `HIGH_THRESH=4.0`, `LOW_THRESH=0.8`, etc.) match exactly what was copied verbatim into fragment-trail's `TriggerBus`.

**6. Scene/State Lifecycle**
- `ExpansionState` enum: `{ IDLE, TRAVEL_OUT, HOLD, TRAVEL_BACK }` — a periodic finite-state sequence sending the crosshair to a corner and holding one quadrant near-fullscreen, auto-firing every 120-300s in IDLE or manually via `'e'/'E'`/`F1`-`F4`. `getUIFadeAlpha()` fades crosshair/HUD out over the last 10% of travel-out and back in over the first 40% of travel-back.
- `QuadrantManager`'s per-quadrant shader cycling: each `Quadrant` runs an independent `PLAYING → SILENCING → READY` cycle (per `docs/update-docs.md` §08; corroborated by `ShaderSlot` states `IDLE/FADE_IN/ACTIVE/FADE_OUT`), popping shader names from a per-quadrant shuffled deck refilled from a shared pool, with randomized dwell (54-72s)/fade(15-30s)/silence-gap (9-15s, 20% chance of 60-120s) timing.
- `DebugMode`: a distinct, orthogonal full-screen mode (`debug.active` bool), entered/exited with `'d'/'D'`, that short-circuits `update()`/`draw()` entirely while active — effectively a second "scene" for shader preview/tuning.
- `hudOverlayActive`: a third exclusive full-screen mode, toggled with `'o'/'O'`, also short-circuiting normal draw.
- Overall: no single top-level "scene manager," but three mutually-exclusive top-level draw/update paths gated by plain booleans (`debug.active`, `hudOverlayActive`, else normal), plus two independent internal finite-state/cycling systems (`ExpansionDirector`'s enum FSM, `QuadrantManager`'s per-quadrant cycle) running within the "normal" path.

**7. Performance & Resource Notes**
- `ofSetFrameRate(24)`, target render resolution 1280×720 per `docs/update-docs.md`'s header: "Target platform: Raspberry Pi 3B (Raspberry Pi OS 64-bit, VideoCore IV, GLSL ES 1.0/GLSL 120)... Render target 1280×720 @ 24fps, HDMI output, no UI chrome."
- Erosion/residue FBO pattern: each `Quadrant` owns a ping-ponged pair of full-canvas FBOs (`fbo_read`/`fbo_write`), swapped every frame — decay-and-blend against `erosion.glsl` (`history * decayRate + current * videoAlpha`).
- `MotionExtraction` runs a separate 160×90 accumulation buffer pipeline with two reference modes, ring buffer sized 15 frames on Pi / 60 on desktop — an explicit Pi-vs-desktop memory/perf tradeoff.
- `Quadrant.cpp`'s unused `sampleAverageBrightness` (flagged by the compiler warning) is dead code superseded by `ofApp::sampleBrightness` (strided scan: every 64px on Pi, every 16px desktop).
- Shaders: `#version 120` throughout. Effect files loaded by `ShaderLibrary::setup()` (per `docs/update-docs.md` §07): `desaturate`, `invert`, `recolor`, `threshold`, `dither`, `solarize`, `scanlines`, `channelshift`, `motion_effect`, `ascii_solarpunk`, plus 8 "nature pack" shaders — 18 total. `erosion.glsl` loaded separately by `Quadrant::setup()`. `temporal_trails` is excluded from the live quadrant pool (needs its own per-quadrant FBO) but previewable in isolation inside `DebugMode`, which owns its own dedicated ping-pong FBO pair just for that shader.

**8. Input / Control Surface**
- No shell script/systemd/kiosk config references this sketch anywhere in the repo. Manual `make RunRelease`/`open bin/quadrant-crosshair.app`.
- No GPIO/MQTT/OSC/network code.
- No crossfade/restart-tied transition logic — `VideoSystem::nextFile()`/playlist advance and `ExpansionDirector`'s crossfades are all in-process.
- Full key map documented (and code-verified for the discrepancy noted in §3) in `docs/keybindings.md` (normal mode + a separate debug-mode key layer) and repeated in `docs/update-docs.md` §14.

---

### shader-effect-debugger

**1. Project Inventory**
- Path: `sketches/shader-effect-debugger/`.
- OF 0.12.1.
- **Builds cleanly** — the **cleanest build of the three** in this agent's group: zero real compiler warnings at all (only the same benign make-tool noise).
- LOC: **498** total — dramatically smaller than fragment-trail/quadrant-crosshair, since `src/` here is only `main.cpp`, `ofApp.h`, `ofApp.cpp` (3 files). All actual effect logic lives in `shared/src/video-effects/`, not in this sketch.
- Git: clean, no nested `.git`.

**2. Build Configuration**
- Makefile: unmodified OF template.
- `addons.make`: `ofxGui`, `ofxOpenCv` — the only one of the three sketches in this group that pulls in `ofxOpenCv`, but this sketch's own `src/` never references OpenCV directly; it's pulled in transitively because `shared/src`'s compiled object set (e.g. `BlobDetector`/`BlobTracker`) depends on it.
- `config.make`:
  ```make
  OF_ROOT = ../../../../..
  PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src
  PROJECT_EXCLUSIONS =
  ```
  `PROJECT_EXCLUSIONS` is **empty** — unique among the three sketches in this group. Confirmed via the build link line: this sketch compiles/links essentially all of `shared/src`'s top level plus `ridgeline/`, `hud_overlay/` (all 4 organisms), the full `video-effects/` tree, and all `hud/` widgets — even though this sketch's own code uses none of `hud`/`hud_overlay`/most top-level classes directly. Consistent with it having zero local class-name forks to protect against collisions.
- No `PLATFORM_PI` conditionals found — no Pi/desktop branching exists in this sketch's own code, despite `docs/implement-shader-effect-debugger-and-service.md` explicitly discussing Pi deployment for its knowledge-base file location ("a sketch deployed independently to the Pi must not depend on a relative path escaping its own `bin/data`" — implying Pi deployability was a design assumption, even though no conditional Pi code exists yet).
- Two manifest files: `effect-manifest.json` at the sketch root and a synced copy at `bin/data/effect-manifest.json` (26 effect IDs, all currently `"enabled": true`), kept in sync by `scripts/sync-video-effect-assets.py`. `bin/data/shared-video-effects/` is a synced copy of the canonical shader assets (marked with a `.generated-from-shared-video-effects` sentinel), organized by effect kind.

**3. ofApp Structure**
- Members: `videoeffects::VideoEffectService service`; `std::vector<std::string> effectIds`; `int currentEffectIndex`; `std::unique_ptr<videoeffects::VideoEffectInstance> currentInstance`; `std::unique_ptr<videoeffects::VideoEffectInstance> auxMotionExtraction`; `ofVideoPlayer video` (declared directly on `ofApp` — see §4); `std::vector<std::string> videoFiles`; `int currentVideoIndex`; `bool videoPaused`; `videoeffects::EffectEvolutionController evolution`; `videoeffects::PatternDriftController drift`; `videoeffects::VideoEffectParameters currentRenderParams`; `ofxPanel gui` + `ofParameterGroup globalGroup` (`pEvolutionEnabled`, `pDriftEnabled`, `pSeed`) + a dynamically-rebuilt `effectParamGroup`; toast-message state.
- `setup()`: window title/background/vsync/`ofEnableAlphaBlending()` → `service.setup(VideoEffectServiceConfig{})` → populate `effectIds` from `service.registry().allIds()`, keeping only effects whose `VideoEffectDefinition::debuggerAvailable == true` → `scanMediaFiles()` then `loadVideo(0)` if any found, else a warning log → `gui.setup(...)`, add `globalGroup` → `switchToEffect(0)` if any effects registered.
- `update()`: `video.update()` → clamp `dt` → `pullGuiIntoCurrentParams()` (copies live GUI slider values into `currentRenderParams` every frame — no `addListener` callbacks anywhere in this class, by design, "so there is nothing to unhook when the group is rebuilt") → update `drift`'s enabled flag from GUI → if `pEvolutionEnabled`: `evolution.update(dt, ...)` then overwrite `currentRenderParams` and push back into GUI sliders → if `pDriftEnabled`: `drift.update(dt, transitionActive)` then `currentRenderParams = drift.apply(currentRenderParams)` → if there's a `currentInstance`, build a `VideoEffectContext` and call `currentInstance->update(...)` → if `auxMotionExtraction` exists (only when the active effect is `motion_composite`) and video is loaded, update it too.
- `draw()`: builds a full-canvas `destRect`; if there's a current instance and loaded video, builds a `VideoEffectContext` (adds `motionTex`/`motionDelayedTex` auxiliary textures if `auxMotionExtraction` is active) and calls `currentInstance->render(...)`; else draws a placeholder message. Always draws an on-screen text overlay (effect name/index, video filename, evolution phase, drift on/off, key legend, effect `kind`, and a `[NOT VALIDATED for automatic selection]` flag if `!capabilities.safeForAutomaticSelection`) plus a 2-second toast queue; finally `gui.draw()`.
- `exit()`: not overridden.
- Input/GUI: `←/→` (prev/next effect), `[`/`]` (prev/next video file), `space` (pause/play), `r/R` (randomize params via `service.randomizer()`), `0` (reset to schema defaults), `w/W` (save param snapshot to whitelist knowledge file), `b/B` (save to blacklist), `e/E` (toggle scene evolution), `p/P` (toggle pattern drift). **GUI**: `ofxGui`-based, but with a genuinely **dynamic** parameter set — `rebuildEffectGui()` clears and repopulates `effectParamGroup` from the *currently selected effect's* `VideoEffectDefinition::params` schema every time the effect changes (skipping any not marked `visibleInDebugger`, and Vec2/3/4 types entirely — "not GUI-exposed in v1"). This is materially different from quadrant-crosshair's hand-rolled `DebugMode` (no `ofxGui` at all) — this is the "reference example" CLAUDE.md refers to for consuming the shared service via a live, schema-driven GUI.
- Window: fixed 1280×720 `OF_WINDOW` (not fullscreen), no explicit frame-rate call (relies on OF's default), no multi-monitor logic.

**4. Video / Media Handling**
- Player: a plain `ofVideoPlayer video` member declared **directly on `ofApp`** — **not** wrapped in any intermediary class. This is the **one sketch of the three where video player usage is inlined directly in `ofApp`**, unlike both siblings which wrap it in `TimeOffsetVideoBuffer`/`VideoSystem`.
- Max concurrent players: **1**, plus whatever the currently-selected effect instance internally allocates (some, e.g. `motion_composite`, additionally instantiate an auxiliary effect instance — same single video texture, not a second player).
- Media path: `scanMediaFiles()` scans `ofToDataPath("media", true)` for `.mp4` **and `.mov`** files (the only sketch of the three that also allows `.mov`).
- Texture format: **no `ofDisableArbTex()` call anywhere in this sketch's `src/`** — unlike both other sketches in this group, which treat this call as load-bearing/mandatory with explicit rationale comments. This is a notable factual inconsistency worth flagging: the other two sketches document this as a required setup step, and this sketch's `setup()` simply doesn't call it.
- **Seek-and-freeze**: partial — `togglePausePlay()` calls `video.setPaused(videoPaused)` on `space`, a genuine pause/resume toggle (freezes at whatever position playback stopped), but there's no accompanying "seek" logic (no `setPosition`/`setFrame` calls anywhere) — pause-only, not a seek-then-freeze pattern.

**5. Shared/Duplicated Infrastructure**
- Shared/src actually used: this sketch is built directly against the service, so its real dependency surface is `shared/src/video-effects/` in full — `core/`, `catalog/DefaultVideoEffectCatalog`, `knowledge/` (`EffectKnowledgeBase`, `EffectRandomizer`), `evolution/` (`EffectEvolutionController`, `PatternDriftController`), and `effects/` (all 7 effect-instance classes) — confirmed both by direct includes and by the full object list in the link line.
- **No local forks**: unlike fragment-trail and quadrant-crosshair, this sketch has **zero** same-named local classes shadowing `shared/src/` — no local `ShaderLibrary`, `LFOBank`, `TriggerBus`, `GridState`, etc. anywhere. The one sketch of the three with no name-collision problem, consistent with `PROJECT_EXCLUSIONS` being empty.
- `hud`/`hud_overlay` linkage is present but unused — compiled/linked only because `PROJECT_EXCLUSIONS` doesn't drop them; zero references to any `hud::`/`hudoverlay::` type in this sketch's own code.
- No `Settings.h` include; essentially no local constants beyond a UI toast-duration constant.
- Knowledge-base data files: `EffectKnowledgeBase` (default `dataDir = "knowledge"`) writes/reads `bin/data/knowledge/{effectId}.whitelist.json`/`.blacklist.json`. **No such files exist yet** in this checkout — the `bin/data/knowledge/` directory itself doesn't currently exist, meaning the `w`/`b` save actions haven't been exercised yet.

**6. Scene/State Lifecycle**
- Effect switching: `currentEffectIndex` cycled with `←/→` via `switchToEffect(index)`, which wraps modulo `effectIds.size()`, tries `service.createInstance(...)`, and — if instantiation fails (e.g. a shader failed to compile) — automatically advances and retries, walking the whole ring once before giving up ("so one broken effect doesn't strand the debugger"). On success: resets `auxMotionExtraction` (recreated only for `motion_composite`), calls `rebuildEffectGui()`, `resetCurrentToDefaults()`, re-seeds `evolution`/`drift`, shows a toast. This is the sketch's explicit "scene"/mode switch — one effect active at a time, no phase/state enum beyond the index itself.
- `EffectEvolutionController` (`'e'/'E'`): its own internal `EvolutionPhase::Transitioning` vs. holding state, external to `ofApp`.
- `PatternDriftController` (`'p'/'P'`): a separate, independently-toggleable modifier layered on top of whatever `currentRenderParams` evolution/GUI produced, not a mode of its own.
- Overall: this is the one sketch of the three built explicitly around live effect-switching as its whole reason for existing — every other "phase" concept (evolution, drift) is a layered modifier on top of the currently-selected effect, not a competing scene.

**7. Performance & Resource Notes**
- No `ofSetFrameRate()` call anywhere — unlike both other sketches in this group. No Pi-specific perf comments in this sketch's own code (consistent with the absence of `PLATFORM_PI` branches).
- `effect-manifest.json` lists all 26 canonical effect ids, `"enabled": true`.
- `VideoEffectKind` enum (`SinglePassShader`, `MultiPassShader`, `TemporalShader`, `Processor`, `Composite`, `CpuRenderer`, `Simulation`) — the on-screen overlay shows the active effect's `kind` every frame.
- FBO/ping-pong usage inherited from whichever effect implementation is active (`ErosionEffect`/`TemporalTrailsEffect` own their own ping-pong FBOs internally, same pattern as quadrant-crosshair's `Quadrant` erosion FBOs, now generalized); `SinglePassShaderEffect` is a single draw call with no FBO. `VideoEffectInstance::resize()`/`reset()` (base class) are the documented hooks for clearing persistent state / reallocating resolution-dependent resources — a generic lifecycle contract every effect implementation follows.
- Shaders: `#version 120` (confirmed identical to fragment-trail's/quadrant-crosshair's `vert.glsl`/`desaturate.glsl` pair via `bin/data/shared-video-effects/common/vert.glsl` — byte-identical source). Synced asset tree includes `caustics.glsl` under `single-pass/`, present here but **not** in quadrant-crosshair's `ShaderLibrary::setup()` list.

**8. Input / Control Surface**
- No shell script, systemd unit, or kiosk boot config anywhere references this sketch. Manual launch only.
- No GPIO/MQTT/OSC/network code.
- No crossfade/restart-tied transition logic — effect/video switching is entirely in-process via keyboard.
- Design docs discuss this sketch's knowledge-base file layout with an explicit Pi-deployment assumption even though, per §2/§7, no `PLATFORM_PI` conditional code or non-default frame-rate pinning currently exists to back that assumption up.

---

### temporal-fields

**1. Project Inventory**
- Path: `sketches/temporal-fields/`.
- OF 0.12.1. Actual invoked compiler flag observed: `-std=c++2b`.
- **Builds cleanly.** No compiler errors; only the harmless `-jN forced in submake` make-tool warning.
- LOC in `src/`: **8,658 total** across 46 files — the largest sketch by far. Full file list: `TFAmbientTextureLayer`, `TFBackgroundLayer`, `TFComposition`, `TFEffectPicker`, `TFFragmentShape.h`, `TFFragmentTransition`, `TFHudLayer`, `TFImageCycler`, `TFMoireUnderlay`, `TFParameterPanel`, `TFPattern.h`, `TFPatternBSP`, `TFPatternBands`, `TFPatternBlobGrid`, `TFPatternColumnGrid`, `TFPatternEcologicalSuccession`, `TFPatternNetworkGrowth`, `TFPatternParticleField`, `TFPatternTelescopingFrames`, `TFPatternTemporalTides`, `TFPatternType.h`, `TFPlayheadAssignment.h`, `TFPresetTimeline`, `TFQuarantineHatch.h`, `TFRandom.h`, `TFSettings.h`, `TFShapeFragmentRenderer`, `TFTextureCropFill.h`, `TFTimelineBinding.h`, `TFTimelineEasing.h`, `main.cpp`, `ofApp` (each `.h`/`.cpp` pair as applicable). Plus `test/tf_timeline_tests.cpp` + `test/Makefile.tests` (sibling of `src/`, same reason as blob-region-prototype's `test/` — avoiding a duplicate `main()` link error, per the test file's own header comment; an earlier attempt at `src/test/` produced exactly that error).
- Git: clean, no nested `.git`.

**2. Build Configuration**
- Makefile: unmodified OF template.
- `addons.make`: `ofxGui`, `ofxOpenCv`. **Note**: no `ofxCv`/`ofxOpenCv`/`cv::` symbol appears anywhere in this sketch's own `src/` — the implementation brief (`docs/temporal-fields-implementation-brief.md`) only calls for `ofxGui`; `ofxOpenCv` is present only because unfiltered `shared/src` inclusion needs it.
- `config.make`: `OF_ROOT = ../../../../..`; `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src`; `PROJECT_EXCLUSIONS =` (empty), with a comment documenting that `shared/src/hud` was previously excluded on a since-disproven collision claim and is now included unexcluded (matches CLAUDE.md's migrated-sketch table). No `PLATFORM_CFLAGS`/`PLATFORM_LDFLAGS` overrides.
- All paths relative. No Pi cross-compilation evidence specific to this sketch — no ARM-specific config, no cross-compile toolchain reference; `TARGET_FPS=24` and Pi-RAM-budget comments throughout imply native-on-Pi build assumption, not cross-compiled.

**3. ofApp Structure**
- Members: `TFComposition composition`; 9 pattern instances (`TFPatternBSP`, `TFPatternBlobGrid`, `TFPatternBands`, `TFPatternColumnGrid`, `TFPatternTelescopingFrames`, `TFPatternParticleField`, `TFPatternEcologicalSuccession`, `TFPatternNetworkGrowth`, `TFPatternTemporalTides`); `TFParameterPanel paramPanel`; `ShaderLibrary shaderLib`; `TFBackgroundLayer backgroundLayer`; `TFAmbientTextureLayer ambientTextures`; `TimeOffsetVideoBuffer timeOffsetBuffer`; `MotionExtraction motionEx`; `TFHudLayer hudLayer`; `bool showDebugGui`; `hudoverlay::HudOverlayLayer hudOverlay`; `hudoverlay::HudOverlayDialPanel hudOverlayPanel`; `hudoverlay::HudOverlayDialState hudOverlayDials`; `bool hudOverlayActive`.
- `setup()`: `ofDisableArbTex()` (must precede FBO/texture allocation, per a comment referencing the identical gotcha in blueprint_emergence) → `ofSetFrameRate(24)` → `shaderLib.setup()` → `paramPanel.setup()` → `ambientTextures.setup(w,h,"backgrounds")` + register its param group → build `TimeOffsetVideoBuffer::Settings` from panel values (quantize bands, max history, min playtime/loop count) → `timeOffsetBuffer.setup("media", bufferSettings)` → `backgroundLayer.setup(...)` → all 9 patterns' `.setup(&timeOffsetBuffer, w, h, params)` → `composition.setup(timing{cycleDuration=90}, {9 pattern-type/pointer pairs}, w, h)` → register `onPatternChanged` (logs, advances media if eligible, calls `hudLayer.onPatternSwitch`) and `onFragmentReassigned` (calls `hudLayer.onFragmentReassigned`) callbacks → `composition.startCycle()` → `motionEx.setup()` → `hudLayer.setup(w,h)` + register its visibility param group → `hudOverlay.setup(w,h)` → `hudOverlayPanel.setup()`.
- `update()`: early-return if `hudOverlayActive` (only overlay+panel updated). Otherwise: `dt` → `paramPanel.update(dt)` → push every pattern's params from the panel each frame (9 `setParams` calls; comment: "cheap POD struct copies... simplest way for a live slider drag to take effect immediately") → push quantize-bands/max-history into `timeOffsetBuffer` → push transition params into `composition` → `backgroundLayer.setParams()` + `.update(dt)` → `composition.setAutoCycleSuspended(paramPanel.isTimelineActive())` → `composition.update(dt)` → `timeOffsetBuffer.update(dt)` → if media loaded, `motionEx.update(rawVideoTexture, 0.85f, 1.0f)` → compute average playhead depth → compute per-pattern "drift" via a switch on active pattern type → `hudLayer.setEventCadence(...)` + `.update(...)` → consume/apply save request → consume/apply waypoint pattern switch → `paramPanel.setActivePattern(...)`.
  - **Timeline/cycle advancement, traced in full** (two independent mechanisms coexist): (1) `TFComposition`'s auto-cycle: `phaseElapsed += dt` every `update(dt)`; when `phase==RUNNING && !autoCycleSuspended && phaseElapsed >= 90s`, calls `beginTransitionToNextPattern()` (walks the registered pattern list, wrapping). (2) `TFPresetTimeline` (owned by `TFParameterPanel`): tracks `phaseElapsed`/`totalElapsed`; in `Transitioning`, computes eased progress and calls `applyInterpolatedFrame()` (lerps every bound numeric parameter from a `fromSnapshot` toward the state's precomputed `resolvedTargetNumeric`); once eased progress ≥1, flips to `Holding`; once `phaseElapsed >= holdDuration`, calls `advanceToNextState()` (next state, loop to state 0, or `finished=true` and park). `ofApp` bridges the two: `composition.setAutoCycleSuspended(paramPanel.isTimelineActive())` suspends `TFComposition`'s independent 90s auto-cycle for as long as a multi-state timeline preset is running.
- `draw()`: early-return for `hudOverlayActive` (near-black + overlay). Otherwise: `ofBackground(GROUND_DARK)` (shared palette token, not literal black) → `ofEnableAlphaBlending()` → `backgroundLayer.draw()` → `ambientTextures.drawUnderlay()` → `hudLayer.drawUnderlay()` → `composition.draw()` → optional `drawTimeOffsetDebugStrip()` → `hudLayer.drawOverlay()` → `ambientTextures.drawOverlay()` (topmost) → debug strings → `paramPanel.draw()`.
- `exit()`: not overridden — no declaration in `ofApp.h`.
- Input: `'o'/'O'` toggles `hudOverlayActive`; `'G'/'g'` toggles `paramPanel`; `'d'` toggles `showDebugGui`; `'r'` restarts cycle; `'t'` forces next pattern; `'S'/'s'` saves preset; `TAB` loads next preset (force-switches pattern if it belongs to a different one); `'P'/'p'` pauses/resumes the active `TFPresetTimeline`; `']'` advances the timeline to its next state. No mouse handling, no GPIO.
- Window: 1280×720, `OF_WINDOW` (not fullscreen). `windowResized(w,h)` propagates to `ambientTextures`, `backgroundLayer`, `composition`, `hudLayer`, `hudOverlay`. No multi-monitor code.

**4. Video / Media Handling**
- Player: exactly one `ofVideoPlayer`, fully wrapped inside `TimeOffsetVideoBuffer` — confirmed no direct `ofVideoPlayer` reference anywhere else in `src/`.
- Max concurrent players: **1**. Concurrency instead comes from a pool of 6 "playheads," each reading a different offset into the CPU-RAM history ring buffer, not additional decoders.
- Media path: `timeOffsetBuffer.setup("media", ...)` scans `bin/data/media/` for `.mp4` files. 25 files actually present (stock/AI-generated nature footage).
- Texture format: history stored as `ofPixels` in system RAM at downscaled 640×360 — only playheads actually in use get uploaded to a texture, once per frame, on demand.
- **Seek-and-freeze**: **not implemented** — `TimeOffsetVideoBuffer` never seeks, per its own header. Playhead assignment (`TFPlayheadAssignment.h`) calls `videoBuffer.jumpPlayhead(p, desiredOffset)` to point a playhead at a new quantized offset into already-buffered history — a pointer reassignment, not a seek. `holdPlayhead()` exists on the class but is **never called anywhere in `src/`** — only the jump/assign path is exercised.

**5. Shared/Duplicated Infrastructure**
- Shared/src includes actually used: `DefaultVideoEffectCatalog.h`, `HudElements.h`, `HudOverlayDialPanel.h`, `HudOverlayDialState.h`, `HudOverlayLayer.h`, `LFOBank.h`, `MotionExtraction.h`, `Settings.h`, `ShaderLibrary.h`, `TimeOffsetVideoBuffer.h`, `VideoEffectRegistry.h`, `VideoEffectTypes.h`.
- **No local forks**: `TFFragmentShape.h` is explicitly documented as a genuinely new concept, not a fork ("BSP and Blob Grid never needed this... there is nothing here to be 'consistent' with; this is genuinely new"). `shared/src/Fragment.{h,cpp}` exists in the repo but is **not included anywhere** in this sketch. `TFComposition` is explicitly documented as deliberately **not** a `CompositionBase` subclass: "`CompositionBase`'s `CyclePhase`... and its phase-transition logic are private and fixed to blueprint_emergence's fill/erode narrative arc, which doesn't fit this sketch's continuous, cycle-then-switch-pattern model." All other `TF*` classes are sketch-local and don't shadow a `shared/src/` name.
- Constants: `TFSettings.h` defines its own large constant set (pattern-specific tuning — BSP irregularity, Blob Grid drift speed, etc.) with no `Settings.h` equivalent to diverge from (they're not repo-wide tokens). `ofApp.cpp` **does** use `Settings.h`'s `GROUND_DARK` token directly for the background clear color, rather than a local duplicate.

**6. Scene/State Lifecycle — the most developed state machine in the repo, spanning three layers**
- **A. `TFComposition::CyclePhase`**: `enum class { RUNNING, PATTERN_TRANSITION }`. Owns which of 9 registered patterns is active, a per-cycle random seed, and a scene-level transition (whole-canvas snapshot-blend via `TFFragmentTransition`, reused at canvas scale). Triggered either by the 90s auto-cycle timer or by `forceNextPattern()`/`forcePattern(type)` (keyboard `'t'`, TAB-loaded preset, or a waypoint pattern switch).
- **B. `TFPatternType`**: enum of the 9 pattern kinds, each an opaque `TFPattern*` registered in `TFComposition`'s ordered vector; cycling walks the vector in registration order, wrapping.
- **C. `TFPresetTimeline`** (385 lines — the deepest state machine in the repo, fully described in `docs/temporal-fields-preset-timeline-format.md`):
  - `enum class Phase { Inactive, Transitioning, Holding }`.
  - State struct: name, `holdDuration`, `transitionDuration`, `EasingType easing`, a sparse `overrides` map, and a precomputed `resolvedTargetNumeric` map (resolved once at load time — never accumulated from a prior state).
  - Loading: `load()` takes a `nlohmann::json` timeline object plus a flat `baseValues` map and a `bindings` map (built by `TFParameterPanel::buildTimelineBindings()` from every live `ofParameter`). Parses `Loop`, `Start_State`, `States[]`; invalid pieces are warned and skipped individually rather than failing the whole preset.
  - Transitions: `beginTransitionTo(idx)` snapshots the *actual current runtime value* (not a remembered previous target) — what keeps pause/resume/jump visually continuous. `update(dt)` in `Transitioning` computes eased progress, calls `applyInterpolatedFrame()` (lerps float/int continuously, switches bool/enum-int at the 50% eased-progress mark). In `Holding`, once `phaseElapsed >= holdDuration`, calls `advanceToNextState()`.
  - Looping: `Loop=true` wraps to state 0 indefinitely; `Loop=false` sets `finished=true` and parks on final values.
  - Runtime controls: `start()/restart()/pause()/resume()/jumpToState(name)/advanceToNextState()`, bound to `'P'`/`']'`.
  - Independence from OF: built against only `nlohmann::json` and plain get/set closures, unit-tested standalone in `test/tf_timeline_tests.cpp` (hand-rolled `TF_CHECK` macros, no test framework — repo has none) via `test/Makefile.tests`, linking only `TFPresetTimeline.cpp` + the test file with zero OF/GL linkage.
  - **Preset system** (`TFParameterPanel`): one JSON file per preset under `bin/data/presets/*.json` (16 present, e.g. `air_and_time_quiet_canopy.json`, `manifest.json`). Every leaf value is a JSON string (matching `ofSerialize`'s convention). Top-level `"pattern"` field selects which of the 9 patterns the preset targets; other groups only override groups actually present.
- **Additional isolated scene**: `hudOverlayActive` (`'o'`) fully replaces the sketch's normal draw with `shared/src/hud_overlay`'s standalone ambient/organism HUD system over a solid `#0D0D0D` background — per that module's own README, deliberately isolated from the composition/Fragment/VideoSampler lifecycle (no wiring between the two).

**7. Performance & Resource Notes**
- Target hardware: Pi 3B (repo-wide). `TARGET_FPS = 24`.
- Explicit Pi RAM budget math in `TimeOffsetVideoBuffer.h`: a naive texture-per-history-frame approach (240 frames at 640×360 RGBA) would burn ~220MB of the Pi's 1GB shared RAM; history is kept CPU-side, only active playheads upload to texture.
- `TFSettings.h` comments repeatedly note values "not yet validated against real footage or Pi hardware" for Particle Field, Ecological Succession, Network Growth, connection-thread/tick-stamp timings, Moire underlay values.
- Ecological Succession explicitly skips its draw call entirely below a sprout threshold "(Section 0's Pi requirement)" rather than drawing at alpha 0 — a stated Pi-driven optimization.
- On-device Pi memory measurement is called out as still outstanding.
- Shaders: `#version 120` (confirmed on `desaturate.glsl`, `fragmentDissolve.frag`, `motion_extract.glsl`). FBOs: 8 files under `src/` reference `ofFbo`, including `TFComposition`'s scene-transition ping-pong-style pair (`outgoingSnapshotFbo` captured from the outgoing pattern, `incomingRenderFbo` re-rendered fresh from the incoming pattern, blended via `TFFragmentTransition`). Shader files present: `shaders/fragmentDissolve.{vert,frag}`, `shaders/textureBlendFade.{vert,frag}`, `shaders/particleExistenceFade.{vert,frag}`, `shaders/motion_extract.glsl`, `shaders/motion_accum.glsl`, `shaders/effects/*.glsl` (12 files), plus a separate `bin/data/of_nature_shader_pack_glsl/` set (7 files).

**8. Input / Control Surface**
- No shell script, systemd unit, or kiosk/boot config anywhere references this sketch. Only launch path documented is `make RunRelease`/direct binary execution.
- No GPIO/MQTT/OSC/network code.
- No crossfade/transition tied to process restart — all transitions are in-process, `dt`-based.
- `prep_textures.sh`: a standalone, manually-run offline asset-prep script (ffmpeg-based, produces `_tint.png`/`_mask.png` texture pairs with a warm/cool/green/none tint option) — not a launch or control-surface script, purely an asset pipeline tool.

---

### hud_validation_harness

**1. Project Inventory**
- Path: `sketches/hud_validation_harness/`.
- OF 0.12.1, same checkout as temporal-fields.
- **Builds cleanly.** No errors; same harmless make-tool warnings.
- LOC in `src/`: **328 total** (267 `ofApp.cpp` + 44 `ofApp.h` + 17 `main.cpp`).
- Git: clean, no nested `.git`.

**2. Build Configuration**
- Makefile: unmodified OF template, identical structure to temporal-fields'.
- `addons.make`: **does not exist** — no addons declared.
- `config.make`: **one line, no other content**:
  ```make
  PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src/hud
  ```
  No `PLATFORM_CFLAGS`/`PLATFORM_LDFLAGS`/`PROJECT_EXCLUSIONS`/cross-compile settings at all — the minimal single-purpose config matches this sketch's narrow scope (only pulls in `shared/src/hud`, nothing else of `shared/src`).
- All paths relative. No Pi cross-compilation evidence; no `OF_ROOT` override (falls through to the Makefile's absolute default).

**3. ofApp Structure**
- Members: a `Scenario` struct (`name`, a `build()` factory returning a `unique_ptr<hud::HudWidget>`, `width`/`height`, `seed`, `steps`, `theme`, `motion`); `std::vector<Scenario> scenarios`; `size_t currentIndex`; `std::unique_ptr<hud::HudWidget> currentWidget`; `int stepsRemaining`; `static constexpr float kFixedDt = 1.0f/60.0f`. No other persistent state — this is a scenario-runner, not a live app.
- `setup()`: `ofSetFrameRate(0)` (uncapped) → `ofSetVerticalSync(false)` → `ofSetLogLevel(OF_LOG_NOTICE)` → `ofBackground(0)` → `buildScenarios()` (constructs the full test matrix — see below) → log scenario count → `startScenario(0)`.
- `update()`: if `stepsRemaining > 0`, calls `currentWidget->update(kFixedDt)` (fixed 1/60s dt regardless of actual frame time) and decrements `stepsRemaining` — a deterministic fixed-frame-count warm-up counter, not a wall-clock timer.
- `draw()`: if all scenarios exhausted, logs completion once and calls `ofExit()`. Otherwise: `ofBackground(0)`; if `stepsRemaining == 0` (warm-up done), calls `captureCurrentScenario()` (draws the widget once inside `ofPushStyle()/ofPopStyle()`, checks blend-mode leakage before/after, `ofSaveScreen("captures/" + name + ".png")`), then either starts the next scenario or sets `currentIndex = scenarios.size()` as a completion signal.
- `exit()`: not overridden.
- Input: **none** — no `keyPressed`/mouse/GPIO overrides declared. This is a fully non-interactive, self-driving batch harness (confirmed by `main.cpp`'s own comment: "Not a sketch to be performed live").
- Window: `700×400`, `OF_WINDOW`, no fullscreen, no multi-monitor. `windowResized` not overridden.
- **Scenario matrix**: base matrix of 8 widget types × 4 bounds sizes; plus targeted scenarios for Gauge styles, Scanner background on/off × 2 time points, Reticle preset × label-override combinations, FlowField density, DataCard default vs. post-mutation, every `FrameStyle`, and one "activeMotion" case per widget family.

**4. Video / Media Handling**
- **No video playback of any kind** — this sketch renders `shared/src/hud` widgets in isolation (vector primitives only, per that library's own "no shaders, FBOs, or textures" design note) and saves PNG screenshots. Not applicable: no wrapping class, no concurrent player count, no media path convention, no seek-and-freeze pattern.

**5. Shared/Duplicated Infrastructure**
- Includes: only `"HudElements.h"`, pulling in the entire `shared/src/hud/` widget library via the `PROJECT_EXTERNAL_SOURCE_PATHS = ../../shared/src/hud` recursive-subdirectory rule.
- No local forks/near-duplicates — `src/` contains only `ofApp.h/.cpp`+`main.cpp`.
- Local theme/motion literal values in `ofApp.cpp` happen to be identical to the example theme shown in `shared/src/hud/README.md`'s own usage snippet (same RGBA numbers) — the harness's reference theme matches the library's documented example rather than diverging from it. No `Settings.h` equivalent exists for these (this sketch doesn't include `Settings.h` at all).

**6. Scene/State Lifecycle**
- No enum-based state machine. State is a linear `currentIndex` walk through the flat `scenarios` vector, gated by the per-scenario `stepsRemaining` countdown. No looping, no branching, no named states — strictly sequential, one-shot, ending in `ofExit()`. Each `Scenario` is a self-contained one-off configuration (build fresh widget → run fixed steps → capture → discard), not a persistent mode the app returns to.

**7. Performance & Resource Notes**
- No FPS target enforced (uncapped — a batch capture tool, not a live display).
- No target-hardware comments in this sketch's own code — its role is a dev-time visual-diff harness, run on the dev machine, not deployed to the Pi. (`shared/src/hud/README.md`'s own "Raspberry Pi notes" — inherited by the widgets this harness exercises — states Pi runtime validation is "currently outstanding.")
- No shaders/FBOs/GLSL — the widget library this harness exercises explicitly avoids them.
- 66 PNGs already present under `bin/data/captures/` from a prior run.

**8. Input / Control Surface**
- No shell script, systemd unit, or kiosk/boot config references this sketch. Its own header comment documents the intended manual workflow instead: "Run once before a HUD change and once after; diff `bin/data/captures/*.png` between the two runs."
- No GPIO/MQTT/OSC/network code.
- No crossfade/transition tied to process restart — not applicable; this harness has no persistent runtime loop concept, it's a single one-shot process invocation.

---

### hud_elements (**not a buildable sketch — historical stub**)

**Path**: `sketches/hud_elements/`.

**Contents**: confirmed via `ls`/`find` — exactly one file, `README.md` (811 bytes). No `bin/`, `obj/`, or `src/` directories; no `Makefile`, `config.make`, or `addons.make`. **This is not a buildable openFrameworks project**, and none of the 8 requested sections (build config, ofApp structure, video handling, etc.) apply.

**README.md, in full**:
```
# hud_elements (moved)

This directory used to hold its own copy of the HUD widget library. That
copy, a second copy under `sketches/quadrant-crosshair/src/hud_elements/`,
and a third under `shared/src/hud/` drifted independently until they were
consolidated into a single canonical implementation.

**The HUD widget library now lives at `shared/src/hud/`.** See
`shared/src/hud/README.md` for the widget list, usage examples, and
integration instructions.

Do not copy the HUD library into a sketch. Add `shared/src/hud` (or
`shared/src`, if the sketch already consumes other `shared/src/` systems) to
the sketch's `PROJECT_EXTERNAL_SOURCE_PATHS` in its `config.make` instead.
`scripts/check-hud-library-uniqueness.sh` (run from the repo root) will fail
if a copy reappears anywhere outside `shared/src/hud/`.
```

**Stated purpose/plan**: this is **not** a forward-looking stub for a not-yet-built sketch — it is a historical marker for a **completed** consolidation. It documents that this directory used to contain its own copy of the HUD widget library; that copy, plus a second copy that lived under `sketches/quadrant-crosshair/src/hud_elements/` (confirmed by the other agent's inventory of quadrant-crosshair to no longer exist there either — see that sketch's §3 doc-vs-code discrepancy notes), and a third under `shared/src/hud/`, had drifted independently and were consolidated into the single canonical `shared/src/hud/` implementation (a full, populated 19-widget library — see the Shared Infrastructure Reference and `hud_validation_harness` above). `scripts/check-hud-library-uniqueness.sh` is stated to fail the build if a second copy of the library reappears anywhere outside `shared/src/hud/`.

**Literal fact confirmed**: no source code exists in `sketches/hud_elements/` — it is a documentation-only placeholder pointing elsewhere, not an active or planned sketch with its own future implementation.

---

## 9. Cross-Sketch Comparison Summary

| Aspect | blueprint_emergence | blob-region-prototype | contour-portrait | radar-effects-gallery | radar-pulse | fragment-trail | quadrant-crosshair | shader-effect-debugger | temporal-fields | hud_validation_harness |
|---|---|---|---|---|---|---|---|---|---|---|
| OF version | 0.12.1 | 0.12.1 | 0.12.1 | 0.12.1 | 0.12.1 | 0.12.1 | 0.12.1 | 0.12.1 | 0.12.1 | 0.12.1 |
| `make Release -j4` | ✅ clean (1 warning) | ✅ clean (0 warnings) | ✅ clean (2 addon warnings) | ✅ clean (0 warnings) | ❌ **fails** (missing `ofxOpenCv`) | ✅ clean | ✅ clean (3 warnings) | ✅ clean (0 warnings) | ✅ clean | ✅ clean |
| LOC (`src/`) | 3,773 | 491 | 2,149 | 769 | 561 | 2,520 | 4,491 | 498 | 8,658 | 328 |
| addons.make | ofxGui, ofxOpenCv | ofxGui, ofxOpenCv | ofxGui, ofxOpenCv | ofxGui, ofxOpenCv | ofxGui only | ofxGui | ofxGui | ofxGui, ofxOpenCv | ofxGui, ofxOpenCv | (none — file absent) |
| Pulls in `shared/src`? | Yes (unfiltered) | Yes (unfiltered) | **No** (commented out) | Yes (unfiltered) | Yes (unfiltered) | Yes (2-entry exclusion) | Yes (1-entry exclusion) | Yes (unfiltered) | Yes (unfiltered) | Yes (`shared/src/hud` only) |
| Video player class | `VideoSampler` (shared) | `TimeOffsetVideoBuffer` (shared) | `ofVideoPlayer` in local `ContourSource` | `ofVideoPlayer` in local `RPVideoSampler` (fork of RP's) | `ofVideoPlayer` in local `RPVideoSampler` (original) | `ofVideoPlayer` in local fork of `TimeOffsetVideoBuffer` | `ofVideoPlayer` in local `VideoSystem` | `ofVideoPlayer` **inlined directly in `ofApp`** | `ofVideoPlayer` in `TimeOffsetVideoBuffer` (shared) | n/a — no video |
| Max concurrent video players | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 (+ aux effect instance, same texture) | 1 | 0 |
| Seek-and-freeze implemented & used? | Class capable (`VideoSampler`), **but never called** — live continuous crop instead | Not used (`TimeOffsetVideoBuffer` never seeks) | No — continuous loop | No — loop-and-rotate | No — loop-and-rotate | No — never seeks | No — continuous loop, only position/layout "holds" | Partial — pause/resume only, no seek | Not used — playhead reassignment into buffered history, not a seek | n/a |
| Video-effects-service migration status | Migrated | Migrated | Not migrated (2nd-wave) | Not migrated (2nd-wave, Contract B) | Not migrated (2nd-wave) | Deliberately excluded (name collisions) | Migrated (production path only; `DebugMode` deliberately not) | Built directly against the service (reference example) | Migrated | n/a |
| HUD widget library (`shared/src/hud`) used? | Yes (12 widgets) | No | No | Yes (`PulseEmitterWidget`) | Yes (`PulseEmitterWidget`) | Yes (`WindowChrome`, `HexGridWidget`, `HudTheme`) | Yes (via `HudManager`) | Linked but unused | Yes | Yes (the subject under test) |
| `hud_overlay` standalone scene (`'o'` key)? | Yes — full-replace | n/a | n/a | n/a | n/a | **No** — `'o'` only toggles the overlay's own panel visibility | Yes — full-replace | n/a | Yes — full-replace | n/a |
| Internal phase/state machine? | Deep (7 nested enums: `CyclePhase`, `CycleMode`, `SlotPhase`, `Fragment::State`, `HudPhase`, `RotationPhase`, `EffectSlot::State`) | None (implicit lifecycle only) | Enum-based rendering toggles only, no scene phases | Single `int modeIndex` (11 modes) | None — fixed 2-stage pipeline | `FTContentMode` × `FTModeSwitchStrategy` (2 enums) | `ExpansionState` FSM + per-quadrant `PLAYING/SILENCING/READY` cycle + 2 orthogonal full-screen modes (`DebugMode`, `hudOverlayActive`) | Flat effect-index switch + 2 layered modifier controllers (evolution, drift) | **Deepest in repo**: `CyclePhase` (2-state) + `TFPatternType` (9-way) + `TFPresetTimeline` (3-phase, multi-state, JSON-driven, unit-tested standalone) | Linear one-shot scenario walk, no machine |
| Target FPS | 24 | none set (vsync-limited only) | 30 | 24 (explicitly "sanity check, not Pi-validated") | 60 (highest of all sketches) | 24 | 24 | none set | 24 | 0 (uncapped) |
| `PLATFORM_PI` conditionals present? | Yes (media path only; never defined by any build file) | No | Yes | Yes | Yes | No | Yes (media path, FBO pass, sampling stride, history depth) | No | No (comments only) | No |
| Own local forks of `shared/src` classes | None (subclasses instead: `BEFragment:Fragment`, `BEComposition:CompositionBase`) | None | n/a (no shared/src usage) | `RPVideoSampler`/`RPRevealMask` (forks of radar-pulse's, not shared/src's) | Originals that RG forks from | 4 forks: `ShaderLibrary`, `LFOBank`, `TriggerBus` (byte-identical to QC's), `TimeOffsetVideoBuffer` (byte-identical `.cpp` to shared's) | 5 forks: `ShaderLibrary`, `LFOBank`, `TriggerBus` (byte-identical to FT's), `MotionExtraction`, `GridState` — all diverged from shared | None | None (`TFComposition` explicitly not a `CompositionBase` subclass, by design) | None |
| Notable finding | `BESettings_presets.h` (348 lines) is dead/orphaned, never included | Only sketch with no `ofSetFrameRate()` call | Only sketch of the 11 with `shared/src` fully unused | Uses "Contract B" shader uniforms, unique in repo | **Fails a clean Release build today** (missing `ofxOpenCv`) | `HANDOFF.md` documents a bug whose real root cause is fixed in code but not reflected in the doc | 2 confirmed doc/code mismatches (`'o'` key behavior; `docs/quadrant-crosshair-reuse-analysis.md` is stale — claims no `shared/src` usage) | Only sketch with no `ofDisableArbTex()` call despite others treating it as mandatory | Only sketch with a standalone-unit-tested (OF-free) state machine (`TFPresetTimeline`) | Not a live/interactive sketch at all — a batch screenshot harness |

`hud_elements` is omitted from the table above (it is not a buildable sketch — see its section for the full explanation).

---

## 10. Open Questions / Unknowns

Everything below could not be determined from static inspection and would require running on actual target hardware (or, in a few cases, further live/dynamic testing on the dev machine) — this is the risk list for the follow-up engineering prompt:

1. **No sketch has ever been profiled on actual Raspberry Pi 3B hardware.** Every FPS target, seek-latency figure, memory-budget estimate, and GPU-instruction-count concern in every sketch's code/docs is explicitly labeled as a desktop proxy or a documented assumption "not yet validated against real footage or Pi hardware" (temporal-fields), "must be profiled on actual Pi 3B+ hardware" (blob-region-prototype), or similar. This applies uniformly across all 11 sketches — actual Pi FPS, actual seek latency, actual peak memory usage, actual GLSL ES dynamic-branch cost on VideoCore IV are all unknown.
2. **No Pi build path exists in this repo today.** `PLATFORM_PI` is referenced by `#ifdef` in 5 sketches (`blueprint_emergence`, `contour-portrait`, `radar-effects-gallery`, `radar-pulse`, `quadrant-crosshair`) plus `shared/src/MotionExtraction`, but is never defined by any Makefile, config.make, toolchain file, or build script anywhere in the repo. Whether these conditional code paths actually compile and behave correctly on-device is untested. No cross-compile toolchain, no Pi-specific config.make, no deploy script (`tools/deploy.sh`, referenced as planned in `docs/blueprint_emergence_engineering_plan.md`, does not exist), and no systemd unit exist anywhere.
3. **`radar-pulse` currently fails a clean `make Release -j4` build** on the dev machine — a real, reproducible, current-state build break (missing `ofxOpenCv` in `addons.make` vs. unfiltered `shared/src` inclusion pulling in `BlobDetector.cpp`), not a hypothetical risk. Whether this has been broken since a specific commit, or was never actually exercised via the Makefile path (only a stale Debug `.app` exists on disk), is not established from static inspection alone — would need `git bisect` or asking the person who last touched `radar-pulse/addons.make`.
4. **Whether `fragment-trail`'s HANDOFF.md-documented bug is actually fixed** is inferred from a code comment describing the root cause as found, but the fix was never validated against the specific repro steps HANDOFF.md lists (nor is there a before/after screenshot). Running the sketch live and checking whether fragments render video detail (not a flat fill) is unverified from static inspection.
5. **Whether quadrant-crosshair's two documented doc/code mismatches reflect a completed-but-undocumented migration, or an accidental regression**, is not fully resolvable from static inspection: the `'o'` key behavior change (motion-overlay-depth-toggle → full HUD-overlay-replace) and the stale reuse-analysis doc both look like intentional migrations whose docs were never updated, based on the surrounding code's coherence — but this is an inference, not a confirmed fact from any commit message or changelog entry.
6. **Actual GPU memory usage on-device** for any sketch using ping-pong FBOs (`ErosionFBO`-family, `MotionExtraction`, `RPRevealMask`, `Quadrant`'s erosion pair, `TemporalTrailsEffect`) is only ever estimated in comments, never measured (no `vcgencmd get_mem gpu` call exists anywhere in any sketch, despite being referenced as a planned check in `docs/blueprint_emergence_engineering_plan.md`).
7. **`hud_validation_harness`'s screenshot-diff workflow has never been run through a real HUD change** as far as static inspection can tell — 66 PNGs exist from *a* prior run, but whether the workflow (run before, run after, diff) has actually been exercised end-to-end around a specific widget change, and whether it catches regressions in practice, is unverified.
8. **`shader-effect-debugger`'s knowledge-base whitelist/blacklist save actions (`w`/`b` keys) have never been exercised** in this checkout — the `bin/data/knowledge/` directory doesn't exist yet, so whether `EffectKnowledgeBase`'s file-write path actually works end-to-end (permissions, directory creation, JSON format) is unverified.
9. **Whether the two independent cycle mechanisms in `temporal-fields` (`TFComposition`'s 90s auto-cycle and `TFPresetTimeline`'s multi-state sequences) interact correctly in every combination** (e.g. a timeline preset that itself changes pattern mid-sequence, interacting with `forcePattern()` from a keyboard press) is only traced through code reading, not exercised live across every combination.
10. **The actual behavioral effect of `radar-effects-gallery` and `radar-pulse` sharing identical `hud::PulseEmitterWidget` config literals and identical fixed emitter placement, independently declared in each `ofApp::setup()`**, has not been visually compared side-by-side — whether the two sketches actually look identical in that respect, or whether some other per-sketch difference (compositor mode vs. fixed color-reveal shader) makes the shared config produce visually different results, would need running both.
11. **Whether `contour-portrait`'s independent (non-`shared/src`) `ContourSource`/`ContourMaskSource` implementations have any behavioral divergence from the shared `VideoSampler`/`BlobDetector` patterns they parallel** (beyond the documented "same discipline, independent implementation" comment) has not been tested side-by-side — only static code comparison was done here.
12. **Actual seek latency for `VideoSampler`'s seek-and-freeze mechanism** (used architecturally by `blueprint_emergence` but never actually invoked by that sketch's current code) has never been measured on any hardware in this repo — the P50/P95 logging exists in the class but no sketch currently exercises the code path that would produce those log lines.
