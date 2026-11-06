# Orbit Console v0.1 - real scene-colour integration (ExperienceRuntime + HUD hookup)

Status: IMPLEMENTED and desktop-verified with real SceneFBO measurements. Not staged/committed. Hardware (Raspberry Pi) NOT qualified.
Repository revision: `03b3bb2` (Typography presets, Experience Settings page, and MENU nav button) + a large uncommitted working tree
(many concurrent owners; ~90 changed/untracked paths outside fragment-trail/blueprint_emergence).

## 1. Public types and source IDs
- `shared/src/hud-runtime/SceneColorSnapshot.h` (OF-free): `scenecolor::State {Unavailable, Valid, Stale}`, `ColorSpace {LinearSrgb}`,
  `Sample {double t; float r,g,b,luma; bool valid}`, `SceneColorSnapshot {revision, generation, sceneId, capturedAtSeconds, state, colorSpace,
  nominalPeriodSeconds, count, history[120]}`; constants `kCapacity=120`, `kNominalPeriodSeconds=0.5`, `kStalePeriods=3`.
- `HudFrameData::sceneColor` : `std::shared_ptr<const scenecolor::SceneColorSnapshot>` (null = none yet, never zeros).
- Source ID `runtime.scene_color` (Scalar presence key): newest VALID relative luminance while the snapshot is Valid; missing for null/Unavailable/Stale.
  The Orbit draw path reads the whole series from `frame.sceneColor` (renderer adapter), not through the scalar.

## 2. Measurement definition (as built)
Source: runtime-owned SceneFBO right after `sceneFbo_.end()` (before HUD, shell, scaling). Validated once per size: RGBA8, no sRGB internal
format, no framebuffer-sRGB (logged: `internalFormat=0x1908 framebufferSRGB=off`); anything else becomes explicit gaps. Alpha is meaningful
(FBO cleared to 0,0,0,0): rgb*alpha composited over opaque black. r,g,b = mean in LINEAR light (sRGB EOTF per source texel, then average);
luma = Rec.709 relative luminance of the averaged linear RGB. 2 Hz, 120 samples (60 s), monotonic source-frame timestamps, generation per scene
activation (+ scene id), stale after 3 periods, valid black stays valid, failures/static transition frames are gaps.
Path A (desktop, used): two GPU passes in RGBA16F (160x90 box with EOTF, then 16x9 box), 16x9x16 B synchronous float readback.
Path B (fallback: GLES2 / no half-float targets / `EXPERIENCE_RUNTIME_SCENE_COLOR_FALLBACK=1`): 8-bit halving chain to ~160x90 + CPU EOTF average
(documented lower accuracy). Async PBO readback applies to the fallback path only and needs GL 3.2 (else falls back to sync).
Snapshots are rebuilt only when series revision, evaluated state, generation or scene id changes (shared_ptr copy per frame otherwise).

## 3. Files and ownership
ExperienceRuntime: `src/SceneColorSeries.h`, `src/SceneColorSampler.h/.cpp`, `src/ExperienceRuntime.h/.cpp` (publish + measurement stats),
`test/scene_color_series_tests.cpp`, `test/Makefile.tests`.
Shared contract (ratified additive): `shared/src/hud-runtime/SceneColorSnapshot.h`, `HudFrameData.h`.
HUD Runtime: `HudWireframeRenderer.cpp/.h` (adapter -> `OrbitSeriesView`: display-encoded RGB for swatch/ribbon/orbit/bars/chart, linear luma),
`HudRealFrameResolver.cpp` (`runtime.scene_color`), `SegmentedHistory.h` (luminance strip brightness = sRGB-encoded relative luminance),
`hud_orbit_tests.cpp`, `hud_vitals_tests.cpp`. No change to Option B placement, segmented-history geometry, skins, scenes, media or input.
Note: another session's `OrbitInjectHarness` fixtures now interpret `OrbitSample::luma` as linear relative luminance.

