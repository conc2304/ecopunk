# Shader/Effect System Probe

Investigation-only report. No implementation performed. Companion document
[`implement-shader-effect-debugger-and-service.md`](./implement-shader-effect-debugger-and-service.md)
converts these findings into an ordered implementation plan.

Sketch abbreviations used throughout: **BRP**=blob-region-prototype,
**BE**=blueprint_emergence, **TF**=temporal-fields, **FT**=fragment-trail,
**QC**=quadrant-crosshair, **RG**=radar-effects-gallery, **RP**=radar-pulse,
**CP**=contour-portrait.

## Executive summary

Six sketches (BRP, BE, TF, FT, QC, RG) implement a genuine multi-effect,
selectable shader system; two (CP, RP) apply exactly one fixed-purpose
shader each and have no registry; two (hud_elements, hud_validation_harness)
have no shader system at all. Three of the six (BRP, BE, TF) share one C++
`ShaderLibrary` class via `shared/src`; two (FT, QC) forked their own local
copy of the identical class years apart and have since drifted in which
effects they register and (until an in-flight uncommitted fix today) in
whether they check `ofShader::load()`'s return value; RG doesn't use
`ShaderLibrary` at all — it owns a flat `std::array<ofShader, NUM_MODES>`
indexed by mode number.

Every GLSL file that shares a filename across 2+ sketches is **byte-for-byte
identical** — there is no shader-source drift, only C++-side drift (which
effects get registered, which get per-effect uniforms bound, error-handling
quality). This means the actual shader assets are already a de facto shared
library in everything but physical location; the fragmentation is entirely
in the five different C++ ownership/dispatch layers built around them.

quadrant-crosshair's `DebugMode` (`sketches/quadrant-crosshair/src/DebugMode.{h,cpp}`)
is a full single-shader preview + live-tunable-parameter-panel + effect-cycling
tool already — it is the closest existing thing to the requested standalone
debugger, and per-file classification below shows most of it extracts cleanly.
temporal-fields has the richest "current state → target state, timed,
eased" machinery in the repo (`TFPresetTimeline`, found during this probe —
not previously catalogued), one layer above the smaller
`TFEffectPicker`/`TFFragmentTransition` machinery quadrant-crosshair's simpler
`ShaderSlot` state machine mirrors in miniature. Nothing resembling a
whitelist/blacklist knowledge base exists anywhere in the repo today — it is
new work, not extraction.

## 1. Sketch inventory

