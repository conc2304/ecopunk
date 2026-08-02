# Contour Portrait

Real-time contour-line displacement portrait effect: dense parallel scanlines
that bend with source-image luminance/edges, reconstructing a subject out of
line deformation rather than by drawing the source image directly, with a
controllable breakup/dissolve toward one edge of the frame.

Standalone sketch in the `EcopunkVideoCollage` monorepo, generated the same
way as its siblings (`radar-effects-gallery`, `radar-pulse`, etc.) — own
`src/`, own `bin/data/`, `ofxGui` + `ofxOpenCv` as its addons.

**v2 additions**: mask-clipped line existence (lines only exist inside a
subject silhouette, with a bright rim at the boundary) and
importance-weighted line spacing (lines converge around high-gradient
regions and spread apart over flat ones, topographic-map style). Both are
additive on top of the v1 architecture below — see "v2: Mask-clipped
existence & importance-weighted spacing" for what's new.

**v3 addition**: line color can blend with the source image's actual color
underneath each vertex, via a selectable blend mode (Mix/Multiply/Screen/
Add/Overlay/Difference/Subtract/Exclusion) — see "v3: Source color
blending".

## Rendering approach

One small GPU preprocessing pass (`bin/data/shaders/contour_preprocess.{vert,frag}`)
samples the source texture (video/camera/image, plus an optional mask) into
a small FBO (`ContourDisplacementEffect::processedFbo`, default 192x108,
smaller under the constrained/low-res quality presets). It bakes:

- **r** — processed luminance (mirror, invert, brightness/contrast/gamma, optional blur)
- **g** — edge magnitude (cheap cross-tap gradient)
- **b** — soft-thresholded silhouette
- **a** — mask value (`1.0` when no mask is supplied)

That FBO is read back to the CPU **once per frame** (`ofFbo::readToPixels`),
and the line mesh's vertex positions/colors are displaced entirely on the
CPU by bilinear-sampling that small buffer per vertex.

This is a deliberate departure from the "vertex-texture-fetch in a GPU mesh
shader" approach the brief describes as primary, in favor of the CPU
fallback it explicitly allows — made primary here, not just a fallback,
because of this repo's actual deployment reality: `EcopunkVideoCollage`'s
real target is a **Raspberry Pi 3B (VideoCore IV / GLES2)**, and that GPU
does not reliably support sampling a texture from a vertex shader. Building
the vertex-texture-fetch path as primary and the CPU path as an untested
afterthought would mean the "recommended" architecture silently doesn't work
on the hardware that matters here. The one readback is small and bounded
(order of 100-300px per side) and happens once per frame regardless of mesh
density — not per vertex — so it stays cheap. `ContourDisplacementEffect.h`'s
top comment carries this same rationale.

Consequences of this choice, made explicitly rather than left implicit:

- Only **one** custom shader exists (the preprocess pass). The suggested
  class skeleton's `contourShader` (vertex displacement + line
  color/alpha/dropout/mask in GLSL) isn't needed — all of that (displacement,
  breakup, dropout, mask gating, line color/alpha) happens in
  `ContourDisplacementEffect::computeDisplacement()` on the CPU, written
  directly into the mesh's per-vertex position/color buffers.
- Mesh topology (vertex count, line-segment index buffer) is built once in
  `rebuildMesh()` and only rebuilt when `lineCount`, `samplesPerLine`, or
  `orientation` change (`updateTopologyIfNeeded()`, checked once per frame,
  cheap). Per-frame work only ever rewrites the *existing* vertex/color
  arrays via `ofMesh::setVertex()`/`setColor()` — never re-allocates or
  re-indexes.
- Lines are drawn as `OF_PRIMITIVE_LINES` with an explicit index buffer of
  adjacent-in-row pairs — never a triangle grid, and never a strip that
  would connect the last point of one line to the first point of the next.
  Dropout/breakup fade is done by writing vertex **alpha**, not by editing
  the index buffer, so the topology genuinely never has to change for it.
