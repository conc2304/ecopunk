> UPDATE: superseded by `orbit-console-v0.1-real-scene-color-integration.md` - the snapshot is published, samples are LINEAR light (GPU reduction), probe is 160x90 / 16x9 float; the encoded 4x4 description below is historical.

# Orbit Console Scene Sampling v0.1 — ExperienceRuntime implementation and HUD Runtime handoff

Status: runtime-private implementation complete and measured on desktop. NOTHING is published:
`HudFrameData`, `shared/src/hud-runtime`, regions, skins, scenes, navigation and input are untouched.
Not staged, committed or pushed.

## 1. What exists

| File | Role |
|---|---|
| `sketches/experience_runtime/src/SceneColorSeries.h` | OF-free core: 120-sample ring, cadence gate, generation identity, validity/staleness, averaging + Rec.709 luma |
| `sketches/experience_runtime/src/SceneColorSampler.h/.cpp` | GL half: halving FBO chain SceneFBO -> 4x4 RGBA8, sync or PBO+fence async readback, stats |
| `src/ExperienceRuntime.h/.cpp` | Private member + hooks in `draw()` right after `sceneFbo_.end()` (before HUD); generation bumps on scene activation; `*ForTesting` accessors only |
| `test/scene_color_series_tests.cpp`, `test/Makefile.tests` | 27 checks, wired into `make -f Makefile.tests test` (all existing suites still pass) |

Env switches (read once in `setup()`): `EXPERIENCE_RUNTIME_SCENE_COLOR=0` disables;
`EXPERIENCE_RUNTIME_SCENE_COLOR_MODE=async` (default sync); `EXPERIENCE_RUNTIME_SCENE_COLOR_STATS=1` logs.

## 2. Answers to the HUD Runtime alignment note

1. **SceneFBO format**: `ofFbo::allocate(w, h, GL_RGBA)` -> 8-bit RGBA, no sRGB internal format,
   no framebuffer-sRGB. Values are whatever the scenes wrote: display-encoded, treated as sRGB.
2. **Alpha**: the FBO is cleared to (0,0,0,0) each live frame, so alpha is meaningful. Each texel
   is composited over opaque black (rgb*a), matching what the viewer sees behind the scene.
3. **Path**: successive linear-filter halving draws (about 8 stages for 1280x720) down to 4x4, then
   one 64-byte readback (16 texels), averaged on the CPU. Not a single blit: one linear blit only
   samples 4 texels and would alias. Chain FBOs are (re)allocated only when the source size changes.
4. **Cadence/capacity**: 2 Hz, 120 samples (60 s), no catch-up after a stall; due-but-busy
   async requests are counted (`droppedBusy`), not queued.
5. **Generation**: runtime counter, +1 on every successful scene activation (the startup scene is
   begun lazily, since setup() activates it outside `activationSucceededThisFrame()`). History is
   cleared, any in-flight readback is dropped/rejected (`push()` refuses another generation). Verified
   over the full five-scene ring: 20 activations -> generations 1..20, each begins with an empty series.
6. **Validity**: a valid black frame is `valid=true` (0,0,0). Static transition frames and readback
   failures record `valid=false` gaps (never synthetic black). Stale after 3 periods (1.5 s).
   A late async result keeps its source-frame time and generation.

### Open items needing a ruling (not decided here)
- **Linear vs encoded averaging.** Implemented: average of sRGB-encoded values (earlier proposal sec. 4).
  HUD Runtime asks for linear light. The GPU chain averages encoded values and cannot linearise per
  texel; true linear averaging would need a linearising first stage (shader) or a larger readback.
  Ruling needed; `Sample` has no color-space field yet.
- **Cadence 2 Hz vs ~1 Hz.** `kPeriodSeconds` is one constant; measured cost below is per sample.
- **Scene id per sample/generation.** `Sample` carries `generation` only; the id is held once on the
  runtime (`sceneColorSceneId_`). A published snapshot should carry the id per generation.

## 3. Desktop measurements (macOS, Release, 1280x720 window, OpenGL; this machine only)

Run via the real app, `STATS=1`, 35 s each, startup Blob scene:

| mode | samples | gaps | busy drops | CPU time per sample (avg / max) | async collect max |
|---|---|---|---|---|---|
| sync  | 59 | 0 | 0 | 3.7 ms / 10.7 ms (first sample: lazy chain alloc) | — |
| async | 58 | 0 | 0 | 3.9–4.1 ms / 9.7 ms | 0.57 ms (first), then ~0.02 ms |

Full five-scene ring (async, 20 activations): 176 issued = 176 completed, 19 gaps (transition
static frames), 0 generation drops, 0 busy drops, avg 3.8 ms / max 11.7 ms, no GL errors, harness PASS lines.

Findings: the readback stall is NOT the dominant cost on this GPU (sync and async are equal); the
~4 ms is CPU-side submission of the ~8-stage chain (FBO begin/end + `ofClear`/draw per stage), paid at
2 Hz (about 0.4% of the time budget but a visible single-frame spike on a 33 ms frame). This exceeds the
proposal's 0.3 ms budget. Likely fixes if needed: raw `glBlitFramebuffer` chain, or a 2-pass compute-free
shader reducing 16x16 per pass (about 3 stages). **Raspberry Pi 3B has NOT been measured** and no
acceptance claim is made; the PBO/fence path needs GL 3.2 (GLES2 target needs a fallback).

Sample values look plausible (Blob scene luma 0.45–0.60 in sync run); no independent pixel-exact
reference check was performed beyond the unit tests of the averaging math.

## 4. Contract impact

None applied. To publish, ONE narrow additive proposal remains as already written in
`orbit-console-v0.1-contract-proposal.md` sec. 3 (`SceneColorSnapshot` in `shared/src/hud-runtime`, optional
`HudFrameData::sceneColor` shared_ptr, source id `runtime.scene_color`), with these amendments from the above:
per-generation scene id, color-space/encoding field, and the linear-vs-encoded ruling. Awaiting Architecture.

## 5. HUD Runtime integration handoff

- Nothing to consume yet: no field, no source id. After Architecture ratifies the type, ExperienceRuntime
  will fill `currentHudFrameData_.sceneColor` from `sceneColor_.series()` as an immutable snapshot copy
  (rebuilt only when `revision()` changes, so the per-frame cost is a refcount bump).
- Series contract to rely on: oldest->newest, strictly increasing monotonic `t`, `valid` per sample,
  single generation per snapshot, `state` Unavailable/Valid/Stale from the newest valid sample.
- Hue stability, saturation floor and drawing remain HUD-side; not touched here.

## 6. Risks
- ~4 ms per-sample spike (above budget); Pi unmeasured. - Encoding ruling pending. - Pausing/immersive
  and HUD-hidden modes still sample (cheap, but could be gated). - No test exercises the GL path in CI;
  coverage there is the desktop runs above.
