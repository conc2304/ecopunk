# phosphor-contours (standalone developer sketch)
Structural luminance -> continuous flowing bands (isophotes and/or displaced scanlines) -> dotted/dashed CRT phosphor raster with analogue
row slip, dropout, persistence and glow. All rendering lives in the shared header-only core `shared/src/phosphor-contours/`
(`PhosphorContoursPipeline.h`, `…Shaders.h`, `…Params.h`); this sketch only owns a video player + developer panel.
The runtime consumes the same core through the opt-in `PhosphorContoursScene` (see
`docs/reviews/phosphor-contours-roster-request.md`).

Build/run: `cd sketches/phosphor-contours && make Release -j4 && make RunRelease`
Media: `bin/data/media/` if it has clips, else the canonical `assets/shared/media/` (SharedMediaRoot.h). No camera, no playlist policy.

## Pipeline
source FBO -> **luma** (AA 4-tap downsample to 1/4 res; 1/8 on PiSafe) -> **structural blur** (2 separable gaussian pairs, sigma `structBlur` px)
-> **structure** (Isophote / RasterRelief / Hybrid line signal; raw luma only modulates emission) -> **raster** (dot/dash cells, per-row slip,
seeded luma-dependent dropout, cell snap) -> persistence (exp(-dt/tau)) -> 1/4-res glow -> final.
Geometry is defined ONLY by the blurred structural luma. Fine detail (raw - structural) modulates emission by `detail` (default 0.18).
Why the old version collapsed into stipple: the isophote field was built from a barely-smoothed luma (3x3 at half res), so on textured clips
dL/dx exceeded one band per phosphor cell and the lines were sub-cell; the old code then faded them to tone stipple.

## Algorithms (`Params::algorithm`, key `A`, env `PHC_ALGO=0..2`)
- **Isophote**: `fract(S*levels)` of the structural luma S; width is constant in OUTPUT pixels (phase distance / local slope), slope from texture
  differences (no `fwidth`, so no `GL_OES_standard_derivatives` dependency in this path). Bands too dense to resolve fade (`conf`) and the raster
  substitutes a light tone stipple.
- **RasterRelief**: a regularly spaced family of base scanlines, each displaced across itself by `reliefAmp*(S - reliefPivot)`. Implicit curve
  family: per pixel the base coordinate is recovered by a 4-step fixed-point iteration, distance to the nearest base line is scaled by the local
  slope. No loop over lines, no geometry (~7 taps). Smooth `reliefDrift` scroll; optional vertical baseline (`reliefVertical`).
- **Hybrid**: `hybridIso*isophote + (1-hybridIso)*relief` mixed BEFORE the phosphor mask, so raster/dropout/glow run exactly once.
Presets select an algorithm: ReferenceDense = Hybrid (default), SparseSignal = Isophote, MeltedRaster = RasterRelief, Strata = Isophote (clean, many levels).
Runtime modes (same order): TOPOLOGY / ISOLINES / SCANLINES / STRATA, plus two scene selector axes DENSITY (line count) and DECAY (persistence, dropout, slip, glow); see `phc::resolveLook` in the Params header.

## Tonal range
`autoRange` (default on): a 16x9 mean-pool of the structural luma is read back every 8 frames, 3rd/97th percentile + 10% padding give the
black/white points, smoothed with a ~1 s time constant, snapped on media change/reset (never fades from the previous clip's exposure). Minimum
span 0.2, so a very dark or flat clip is lifted but not amplified into noise. Off => `tonalLo/tonalHi/contrast/gamma`.
The readback is tiny but is a GPU sync once per 8 frames: unmeasured on Pi.

## Quality
Desktop (default) / PiSafe (1/8-res structural luma, 1/2-res contour, 1/8 glow, no persistence). Pi 3B+ feasibility is UNMEASURED.

## Controls
Keys: 1/2/3/4 preset, **A** algorithm, **X** auto range, **B** bare structure (all finishing off), **D** dropout, **L** slip, **K** grain, P persistence,
G glow, V view (final/source/luma/contour/raster), R reseed, 0 reset, Up/Down select parameter, Left/Right adjust, Q quality, N/M video, T GPU timing,
H hide panel, S screenshot. Parameters are grouped in the panel: structure, isophote, relief, hybrid, phosphor.
Env: PHC_VIDEO=<substr> PHC_PRESET=0..2 PHC_ALGO=0..2 PHC_SEED=n PHC_FIXED_DT=sec PHC_FREEZE=0..1 PHC_QUALITY=pisafe
     PHC_SET="levels=14,reliefAmp=80,autoRange=0" (knob overrides; also toneFill, fill, reliefVertical)

## Evidence tooling (all need PHC_AUTOSHOT_DIR=<dir>; fixed dt; frames come from the real OF renderer)
- `PHC_MATRIX=<tag> PHC_VIDEO=<clip> PHC_FREEZE=0.4`: ONE frozen frame -> source, structural luma, bare Isophote/RasterRelief/Hybrid (contour only,
  every finishing stage off), finished x3, preferred preset, plus a labelled 3x3 contact sheet.
- `PHC_BENCH=1`: ms/frame (glFinish-bracketed pass cost), GPU target MiB and RSS for every algorithm x quality x autoRange.
- `PHC_SEQ=<N> PHC_FREEZE=<start> PHC_FIXED_DT=0.033333`: N frame-accurate motion frames (video stepped 1/30 s per frame) + temporal stability stats.
- (older) default autoshot of the three presets, `PHC_STABILITY=1`.

Tests: `make -C sketches/experience_runtime/test -f Makefile.tests phosphor_contours_tests && ./phosphor_contours_tests`.
