# Engineering Plan: Consolidated Video Effect Library and Single Source of Truth

## Purpose

Create one canonical video-effect system for every shader/effect-capable sketch under:

```
apps/myApps/EcopunkVideoCollage/sketches/
```

The system must include ordinary single-pass shaders and the existing non-trivial video effects currently implemented outside the main effect registry, including motion extraction, erosion, ridgeline rendering, temporal trails, reaction diffusion, and the internal reusable shaders currently housed in quadrant-crosshair/bin/data/shaders.

The end state must eliminate drift in shader source, registration, parameter definitions, defaults, safe ranges, randomization ranges, uniform binding, auxiliary texture requirements, multi-pass orchestration, lifecycle behavior, diagnostics, and debugger exposure.

All sketches should consume one canonical implementation while retaining ownership of composition, geometry, masks, source selection, scheduling, and sketch-specific overrides.

## 1. Architectural decision

### 1.1 Use "video effect" as the shared abstraction

Do not model the shared system as only a shader registry. A video effect may be:

- A single-pass shader
- A multi-pass shader pipeline
- A temporal effect with history buffers
- A CPU/GPU hybrid renderer
- A processor that produces auxiliary textures
- A compositor that consumes auxiliary textures
- A stateful simulation used as a video-processing effect

Replace the narrow public concept of ShaderLibrary with:

```
VideoEffectService
    ├── VideoEffectRegistry
    ├── VideoEffectDefinition
    ├── VideoEffectInstance
    ├── VideoEffectParameterSchema
    ├── VideoEffectAssetRegistry
    ├── EffectRandomizer
    ├── EffectKnowledgeBase
    ├── EffectEvolutionController
    └── PatternDriftController
```

ShaderLibrary may remain temporarily as a low-level loading utility, but new sketch code should not depend on it directly.

### 1.2 Canonical source location

Create the shared implementation under:

```
apps/myApps/EcopunkVideoCollage/shared/src/video-effects/
```

Recommended structure:

```
shared/src/video-effects/
├── core/
│   ├── VideoEffectService.h/.cpp
│   ├── VideoEffectRegistry.h/.cpp
│   ├── VideoEffectDefinition.h
│   ├── VideoEffectTypes.h
│   ├── VideoEffectContext.h
│   ├── VideoEffectInstance.h
│   ├── VideoEffectParameterSchema.h
│   ├── VideoEffectParameters.h
│   ├── VideoEffectAssetRegistry.h/.cpp
│   ├── VideoEffectLoadReport.h
│   └── VideoEffectCapabilities.h
├── effects/
│   ├── SinglePassShaderEffect.h/.cpp
│   ├── MotionExtractionEffect.h/.cpp
│   ├── MotionCompositeEffect.h/.cpp
│   ├── ErosionEffect.h/.cpp
│   ├── RidgelineEffect.h/.cpp
│   ├── TemporalTrailsEffect.h/.cpp
│   └── ReactionDiffusionEffect.h/.cpp
├── knowledge/
│   ├── EffectKnowledgeBase.h/.cpp
│   └── EffectRandomizer.h/.cpp
├── evolution/
│   ├── EffectEvolutionController.h/.cpp
│   └── PatternDriftController.h/.cpp
└── catalog/
    └── DefaultVideoEffectCatalog.h/.cpp
```

## 2. Canonical asset strategy

### 2.1 One source tree for shader assets

Create:

```
apps/myApps/EcopunkVideoCollage/shared/assets/video-effects/
```

Recommended layout:

```
shared/assets/video-effects/
├── common/vert.glsl
├── single-pass/
│   ├── desaturate.glsl
│   ├── invert.glsl
│   ├── recolor.glsl
│   ├── threshold.glsl
│   ├── dither.glsl
│   ├── solarize.glsl
│   ├── scanlines.glsl
│   ├── channelshift.glsl
│   ├── heatmap_recolor.glsl
│   ├── hue_rotate.glsl
│   ├── ascii_threshold_solarpunk.glsl
│   ├── bioluminescence.glsl
│   ├── caustics.glsl
│   ├── chromatic_aberration.glsl
│   ├── edge_glow.glsl
│   ├── ink_outlines.glsl
│   ├── pixel_drift.glsl
│   ├── pixel_sorting.glsl
│   └── water_refraction.glsl
├── motion-extraction/
│   ├── motion_accum.glsl
│   ├── motion_extract.glsl
│   └── motion_effect.glsl
├── erosion/
│   ├── erosion_update.glsl
│   └── erosion_composite.glsl
├── temporal-trails/temporal_trails.glsl
├── reaction-diffusion/rd_step.glsl
└── utility/
    ├── passthrough.glsl
    └── video_adjust.glsl
```