| Sketch | App class | Effect owner | Registry | Asset dir | Vertex convention | GLSL | Sampler | Primary tex uniform | Selection | Param ownership | Param UI | Randomization | Presets | Scene evolution | Drift | Temporal deps | Masks/aux |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| BRP | `ofApp` | `VideoRegionController`/`VideoRegionEffectRenderer` (`shared/src`) | shared `ShaderLibrary` | `bin/data/shaders/effects/*.glsl` | `vTexCoord` | 120 | sampler2D | `tex`/`tex0` | `ofParameter<int> pEffectIndex` (0-16) indexes `kEffectChoices` (`ofApp.cpp`) | `VideoRegionEffectRenderer::setEffectUniforms()` — full per-effect dispatch, fixed (non-randomized) values per effect | `ofxGui` (`renderingGroup`) | None | None | No | No | none (no history buffer used by this render path) | none |
| BE | `ofApp`→`BEComposition`→`BEFragment` | `BEFragment` | shared `ShaderLibrary` | same | `vTexCoord` | 120 | sampler2D | `tex`/`tex0` | random pick from `kEffectPool` (18 entries) in `pickAndStartEffect()` | `BEFragment::EffectSlot.params` (`glm::vec4`), randomized per-pick | none | Yes, `ofRandom` per-effect ranges | No | `EffectSlot` state machine (IDLE→FADE_IN→ACTIVE→FADE_OUT, `BEFragment.cpp:109-147`) | No | none | none |
| TF | `ofApp`→`TFComposition`→pattern classes | `TFEffectPicker` | shared `ShaderLibrary` | same | `vTexCoord` | 120 | sampler2D | `tex`/`tex0` | weighted-random via `tfWeightedPick` on `weights.cycleInterval` timer (`TFEffectPicker.cpp:24-32`) | `paramX..W`, randomized per-pick (`randomizeEffectParams`) | `ofxGui` weight sliders (`TFParameterPanel`) + `TFPresetTimeline` JSON | Yes, weighted + per-effect ranges | Yes — 11 JSON files in `bin/data/presets/`, tolerant loader | `TFPresetTimeline` (state+phase+easing, see §6) **and** `TFFragmentTransition`/`TFBackgroundLayer.pickNextMode()` | `TFPatternParticleField` bounded-velocity spawn drift + noise-driven playhead selection | `TimeOffsetVideoBuffer` playheads | none |
| FT | `ofApp`→`FTFragmentPool`→`FTFragment` | `FTFragment` (local) | **local fork** `ShaderLibrary` | `bin/data/shaders/*.glsl` (flat) | `vTexCoord` | 120 | sampler2D | `tex` | random pick from `kEffectPool` (9 entries, `FTFragmentPool.cpp`) | `FTFragmentSpawnParams` struct fields, partially randomized in `EFFECT_PARAM_VARIANT` mode only | none | Yes, limited (1 param per effect typically) | No | `FTFragment` age/decay two-phase (sustain→decay, `FTFragment.h:49-79`, simpler than BE/QC's 4-state machine) | No | `TimeOffsetVideoBuffer` playheads (Mode A only) | none |
| QC | `ofApp`→`Quadrant`/`QuadrantManager`/`DebugMode` | `Quadrant` (production) + `DebugMode` (dev) | **local fork** `ShaderLibrary` | `data/shaders/*.glsl` (symlinked to `bin/data/shaders`) | `vTexCoord` | 120 | sampler2D | `tex`/`tex0` | `QuadrantManager` trigger→`Quadrant::pushShader()` (production); manual L/R cycling of `DebugMode::SHADER_NAMES` (dev) | `Quadrant::ShaderSlot` (production, hardcoded literals per effect in `drawWithEffect()`) / `DebugMode`'s ~50 `pXxx` float fields (dev) | none (production) / custom on-screen panel, not ofxGui (`DebugMode::drawPanel`) | Yes, `QuadrantManager` — incl. reject/retry (`chooseDitherParams`, §9) | No | `Quadrant::ShaderSlot` 4-state machine (IDLE/FADE_IN/ACTIVE/FADE_OUT) — same shape as BE's `EffectSlot` | No | `MotionExtraction` (local fork), `gridTex` | `gridState`, `motionTex`, `motionDelayedTex` |
| RG | `ofApp`→`GalleryCompositor` | `GalleryCompositor` | flat `std::array<ofShader,11>` indexed by mode, **not** `ShaderLibrary` | `bin/data/shaders/gallery_modeN_*.frag` | `texCoordVarying` | 120 (+`PLATFORM_PI` precision guard) | sampler2D (`videoTex`/`maskTex`) | `videoTex` | keyboard `n`/`p` cycling (`GalleryCompositor::nextMode/prevMode`) | literal constants in `applyModeUniforms()` switch | none | Mode 7 only (ghost-text spawn, unrelated to mode/param selection) | No | No | No | `RPRevealMask`-produced `maskTex` (freshness) | `maskTex` |
| RP | `ofApp`→`RPCompositor`/`RPRevealMask` | `RPCompositor` + `RPRevealMask`, one shader each | none — fixed 2-stage pipeline | `bin/data/shaders/{color_reveal,reveal_mask}.{frag,vert}` | `texCoordVarying` | 120 | sampler2D | `videoTex` | none — always both shaders, always on | hardcoded (`RPCompositor.h:22` `colorThreshold=0.55f` set once) | none | None | No | No | No | `history`/`current` ping-pong FBO inside `RPRevealMask` (own local temporal pass) | mask is the product, not an input |
| CP | `ofApp`→`ContourDisplacementEffect` | `ContourDisplacementEffect` | none — one fixed shader | `bin/data/shaders/contour_preprocess.{frag,vert}` | `sampler2D` (own convention, not vTexCoord/texCoordVarying family — not diffed against those) | 120 | sampler2D | `tex0` | none — `ContourPresets` writes `ofParameter`s directly, no shader swap | `ofParameter`/`ofxGui`, extensive (blur/contrast/threshold/etc.) | `ofxGui` | None | XML via `ofxPanel::saveToFile` to (empty) `bin/data/presets/` | No | No | none | `maskTex` |
| hud_elements | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| hud_validation_harness | `ofApp` | none | none | none | — | — | — | — | — | — | — | — | — | — | — | — | — |

**Not shader-enabled, excluded from any migration:** hud_elements
(documentation stub only, no `src/`/`bin/`/`Makefile` — canonical code lives
in `shared/src/hud/`) and hud_validation_harness (real buildable project,
confirmed zero `ofShader`/`.glsl`/`.frag`/`.vert` references in `src/` —
pure HUD widget overlay rendering, nothing to recolor).

**Migration-candidate ranking** (subjective, based on how much of each
sketch's effect logic is generic vs. load-bearing for that sketch's unique
composition):
1. **BRP** — thinnest wrapper around `shared/src`; effect selection is a
   single `ofParameter<int>`. Best first-migration candidate.
2. **BE** / **TF** — both already on shared `ShaderLibrary`; their
   randomization/state-machine logic is sketch-owned but structurally
   near-identical to each other (`paramX..W` ≈ `EffectSlot.params`).
3. **FT** / **QC** — local forks; migrating means retiring the fork, which
   is exactly the duplication this project wants removed, but both have
   sketch-specific dispatch code (QC's texture-unit-heavy `drawWithEffect`,
   FT's scissor-crop draw path) that must stay put.
4. **RG** — structurally different enough (mode-array, not name-registry;
   `videoTex`/`maskTex` contract) that it's a second, explicit shader
   contract rather than a drop-in migration (see §4).
5. **RP**, **CP** — not migration candidates; no registry to migrate *to*.

## 2. quadrant-crosshair DebugMode extraction audit

Full detail from a dedicated extraction pass; summarized here.

**Ownership & wiring** (`sketches/quadrant-crosshair/src/ofApp.h:41`,
`ofApp.cpp:22,50,165,191-192`): `DebugMode debug;` is a plain member of
`ofApp`. `update()`/`draw()` both **short-circuit** the entire normal
pipeline when `debug.active` — quadrants, HUD, crosshair, `TriggerBus` are
not touched while debugging (`ofApp.cpp:50,165`). `'d'`/`'D'` toggles
`debug.active`; consumed key events forward via `debug.keyPressed(key)`
(`ofApp.cpp:191-192`).

**Contrast case — the non-debug production selection path**
(`QuadrantManager.cpp:360-364`): `TriggerBus`-event-driven, e.g.
`CORNER_NEAR` → `quads[qid].pushShader("recolor", 0.8f, 4.f)`. This is a
completely separate mechanism from `DebugMode`'s manual cycling and stays in
`QuadrantManager` regardless of what happens to `DebugMode`.

| Dependency | What DebugMode uses it for | Classification |
|---|---|---|
| `ShaderLibrary` (local fork) | the effect registry itself | directly reusable (already the artifact being consolidated — see §7) |
| `VideoSystem` | `update()`, `isFrameNew()`, `getTexture()`, `getPixels()` only — no per-quadrant assignment logic touched | reusable-after-decoupling — swap for a minimal single-clip source exposing the same 4 methods |
| `MotionExtraction` (local fork) | `extractNeutralGrey/Boost/Gamma`, `update()`, `setOutputMode()`, `getMotionTexture()`, `getDelayedMotionTexture()` — feeds `motion_effect`'s uniforms and other shaders' `motionTex`/`motionDelayedTex` blend inputs | reusable-after-decoupling — self-contained GPU module, no quadrant coupling, can link in as-is |
| `RidgelineRenderer` (`shared/src`) | CPU-side "ridgeline" effect, fed `video->getPixels()` | directly reusable — already promoted to `shared/src`, zero quadrant coupling |
| `TriggerBus.h` | **types only** (`TriggerID` enum, for DebugMode's own `TriggerSim` HUD row) — no `TriggerBus` instance ever held | obsolete-after-extraction (the class); types droppable if the HUD "simulate trigger" row is cut |
| `QuadrantManager` | **not referenced by DebugMode at all** | quadrant-specific-must-stay — sibling consumer, not on this dependency chain |

**Dependency chain:**
```
ofApp --(owns)--> DebugMode --> { ShaderLibrary, VideoSystem, MotionExtraction, RidgelineRenderer[shared], TriggerBus[types only] }
```
Hard requirements: `ShaderLibrary` + any single-texture provider matching
`VideoSystem`'s 4-method shape. Everything else is optional/stubbable.
`QuadrantManager` is never touched by `DebugMode` and requires zero changes
for the extraction to proceed.

**No hardcoded resolution/paths** — canvas size read generically via
`ofGetWidth()/ofGetHeight()` throughout (`DebugMode.cpp:202,396`).
`SHADER_NAMES` and the ~50 `pXxx` param fields are QC's specific shader
catalog (nature pack, `ascii_solarpunk`, now `heatmap_recolor`), tied to
`ShaderLibrary`'s registered set rather than to quadrant geometry — portable
as long as the debugger ships the same shader files.

## 3. Effect catalog

**A. `ShaderLibrary`-registered effects** (union across shared, FT-local, QC-local):

| Effect | Registered by | Frag path | Required uniforms | Optional/effect-specific uniforms |
|---|---|---|---|---|
| desaturate | shared, FT, QC | `.../desaturate.glsl` | `tex`, `alpha` | — |
| invert | shared, FT, QC | `.../invert.glsl` | `tex`, `alpha` | — |
| recolor | shared, FT, QC | `.../recolor.glsl` | `tex`, `alpha` | `tint` |
| threshold | shared, FT, QC | `.../threshold.glsl` | `tex`, `alpha` | `threshold` |
| dither | shared, FT, QC | `.../dither.glsl` | `tex`, `resolution`, `opacity` | `alpha` (arc position, not opacity — see below), `maxPixelation` |
| solarize | shared, FT, QC | `.../solarize.glsl` | `tex`, `alpha` | — |
| scanlines | shared, FT, QC | `.../scanlines.glsl` | `tex`, `resolution`, `alpha` | — |
| channelshift | shared, FT, QC | `.../channelshift.glsl` | `tex`, `alpha` | `shift` |
| heatmap_recolor | shared, FT, QC | `.../heatmap_recolor.glsl` | `tex`, `alpha` | `intensity`, `gamma`, `minLuminance`, `maxLuminance`, `palette`, `reverse` (all have shader-side safe fallbacks — see `docs`-adjacent implementation notes) |
| hue_rotate | shared, QC (not FT) | `.../hue_rotate.glsl` | `tex`, `alpha` | `hueOffset`, `hueSpeed`, `time`, `saturationMult`, `valueMult` |
| ascii_solarpunk | shared, QC (not FT) | filename **`ascii_threshold_solarpunk.glsl`** (registry-name/filename mismatch) | `tex`, `resolution`, `alpha` | `cellSize`, `thresholdMin/Max`, `thresholdMode`, `opacity`, `contrast`, `bias`, `softness`, `asciiColorMode`, `asciiInvertMono`, `asciiBackgroundMode` |
| bioluminescence, chromatic_aberration, edge_glow, ink_outlines, pixel_drift, pixel_sorting, water_refraction | shared, QC (not FT) | `of_nature_shader_pack_glsl/*.glsl` | `tex0`, `resolution` | per-effect (`time`, `threshold`, `intensity`, `glowColor`, `amount`, `radial`, `edgeStrength`, `glowStrength`, `inkStrength`, `posterizeLevels`, `scale`, `speed`, `rangePx`, `direction`, `amplitude`, `frequency`) — **none of these are bound by name-generic code; every consumer hand-writes the same literal-constant block** (BE's `setEffectUniforms`, TF's `applyEffectUniforms`, QC's `drawWithEffect`/`DebugMode::drawShaderFullScreen`, BRP's `VideoRegionEffectRenderer::setEffectUniforms` — four independent copies of near-identical literal values) |
| temporal_trails | QC only | `of_nature_shader_pack_glsl/temporal_trails.glsl` | `currentTex`, `previousTex` | `decay`, `currentWeight`, `brighten` — needs its own ping-pong FBO (`DebugMode.cpp:315-345`), the only registered effect requiring a second render target |
| rd_step | QC only | `shaders/rd_step.glsl` | `rdState`, `resolution` | `feedRate`, `killRate` — reaction-diffusion, driven by `ReactionDiffusion.cpp`, not a simple recolor |
| erosion (QC's registry entry) | QC only | `shaders/erosion.glsl` | `accumulated`, `videoFrame`, `decayRate` | `videoAlpha` |
| motion_effect | QC only | `shaders/motion_effect.glsl` | `tex`, `motionTex`, `motionDelayedTex`, `alpha` | `motionGamma`, `blendMode`, `motionSourceMode` |

**B. Data files driven directly by dedicated C++ classes, never through any `ShaderLibrary`:**

| File | Driven by | Sketches | Notes |
|---|---|---|---|
| `erosion.frag`/`.vert` | `shared/src/ErosionFBO.cpp` | BRP, BE | **Bug found during this probe**: `ErosionFBO.cpp` never calls `setUniform1f("currentAlpha", ...)` though `erosion.frag` declares and reads it — GLSL zero-inits it, so the current frame contributes nothing to the blend as currently coded. Independent of and unrelated to QC's same-named `erosion.glsl` (different uniform set — see §7). |
| `motion_accum.glsl`, `motion_extract.glsl` | `MotionExtraction.cpp` (shared and QC-local forks) | BE, TF, QC (BRP carries the data files but never instantiates `MotionExtraction`) | |
| `video_adjust.glsl` | `VideoSystem.cpp` (`adjustShader`) | QC only | |
| `bg_dim.glsl` | `FTBackgroundLayer.cpp` | FT only | |
| `caustics.glsl`, `passthrough.glsl` (nature-pack) | none found | QC only | dead/unused data files — `caustics.glsl` is never registered anywhere; `passthrough.glsl` is actually a **vertex** shader mislabeled among fragment files |

**C. RG's gallery_mode0–10** (single-purpose, not part of any name registry — listed separately as they use a different uniform contract, see §4):

11 modes, `videoTex`/`maskTex` uniforms throughout, `gallery_passthrough.vert`
(`texCoordVarying`). Mode-specific extras only for modes 5/6/8/9/10
(`resolution`, `splitAmount`, `distortAmount`, `arrivingCenters[]`/etc., or
the 6 heatmap uniforms) — set in `GalleryCompositor::applyModeUniforms()`'s
`switch (modeIndex)`.

**Aliases / name collisions found:**

| Names | Relationship |
|---|---|
| `ascii_solarpunk` ↔ `ascii_threshold_solarpunk.glsl` | registry name vs. filename mismatch (harmless but worth normalizing if a canonical metadata table is introduced) |
| `scanlines` (effects pool) ↔ `gallery_mode4_scanlines.frag` | name collision only — independently authored, different algorithm, different uniform set |
| `erosion` | two unrelated implementations sharing the name: QC's `ShaderLibrary`-registered `erosion.glsl` (`accumulated`/`videoFrame`/`decayRate`/`videoAlpha`) vs. BRP/BE's `ErosionFBO`-driven `erosion.frag`+`.vert` pair (`history`/`current`/`decayRate`/`currentAlpha`/`desatAmount`), bypassing `ShaderLibrary` entirely |
| `passthrough` | three unrelated things: unused nature-pack vertex shader; FT's shader-less "no effect" concept (no file at all); `gallery_mode0_passthrough.frag`, a real fragment shader |
| `motion_effect` vs. `motion_accum`/`motion_extract` | pipeline, not aliases — the latter two are `MotionExtraction`'s internal passes producing `motionTex`/`motionDelayedTex`; QC's `motion_effect.glsl` is a third, separately-registered composite shader consuming their output. Only QC exposes this composite as a selectable pool effect. |
| `DITHER_ERROR` | referenced only in a comment in `FT/src/ShaderLibrary.h` as an excluded name — no corresponding file exists; a planned-but-never-built variant, not a real duplicate |

**Recommended canonical names:** keep all current registry names as-is
(`ascii_solarpunk`, not the filename) — renaming would touch every C++
dispatch site listed above for zero behavioral gain. The one thing worth
fixing going forward: any *new* shared-metadata table should key off the
registry name, not the filename, to avoid perpetuating the mismatch.

## 4. Shader compatibility matrix

| Property | Effects-pool family (BRP/BE/TF/FT/QC) | RG gallery modes | RP (color_reveal/reveal_mask) | CP (contour_preprocess) |
|---|---|---|---|---|
| GLSL version | 120, unconditional | 120, unconditional | 120 | 120 |
| Sampler | `sampler2D` | `sampler2D` | `sampler2D` | `sampler2D` |
| Texture coords | normalized [0,1] | normalized [0,1] | normalized [0,1] | normalized [0,1] |
| `texture2D` used | yes | yes | yes | yes |
| Vertex varying name | `vTexCoord` | `texCoordVarying` | `texCoordVarying` | own convention (not diffed) |
| Primary sampler uniform | `tex` (+ `tex0` alias for nature-pack shaders) | `videoTex` + `maskTex` | `videoTex`/`history`+`current` | `tex0` + `maskTex` |
| Alpha behavior | source alpha usually re-derived from an `alpha`/`opacity` uniform, not preserved from input | `video.a` sometimes hardcoded to `1.0` (duotone), sometimes preserved (heatmap_recolor mode) — **inconsistent within RG itself** | not applicable (opaque compositing) | not applicable |
| FBO format | `GL_RGBA`, no depth | not FBO-based (direct draw) | `GL_RGB` (`RPRevealMask`'s ping-pong) / not FBO for `RPCompositor` | `GL_RGBA` |
| Pi conditional compilation | none in the `vTexCoord` family's `vert.glsl` | `#ifdef PLATFORM_PI` → `precision mediump float` in `gallery_passthrough.vert` | same `#ifdef PLATFORM_PI` precision guard | not checked (own convention) |
| Desktop-only behavior | none found | none found | none found | none found |

**Two unrelated vertex-shader lineages exist**, found during the duplication
audit: the `vTexCoord` family (BRP/BE/TF/FT/QC's `effects/vert.glsl` /
`vert.glsl`) has no Pi conditional at all; the `texCoordVarying` family
(RG/RP's `gallery_passthrough.vert`/`reveal_mask.vert`, and BRP/BE's
`erosion.vert`/`fragmentEffects.vert`) has the `PLATFORM_PI` precision
guard. Within the `texCoordVarying` family, `desaturate.vert` (BRP/BE)
diverges further still — it switches to `#version 100` on Pi instead of
just adding a precision qualifier, a *third*, inconsistent Pi-compat
strategy for the same varying-name convention.

**Recommendation: support two explicit contracts, do not force one.**
Contract A ("effects pool": `tex`/`vTexCoord`, no PLATFORM_PI conditional,
single-texture) already covers 5 of 6 multi-effect sketches and all newly
authored effects (including `heatmap_recolor`, `heatmap_recolor.glsl`
Variant A) should target it. Contract B (`videoTex`+`maskTex`/
`texCoordVarying`, `PLATFORM_PI`-aware) is RG's two-texture
video+freshness-mask convention and is not a drop-in superset or subset of
Contract A — a shared metadata layer must record which contract each effect
definition targets rather than assuming interchangeability. RP and CP are
one-off pipelines and don't need to join either contract to benefit from a
shared debugger (see §11 open questions).

## 5. Parameter catalog

Parameter *names* are consistent per-effect across all consumers (every
copy of `recolor.glsl` wants `tint`+`alpha`, everywhere) — the divergence is
entirely in **who supplies values and how**:

| Pattern | Where | Shape |
|---|---|---|
| Fixed literal constants, no randomization, no UI | BRP (`VideoRegionEffectRenderer::setEffectUniforms`), QC production (`Quadrant::drawWithEffect`), RG (`applyModeUniforms`) | `sh.setUniform1f("intensity", 1.2f)` style — hand-copied across 3+ files with the same literal values (bioluminescence's `threshold=0.3f, intensity=1.2f, glowColor=(0.1,1.0,0.75)` appears verbatim in BRP, BE, TF, QC production, and QC DebugMode — 5 independent copies of the same 3 numbers) |
| Randomized per-pick, stored on a struct | BE (`EffectSlot.params` as `glm::vec4`), TF (`paramX..W` as 4 loose floats) | Structurally identical 4-float-slot pattern; BE stores it in a struct, TF as bare member floats — otherwise the same shape, same `ofRandom` range values copy-pasted between the two files |
| Randomized per-pick, struct fields | FT (`FTFragmentSpawnParams`: `tint`, `thresholdVal`, `shiftVal`, `ditherArc`, `ditherPx`) | Named fields instead of `paramX..W`, functionally the same idea, only 1 field randomized per effect typically |
| Live-tunable via custom on-screen panel (dev-only) | QC `DebugMode`'s ~50 `pXxx` floats + `DebugParam{label,value*,step,min,max}` rows | The only place in the repo where min/max/step per parameter are declared explicitly and reusably (`DebugMode.cpp::buildParams()`) — closest existing thing to real parameter metadata |
| `ofParameter`/`ofxGui`-owned, persisted | CP (`ContourDisplacementEffect`'s params), TF (`TFParameterPanel`'s effect *weights*, not per-effect internal params) | CP's panel governs its one fixed shader's params directly; TF's panel governs which effects get picked, not their internal randomized values |

**Naming collisions:** `alpha` means at least three different things
depending on shader: overall opacity (`desaturate`, `invert`, `solarize`,
`scanlines`), effect-mix-not-opacity (`recolor`, `threshold`, `channelshift`,
`heatmap_recolor` — opacity is separately carried by the outer FBO composite
alpha in most consumers), and "arc position along a clean→dither→pixelate
sweep, not opacity at all" for `dither` specifically (`dither.glsl`'s own
in-file comment, referenced consistently in every consumer's code comments).
Any parameter-metadata layer must record this per-effect, not assume `alpha`
is safe to treat uniformly.

**Equivalent parameters, different names:** `intensity` (nature-pack
effects) and `alpha`/`effectAmount` (effects-pool family) both mean
"how strong is this effect," but nothing unifies them today — a debugger
showing "only the active effect's parameters" must already special-case
this per §Standalone Debugger Requirements below; it isn't solvable by a
single generic slider.

**Safe-to-animate / hard-limits data does not exist anywhere as data** — every
range currently seen (`ofRandom(0.15f, 0.85f)` for dither's alpha-arc, etc.)
is hardcoded inline at each of BE/TF/FT's three independent call sites, with
no shared source of truth. This is precisely the gap `EffectParameterMetadata`
(see implementation prompt) needs to fill.

## 6. Scene evolution and pattern drift inventory

| System | Lives in | Owns time | Current/target ownership | Interpolation | Transition rules | Randomization | Serialization | Reusable independently? |
|---|---|---|---|---|---|---|---|---|
| `TFPresetTimeline` | `sketches/temporal-fields/src/TFPresetTimeline.{h,cpp}` | receives `dt`, propagated from `ofApp::update()` | explicit `Transitioning`→`Holding` phase states (`TFPresetTimeline.h:37`) | per-state easing: linear / smoothstep / smootherstep / easeInOutSine (`TFTimelineEasing.h`) | duration + hold, sparse-override resolution against a base preset, loop/park semantics | can auto-advance to next timeline state | full JSON (`bin/data/presets/*.json` + timeline format doc) | **Best candidate for the shared `EffectEvolutionController` basis** — most complete state+phase+easing model in the repo, already documented (`docs/temporal-fields-preset-timeline-format.md`) |
| `TFEffectPicker` | `sketches/temporal-fields/src/TFEffectPicker.{h,cpp}` | receives `dt` | weighted pick on a cycle timer, no "current→target" blend, hard-switches | none (instant switch) | `weights.cycleInterval`, `rawWeight` | weighted-random via `tfWeightedPick` | weights persisted via `TFParameterPanel`/JSON | reusable — this is effectively the shared "pick next effect by weight" primitive already |
| `TFFragmentTransition` | `sketches/temporal-fields/src/TFFragmentTransition.{h,cpp}` | receives `dt`; `elapsed` accumulates internally | "current" = one-time FBO snapshot at `begin()`; "target" = whatever texture is passed live to `draw()` every frame (not owned) | linear progress (`elapsed/duration`); style-specific: `HARD_CUT`/`CROSSFADE` (linear alpha) / `EROSION` (GLSL dissolve) | `duration`, externally triggered `begin()` | style picked via weighted `tfPickTransitionStyle()`, called by callers not itself | none | reusable-after-decoupling — content-transition primitive, not scene-level |
| `TFPatternParticleField` | `sketches/temporal-fields/src/TFPatternParticleField.cpp` | receives `dt` | per-particle spawn/age/die; velocity chosen once at spawn (ballistic, not continuously noise-steered) | `existenceAlpha()` reuses `TFFragmentTransition::Style` + weighted picker for age-based fade only | lifespan-driven | velocity angle/speed at spawn; Perlin noise only selects which time-offset texture a particle samples | none | reusable-after-decoupling — independent of `TFFragmentTransition` instance, reuses its enum/picker only |
| `TFBackgroundLayer.pickNextMode()` | `sketches/temporal-fields/src/TFBackgroundLayer.cpp:37-55` | owns its own `modeTimer` | weighted pick (FULL_VIDEO/FULL_IMAGE/SPLIT) on `modeChangeInterval` (~20s) | none (instant) | timer-driven | weighted via `tfWeightedPick` | via `TFParameterPanel` weights | reusable — same primitive as `TFEffectPicker`, applied to a different enum |
| `TFParameterPanel`'s "Phase 7" wobble system | `sketches/temporal-fields/src/TFParameterPanel.h:27-29` (`LFOBank wobbleLfo`, `waypointTimer`, `pickNewWaypoint()`) | own timer | per-dial waypoint targets | LFO-driven | `waypointTimer` | random waypoint selection | via `registerEvolving()` | a **third**, independent per-dial drift mechanism layered under the timeline — not audited in depth here; flagged as needing its own look before consolidation |
| `BEFragment::EffectSlot` state machine | `sketches/blueprint_emergence/src/BEFragment.h:81` / `.cpp:109-147` | receives `dt` | `IDLE/FADE_IN/ACTIVE/FADE_OUT` enum, `dwellDur`/`dwellAcc`, `fadeDur` | linear alpha ramp | fixed fade/dwell durations (randomized ranges) | which effect (`pickAndStartEffect`), not the transition itself | none | a **smaller version** of the same lifecycle idea — good evidence this pattern is repo-wide-desired, not TF-specific |
| `Quadrant::ShaderSlot` state machine | `sketches/quadrant-crosshair/src/Quadrant.h:9`, `.cpp:254-262` | receives `dt` | identical `IDLE/FADE_IN/ACTIVE/FADE_OUT` enum, `dwellDur`/`dwellAcc`, `decayRate` | linear alpha ramp | fixed durations | which effect (`QuadrantManager` triggers), not the transition | none | same shape as BE's, independently reimplemented, not shared |
| `FTFragment` age/decay | `sketches/fragment-trail/src/FTFragment.h:49-79` | receives `dt` | two-phase `sustain`→`decay` only, `isExpired()` | linear alpha ramp | fixed durations | which effect (`FTFragmentPool::spawnOne`) | none | simplest version of the same idea, two states instead of four |

**Recommendation:** base the shared `EffectEvolutionController` on
`TFPresetTimeline`'s state+phase+easing model (richest, already documented,
already handles sparse-override resolution against a base state — directly
analogous to "current effect state vs. target effect state" the task
describes). Base the shared `PatternDriftController` on
`TFPatternParticleField`'s bounded-spawn-velocity + noise-driven-selection
approach, generalized away from "which time-offset texture" to "which
parameter value," since it's the only implementation with genuine
continuous drift math (the four `IDLE/FADE_IN/ACTIVE/FADE_OUT` state
machines are *transitions*, not *drift*, per the task's own distinction).
The `TFParameterPanel` wobble/LFO system needs a follow-up read before
committing — it may turn out to be an even better drift-primitive candidate
or a redundant third implementation; flagged as an open question (§13).

## 7. Duplicate asset and code inventory

**Shader files: zero content drift.** Every `.glsl`/`.frag`/`.vert` file
that shares a filename across 2+ sketches (the effects pool, the nature
pack, the `vTexCoord`-family vertex shaders, `motion_accum`/`motion_extract`)
is byte-for-byte identical everywhere it appears — confirmed via pairwise
`diff` across BRP/BE/TF/FT/QC's data directories. The only shader-level
divergence found:
- `erosion.frag`/`.vert` (BRP/BE, via `ErosionFBO`) vs. QC's same-named but
  structurally different `erosion.glsl` (via `ShaderLibrary`) — two
  unrelated implementations sharing a name, not a fork of one file (§3B).
- `desaturate.vert` (BRP/BE) uses a different Pi-compat strategy
  (`#version 100` swap) than sibling `texCoordVarying`-family vertex shaders
  (`erosion.vert`, `fragmentEffects.vert`, RG's `gallery_passthrough.vert`)
  which only add a precision qualifier (§4).

**C++ class duplication:**

| Class | Locations | Verdict |
|---|---|---|
| `ShaderLibrary` | `shared/src/ShaderLibrary.{h,cpp}`, `sketches/fragment-trail/src/ShaderLibrary.{h,cpp}`, `sketches/quadrant-crosshair/src/ShaderLibrary.{h,cpp}` | 3-way fork of a genuinely identical class shape (`std::map<string,ofShader>`, `setup/load/get/has`); `shared/src/ShaderLibrary.h`'s own header comment states it was "promoted from quadrant-crosshair/src/ShaderLibrary.h unchanged." FT/QC's forks exist because both sketches predate or bypass `shared/src` integration (QC's `config.make` doesn't pull `shared/src` in for this class the way BRP/BE/TF's does; FT explicitly excludes all of `shared/src` via `PROJECT_EXCLUSIONS` due to an unrelated symbol collision). **Registered-effect-name lists have diverged** across all three (§3A table) and, as of the in-flight uncommitted edits made earlier in this session, `shared/src/ShaderLibrary.cpp` is now the *only* one of the three that still leaves a broken map entry on load failure (`has()` false-positives) — FT and QC both now erase on failure. |
| `MotionExtraction` | `shared/src/MotionExtraction.{h,cpp}`, `sketches/quadrant-crosshair/src/MotionExtraction.{h,cpp}` | Genuine fork of the same implementation (`shared/src/MotionExtraction.h`: "Ported from quadrant-crosshair/src/MotionExtraction.h/.cpp — self-contained, no sketch-specific dependencies"). Line-for-line identical algorithm; real divergence found: `shared`'s `setup()` checks `accumShader.load()`/`extractShader.load()` return values and logs on failure, QC's original does not (same class of bug as the `ShaderLibrary` load-checking drift). |
| `ErosionFBO` | `shared/src/ErosionFBO.{h,cpp}` only | **Not duplicated.** Single canonical copy, used by BE and (indirectly, via `RPRevealMask`) by RP/RG. Carries the `currentAlpha`-never-set bug noted in §3B. |

**Runtime asset sharing across independently-deployed sketches is not
practical today and was never attempted** — no `Makefile`/`config.make` copy
step exists anywhere; `PROJECT_EXTERNAL_SOURCE_PATHS` only feeds the C++
compiler, never `bin/data`. `shared/src/shaders/effects/` today holds only
`hue_rotate.glsl` (plus, as of this session's uncommitted work,
`heatmap_recolor.glsl`) as a documentation/reference copy — every sketch's
actual runtime asset is a separately-maintained physical file in its own
`bin/data`. **Recommendation: keep this pattern** (source-of-truth copy in
`shared/src/shaders/` for reference + a physical copy in each sketch's data
directory, kept in sync by whatever tooling introduces the shared service —
see implementation prompt) rather than attempting true runtime sharing,
which openFrameworks' per-app `bin/data` packaging model doesn't support
without introducing a build-time copy step this project has so far avoided
by policy (each sketch must build/deploy independently, including to Pi).

## 8. Whitelist/blacklist model recommendation

**Nothing resembling this exists in the repo today** — confirmed via
exhaustive term search ("whitelist," "blacklist," "favorite," "rating,"
"score," "known good/bad," "reject," "avoid," "exclusion," "curated,"
"approved," "banned") across all of `sketches/`, `shared/`, `docs/`. The
closest analogues are unrelated: CP's empty, never-populated
`bin/data/presets/` (manual XML panel-state saves, not a
randomization-feedback loop) and TF's 11 hand-authored JSON presets
(artistic starting points, not outputs of an approve/reject workflow).

**Recommended MVP schema** (per-effect, versioned, JSON):

```json
{
  "schemaVersion": 1,
  "effect": "heatmap_recolor",
  "list": "whitelist",
  "snapshot": { "mix": 0.85, "intensity": 1.0, "gamma": 0.9, "minLuminance": 0.05, "maxLuminance": 0.95, "palette": 2, "reverse": false },
  "tolerance": { "mix": 0.1, "gamma": 0.15 },
  "label": "warm solarpunk foliage",
  "notes": "",
  "sourceSketch": "shader-effect-debugger",
  "sourceVideo": "moss_growth.mp4",
  "timestampUtc": "2026-08-01T00:00:00Z",
  "perfObservedFps": null,
  "qualityScore": null,
  "sceneContext": null
}
```

`tolerance` is optional per-parameter and only meaningful for numeric
fields; when absent, the entry is an exact-match reference point only used
for weighting (see below), not a region.

**Blacklist rejection strategy — recommend starting with per-parameter
forbidden ranges (not full multidimensional exclusion zones), with an
explicit path to weighted-penalty later:** exact-match rejection is nearly
useless against continuous float params (near-zero probability of ever
re-rolling the exact same floats); multidimensional exclusion zones are
correct in principle but require a distance metric across
mixed-type/mixed-scale parameters (float ranges vs. an integer `palette`
enum) that nothing in this codebase currently models — building that well
is a real design task, not an MVP. Per-parameter forbidden ranges
(`"gamma": {"min": 0.0, "max": 0.15}` meaning "never generate gamma in this
band regardless of other params") are cheap, explainable to a human tuning
a blacklist by hand, and directly reusable as simple `min`/`max` clamps in
the same `EffectRandomizer` pipeline that already needs hard-safety-limit
clamping. Recommend deferring true multidimensional/weighted-penalty
rejection until the per-parameter-range MVP proves insufficient in
practice.

**Whitelist favoring strategy:** weight candidate generation toward
whitelist snapshots by sampling *near* a randomly-chosen whitelist entry
(within its `tolerance`, or a small default jitter if `tolerance` is absent)
some configurable fraction of the time, falling back to full artistic-range
random sampling otherwise — mirrors `tfWeightedPick`'s existing
weighted-choice shape (already proven, already in the codebase) rather than
inventing a new weighting mechanism.

**Consumption without tight coupling:** other sketches should read
whitelist/blacklist JSON through the shared `EffectKnowledgeBase` API only
(load-once, query-by-effect-name), never touch the debugger's file-write
path or UI code — the debugger is a *producer* of this data, sketches (and
the debugger itself, for self-consistency) are *consumers* via the same
shared-service surface described in §9/implementation prompt.

## 9. Randomization audit

| System | Uniform/weighted | Range source | Dependencies | Constraints | Retries | Seeding |
|---|---|---|---|---|---|---|
| BE `pickAndStartEffect` | uniform (`ofRandom` index into `kEffectPool`) | inline `ofRandom(lo,hi)` per effect, hardcoded at call site | none checked | none | none (`shaderLib->has()` check just skips this turn on load failure) | `ofRandom()` global, unseeded |
| TF `TFEffectPicker::pickNext/randomizeEffectParams` | weighted (`tfWeightedPick` on `weights.effectWeights`) | inline `ofRandom(lo,hi)` per effect | none checked | none | none | mixed — `TFRandom.h`'s `tfRandRangeF/I` are deliberately `std::rand()`-based for reproducibility within a seeded cycle (`TFComposition::srand(cycleSeed)`), but `cycleSeed` itself is chosen via non-deterministic `ofRandom(1,1000000)`, and `TFEffectPicker` itself still calls plain `ofRandom` — determinism is partial, not end-to-end |
| FT `FTFragmentPool::spawnOne` | uniform (`ofRandom` index into `kEffectPool`) | inline `ofRandom(lo,hi)`, 1 param typically | none checked | none | none | `ofRandom()` global, unseeded |
| QC `QuadrantManager` (18 `ofRandom` call sites) | mix of uniform and weighted-by-probability-branch | inline, spread across many trigger handlers | some (e.g. `chooseDitherParams` scores risk from arc+brightness together) | yes — the only system with real validity constraints | **yes — the only system with genuine reject/retry**: `chooseDitherParams()` loops up to 5 attempts, computes a `risk` score, breaks early if `risk < 0.45` else falls through with the last attempt (`QuadrantManager.cpp:102-113`) | `ofRandom()` global, unseeded |
| RG `GalleryCompositor` | uniform, but only for Mode 7 ghost-text (spawn position/text/timer) — **not** mode/param selection (that's pure keyboard cycling) | inline | none | none | none | `ofRandom()` global, unseeded |
| BRP | **none** — `pEffectIndex` is GUI-selected only | n/a | n/a | n/a | n/a | n/a |
| CP, RP | **none confirmed** (zero `ofRandom` call sites in `ContourDisplacementEffect.cpp`, `RPRevealMask.cpp`, `RPCompositor.cpp`) | n/a | n/a | n/a | n/a | n/a |

No system anywhere in the repo uses a seedable PRNG for effect/parameter
randomization specifically (`ofSeedRandom`/`mt19937` never appear repo-wide;
the 3 `std::default_random_engine` uses found are all video-playlist
shuffling, seeded from wall-clock time, unrelated to effect params).
**Recommendation:** the shared `EffectRandomizer` should support an
optional deterministic seed from day one (the task explicitly asks for
"deterministic seeding when seeded" in evolution validation) — this is new
capability, not extraction, since nothing today provides it end-to-end for
effect parameters. QC's `chooseDitherParams` reject/retry loop is the one
piece of *real* prior art for "bounded retry, validity-scored" generation
and should inform the shared pipeline's retry step.

**Recommended pipeline** (validated against the above — the task's suggested
shape holds up well against actual code, with QC's reject/retry as the one
concrete precedent for the "dependency validation"/retry stage):
```
effect definition (hard limits, defaults)
  -> sketch overrides (safe ranges narrowed per-sketch)
  -> artistic range (the ofRandom(lo,hi) values currently hardcoded per-call-site)
  -> whitelist weighting (bias toward known-good regions)
  -> blacklist rejection (per-parameter forbidden ranges, §8)
  -> bounded retry (QC's chooseDitherParams pattern, capped attempts)
  -> generated effect state
```

## 10. Rendering and lifecycle audit

Every effects-pool consumer follows the same three-stage shape (source
crop → shader pass into a scratch FBO sized to the *destination*, not the
window → plain-textured composite draw so outer alpha blends correctly even
for effects whose own `alpha` uniform means something else) —
independently reimplemented four times: `VideoRegionEffectRenderer::render()`
(BRP, `shared/src`), `BEFragment::drawOverlay()` (BE), `TFEffectPicker::drawCurrent()`
(TF), and (without the scratch-FBO-sizing step, since it draws at window
resolution directly) `Quadrant::drawWithEffect()`/`DebugMode::drawShaderFullScreen()`
(QC) and `FTFragment::drawContent()` (FT, via `glScissor` crop instead of an
FBO). BRP's own code comments explicitly credit BE's FBO-sizing fix as the
origin of the pattern (`VideoRegionEffectRenderer.cpp:129-134`), and TF's
comments credit BE likewise (`TFEffectPicker.cpp:84-88`) — **this exact
three-stage render path has already been manually propagated by copy-paste
three times and is a prime extraction target** for a shared
`EffectRenderer`.

| Stage | BRP | BE | TF | QC (production) | FT |
|---|---|---|---|---|---|
| shader begin/end | `VideoRegionEffectRenderer.cpp:170,177` | `BEFragment.cpp:363,370` | `TFEffectPicker.cpp:101,108` | `Quadrant.cpp:273,364` | `FTFragment.cpp:76,93` |
| uniform binding | same file, `setEffectUniforms()` | same file, `setEffectUniforms()` | same file, `applyEffectUniforms()` | same file, inline `if` cascade | same file, inline `if` cascade |
| FBO allocation | `ensureScratchFbos()`, grow-only high-water-mark | `effectSourceFbo`/`effectResultFbo`, per-fragment | `sourceFbo`/`resultFbo`, per-instance | none (direct window-resolution draw) | none (scissor crop, no FBO) |
| history/temporal buffer update | n/a | n/a | `TimeOffsetVideoBuffer` (separate class) | `MotionExtraction`, separate class | `TimeOffsetVideoBuffer`, Mode A only |
| effect state update | `syncParamsToSystems()` each frame from GUI | `updateEffectCycle()` state machine | `TFEffectPicker::update()` timer | `Quadrant::update()` ShaderSlot state machine | `FTFragment::update()` age accumulation |
| GUI edits applied | every frame, `ofParameter` read directly | n/a (no GUI) | every frame via `ofParameter` | n/a (production) / `DebugMode::adjustParam()` (dev) | n/a (no GUI) |
| randomization occurs | never | `pickAndStartEffect()`, on state-machine IDLE→FADE_IN edge | `pickNext()`, on cycle timer | `QuadrantManager` trigger handlers | `spawnOne()`, on fragment spawn |

**Ownership drift observed:** uniform-binding literal-constant blocks for
the "nature pack" effects are hand-duplicated in 5 places (BRP, BE, TF, QC
production, QC DebugMode) with identical values — a textbook case for the
shared `EffectParameterMetadata`/`EffectRenderer` split proposed in the
implementation prompt.

## 11. Migration risks

- **Two incompatible shader contracts (§4)** mean a naive "one
  `EffectRenderer` for everything" design will either silently mis-bind RG's
  `videoTex`/`maskTex` shaders or force an unwanted rewrite of RG's mode
  shaders. The service must model contract as first-class metadata, not
  assume uniformity.
- **QC's production (`Quadrant`) and debug (`DebugMode`) uniform-binding
  code are two independent copies today** (§10) — migrating only `DebugMode`
  to a shared service while leaving `Quadrant::drawWithEffect()` untouched
  keeps that duplication alive under a new name unless both are migrated
  together or the shared `EffectRenderer` is adopted by both.
- **`shared/src/ShaderLibrary.cpp`'s failed-load map entry bug (§7)** — if a
  new shared service builds on top of the *current* shared `ShaderLibrary`
  without also fixing this, `EffectRegistry::has()`-equivalent checks would
  false-positive on shaders that failed to compile, exactly the
  "misleading load" failure mode the task calls out.
- **FT's `PROJECT_EXCLUSIONS = ../../shared/src`** (a blanket exclusion, not
  a narrow one — a prior narrower attempt caused duplicate-symbol link
  errors per FT's own `config.make` history) means FT cannot simply start
  `#include`ing a new `shared/src` service without first resolving whatever
  symbol collision caused the blanket exclusion; this is a real,
  non-cosmetic blocker for FT specifically, not a formality.
- **RP and CP have no registry to migrate into** — forcing them into the
  shared service means designing a "single fixed effect, but expose it
  through the shared metadata anyway" affordance that doesn't fit either the
  service's likely primary use case (many effects, pick one) or these
  sketches' current simplicity. Recommend treating both as out-of-scope for
  now (see open questions).
- **TF's `TFPresetTimeline`/wobble-LFO systems are the most complex code
  being proposed as an extraction source**, and this probe did not
  fully audit `TFParameterPanel`'s wobble/waypoint system (flagged, not
  investigated in depth) — treat any TF-sourced evolution/drift code as
  needing a dedicated decoupling pass, not a copy-paste.
- **Non-deterministic seeding is currently load-bearing behavior** (every
  sketch's current visual character depends on `ofRandom()`'s global,
  unseeded stream) — introducing seedable randomization must be additive
  (opt-in seed) not a silent behavior change to existing sketches.

## 12. Recommended migration sequence

Sequencing rationale only — file-level detail lives in the companion
implementation prompt.

1. Fix `shared/src/ShaderLibrary.cpp`'s failed-load map-entry bug (small,
   isolated, removes a foot-gun before anything builds on top of it).
2. Establish shared effect metadata (names, uniform contracts A/B, defaults,
   hard limits, artistic ranges) as pure data, sourced by reading the
   literal constants already duplicated in BRP/BE/TF/QC/RG's dispatch code
   — no behavior change yet, just consolidating already-duplicated numbers
   into one table.
3. Build the standalone debugger sketch against that metadata + the
   existing `ShaderLibrary` (shared copy), using QC's `DebugMode` as the
   direct extraction source per §2's classification table.
4. Add whitelist/blacklist persistence (net-new) inside the debugger.
5. Add the shared `EffectRandomizer` pipeline (§9) — retrofit BE/TF/FT to
   call it instead of their inline `ofRandom` blocks, one sketch at a time,
   preserving each sketch's existing artistic ranges as its "sketch
   override" tier so visual behavior doesn't change on migration.
6. Consolidate scene evolution around `TFPresetTimeline`'s model; consolidate
   pattern drift around `TFPatternParticleField`'s model — both as reusable
   library code TF itself then migrates to consume, not just other sketches.
7. Migrate BRP fully (thinnest sketch, lowest risk) to the shared
   `EffectRenderer` from §10, retiring `VideoRegionEffectRenderer`'s
   hand-copied uniform block.
8. Migrate BE and TF (`shared/src`-based already, so no build-system change
   needed, only call-site changes).
9. Resolve FT's `PROJECT_EXCLUSIONS` blocker, then migrate FT.
10. Migrate QC last — both `Quadrant::drawWithEffect()` (production) and
    `DebugMode` need to land on the same shared `EffectRenderer` together to
    avoid re-forking; remove `DebugMode`'s now-redundant standalone
    functionality once the extracted debugger sketch covers it, per §2's
    obsolete-after-extraction column.
11. Leave RG on its own two-texture contract, RP and CP out of the registry
    entirely, per §11's migration-risk assessment, unless a future decision
    explicitly wants them folded in.

## 13. Open questions requiring a product/engineering decision

1. **Does RG (two-texture `videoTex`/`maskTex` contract) get folded into the
   shared service as a second explicit contract, or does it stay
   permanently separate?** Both are defensible; §4 recommends "support two
   explicit contracts" but whether that means RG's *shaders* also become
   `ShaderLibrary`-registered (a real rewrite of `GalleryCompositor`) or
   just "the metadata layer knows about contract B" (much smaller) needs a
   call.
2. **Should RP/CP be given a trivial one-entry "registry" purely so the
   debugger can preview them, without adopting the full effect-pool
   machinery?** Low cost, but expands the debugger's scope beyond "preview
   the effects pool."
3. **`TFParameterPanel`'s Phase-7 wobble/LFO system was not audited in
   depth** — is it a third pattern-drift candidate worth comparing against
   `TFPatternParticleField` before committing to a drift-controller design,
   or intentionally legacy/superseded already? Needs a dedicated read
   before §6's recommendation is locked in.
4. **How much of `TFPresetTimeline`'s scope belongs in the shared service
   vs. staying TF-specific?** It currently operates over TF's entire scene
   configuration (pattern choice, composition params, not just shader
   effect params) — the shared `EffectEvolutionController` needs a scoped-down
   version of just the effect-parameter-transition part, and drawing that
   line is a design decision, not something the probe can resolve
   unilaterally.
5. **Blacklist rejection sophistication** — §8 recommends starting with
   per-parameter forbidden ranges; is that acceptable as a permanent MVP, or
   is multidimensional/weighted-penalty rejection a near-term requirement
   that should be designed now even if not built first?
6. **Does FT's `PROJECT_EXCLUSIONS` blocker get resolved by narrowing the
   exclusion (previously attempted, caused duplicate-symbol errors — root
   cause not investigated in this probe) or by some other mechanism (e.g.
   namespacing)?** This needs its own investigation before FT migration
   (step 9 above) can be scoped accurately.
7. **Where should whitelist/blacklist JSON physically live** — one file per
   effect under a new `shared/src/effect-knowledge/` (mirroring the
   `shared/src` convention) with each sketch getting its own physical copy
   at build/deploy time (matching the asset-duplication precedent in §7), or
   a single sketch-agnostic file the debugger owns and other sketches read
   via a relative path that may not resolve once a sketch is deployed
   independently to the Pi? The latter risks the exact "fragile relative
   path escaping a sketch's bin/data" problem this project has avoided
   elsewhere.

## 14. Files likely to be created, moved, or modified

Indicative, not exhaustive — see the companion implementation prompt for the
authoritative, phase-by-phase file list.

**New:**
- `EcopunkVideoCollage/sketches/shader-effect-debugger/` (new sketch: `src/ofApp.{h,cpp}`, `src/main.cpp`, `bin/data/shaders/` copies, `Makefile`/`config.make`/`addons.make` generated via `projectGenerator`)
- `shared/src/effects/` (new, name TBD in implementation prompt): `EffectRegistry`, `EffectDefinition`, `EffectParameterMetadata`, `EffectRandomizer`, `EffectRenderer`, `EffectKnowledgeBase` (whitelist/blacklist), `EffectEvolutionController`, `PatternDriftController`
- Whitelist/blacklist JSON data files (location per open question 7)

**Modified:**
- `shared/src/ShaderLibrary.cpp` (load-failure map-entry fix, §7/§11)
- `sketches/quadrant-crosshair/src/DebugMode.{h,cpp}` (shrink to whatever remains quadrant-specific per §2, once extraction lands)
- `sketches/quadrant-crosshair/src/Quadrant.cpp` (`drawWithEffect()`, once migrated to shared `EffectRenderer`)
- `shared/src/VideoRegionEffectRenderer.cpp`, `sketches/blueprint_emergence/src/BEFragment.cpp`, `sketches/temporal-fields/src/TFEffectPicker.cpp`, `sketches/fragment-trail/src/FTFragment.cpp`, `sketches/fragment-trail/src/FTFragmentPool.cpp` (progressively, per migration sequence §12)
- `sketches/fragment-trail/config.make` (if the `PROJECT_EXCLUSIONS` blocker is resolved, open question 6)

**Possibly removed (post-migration, not before):**
- `sketches/fragment-trail/src/ShaderLibrary.{h,cpp}` and `sketches/quadrant-crosshair/src/ShaderLibrary.{h,cpp}` (retired in favor of the shared registry, once both sketches' blockers are resolved)
- Parts of `sketches/quadrant-crosshair/src/DebugMode.{h,cpp}` (whatever the extraction audit's "obsolete-after-extraction" column identifies)