- Line thickness uses `ofSetLineWidth()` (native GL line rendering). Core
  GL profiles commonly clamp line width to 1px — if you need reliably thick
  lines cross-platform, the next step is expanding each segment into a
  camera-facing quad (2 triangles) in `computeDisplacement()`/`draw()`
  instead of relying on `GL_LINES` width. Not done here to keep the first
  pass simple; flagged in Known Limitations below.

## Parameter groups

`ContourDisplacementEffect::parameters()` returns one `ofParameterGroup`
("Contour Effect") with nine nested groups, rendered by `ofApp`'s `ofxPanel`:

| Group | Controls |
|---|---|
| **Input** | `inputEnabled`, `inputMode` (0=Image/1=Video/2=Camera), `mirrorX`, `invertSource`, `sourceFitMode` (0=Stretch/1=Contain/2=Cover), `sourceOpacity` |
| **Preprocess** | `blurRadius`, `contrast`, `brightness`, `gamma`, `threshold`, `thresholdSoftness`, `edgeAmount`, `luminanceEdgeMix` |
| **Geometry** | `orientation` (0=Horizontal/1=Vertical), `lineCount`, `samplesPerLine`, `lineSpacing`, `lineThickness`, `lineLength`, `meshScale`, `positionX`, `positionY` |
| **Displacement** | `displacementSource` (0=Luminance/1=InvLuminance/2=Edge/3=Threshold/4=Mask/5=Hybrid), `displacementAmount`, `displacementBias`, `displacementExponent`, `secondaryDisplacement`, `spatialSmoothing`, `temporalSmoothing`, `displacementClamp` (min/max as one vec2), `depthInvert` |
| **Breakup** | `breakupEnabled`, `breakupAmount`, `breakupStart`, `breakupWidth`, `breakupDirection` (LTR/RTL/TTB/BTT/Radial), `dropout`, `noiseScale`, `noiseSpeed`, `noiseStrength`, `jitter`, `fragmentStretch`, `fragmentDensity`, `seed` |
| **Mask** (v2) | `maskEnabled`, `maskSoftness`, `maskSource` (0=None/1=BgSubtract/2=ExtTexture), `maskSpansPerRow`, `maskEdgeGlow`, `maskTemporalSmoothing`, `maskMatchThreshold` |
| **Line Density** (v2) | `densityMode` (0=Uniform/1=ImportanceWeighted), `densityFloor`, `densitySource` (0=LumGrad/1=Edge/2=Hybrid), `densityTemporalSmoothing`, `densityLUTResolution` |
| **Appearance** | `lineColor`, `lineAlpha`, `backgroundColor`, `backgroundAlpha`, `renderMode` (0=LineOnly/1=LineOverSource/2=MaskedOverlay/3=EffectPlusDissolve), `blendMode` (0=Alpha/1=Additive), `showSource`, `sourceAlpha`, `sourceColorEnabled` (v3), `sourceColorAmount` (v3), `colorBlendMode` (v3, 0=Mix/1=Multiply/2=Screen/3=Add/4=Overlay/5=Difference/6=Subtract/7=Exclusion) |
| **Debug** | `showGui`, `showSourceDebug`, `showProcessedDebug`, `showMaskDebug`, `showColorDebug` (v3), `showMeshBounds`, `freezeFrame`, `reloadShadersTrigger`, `showFps`, `qualityPreset` (0=Desktop/1=Constrained/2=LowResProjection) |

`maskEnabled`/`maskSoftness` lived in Appearance at v1; the v2 brief frames
its new mask fields as "extends existing mask fields," so they moved into a
dedicated Mask group alongside them rather than splitting mask controls
across two groups.

Three additions beyond the two briefs' literal per-group lists, all
required by other sections of the briefs that don't map onto a single named
slider:

- **`renderMode`** (Appearance) — the v1 brief's "Rendering Modes" section
  requires 4 switchable output modes but its own `ofxGui Controls` list
  doesn't name a parameter for it. Added as an int slider rather than left
  unreachable from the GUI.