Before moving files, inventory every file currently in:

```
sketches/quadrant-crosshair/bin/data/shaders/
sketches/quadrant-crosshair/data/shaders/
```

Classify each as reusable effect, pipeline-internal shader, sketch-specific compositor, utility pass, unused asset, duplicate, or same-name/different-behavior collision.

Do not merge unrelated shaders merely because they share a filename. The earlier probe already found two different erosion implementations.

### 2.2 Build-time synchronization instead of manual copies

Add:

```
apps/myApps/EcopunkVideoCollage/scripts/sync-video-effect-assets.py
```

The script should:

- Read a per-sketch manifest
- Copy only required canonical assets
- Preserve canonical relative paths
- Remove stale managed files
- Never remove unrelated sketch assets
- Emit a synchronization report
- Support `--check`
- Fail when a generated sketch-local shader differs from the canonical source

Generated destination:

```
<sketch>/bin/data/shared-video-effects/
```

Add a marker file such as:

```
.generated-from-shared-video-effects
```

### 2.3 Per-sketch manifests

Each consumer declares enabled effects and overrides:

```json
{
  "schemaVersion": 1,
  "effects": {
    "heatmap_recolor": {
      "enabled": true,
      "weight": 1.0,
      "parameterOverrides": {
        "gamma": {
          "artisticMin": 0.6,
          "artisticMax": 1.8
        }
      }
    },
    "motion_extraction": {
      "enabled": true
    },
    "ridgeline": {
      "enabled": true
    }
  }
}
```

Recommended path:

```
<sketch>/effect-manifest.json
```

The manifest drives asset sync, runtime filtering, startup validation, and debugger availability.

## 3. Shared video-effect model

### 3.1 Effect kinds

```cpp
enum class VideoEffectKind {
    SinglePassShader,
    MultiPassShader,
    TemporalShader,
    Processor,
    Composite,
    CpuRenderer,
    Simulation
};
```

Examples:

| Effect | Kind |
|---|---|
| recolor | SinglePassShader |
| heatmap_recolor | SinglePassShader |
| motion_extraction | Processor |
| motion_composite | Composite |
| erosion | TemporalShader |
| temporal_trails | TemporalShader |
| ridgeline | CpuRenderer |
| reaction_diffusion | Simulation |

### 3.2 Capabilities and requirements

Each definition declares:

```cpp
struct VideoEffectCapabilities {
    bool requiresSourceTexture = true;
    bool requiresPreviousFrame = false;
    bool requiresHistoryBuffer = false;
    bool requiresMotionTexture = false;
    bool requiresDelayedMotionTexture = false;
    bool requiresMaskTexture = false;
    bool requiresCpuPixels = false;
    bool producesAuxiliaryTexture = false;
    bool requiresPersistentState = false;
    bool supportsAlphaMix = true;
    bool safeForAutomaticSelection = true;
    int passCount = 1;
};
```

This metadata drives debugger UI, runtime validation, FBO allocation, effect eligibility, automatic selection, and Raspberry Pi filtering.

### 3.3 Strongly typed parameters

Use:

```cpp
using VideoEffectParameterValue =
    std::variant<float, int, bool, glm::vec2, glm::vec3, glm::vec4>;
```

Each parameter definition should include ID, label, type, default, hard bounds, artistic bounds, step, visibility, animation safety, randomization eligibility, performance sensitivity, and dependencies.

Do not assign universal meaning to parameter names. The probe found that alpha means different things in different effects.

## 4. Promote quadrant-crosshair internal effects

### 4.1 Produce a promotion inventory

For every QC shader/effect, document:

- current path;
- current C++ owner;
- uniforms;
- required textures;
- state/FBO requirements;
- whether reusable;
- proposed canonical ID;
- shared asset path;
- shared implementation class;
- eligible sketches.

Deliverable:

```
docs/video-effect-promotion-inventory.md
```

### 4.2 Motion extraction

Promote:

```
motion_accum.glsl
motion_extract.glsl
MotionExtraction.{h,cpp}
```

Canonical ID:

```
motion_extraction
```

Treat this as a processor that produces accumulation-based and delayed-frame motion textures plus any existing diagnostics.

### 4.3 Motion composite

Promote:

```
motion_effect.glsl
```

Canonical ID:

```
motion_composite
```

Keep extraction and compositing separate:

```
motion_extraction
    -> motion textures
    -> motion_composite
```

### 4.4 Erosion

Compare both current erosion implementations, rename them to unambiguous IDs, and decide whether both remain supported.

Possible IDs:

