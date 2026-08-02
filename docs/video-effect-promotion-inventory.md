# Video Effect Promotion Inventory (Phase 0)

Produced per [`shared-video-effect-architecture.md`](./shared-video-effect-architecture.md)
§4.1/§2.1. Investigation only — no files moved or code changed. Scope: every
shader file in `sketches/quadrant-crosshair/data/shaders/` and
`data/of_nature_shader_pack_glsl/` (canonical source — `bin/data/shaders` and
`bin/data/of_nature_shader_pack_glsl` are, respectively, a symlink to the
former and a byte-identical real copy of the latter, confirmed during the
prior probe), plus every QC C++ class that owns a non-trivial video effect:
`ShaderLibrary`, `Quadrant` (embedded erosion pass), `MotionExtraction`,
`ReactionDiffusion`, and `DebugMode` (embedded temporal-trails orchestration).
`RidgelineRenderer` is included for completeness even though it already
lives in `shared/src/ridgeline/` (already promoted, pre-dating this effort).

## 1. File-by-file classification

28 shader files total across the two directories (plus one `README.md` in
`of_nature_shader_pack_glsl/`, not a shader). Classification categories per
architecture doc §2.1: **reusable effect**, **pipeline-internal shader**,
**sketch-specific compositor**, **utility pass**, **unused asset**,
**duplicate**, **same-name/different-behavior collision**.

### `data/shaders/` (18 files)

| File | Registered in QC `ShaderLibrary`? | Classification | Notes |
|---|---|---|---|
| `desaturate.glsl` | yes | reusable effect | byte-identical to BRP/BE/TF/FT's copy (confirmed prior probe) |
| `invert.glsl` | yes | reusable effect | byte-identical everywhere |
| `recolor.glsl` | yes | reusable effect | byte-identical everywhere |
| `threshold.glsl` | yes | reusable effect | byte-identical everywhere |
| `dither.glsl` | yes | reusable effect | byte-identical everywhere |
| `solarize.glsl` | yes | reusable effect | byte-identical everywhere |
| `scanlines.glsl` | yes | reusable effect | byte-identical everywhere (unrelated to RG's same-named `gallery_mode4_scanlines.frag` — different algorithm, out of scope here per architecture doc's Contract-A-only boundary) |
| `channelshift.glsl` | yes | reusable effect | byte-identical everywhere |
| `hue_rotate.glsl` | yes | reusable effect | byte-identical to shared/BE/TF's copy; not present in FT |
| `ascii_threshold_solarpunk.glsl` | yes, as `"ascii_solarpunk"` | reusable effect | registry-name/filename mismatch, pre-existing, not a QC-specific issue |
| `heatmap_recolor.glsl` | yes | reusable effect | added this session; byte-identical to shared/BRP/BE/TF/FT copies |
| `erosion.glsl` | yes, as `"erosion"` | **same-name/different-behavior collision** | uniforms `accumulated`/`videoFrame`/`decayRate`/`videoAlpha` — structurally different from BRP/BE's `erosion.frag`+`.vert` pair (`history`/`current`/`decayRate`/`currentAlpha`/`desatAmount`, driven by `shared/src/ErosionFBO`). Two unrelated implementations sharing a name; do not merge. |
| `rd_step.glsl` | yes, as `"rd_step"` | reusable effect (currently dormant — see §2 `ReactionDiffusion`) | |
| `motion_accum.glsl` | **no** — loaded directly by `MotionExtraction::setup()`, bypasses `ShaderLibrary` | pipeline-internal shader | private implementation detail of the `motion_extraction` processor, not independently selectable |
| `motion_extract.glsl` | **no** — same as above | pipeline-internal shader | same |
| `motion_effect.glsl` | yes, as `"motion_effect"` | reusable effect (currently single-sketch) | consumes `MotionExtraction`'s output textures; QC-only today but has no QC-specific logic in the shader itself |
| `video_adjust.glsl` | **no** — loaded directly by `VideoSystem.cpp` (`adjustShader`) | utility pass, **currently sketch-specific in practice** | brightness/contrast/saturation correction applied to the raw video source *before* any stylistic effect — conceptually closer to camera/source correction than a "video effect" the other entries represent. Architecture doc's asset layout places it under `utility/`; flagging that promoting it doesn't grant new capability to other sketches until they adopt a comparable pre-adjustment stage of their own. Recommend confirming intent before promoting (see open question in §4). |
| `vert.glsl` | n/a (paired with every `load()` call) | utility pass | byte-identical to the `vTexCoord` family's `vert.glsl`/`effects/vert.glsl` used by BRP/BE/TF/FT (confirmed prior probe) — this is exactly the `common/vert.glsl` the architecture doc's asset layout already names |

### `data/of_nature_shader_pack_glsl/` (10 files + README.md)