- **`qualityPreset`** (Debug) — the v1 brief's "Performance Constraints"
  section requires "a quality mode that controls mesh density and
  preprocessing resolution" with specific recommended numbers per tier;
  `applyQualityPreset()` applies those numbers (see table below) when this
  changes. Placed in Debug since it's a technical/performance dial, not an
  artistic one.
- **`maskSource`** (Mask, v2) — the v2 brief lists this as a parameter
  (`none / backgroundSubtract / externalTexture`) without it appearing in
  its own "Updated Parameter Groups" listing; added to the Mask group since
  that's clearly where it belongs and it's required to pick between
  `ContourMaskSource`'s two input paths (see below).

`ofxGui` in this OF version has no dropdown widget (checked: `addons/ofxGui/src`
only has slider/toggle/color/button controls), so every enum above is an
`ofParameter<int>` slider — the same pattern used elsewhere in this repo
(no sketch here uses a dropdown either). The parameter's display name spells
out what each integer means, e.g. `"Input Mode (0=Img 1=Vid 2=Cam)"`.

## Performance tradeoffs

| Quality preset | Working resolution | Lines / Samples | Notes |
|---|---|---|---|
| Desktop (0) | 192x108 | user-set (default 180x320) | full blur/edge available |
| Constrained (1) | 96x54 | 110 / 220 | blur forced off; brief's Pi 3B "80-140 lines / 160-320 samples" range |
| Low-Res Projection (2) | 64x36 | 80 / 160 | blur forced off; brief's low end |

Applied once when `qualityPreset` changes (`applyQualityPreset()`), not
every frame — you can still hand-tune `lineCount`/`samplesPerLine`
afterward without the quality preset fighting you back.

Other constraints from the brief and how they're met:

- **No FBO allocation in update()/draw()** — `processedFbo` and `outputFbo`
  are allocated in `setup()`/`allocateFbos()`/`applyQualityPreset()` (only
  on an explicit quality-preset change or canvas resize), never per-frame.
- **No per-frame mesh rebuild** — see "Rendering approach" above.
- **Bounded texture reads** — exactly one `readToPixels()` per frame,
  regardless of `lineCount`/`samplesPerLine`; the CPU displacement pass then
  does cheap bilinear lookups into that already-resident buffer, not GL calls.
- **The explicit "Avoid CPU readback" performance note is knowingly not
  followed to the letter** — see "Rendering approach" for why the CPU
  fallback was made primary for this repo's real target hardware. It's a
  bounded, single, small-resolution readback, not an unbounded one.

Not yet done, listed here rather than silently skipped: no GPU/CPU timing
instrumentation was added (this repo has none to plug into either —
see "Existing infrastructure reused" below). `showFps` gives frame rate
only. If you need harder numbers, `ofGetLastFrameTime()` is already
threaded through `update()` and is the obvious place to add a rolling
average or `ofxTimeMeasurements`-style probe.

## v2: Mask-clipped existence & importance-weighted spacing

Both v2 capabilities follow the same CPU-first policy as the base effect,
for the same reason (see "Rendering approach"): the v2 brief's own GPU path
(a row-span texture sampled in a vertex shader; a GPU column-reduction for
the density LUT) is offered there with an explicit CPU fallback "consistent
with constrained hardware," and that's what's built here as the primary
path, not a separate decision. Concretely, that means: no `rowSpanTex`, no
`densityLUT` texture, no `densityReduceShader`, and `contourShader` still
doesn't exist — everything lives in
`ContourDisplacementEffect::computeDisplacement()` and its new helpers
(`computeLinePlacement`, `computeDensityLUT`, `computeRowSpans`,
`matchAndSmoothSpans`), operating on plain `std::vector`s.