```
erosion_history_blend
erosion_accumulation
```

Fix the identified missing currentAlpha binding in the ErosionFBO path as a separate correctness change with its own regression test.

### 4.5 Ridgeline

Promote shared/src/RidgelineRenderer.* into the shared effect system.

Canonical ID:

```
ridgeline
```

Declare:

```
kind = CpuRenderer
requiresCpuPixels = true
```

Expose it in the same registry and debugger as shader effects.

### 4.6 Temporal trails

Promote the shader and its ping-pong orchestration.

Canonical ID:

```
temporal_trails
```

The shared effect instance should own ping/pong FBOs, swap state, resize, clear/reset, and parameters such as decay, current weight, and brighten.

### 4.7 Reaction diffusion

Promote only if it remains part of the intended video-effect palette.

Canonical ID:

```
reaction_diffusion
```

Mark it performance-sensitive and exclude it from automatic Raspberry Pi selection until validated.

### 4.8 Other QC shaders

Promote shaders that transform video or produce reusable video-derived textures. Keep quadrant-layout, HUD, crosshair, and trigger-visualization shaders local. Utility passes should remain private implementation assets of their owning effect.

## 5. VideoEffectService API

Suggested sketch-facing API:

```cpp
class VideoEffectService {
public:
    bool setup(const VideoEffectServiceConfig& config);

    bool hasEffect(const std::string& effectId) const;
    const VideoEffectDefinition* getDefinition(
        const std::string& effectId) const;

    std::unique_ptr<VideoEffectInstance> createInstance(
        const std::string& effectId,
        const VideoEffectInstanceConfig& config);

    VideoEffectLoadReport getLoadReport() const;

    const VideoEffectRegistry& registry() const;
    EffectKnowledgeBase& knowledgeBase();
    EffectRandomizer& randomizer();
};
```

Effect instances should support setup, reset, resize, update, render, parameter access, definition access, and readiness checks.

The render context should carry source texture/pixels, optional mask, source and destination rectangles, time, delta time, alpha, auxiliary textures, and optional destination FBO.

Sketches keep ownership of video source, fragment layout, masks, final composition, scheduling, effect weights, and lifecycle decisions.

## 6. Canonical registration

Create one catalog:

```cpp
void registerDefaultVideoEffects(VideoEffectRegistry& registry) {
    registerSinglePassEffects(registry);
    registerMotionExtraction(registry);
    registerMotionComposite(registry);
    registerErosionEffects(registry);
    registerRidgeline(registry);
    registerTemporalTrails(registry);
    registerReactionDiffusion(registry);
}
```

This catalog is the single source of truth for IDs, display names, parameter schemas, defaults, ranges, kind, assets, auxiliary inputs, platform safety, factories, debugger availability, and automatic-selection eligibility.

No migrated sketch should maintain a separate hard-coded canonical effect registry.

## 7. Asset resolution and load reporting

Resolve logical asset paths into:

```
bin/data/shared-video-effects/
```

No shared effect definition should hardcode a sketch name or escape the current sketch's bin/data.

Startup load reporting must include requested, registered, skipped, missing, failed, unsupported, and fallback effects. Failed effects must never remain marked usable.

## 8. Standalone debugger updates

The debugger must create effects through `VideoEffectService::createInstance()` and adapt execution to each effect kind.

Examples:

- single-pass shader: source through shader;
- motion extraction: preview source, accumulation, delayed output, diagnostics;
- motion composite: auto-create or attach motion extraction;
- erosion: expose reset and persistent-history state;
- temporal trails: expose clear/reset;
- ridgeline: supply CPU pixels;
- reaction diffusion: expose simulation reset and performance warning.

The dynamic GUI should show only global controls, active effect parameters, effect-specific diagnostics, dependency outputs, reset controls, performance metrics, whitelist/blacklist controls, and compatible evolution/drift controls.

## 9. Migration phases

**Phase 0 — Confirm inventory**

Inventory QC internal shaders and classes, classify them, and produce the promotion table.

**Phase 1 — Canonical assets and sync tooling**

Create `shared/assets/video-effects/` and `scripts/sync-video-effect-assets.py`. Do not change runtime code yet.

**Phase 2 — Shared core types and registry**

Create the shared registry, definitions, parameter schemas, capabilities, asset registry, and load reporting. Register single-pass effects first.

**Phase 3 — Promote stateful and processor effects**

Promote in this order:

1. Ridgeline
2. Motion extraction
3. Motion composite
4. Temporal trails
5. Erosion variants
6. Reaction diffusion, if retained

Each must include definition, schema, capabilities, implementation, factory, assets, lifecycle, debugger support, and performance metadata.