| File | Registered in QC `ShaderLibrary`? | Classification | Notes |
|---|---|---|---|
| `bioluminescence.glsl` | yes | reusable effect | byte-identical to shared/BRP/BE/TF's copy |
| `chromatic_aberration.glsl` | yes | reusable effect | byte-identical everywhere |
| `edge_glow.glsl` | yes | reusable effect | byte-identical everywhere |
| `ink_outlines.glsl` | yes | reusable effect | byte-identical everywhere |
| `pixel_drift.glsl` | yes | reusable effect | byte-identical everywhere |
| `pixel_sorting.glsl` | yes | reusable effect | byte-identical everywhere |
| `water_refraction.glsl` | yes | reusable effect | byte-identical everywhere |
| `temporal_trails.glsl` | yes, as `"temporal_trails"` | reusable effect (currently dormant in production — see §2) | only QC has this file; ping-pong orchestration lives in `DebugMode.cpp`, not a dedicated class |
| `caustics.glsl` | **no** | unused asset | confirmed not referenced anywhere in `src/` (grep, this pass and the prior probe agree) — dead data file, QC-only |
| `passthrough.glsl` | **no** | unused asset | actually a **vertex** shader (`varying vec2 vTexCoord; ... gl_Position = ftransform();`), mislabeled among the fragment-only nature pack, unregistered, QC-only |
| `README.md` | n/a | documentation, not a shader | describes the nature pack's origin; not part of this inventory's scope beyond noting it exists |