**Coordinate-space contract** (this is what the brief's "Known Interaction
to Resolve" is actually asking for): the mask polygon passed to
`setMaskPolygon()`, the per-line spans computed from it, and the
density-weighted line positions all live in the same normalized `[0,1]`
canvas space, `(0,0)` = top-left — the same space the preprocess field and
`sampleField()` already use. Concretely:

1. Each frame, `computeLinePlacement()` runs **first** and decides where
   every line actually sits (`lineAcrossNorm[lineCount]`) — either the
   original uniform spacing, or the Capability 2 density-remapped positions.
2. `computeRowSpans()` then tests the mask polygon against each line's
   *actual* position (`lineAcrossNorm[L]`), not a uniform index.
3. Per-vertex field sampling (displacement) and breakup-direction also read
   the same `lineAcrossNorm[L]`.

Because both capabilities are computed from the *same* per-line position
array, in that order, mask clipping and density-weighted spacing
automatically agree about where "row" and "column" correspond to on
screen when both are active — that's the resolution the brief asked for
implemented as a straight-line data dependency rather than a coordinate
transform or a runtime toggle.

`scanlineCrossings()` implements the brief's even-odd polygon test as
given, generalized for orientation: for horizontal lines it's exactly the
brief's pseudocode (test against y, return x-crossings); for vertical
lines the roles are transposed (test against x, return y-crossings) — the
brief's algorithm is written for one orientation, this fills in the other.

**Mask sources** (`maskSource` in the Mask group), both funneled through
`ContourMaskSource` (`ofxCvContourFinder` on a binary image, largest blob
only — same as the brief's own `blobs[0].pts`):

- **`None` (0)** — no polygon, no clipping. Default; fully stable, same
  "no mask supplied" behavior as v1.
- **`BgSubtract` (1)** — background subtraction
  (`ofxCvGrayscaleImage::absDiff` + `threshold`) against a captured
  reference frame, for a static-camera live setup, per the brief. `ofApp`
  auto-captures the reference the first frame this mode is selected (also
  bindable to `b` to re-capture after the scene changes — e.g. lighting
  shifted, or someone walked through frame during the reference capture).
  Needs a live video/camera source; doesn't make sense for a single static
  image (there's nothing to diff against).
- **`ExtTexture` (2)** — threshold + contour trace directly on whatever
  texture was last passed to `effect.setMask()` (e.g. `bin/data/mask.png`,
  or a foreground mask computed elsewhere in a larger composition). No
  background reference needed; this is the path for still-image mode, or
  for reusing an externally-computed mask instead of diffing.

Both paths run at a small, bounded, independent working resolution
(`ContourMaskSource::setup(160, 90)` in `ofApp`) — one more small CPU pass
per frame, same discipline as the main effect's own preprocessing, not an
unbounded second cost source.

**Rim/glow**: implemented as the brief's cheap Option 1 (an edge-distance
term, no extra render pass) — `4 * vis * (1 - vis)` on the already-computed
span visibility, which peaks exactly at a transition and is zero deep
inside/outside, added into vertex alpha scaled by `maskEdgeGlow`. The
brief's Option 2 (threshold-and-blur bloom pass) is not implemented — it's
explicitly framed there as the higher-cost, quality-gated alternative; left
for Next Improvements.

**Temporal span matching**: `matchAndSmoothSpans()` implements the brief's
match-by-center-proximity-then-lerp algorithm essentially verbatim,
per-line. Static-image mode never calls `updateFromLiveDiff()` (there's
nothing to re-diff every frame), so there's nothing to smooth — the brief's
"skip this entirely for static-image mode" falls out automatically rather
than needing a special case.

**Density LUT edge cases** (brief's validation list): a near-uniform image
produces a near-flat importance signal, whose CDF is then near-linear, so
the inversion recovers uniform spacing on its own — no special-casing
needed. An extreme single-edge image is guarded explicitly: after
inversion, a minimum-separation clamp (`0.15 / lineCount` in normalized
units) prevents lines from collapsing onto one degenerate point, per the
brief's own validation requirement for that case.

**Known limitation this section resolves partially, not fully**: `blobs[0]`
is the *largest* blob only (matching the brief's own algorithm exactly), so
genuinely disjoint regions (a separate, smaller blob, not a shape still
connected by a thin bridge) aren't merged into one mask. See Known
Limitations.

## v3: Source color blending

Lines can pick up the source image's actual color instead of always drawing
in the flat `lineColor`. Same CPU-first shape as the rest of this effect,
but with its own small GPU pass rather than reusing `processedFbo`:

- **Why a second pass/texture, not a repurposed channel of the existing
  one**: `processedFbo` is a 4-channel `RGBA8` target already fully spoken
  for (luminance/edge/silhouette/mask). Getting real RGB color out means
  either GLES2 multi-render-targets (not guaranteed on this repo's Pi 3B
  target — the same reasoning that's kept this effect off vertex-texture-
  fetch throughout) or a second small pass. `contour_color.{vert,frag}`
  does the latter: it reuses the *same* `uvScale`/`uvOffset`/`mirrorX`
  transform `preprocessSource()` already computed this frame (so the two
  textures stay pixel-aligned for a given `(x, y)`), but skips grayscale
  conversion entirely and just outputs the source's RGB.
- **Fully opt-in cost**: the color pass/readback (`preprocessColor()`,
  another bounded working-resolution `readToPixels()`) only runs when
  `sourceColorEnabled` is on — with it off (the default), this feature
  costs nothing beyond one boolean check in `update()`.
- **Blend modes** (`colorBlendMode`, per-channel, standard compositing
  formulas): **Mix** (straight crossfade to the source color — the
  simplest, and the natural default), **Multiply** (darkens toward the
  source color, good for tinting white lines with photographic color
  without blowing out), **Screen** (brightens, good over a dark
  background), **Add** (strong, tends to blow out highlights fast —
  useful for a hot/glowing look), **Overlay** (contrast-preserving mix,
  often the closest to "recognizable photo color, still reads as
  lines"), **Difference** (`|base - src|` — inverts toward the source
  color's complement where they diverge; high-contrast, glitchy/analog
  look, good with saturated `lineColor`), **Subtract** (`base - src`,
  clamped at 0 — source color eats into line brightness, tends toward
  black where the source is bright; moody/high-contrast), **Exclusion**
  (`base + src - 2·base·src` — same shape as Difference but softer/lower-
  contrast, less likely to crush to pure black/white). Every mode is
  computed as its full per-channel effect, then `sourceColorAmount`
  linearly blends that against the plain `lineColor` — so `amount` reads
  consistently as "how much of this mode's result" across all eight,
  including Mix (whose "full effect" is simply the source color).
- **Where it samples**: the exact same per-vertex `(screenX, screenY)`
  already used for displacement field sampling and breakup direction — so
  the color under a line segment matches the content that's actually
  displacing it, including under v2's density-remapped line positions.
- **Composes with breakup/dissolve**: the blended RGB (not the flat
  `lineColor`) is what dissolve-mode particles inherit too, so fragments
  breaking off a photographically-tinted line keep that color rather than
  reverting to flat `lineColor`.
- **Try it**: enable `Source Color Enabled`, then flip through
  `Color Blend Mode` with `Source Color Amount` around 0.6-0.8 — `Show
  Color Debug` (Debug group) shows the raw sampled-color texture directly
  if you want to see exactly what's being blended in.

## How to supply source and mask textures

**Video** (default): `ofApp` scans `bin/data/media/` first; if empty, falls
back to `bin/data/sharedMedia/`, a symlink to
`sketches/blueprint_emergence/bin/data/media` — the same shared clip pool
`quadrant-crosshair`, `radar-effects-gallery`, `radar-pulse`, and
`temporal-fields` already point at. Drop your own `.mp4` into
`bin/data/media/` to override with a specific test clip (e.g. a real
portrait/silhouette clip) without touching the shared pool.

**Image**: drop a file at `bin/data/media/sample.jpg`, then switch `Input
Mode` to `0` (Image) in the GUI or press `i` to cycle modes.

**Camera**: switch `Input Mode` to `2`. Guarded against the machine having
no camera or camera permission not granted to the process — see Known
Limitations.

**Mask (raster, v1)**: call `effect.setMask(&someTexture)` from `ofApp` with
any `ofTexture*` (a real-time blob/foreground mask, a static PNG, whatever's
available) — `ofApp::setup()` shows the pattern by optionally loading
`bin/data/mask.png` if present. Then enable `Mask Enabled` (Mask group) to
gate breakup by it, or set `Displacement Source` to `4` (Mask) to drive
displacement from it directly. The effect is fully stable with no mask ever
supplied (`activeMask == nullptr` is the default, checked everywhere it's
read).

**Mask (polygon, v2, existence clipping)**: independent of the above — call
`effect.setMaskPolygon(polygon)` with a `std::vector<glm::vec2>` in
normalized `[0,1]` canvas space (see the coordinate-space contract above).
`ofApp` doesn't call this directly; it goes through `ContourMaskSource` and
the `maskSource` parameter (`updateMaskPolygon()` in `ofApp.cpp`) — set
`maskSource` to `BgSubtract` for a live camera/video setup (press `b` once
to capture the background reference) or `ExtTexture` to contour-trace
whatever's already wired via `effect.setMask()`. Pass an empty vector (or
leave it untouched) to clear/skip clipping; fully stable with none supplied.

## How to create presets

Two independent layers:

1. **Seven built-in presets** (Clean Portrait, Topographic Figure, Side
   Dissolve, Analog Scan, Ghost Contour, and v2's Silhouette Emergence,
   Converging Contours) — `ContourPresets::apply(effect,
   name)` in `src/ContourPresets.cpp`, writing directly to
   `ContourDisplacementEffect`'s private `ofParameter` members (it's a
   declared friend function, not a string-keyed lookup through the
   `ofParameterGroup` tree — a typo in a member name fails to compile
   instead of silently no-oping at runtime). Add a sixth preset by adding
   another `else if (name == "...")` block and a button in `ofApp::setup()`.
2. **User-saved custom presets** — the "Save Custom Preset" GUI button calls
   `ofxPanel::saveToFile()`, writing the *entire* current parameter state
   (all nine groups) to `bin/data/presets/custom_<timestamp>.xml`. "Load
   Custom Preset" loads the most recently saved one via
   `ofxPanel::loadFromFile()`. This is `ofxGui`'s own built-in
   serialization — no custom preset-file format was invented.

Note on the two v2 presets: **Converging Contours** works immediately on
any source (it only touches Line Density, mask stays off). **Silhouette
Emergence** sets `maskSource = BgSubtract`, so it needs a live video/camera
source and a captured background reference (`ofApp` auto-captures one the
frame this preset's mode is selected) to actually clip lines to a subject —
with no background captured yet, `maskPolygon` stays empty and it falls
back to the full-frame look, which is the documented no-mask-supplied
stability behavior, not a bug.

## Debug views

All off by default (Debug group): `Show Source Debug` / `Show Processed
Debug` / `Show Mask Debug` draw thumbnails in the top-right corner (raw
source, the preprocessed r/g/b/a field, and the mask respectively). `Show
Mesh Bounds` draws a red bounding box around the current frame's displaced
vertex extents, **and** (v2) a green outline of the current mask polygon if
one is set — the brief's "Breakup gradient"/"Displacement field" debug
views are covered by the existing Processed Debug view (edge/luminance/
silhouette channels are visible there directly) rather than duplicating
separate views for them. `Freeze Frame` stops both preprocessing and
displacement (mesh holds its last computed state). `Reload Shaders`
re-loads `contour_preprocess.{vert,frag}` from disk without restarting the
app.

## Controls (window)

- `i` — cycle Input Mode (Image → Video → Camera → …)
- `m` — toggle Mask Enabled
- `b` — capture the background reference for `maskSource = BgSubtract`
- `g` — toggle the on-screen fps/vertex-count overlay
- `f` — toggle the Debug group's fps readout

## Files

**v1: all new** (nothing existing in the monorepo was modified). **v2:
modifies only files inside this sketch** (`ContourDisplacementEffect.*`,
`ContourPresets.*`, `ofApp.*`, `addons.make`, this README) plus two new
files (`ContourMaskSource.h/.cpp`) — still nothing outside
`sketches/contour-portrait/` touched.

```
sketches/contour-portrait/
  Makefile, config.make, addons.make, Project.xcconfig,
  openFrameworks-Info.plist, of.entitlements
  src/
    main.cpp
    ofApp.h / .cpp                        -- thin orchestrator only
    ContourSource.h / .cpp                -- image/video/camera input wrapper
    ContourDisplacementEffect.h / .cpp     -- the effect itself
    ContourPresets.h / .cpp                -- 7 built-in presets
    ContourMaskSource.h / .cpp             -- v2: ofxOpenCv mask-polygon source
  bin/data/
    shaders/contour_preprocess.vert
    shaders/contour_preprocess.frag
    shaders/contour_color.vert             -- v3: source color pass
    shaders/contour_color.frag
    media/            -- empty; local clip override (see above)
    presets/           -- user-saved custom presets land here
    sharedMedia -> ../../../blueprint_emergence/bin/data/media  (symlink)
```

## Existing infrastructure reused

- **`ofxGui`** — the only addon this sketch depends on, same as every other
  sketch in this monorepo.
- **Shared clip pool** (`bin/data/sharedMedia` symlink convention) — reused
  exactly as `radar-effects-gallery`/`radar-pulse` do it, rather than
  inventing a new media location.
- **Shader `#version` convention** — matches the pattern actually used by
  every shipped shader in this repo (`#version 120` unconditional first
  line, `#ifdef PLATFORM_PI` precision qualifier after). Initially written
  using the *other* pattern seen in `shared/src/shaders/*.vert`
  (`#ifdef PLATFORM_PI #version 100 #else #version 120 #endif`), which
  fails to compile on this machine's desktop GL driver (`#version` must be
  the first token, and the conditional wrapper violates that) — caught by
  actually building and running the sketch, not just by reading other
  sketches' source. `shared/src/shaders/*` appear to be unused by any
  currently-buildable sketch (its `ShaderLibrary.cpp` references `.glsl`
  paths that don't match the actual `.frag`/`.vert` files sitting next to
  it), so this divergence was never previously exercised by a real build.
- **`ofxPanel` save/load** — used as-is for preset serialization (see
  above) rather than introducing a bespoke JSON preset format, since no
  dedicated multi-preset manager exists at the shared level for `ofxGui`-based
  sketches (`FireplaceWaterfall`'s `PresetManager` is a different app with a
  different GUI stack, not something this monorepo shares).
- **`ofxOpenCv`** (v2) — not a dependency anywhere else in this sketch's own
  history, but already present and building successfully elsewhere in this
  monorepo (`sketches/blob-region-prototype`, built concurrently with this
  work) by the time v2 needed it, confirming it's a safe, working addon
  choice on this repo's toolchain rather than an unverified new dependency —
  `pkg-config`/the bundled `opencv.xcframework` resolve automatically
  through the standard `ADDON_PKG_CONFIG_LIBRARIES` mechanism, no
  sketch-side config needed beyond listing it in `addons.make`.
  `ContourMaskSource` uses `ofxCvColorImage`/`ofxCvGrayscaleImage`
  (`absDiff`, `threshold`) and `ofxCvContourFinder` directly, per the v2
  brief's own algorithm description — no shared/src OpenCV wrapper exists
  in this monorepo to reuse instead.

**Not reused, deliberately**: `shared/src/VideoSampler` (built for
seek-and-freeze single-still capture into collage fragments, not continuous
playback), `shared/src/ShaderLibrary` (name→path map for a fixed roster of
single-texture fullscreen effects; this effect only ever loads one shader),
`MotionExtraction`/`GridSystem`/`Fragment` (all built around a specific
sketch's compositional model, not a generic displacement-field effect).
Pulling any of them in would have added coupling without saving meaningful
code, the same call `radar-effects-gallery` made for its own local video
sampler/reveal-mask rather than depending on `radar-pulse`'s.

## Known limitations

- **Line thickness above 1px is not guaranteed** on core GL profiles (see
  Rendering Approach). Works as expected on this dev machine's driver;
  treat `lineThickness` as "thin vs. thicker," not a precise pixel value,
  until/unless a quad-strip renderer replaces `GL_LINES`.
- **`ofVideoGrabber` (camera) can throw an uncaught Objective-C exception
  on macOS** when the OS can't hand the process a real capture input (e.g.
  camera permission not granted) — discovered by actually running this
  sketch on the dev machine used to build it, which has no camera access
  granted to the build process. This is a gap in `ofVideoGrabber`/
  `ofAVFoundationGrabber` itself, not specific to this effect, but it's
  guarded here at the point of impact: `ContourSource::applyMode()` wraps
  `grabber.setup()` in `@try`/`@catch` (this repo's `.cpp` files already
  compile as Objective-C++ on macOS, confirmed from the actual build
  command), latches a `cameraKnownBroken` flag so a flaky permission state
  isn't retried every time the mode flips back to camera, and
  `ContourSource`'s destructor wraps its own cleanup the same way (a setup()
  exception can leave the underlying `AVCaptureSession` stuck mid
  `beginConfiguration`, which then throws again on close — this stops that
  second throw from reaching the process's default terminate handler at
  shutdown). If you hit this, grant camera permission to the terminal/app
  running this sketch, or just use Video/Image input.
- **No GPU/CPU timing instrumentation** (see Performance Tradeoffs) —
  none existed anywhere in this repo to plug into.
- **Secondary (along-line) displacement and jitter share the same
  `ofSignedNoise`/`ofNoise` calls** as the perpendicular displacement and
  breakup drift, phase-shifted rather than using independent noise fields.
  Cheap and visually sufficient, but not literally independent axes if you
  push several of those parameters to their extremes simultaneously.
- **(v2) Only the largest blob becomes the mask polygon** — matches the v2
  brief's own algorithm (`blobs[0].pts`) exactly, but means a genuinely
  disjoint region (a limb separated from the torso by open space, as
  opposed to one still connected by a thin bridge) is simply not
  represented; it's a separate, smaller, unused blob. Multi-blob merging
  wasn't added since the brief's own algorithm doesn't call for it.
- **(v2) `ContourMaskSource`'s threshold and blob-area percentage bounds are
  fixed constants in `ofApp::updateMaskPolygon()`** (`thresholdValue = 40`,
  `minAreaPct = 1`, `maxAreaPct = 90`), not exposed as GUI parameters — the
  v2 brief's own `Mask` parameter list doesn't name them either. Reasonable
  general-purpose defaults, but a genuinely dim/bright scene or an
  unusually small/large subject may need them hand-tuned in code.
- **(v2) The full bloom-pass rim-glow option (brief's Option 2) isn't
  implemented** — only the cheap edge-distance term (Option 1). See "v2:
  Mask-clipped existence" above; the brief itself frames Option 2 as the
  higher-cost, quality-gated alternative.
- **(v2) `lineSpacing` becomes a no-op when `densityMode = ImportanceWeighted`**
  — density weighting takes over line placement entirely in that mode
  (`ContourDisplacementEffect::computeLinePlacement()`). Deliberate and
  documented in code, not an oversight, but it's easy to go looking for
  `lineSpacing`'s effect and not find it while density mode is on.

## Recommended next improvements

1. **Quad-strip line rendering** for guaranteed line thickness across GL
   profiles/hardware (see Known Limitations).
2. **Independent noise fields** for secondary displacement vs. breakup
   drift vs. jitter, if pushing all three at once starts looking
   correlated in practice.
3. **A preset *browser*** (list + pick, not just "load most recent") if
   users start accumulating more than a couple of saved custom presets.
4. **GPU/CPU timing overlay** wired to `showFps`, once this repo has a
   shared pattern for it worth following rather than inventing a one-off.
5. **(v2) Multi-blob mask support** — merge more than the single largest
   blob when genuinely disjoint regions matter for a given subject (see
   Known Limitations).
6. **(v2) Expose `ContourMaskSource`'s threshold/area-percentage bounds as
   GUI parameters** if the fixed defaults prove wrong often enough in
   practice to be worth the extra Mask-group sliders.
7. **(v2) The full bloom-pass rim-glow (brief's Option 2)**, gated behind
   `qualityPreset`, for a softer/higher-fidelity glow than the current
   cheap edge-distance term.