## 4. Tests
- `scene_color_series_tests`: 47 checks (sRGB 128 -> 0.2158 linear; black/white mean 0.5 in linear light; red -> Y 0.2126; alpha 0 -> black; valid black vs
  unavailable; gaps; generation reset and late-result rejection; revision/snapshot rebuild rules; stale republish; bounded ordered history; colour space field).
- HUD: 227 vitals+orbit checks (adapter: encoding, generation reset, gap stays gap, Stale, revision-keyed cache, resolver), presentation 301, real-frame 357, orbit 34.
- Runtime suite (`make -f Makefile.tests test`): all pass.
Desktop (isolated app copy, Dark Moss + production typography): five-scene ring 102 passed / 1 failed (separately owned Fragment `maxFragments`), 0 GL errors,
51 generations, ~57 gaps of 392 samples (transition static frames). HUD-006 v2 harness 23/23, focus 21/21.

## 5. Accuracy against an exact reference (not a fixture)
Self-test (`EXPERIENCE_RUNTIME_SCENE_COLOR_SELFTEST=1`) compares each sample with the exact full-resolution linear average of the same frame.
GPU path, five scenes, 399 samples: mean |dY| 0.00009, max 0.00043, max channel error 0.0008. The earlier CPU-linearised probe (40 wide) measured mean 0.008 / max 0.044
on dithered Quadrant content and was replaced. Fallback path (blob run): mean |dY| 0.0018 (higher on high-contrast detail).

## 6. Cost (desktop, same scene/conditions, CPU time of ExperienceRuntime::draw, 600-frame windows, Dark Moss)
Enabled: 1996 / 1867 / 2007 us (mean 1957); disabled (`SCENE_COLOR=0`): 1797 / 1533 / 1883 us (mean 1738) -> about +0.22 ms per frame on average.
Per sample (2 Hz): issueAvg 3.3-3.4 ms CPU wall (includes the synchronous readback wait), first sample up to ~16-18 ms (shader/FBO creation).
Fallback path: issueAvg 8.8 ms. Snapshot publication: one rebuild per sample (~58/30 s), a 120-sample copy, not separately measurable in the draw window.
This exceeds the original 0.3 ms-per-frame budget only if the per-sample spike is counted per frame; open Architecture question (budget vs 2 Hz spike).

## 7. Screenshots (real, Dark Moss, production fonts)
`docs/reviews/orbit-placement/real-data/`: real_blob/temporal/blueprint/fragment/quadrant.png (live Orbit values), loading_real_stale_orbit.png
(real Loading frame: Orbit Stale with held dimmed readings). Earlier synthetic fixtures (`*_SYNTHETIC.png`) are NOT proof of live data.

## 8. Option B reference documentation
Option B placement unchanged: #4 badge, #5 scene status/effect chips, #6 ACTIVITY, #7/#8 Orbit (now live), #15 vitals. Orbit state semantics on live data:
Unavailable -> NO DATA; Valid -> readings; Stale (no valid sample for 1.5 s, e.g. during a scene transition's static frames) -> held readings dimmed + STALE;
new scene generation -> history cleared. Env switches: `EXPERIENCE_RUNTIME_SCENE_COLOR=0|1`, `_STATS=1`, `_SELFTEST=1`, `_FALLBACK=1`, `_PROBE`, `_MODE=async`.

## 9. Hardware / qualification
Raspberry Pi 3B NOT measured. Blockers: GLES2 -> only the fallback path applies (needs its own accuracy and cost qualification); sync readback stall on
VideoCore is unmeasured; hardware safety requirements not exercised. Fullscreen: the RT-007 harness could not be re-run at the end of this session
(Cocoa `NSInternalInconsistencyException` in `setFullscreen`, reproduced with the previous build too, i.e. a display-environment fault); fullscreen was verified
earlier in the session on the same HUD layout but not with live Orbit data.

## 10. Frozen-contract incompatibilities
None found. `HudFrameData` change is the ratified additive field. Open (not blocking): per-sample cost budget; linear-light accuracy on the Pi fallback.