**Summary:** 24 of 28 files are reusable, byte-identical-everywhere effects
already covered by the architecture doc's `single-pass/` asset list (no new
work needed for those beyond the sync tooling itself). 2 are pipeline-
internal (`motion_accum`/`motion_extract`, private to `MotionExtraction`). 1
is a same-name collision requiring disambiguated IDs (`erosion.glsl` vs.
BRP/BE's `erosion.frag`). 1 is sketch-specific-in-practice utility
(`video_adjust.glsl`, flagged for a decision). 2 are confirmed unused/dead
(`caustics.glsl`, `passthrough.glsl`) and should **not** be promoted or
copied into the canonical tree — recommend deleting them from QC's data
directory as part of whichever phase touches that directory next, rather
than migrating dead weight.

## 2. Stateful/processor effect classes

| Class | Path | Uniforms (via owned shader(s)) | Required textures | State/FBO requirements | Currently wired into QC production? | Reusable? | Proposed canonical ID | Shared asset path | Shared implementation class | Eligible sketches |
|---|---|---|---|---|---|---|---|---|---|---|
| `MotionExtraction` | `sketches/quadrant-crosshair/src/MotionExtraction.{h,cpp}` | `motion_accum.glsl`: `currentFrame`, `previousAccum`, `decayWeight`. `motion_extract.glsl`: `currentFrame`, `delayedFrame`, `accumFrame`, `sensitivity`, `neutralGrey`, `boost`, `gamma`, `outputMode`(int), `referenceMode`(int) | video source texture only as input; produces `motionTex` (`fboMotion`) and `motionDelayedTex` (`fboMotionDelayed`) as outputs | 2 ping-pong accum FBOs (`ACCUM_W×ACCUM_H`=160×90, `GL_RGB`) + 2 extraction-resolution FBOs (full res, capped 640×360 on Pi) + a ring buffer of up to `MAX_HISTORY_FRAMES` (15 Pi / 60 desktop) low-res history FBOs for delayed-frame comparison; also does a CPU `readToPixels` every `ENERGY_SNAPSHOT_INTERVAL`=8 frames to compute `motionEnergy`/`motionCentroidX/Y` | **Yes** — driven every frame by `QuadrantManager`/`ofApp` for the fullscreen motion overlay and per-quadrant variety (per `DebugMode.cpp`'s dependency audit from the prior probe) | Yes — probe already classified this "reusable-after-decoupling," fully self-contained, no quadrant coupling | `motion_extraction` | `motion-extraction/motion_accum.glsl`, `motion-extraction/motion_extract.glsl` | `MotionExtractionEffect` (`kind = Processor`) | BRP/BE/TF already carry `MotionExtraction` via `shared/src` (probe §7) but none of them currently *instantiate* it (BRP: not instantiated at all; BE/TF: instantiated, need to confirm exact call sites before assuming parity — not verified in this pass, flag as follow-up) — QC is the only confirmed active consumer today |
| `Quadrant`'s embedded erosion pass | `sketches/quadrant-crosshair/src/Quadrant.cpp:20-37,144,153-168` (no dedicated class — inline in `Quadrant::setup()`/`draw()`) | `erosion.glsl`: `accumulated`, `videoFrame`, `decayRate`, `videoAlpha` | reads `fbo_read`, writes `fbo_write`, then swaps (`std::swap(fbo_read, fbo_write)`) | 2 full-window-resolution ping-pong FBOs (`fbo_read`/`fbo_write`), allocated once in `Quadrant::setup()`; no separate "primed" first-frame handling (unlike `shared/src/ErosionFBO`) | Yes — runs every `Quadrant::draw()` call unconditionally when `shaderLib->has("erosion")` | Yes, but **needs its own dedicated class** (`Quadrant` currently owns the FBOs and pass inline, not extracted even locally) | `erosion_accumulation` | `erosion/erosion_accumulation.glsl` (rename from `erosion.glsl` to disambiguate — see §3) | `ErosionEffect` variant (`kind = TemporalShader`), configured for the accumulation variant | QC only today; BRP/BE use the other erosion variant (`shared/src/ErosionFBO`, see next row) — both variants are candidates for the same sketches once disambiguated |
| `shared/src/ErosionFBO` (for comparison, not a QC file) | `shared/src/ErosionFBO.{h,cpp}` | `erosion.frag`/`.vert`: `history`, `current`, `decayRate`, `desatAmount`, and a declared-but-**never-bound** `currentAlpha` (confirmed again this pass — `ErosionFBO.cpp::update()` never calls `setUniform1f("currentAlpha", ...)`, matching the bug the prior probe already flagged) | `captureFbo` (scene capture) → ping-pong `fboA`/`fboB` | full-resolution ping-pong pair + a separate capture FBO + explicit `primed` flag to skip blending against the opaque-black initial history on the very first frame (a robustness feature QC's inline version lacks) | n/a (BRP/BE only) | Already a standalone, reusable class (probe: "not duplicated, single canonical copy") | `erosion_history_blend` | `erosion/erosion_history_blend.glsl` (rename from `erosion.frag`/`.vert`) | `ErosionEffect` variant, configured for the history-blend variant (richer: desaturation + primed-frame guard) | BRP, BE today; QC could adopt this richer variant instead of its own once promoted, or both variants can coexist as documented alternates per the architecture doc's "decide whether both remain supported" instruction |
| `ReactionDiffusion` | `sketches/quadrant-crosshair/src/ReactionDiffusion.{h,cpp}` | `rd_step.glsl`: `rdState`, `resolution`, `feedRate`, `killRate` | none besides its own ping-pong state; outputs a texture and (via `readPixels`) full-frame CPU pixels | 2 low-res (160×90) ping-pong FBOs, seeded on `setup()` with a red field + one small green seed square in the center | **No** — confirmed via `grep -rn "ReactionDiffusion\b"` across all of `src/`: the class is defined and implemented but **never instantiated anywhere** (not in `ofApp`, `Quadrant`, or `QuadrantManager`). It is dead code in the current build, despite `rd_step` being registered in `ShaderLibrary` and reachable via `DebugMode`'s manual shader-cycling UI. | Yes, structurally self-contained (no quadrant coupling) — but **flag prominently**: promoting genuinely-dead code changes its status from "harmless dead code" to "maintained shared surface area with zero current callers," which is a real cost or a real feature depending on product intent | `reaction_diffusion` | `reaction-diffusion/rd_step.glsl` | `ReactionDiffusionEffect` (`kind = Simulation`) | none currently — architecture doc §4.7 already says "promote only if it remains part of the intended video-effect palette" and to mark it `safeForAutomaticSelection = false` pending validation; this pass's finding (zero current callers) is a direct input to that decision, not a reason to skip the inventory entry |
| Temporal trails orchestration | **No dedicated class** — lives entirely inline in `sketches/quadrant-crosshair/src/DebugMode.cpp:314-345` (`trailPrevFbo`/`trailOutFbo`) | `temporal_trails.glsl`: `currentTex`, `previousTex`, `decay`, `currentWeight`, `brighten` | `video->getTexture()` as `currentTex`; own `trailPrevFbo` as `previousTex` | 2 full-window-resolution `GL_RGB` FBOs (`trailPrevFbo`/`trailOutFbo`), lazily allocated on first use in `DebugMode::drawShaderFullScreen()`, manually blitted (draw-to-fbo, then fbo-to-fbo copy) each frame to advance the ping-pong | **No** — confirmed via `QuadrantManager.h:43`'s own comment: `"nature pack (temporal_trails excluded — needs per-quadrant FBO)"`. Like `rd_step`, it is registered and debugger-reachable but not wired into any production `Quadrant` draw path. | Yes, but needs genuine extraction — today it's the *only* effect in the entire nature pack whose orchestration is hand-written inside `DebugMode` rather than reusable, per the file classification `DebugMode.cpp:332`'s own comment ("temporal_trails needs its own ping-pong draw path") | `temporal_trails` | `temporal-trails/temporal_trails.glsl` | `TemporalTrailsEffect` (`kind = TemporalShader`) | none currently in production (QC excludes it by the comment above); a natural first real-world consumer once `TemporalTrailsEffect` owns its own ping/pong+resize+clear per architecture doc §4.6, since the per-quadrant-FBO blocker the `QuadrantManager` comment cites is exactly what a shared, instance-owned FBO pair (one instance per quadrant) resolves |
| `RidgelineRenderer` | `shared/src/ridgeline/RidgelineRenderer.{h,cpp}` (already shared, not a QC-local file — listed for completeness) | none (CPU-only; no `ofShader` at all — explicit design choice per the class's own header comment: "no per-pixel fragment shader, no FBO ping-pong," Pi/GLSL-ES-1.0-safe by construction) | none — consumes `ofPixels` directly (`update(const ofPixels&)`), draws with plain GL line/polyline calls, optional `ofTexture*` for `overlayMode` | none (no FBOs) — internal state is just a `std::vector<Line>` of CPU-built polylines, rebuilt in `update()` | Yes — `Quadrant.cpp:127-141` rebuilds ridgeline geometry live whenever a `"ridgeline"` `ShaderSlot` is active, feeding it `videoPixels` | Already reusable/shared (pre-existing, not part of this promotion effort) — included here only to confirm its `kind`/`requiresCpuPixels` metadata for the registry (architecture doc §4.5) | `ridgeline` | n/a (no shader asset — CPU renderer) | `RidgelineEffect` wrapping the existing `RidgelineRenderer` (`kind = CpuRenderer`, `requiresCpuPixels = true`) | BE already uses `RidgelineRenderer` directly (`BEFragment.cpp`'s `ridgelineRenderer` member, confirmed prior probe) in addition to QC — both are real current consumers, making this the *only* item in this inventory with two independent existing callers today |

## 3. Erosion disambiguation recommendation

Per architecture doc §4.4's instruction to "compare both current erosion
implementations, rename them to unambiguous IDs, and decide whether both
remain supported":

| | `erosion_accumulation` (QC's `erosion.glsl`) | `erosion_history_blend` (BRP/BE's `erosion.frag`) |
|---|---|---|
| Uniforms | `accumulated`, `videoFrame`, `decayRate`, `videoAlpha` | `history`, `current`, `decayRate`, `currentAlpha` *(unbound — bug)*, `desatAmount` |
| Desaturation stage | no | yes (`desatAmount`) |
| First-frame handling | none — blends against whatever `fbo_read` was cleared to (opaque black) on frame 1 | explicit `primed` flag skips blending on frame 1, copies through instead |
| Owning class today | none — inline in `Quadrant::setup()`/`draw()` | `shared/src/ErosionFBO` (already a real, reusable class) |
| Recommendation | promote as-is once wrapped in `ErosionEffect`; simpler, no desaturation option | promote as-is; fix the `currentAlpha` bug (architecture doc §4.4/§11 already calls this out as its own correctness change with its own regression test) before or during promotion, not as a silent side effect of the promotion itself |

Both are worth keeping as named variants — they produce visibly different
results (with vs. without desaturation, with vs. without first-frame
priming) and neither is a strict improvement on the other.

## 4. Open questions surfaced by this inventory

1. **`video_adjust.glsl`** — is this actually in scope as a "video effect,"
   or is it source/camera-correction plumbing that belongs outside the
   `VideoEffectService` boundary entirely (i.e. stays a `VideoSystem`-owned
   detail, not promoted)? The architecture doc's asset layout already
   places it under `utility/`, implying "promote it," but nothing else in
   this inventory is applied *before* the effect stage the way this shader
   is. Recommend a decision before Phase 1 copies it into the canonical
   tree, since "utility pass" and "sketch-specific compositor" would lead
   to different placement.
2. **`reaction_diffusion` and `temporal_trails` are both currently dead in
   production** (registered + debugger-reachable only). Promoting dead
   code is legitimate — the architecture doc anticipates it — but confirm
   this is understood before Phase 3 spends implementation effort on two
   effects with zero current callers; the debugger will become the first
   real consumer for both, per architecture doc §4 phase ordering.
3. **`MotionExtraction` instantiation in BE/TF was not verified in this
   pass** — `shared/src/MotionExtraction` is compiled into BE/TF via
   `shared/src`, but whether either sketch actually *creates and drives* an
   instance (vs. just linking the class in unused, the way BRP does) needs
   a direct check before assuming any sketch besides QC has real behavior
   to preserve during `motion_extraction`'s promotion.
4. **`caustics.glsl` and `passthrough.glsl` are recommended for deletion**,
   not promotion — confirm this is acceptable before any phase touches
   `sketches/quadrant-crosshair/data/of_nature_shader_pack_glsl/`, since
   deleting them is a small behavior-preserving cleanup opportunity
   adjacent to this work but is technically outside "promote reusable
   effects" scope.
