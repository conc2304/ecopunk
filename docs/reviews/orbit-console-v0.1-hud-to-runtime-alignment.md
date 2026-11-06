# HUD Runtime -> ExperienceRuntime: Orbit Console Scene Sampling v0.1 semantic alignment

Status: proposed alignment, pending SceneFBO format verification. No HUD rendering, navigation,
skin or schema change is authorized by this note. Checked against the uncommitted runtime sampler
(SceneColorSeries.h / SceneColorSampler.h) on inspection only.

## Requested semantics (HUD Runtime)
1. Sample the rendered SceneFBO before HUD composition; RGB in [0,1]; average in LINEAR light
   (convert if the FBO is encoded sRGB; never read encoded as linear); record the color space.
2. Relative luminance Y = 0.2126 R + 0.7152 G + 0.0722 B on the averaged linear RGB; label it
   "relative luminance", not nits.
3. Alpha: composite against a documented black background only if alpha is meaningful.
4. Cadence ~1 Hz (2 Hz only if profiling justifies the GPU sync cost); 60 s fixed ring;
   no catch-up samples after stalls; RGB and luminance in one record.
5. Each sample: monotonic capture time of the SOURCE frame, scene identity, scene generation,
   RGB, luminance, validity. A late async readback keeps its original identity.
6. Valid / Unavailable / Stale (3 nominal periods). A valid black frame stays valid (0,0,0).
   Failures are never synthetic black.
7. History is generation-aware; no trace connects two generations.
8. Publish as a small optional immutable snapshot SEPARATE from system vitals; do not reuse
   `runtime.vitals.*` IDs.

## Differences found in the current runtime sampler (for ExperienceRuntime to resolve)
- Averaging: the current core averages sRGB-ENCODED 8-bit values (per the earlier proposal sec. 4).
  This note asks for linear-light averaging. Needs a ruling; both cannot be the contract.
- Cadence: current 2 Hz default (kPeriodSeconds = 0.5) vs requested ~1 Hz target.
- Identity: Sample carries a generation but no scene id; the scene id is held once on the runtime
  (sceneColorSceneId_). Requested: scene identity per sample or per generation in the snapshot.
- The SceneFBO is allocated GL_RGBA (8-bit), so it is normalized RGBA8; whether the content is
  sRGB-encoded and whether alpha is meaningful per scene is for ExperienceRuntime to confirm.
- Not yet published: HudFrameData has no scene-color field and the handoff doc named in the
  sampler header does not exist.

## Required ExperienceRuntime response
1 SceneFBO format/encoding; 2 alpha meaning; 3 sampling path + measured sync cost; 4 cadence and
capacity; 5 generation source; 6 whether a new shared type/semantic source is needed (if so, a
narrow additive proposal to Architecture).