**Phase 4 — Build debugger against the service**

Make the debugger the first complete consumer. Keep QC DebugMode until parity is proven.

**Phase 5 — Migrate BRP**

Replace local effect lists and uniform binding with canonical registry filtering and shared instances. Preserve BRP composition and FBO behavior.

**Phase 6 — Migrate BE and TF**

Replace local pools, duplicated randomization, duplicated uniform binding, and direct ShaderLibrary use. Preserve their timing and fragment lifecycle systems initially.

**Phase 7 — Migrate QC comprehensively**

Migrate both production and debug paths. Use shared implementations for motion extraction, motion composite, erosion, ridgeline, temporal trails, reaction diffusion, and all single-pass effects.

Keep QC ownership of quadrant geometry, triggers, grid state, crosshair, HUD, and scheduling.

After parity:

- remove generic shader preview logic from QC DebugMode;
- keep QC-specific diagnostics only;
- remove local ShaderLibrary and MotionExtraction forks;
- remove promoted local classes;
- remove manually managed canonical shader copies from old QC paths.

**Phase 8 — Resolve and migrate FT**

Investigate and fix the blanket `PROJECT_EXCLUSIONS = ../../shared/src`. Do not create a new fork as a workaround. Then migrate FT while preserving its scissor/crop and fragment lifecycle behavior.

**Phase 9 — Evaluate RG, RP, and CP**

Treat these as second-wave integrations. Do not force them into Contract A.

**Phase 10 — Add drift enforcement**

Add checks for asset synchronization, duplicate IDs, manifest resolution, local re-registration, local managed shader copies, and duplicated implementations.

## 10. Whitelist, blacklist, evolution, and drift

These systems should operate on canonical typed parameters regardless of effect kind.

Whitelist entries should use canonical IDs and store snapshots, tolerances, source media, platform, performance, notes, and labels.

Blacklist support should include per-parameter forbidden ranges, discrete-state rejection, effect/platform disable entries, and performance-based exclusion.

Only parameters marked `safeToAnimate` may evolve. Stateful effects must define whether parameter changes preserve history, reset history, crossfade instances, or apply immediately.

Drift must never alter texture units, unsupported enums, buffer dimensions, pass counts, or platform capability flags.

## 11. Testing and acceptance criteria

**Asset consistency**

- Every migrated sketch receives managed effect assets only through the sync script
- `--check` reports no drift
- Generated assets are never manually edited
- Canonical paths remain stable

**Registry consistency**

- One canonical ID and parameter schema per effect
- Duplicate registration fails loudly
- Sketch manifests narrow behavior but do not redefine core semantics

**Effect behavior**

- Single-pass effects match prior output
- Motion extraction and composite match QC baselines
- Erosion variants are clearly differentiated
- Missing currentAlpha is fixed
- Ridgeline matches current behavior
- Temporal trails preserve and reset correctly
- Reaction diffusion is validated or explicitly excluded

**Sketch behavior**

- BRP, BE, TF, QC, and eventually FT build independently
- Composition and scheduling remain stable
- Sketch-specific overrides still work

**Debugger behavior**

- Every shared effect is discoverable
- GUI shows only relevant parameters
- Stateful effects expose reset/clear
- Dependencies and failures are visible
- No unexplained all-black output
- Whitelist/blacklist uses canonical IDs

**Raspberry Pi**

For every effect, record support status, resolution, FPS, pass count, texture/FBO memory, and automatic-selection eligibility.

## 12. Deliverables

```
docs/video-effect-promotion-inventory.md
docs/shared-video-effect-architecture.md
shared/assets/video-effects/**
shared/src/video-effects/**
scripts/sync-video-effect-assets.py
sketches/shader-effect-debugger/**
<sketch>/effect-manifest.json
```

Progressively remove local forks only after migration and validation.

## 13. Final ownership boundary

**Shared video-effect system owns**

- canonical IDs;
- canonical shader assets;
- factories;
- parameter schemas;
- defaults and ranges;
- uniform binding;
- stateful effect internals;
- intrinsic history/FBO ownership;
- dependency graph;
- diagnostics;
- randomization;
- whitelist/blacklist consumption;
- parameter evolution;
- parameter drift;
- platform capability metadata.

**Individual sketches own**

- source video selection;
- geometry;
- fragment layout;
- sketch-created masks;
- final composition;
- scheduling;
- enabled effects;
- weights;
- parameter overrides;
- triggers;
- scene orchestration beyond an individual effect;
- when instances are created, reset, switched, or destroyed.

This boundary gives the project one source of truth without making the shared service responsible for every sketch's rendering architecture.
