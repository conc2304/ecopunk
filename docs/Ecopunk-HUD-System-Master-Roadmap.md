# Ecopunk HUD System — Master Roadmap

> **Authority notice:** This document defines planned sequence and intended milestones.
> It is not the authoritative source of current completion status.
> For current state, see the [Canonical Work Registry](shared-project-docs/03-work-registry.md).
> Checkboxes and the "Current State" section below are not updated after every engineering session and may be stale.

**Purpose:** This document is the high-level execution plan for delivering the complete Ecopunk HUD system, from shared runtime contracts through scene integration, Raspberry Pi validation, and a repeatable art-production pipeline for generating additional HUD skins.

It is intended to serve as:

- the planned milestone sequence (current status lives in the Work Registry, not here),
- the source document for generating phase-specific engineering prompts,
- the boundary between runtime engineering and art production,
- the reference for ownership, dependencies, and completion criteria,
- the roadmap for reaching a final system in which new HUD skins can be produced consistently without redesigning the runtime.

---

# 1. Final Product Definition

The final system is a cinematic, science-fiction HUD for six Ecopunk Video Collage scenes running in a fixed living-room art installation.

## Supported runtime scenes

1. `blob-region-prototype`
2. `contour-portrait`
3. `temporal-fields`
4. `fragment-trail`
5. `quadrant-crosshair`
6. `blueprint_emergence`

## Excluded from the runtime scene cycle

- `radar-effects-gallery`
- `radar-pulse`
- `shader-effect-debugger`
- `hud_validation_harness`
- `hud_elements`
- FireplaceWaterfall and its sketches

## Runtime characteristics

The completed system should provide:

- one canonical 1280×720 HUD layout,
- one upper-left media/scene viewport,
- rounded media corners,
- a 45-degree lower-right media bevel,
- an L-shaped HUD body below and to the right,
- a stable universal layout,
- fixed universal controls,
- one or two flexible scene-specific telemetry regions,
- interchangeable HUD skins,
- scene and video navigation,
- curated scene reseeding,
- standardized video playback,
- shared video-effect selection and preset knowledge,
- semantic and derived signals that drive data visualizations,
- Raspberry Pi 3B+ compatibility,
- a repeatable asset-generation pipeline for future skins.

---

# 2. Product Principles

## 2.1 The HUD is not a parameter panel

The runtime HUD must not mirror `ofxGui`.

`ofxGui` remains developer-only and is used to:

- tune scenes,
- test parameter ranges,
- author presets,
- validate effects,
- establish safe randomization ranges,
- inspect performance,
- debug scene behavior.

The runtime HUD shows curated controls and semantic telemetry.

## 2.2 The HUD is cinematic, not explanatory

The HUD should feel like a believable system interface from a science-fiction film.

It may use:

- real data,
- normalized data,
- derived data,
- ambient data driven by real semantic state.

It should not read like a museum label or debugging console.

## 2.3 Layout is stable; content is modular

The layout remains fixed across scenes and skins.

Only these change:

- skin assets,
- vocabulary,
- scene-specific widgets,
- scene-specific data bindings,
- active effects and semantic state.

## 2.4 Words may be compiled into the application

Hot-reloading or runtime editing of vocabulary is not required.

Vocabulary and semantic mappings should still be centralized and easy for a developer to update without searching through rendering code.

## 2.5 Real scene data should drive the system whenever practical

The HUD should prefer:

1. literal runtime data,
2. normalized scene data,
3. derived historical data,
4. ambient data influenced by real state.

Purely arbitrary looping decoration should be minimized.

---

# 3. Current State

> Historical snapshot from roadmap authoring — superseded. See the [Canonical Work Registry](shared-project-docs/03-work-registry.md) for current state.

## Completed

- HUD feasibility investigation
- Scene render-boundary investigation
- Scene/HUD alignment investigation
- Scene/HUD Contract v1 draft
- HUD architecture review
- Scene/HUD Contract v1 revision
- Scene observability profile
- Product-direction and scope addendum
- Initial visual concepts for five HUD skins
- Decision to use one canonical layout
- Decision to keep `shader-effect-debugger` as developer tooling
- Decision to exclude camera input
- Decision to standardize video playback
- Decision to keep runtime controls minimal
- Decision to make scene randomization curated rather than raw

## Not yet implemented

- ExperienceRuntime
- SceneManager
- HudCompositor
- canonical HUD layout
- scene semantic-data model
- vocabulary slots
- shared video-playback service
- shared effect-selection knowledge pipeline
- wireframe HUD harness
- real scene adapters
- Pi build/deploy path
- Dark Moss production assets
- skin loader
- art-production pipeline
- remaining four skins

---

# 4. Workstream Overview

The project should be executed through parallel but coordinated workstreams.

## Workstream A — Experience Runtime

Owns:

- main application lifecycle,
- final window ownership,
- scene output FBO,
- SceneManager integration,
- HudCompositor integration,
- InputRouter,
- RuntimeServices,
- RuntimeTelemetry,
- update and draw order.

## Workstream B — SceneManager and Scene Migration

Owns:

- `IEcopunkScene`,
- lifecycle,
- scene activation/deactivation,
- scene capabilities,
- scene commands,
- scene status,
- native-resolution rendering,
- scene switching,
- adapters for all six scenes.

## Workstream C — Shared Video Playback

Owns:

- canonical media directory,
- playlist creation,
- randomized selection,
- repeat and hold behavior,
- previous/next video,
- playback status,
- curated media names,
- error handling.

## Workstream D — Shared Effect Selection

Owns:

- canonical effect library use,
- favored presets,
- blocked presets,
- selection weights,
- scene compatibility,
- Pi-safe compatibility,
- recent-history avoidance,
- effect metadata exposed to scenes and HUD.

## Workstream E — HUD Semantic Model

Owns:

- semantic state IDs,
- normalized signals,
- scene timing,
- scene-specific telemetry,
- vocabulary slots,
- data classifications,
- widget data bindings,
- fallback rules.

## Workstream F — HUD Framework

Owns:

- canonical layout,
- media viewport,
- `MediaViewportMesh`,
- HUD slots,
- universal widgets,
- scene-specific widget region,
- skin loading,
- text rendering,
- command display,
- scene status display,
- performance-conscious rendering.

## Workstream G — HUD Validation Studio

Owns:

- fake semantic data,
- real-scene preview,
- skin preview,
- mask inspection,
- layer inspection,
- widget bounds,
- screenshot capture,
- state simulation,
- performance timing.

## Workstream H — Art Production Pipeline

Owns:

- layout wireframe export,
- visual reference generation,
- material-board generation,
- asset cleanup,
- shell-layer extraction,
- masks,
- atlases,
- emission layers,
- metadata manifests,
- visual QA,
- reproducibility across skins.

## Workstream I — Raspberry Pi Validation

Owns:

- Pi build configuration,
- `PLATFORM_PI`,
- native deployment,
- kiosk boot,
- logging,
- runtime telemetry,
- frame-time profiling,
- memory profiling,
- thermal/throttle monitoring,
- quality profiles.

---

# 5. Phase-by-Phase Plan

---

# Phase 0 — Freeze the Shared Foundations

## Goal

Turn the approved architectural decisions into stable source documents and initial shared headers.

## Tasks

- Freeze Scene/HUD Contract v1.
- Store the product-direction addendum in project documentation.
- Store the scene observability report in project documentation.
- Establish the six-scene runtime scope.
- Establish canonical naming for:
  - `ExperienceRuntime`
  - `SceneManager`
  - `HudCompositor`
  - `InputRouter`
  - `RuntimeServices`
  - `RuntimeTelemetry`
  - `SceneFrame`
- Create the initial `shared/src/scene/SceneContract.h`.
- Do not implement SceneManager behavior yet.
- Add a contract version constant.

## Deliverables

- `docs/scene-hud-contract-v1.md`
- `docs/hud-scope-addendum.md`
- `docs/scene-observability-profile.md`
- `shared/src/scene/SceneContract.h`

## Exit criteria

- Both HUD and SceneManager workstreams use the same header.
- No competing scene interface exists.
- Contract changes require deliberate review.

## Best owner

SceneManager/code agent, reviewed by HUD architecture owner.

---

# Phase 1 — Define the HUD Semantic Slot Model

## Goal

Define the stable data and vocabulary slots that let engineering proceed without finalizing every word or widget.

## Scope

This phase should not finalize artistic wording.

It should define stable IDs, types, and ownership.

## Tasks

### Define universal semantic data

Candidate types:

```cpp
struct SceneActivity {
    float overall;
    float motion;
    float density;
    float variation;
    float transition;
};

struct SceneSemanticState {
    std::string primaryStateId;
    std::optional<std::string> secondaryStateId;
    std::optional<float> progress;
};

struct SceneTimingStatus {
    float activeSeconds;
    std::optional<float> stateProgress;
    uint64_t generation;
};
```

### Define data classifications

```cpp
enum class HudDataClass {
    Literal,
    Normalized,
    Derived,
    Ambient
};
```

### Define universal slot IDs

Examples:

- `scene.title`
- `scene.primary_state`
- `scene.secondary_state`
- `scene.activity`
- `scene.motion`
- `scene.density`
- `scene.variation`
- `scene.transition`
- `scene.timer`
- `scene.effects`
- `media.title`
- `media.playback_progress`
- `media.hold_progress`
- `control.previous_scene`
- `control.next_scene`
- `control.previous_video`
- `control.next_video`
- `control.reseed`

### Define scene-specific metric records

```cpp
struct SceneMetric {
    std::string metricId;
    float value;
    std::optional<float> normalizedValue;
    HudDataClass dataClass;
};
```

### Define vocabulary storage

Vocabulary may be compiled from:

- C++ maps,
- constexpr tables,
- JSON loaded at setup.

Runtime editing is not required.

Recommended separation:

- canonical default vocabulary,
- optional vocabulary pack,
- optional scene overrides.

### Define fallback rules

Recommended order:

```text
scene-specific override
→ selected vocabulary pack
→ canonical vocabulary
→ stable ID
```

### Define presentation profiles

Each scene should have a presentation profile that binds:

- universal slots,
- scene-specific metrics,
- widget types,
- labels,
- visibility,
- formatting.

The renderer should not contain scene-ID conditionals.

## Deliverables

- `docs/hud-semantic-slot-model-v1.md`
- proposed shared types
- six initial scene slot maps
- vocabulary ID catalog
- unresolved data gaps list

## Exit criteria

- All six scenes can be represented by the model.
- Vocabulary can change without editing widget rendering code.
- Scene-specific widgets can bind without changing the universal layout.
- The model distinguishes literal, normalized, derived, and ambient data.

## Best owner

HUD/system-architecture agent, reviewed by the SceneManager agent and code agent.

---

# Phase 2 — Shared Video Playback Service Investigation and Design

## Goal

Define the shared video service before each scene migrates independently and preserves incompatible behavior.

## Tasks

- Inventory the current playback wrappers:
  - `VideoSampler`
  - `TimeOffsetVideoBuffer`
  - `VideoSystem`
  - fragment-trail's local fork
  - contour source video path
- Separate source decoding from scene-specific temporal sampling.
- Determine whether one service can own:
  - playlist,
  - active file,
  - random selection,
  - hold timer,
  - previous/next behavior,
  - canonical media root.
- Preserve temporal history needs for `temporal-fields`.
- Preserve source texture access for blob and contour.
- Define video status:
  - media ID,
  - display title,
  - playback progress,
  - hold progress,
  - time remaining,
  - error state,
  - manual selection state.
- Define canonical media metadata.
- Define no-immediate-repeat behavior.
- Define behavior after manual previous/next.
- Define ownership under `RuntimeServices`.
- Define migration adapters for all six scenes.

## Deliverables

- video playback current-to-target ownership map
- `VideoPlaybackService` conceptual API
- migration sequence
- risk matrix
- media directory strategy
- media metadata schema

## Exit criteria

- One agreed target behavior exists.
- Temporal buffering is not accidentally broken.
- No scene needs its own duplicated media directory.
- The HUD status requirements are supported.

## Best owner

Codebase probe agent or SceneManager code agent.

---

# Phase 3 — Shared Effect Knowledge and Selection Design

## Goal

Make `shader-effect-debugger` the source of truth for effect presets and prevent per-scene effect drift.

## Tasks

- Inventory which of the six scenes use:
  - shared effects,
  - local forks,
  - scene-specific shader systems.
- Define canonical effect IDs.
- Define effect compatibility by scene.
- Define effect preset format.
- Define favored presets.
- Define blocked presets.
- Define Pi-safe presets.
- Define selection weights.
- Define cooldown/recent-history behavior.
- Define how scenes request an effect selection.
- Define active-effect status for the HUD.
- Define migration path out of quadrant-crosshair `DebugMode`.
- Preserve debug-only raw parameter editing in `shader-effect-debugger`.
- Decide how unsupported scene-local shaders are handled.

## Deliverables

- shared effect knowledge schema
- effect-selection service design
- debugger output format
- per-scene compatibility matrix
- migration plan

## Exit criteria

- Production scenes do not independently randomize uncurated effect parameters.
- Debugger outputs can be consumed without manual code changes.
- Active effect names are available to semantic status.
- Pi constraints can be encoded.

## Best owner

Shader/effects code agent.

---

# Phase 4 — Build the ExperienceRuntime Skeleton

## Goal

Create the top-level runtime without migrating all scenes.

## Tasks

- Create `ExperienceRuntime`.
- Create `SceneManager` shell.
- Create `HudCompositor` shell.
- Create `InputRouter`.
- Create `RuntimeServices`.
- Create `RuntimeTelemetry`.
- Allocate and own the shared scene output FBO.
- Implement `SceneFrame`.
- Implement update and draw order from the frozen contract.
- Implement `SceneRenderGuard`.
- Establish the global OpenGL baseline.
- Establish global window/frame-rate/fullscreen ownership.
- Add fake scene implementation.
- Add fake HUD status.
- Add fake runtime telemetry.
- Add unit or harness tests where practical.

## Deliverables

- buildable runtime skeleton
- fake scene
- fake SceneFrame
- fake status and telemetry
- no production scenes required yet

## Exit criteria

- Runtime draws a fake scene into the scene FBO.
- HudCompositor receives a read-only texture view.
- GL state is restored between scene and HUD draws.
- No scene can exit or reconfigure the host directly.

## Best owner

SceneManager/runtime code agent.

---

# Phase 5 — Build the Wireframe HUD and Validation Studio

## Goal

Validate layout, slots, bindings, controls, and rendering before producing textured skins.

## Tasks

- Create a dedicated HUD validation/design app.
- Implement canonical 1280×720 wireframe layout.
- Implement media viewport bounds.
- Implement rounded/beveled `MediaViewportMesh`.
- Implement fixed L-shaped panel regions.
- Implement universal slots:
  - scene title,
  - media title,
  - state,
  - activity,
  - effect summary,
  - timing,
  - navigation,
  - reseed.
- Implement one flexible scene-specific region.
- Bind fake semantic data.
- Simulate all six scene profiles.
- Simulate loading/degraded/failed states.
- Add layer and bounds debugging.
- Add screenshot capture.
- Add fixed-dt animation testing.
- Add widget timing instrumentation.
- Add support for ambient widgets driven by fake semantic data.
- Avoid final skin artwork.

## Deliverables

- HUD validation studio
- wireframe HUD
- six fake scene profiles
- screenshot baseline
- slot-binding verification

## Exit criteria

- Same layout works for all six scene profiles.
- Scene-specific content changes without layout code changes.
- Vocabulary changes do not require editing widget classes.
- Universal controls remain stable.
- HUD can render with missing optional data.
- Performance cost is measurable.

## Best owner

HUD implementation code agent, guided by HUD architecture owner.

---

# Phase 6 — Integrate the First Real Scene

## Goal

Prove real data and commands through the runtime and HUD.

## Recommended first scene

`blob-region-prototype`

Reasons:

- lowest render-boundary complexity,
- simple continuous runtime,
- no major nested-FBO problem,
- useful region/activity data,
- good test for normalized signals.

## Tasks

- Wrap the scene with `IEcopunkScene`.
- Remove scene ownership of host globals.
- Expose `SceneHudStatus`.
- Expose semantic activity values.
- Expose scene-specific metrics.
- Implement curated reseed behavior.
- Integrate shared video status when available.
- Route commands through capabilities.
- Verify scene render output inside the media viewport.
- Verify GL state restoration.
- Add visual parity screenshots.
- Add performance instrumentation.

## Deliverables

- one production scene in the HUD
- real semantic signals
- real scene-specific widget
- real command path

## Exit criteria

- Scene output matches the original.
- HUD responds to real state.
- No raw developer parameter is exposed.
- Reseed is safe and coherent.
- Runtime remains stable across repeated activations.

## Best owner

Scene migration code agent.

---

# Phase 7 — Integrate the Second Scene and Prove Scene Switching

## Goal

Validate lifecycle, scene switching, transition state, and non-uniform scene telemetry.

## Recommended second scene

`temporal-fields`

Reasons:

- richest scene status,
- strong timing concepts,
- temporal-depth data,
- multiple patterns,
- existing transition behavior,
- different shape from Blob.

## Tasks

- Migrate to `IEcopunkScene`.
- Validate internal transition FBO behavior under the outer scene FBO.
- Map pattern and timing state into semantic data.
- Add temporal-depth telemetry.
- Add scene-specific widget profile.
- Implement scene-manager switching between Blob and Temporal.
- Implement static outgoing-frame transition:
  - fade out,
  - load,
  - first frame,
  - fade in.
- Verify deactivate/shutdown/reactivate behavior.
- Verify memory release.
- Verify command suppression during transition.

## Deliverables

- two-scene runtime
- real scene switching
- transition status
- two distinct scene-specific telemetry profiles

## Exit criteria

- Scene switching is stable.
- Last-frame transition is visually acceptable.
- No GL leakage occurs.
- Scene lifecycle works repeatedly.
- HUD remains stable while scene content changes.

## Best owner

SceneManager and scene migration code agent.

---

# Phase 8 — Migrate Remaining Scenes

## Goal

Bring all six final runtime scenes under the shared contract.

## Recommended order

1. `blueprint_emergence`
2. `contour-portrait`
3. `fragment-trail`
4. `quadrant-crosshair`

## Per-scene priorities

### Blueprint Emergence

- preserve overlay ordering,
- validate nested FBO behavior,
- expose cycle phase,
- expose zone balance and density,
- add media title getter,
- add curated reseed.

### Contour Portrait

- disable camera mode,
- preserve video-only path,
- define contour/displacement semantic metrics,
- define safe reseed from presets,
- preserve performance quality choices.

### Fragment Trail

- complete CrosshairSystem graduation first,
- parameterize scissor height,
- expose movement preset names,
- expose fragment and trail metrics,
- integrate shared video and effects where possible.

### Quadrant Crosshair

- remove host-global ownership,
- complete CrosshairSystem graduation,
- validate nested FBO and scissor behavior,
- expose four-channel telemetry,
- replace useful DebugMode responsibilities with debugger/shared effect knowledge,
- preserve expansion behavior,
- implement safe reseed.

## Deliverables

- all six scenes implement the shared contract
- all six have semantic profiles
- all six use capability-driven controls
- all six use standardized media paths
- shader-capable scenes use shared effect knowledge where compatible

## Exit criteria

- all six scenes run under one ExperienceRuntime
- next/previous scene works
- next/previous video works consistently
- reseed works safely
- HUD layout remains unchanged
- only the flexible telemetry region varies

## Best owner

Scene migration code agent, potentially split into one agent per scene after prerequisites are resolved.

---

# Phase 9 — Raspberry Pi Build, Deployment, and Baseline Profiling

## Goal

Establish the real hardware constraints before final art layers increase GPU and memory usage.

## Tasks

- Add Pi build config.
- Define `PLATFORM_PI`.
- Establish Raspberry Pi OS target.
- Decide X11 versus Wayland path.
- Adapt deployment scripts.
- Add systemd or kiosk launch.
- Add `RuntimeTelemetry` sources:
  - FPS,
  - frame time,
  - memory,
  - temperature,
  - throttle status.
- Profile:
  - no HUD,
  - wireframe HUD,
  - scene output FBO,
  - media clipping,
  - universal widgets,
  - scene-specific widgets,
  - transitions,
  - each production scene.
- Define quality profiles:
  - Pi Safe,
  - Pi Enhanced,
  - Desktop.
- Validate one active skin placeholder layer stack.

## Deliverables

- repeatable Pi deployment
- performance report
- memory report
- scene-by-scene risk matrix
- quality-profile definitions

## Exit criteria

- all six scenes launch on the Pi
- stable frame-rate target is agreed from measurement
- no thermal throttling under expected duration
- skin-layer memory budget is known
- high-risk scenes have explicit reduced-quality options

## Best owner

Pi deployment/performance code agent.

---

# Phase 10 — Define the Canonical HUD Blueprint

## Goal

Freeze the production layout and content hierarchy after the semantic model and real-scene tests prove what is needed.

## Tasks

- Lock canonical canvas.
- Lock media viewport geometry.
- Lock bevel and radius.
- Lock panel regions.
- Lock universal slot positions.
- Lock flexible telemetry region.
- Lock universal control positions.
- Define typography scale.
- Define icon bounds.
- Define safe margins.
- Define layer order.
- Define animation rules.
- Define missing-data behavior.
- Define transition behavior.
- Define vocabulary slot locations.
- Define skin-editable versus runtime-owned features.

## Deliverables

- `HUD-Blueprint-v1.md`
- exact pixel geometry
- normalized geometry
- annotated wireframe
- widget hierarchy
- render-layer diagram

## Exit criteria

- no unresolved layout decisions block asset production
- the same blueprint supports all six scenes
- art assets can be authored without guessing live-content bounds

## Best owner

HUD/product-design owner, reviewed by HUD implementation agent.

---

# Phase 11 — Define the Skin Package Specification

## Goal

Create the stable runtime and art contract for all five skins.

## Tasks

Define required files such as:

```text
skin.json
palette.json
shell-back.png
shell-front.png
shell-emission.png
media-mask.png
panel-mask.png
decoration-atlas.png
decoration-atlas.json
preview.png
asset-generation-notes.md
```

Define:

- canonical resolution,
- file formats,
- premultiplied-alpha policy,
- color-space policy,
- layer order,
- occlusion behavior,
- mask conventions,
- atlas rules,
- typography ownership,
- button states,
- emission intensity,
- animation assets,
- fallback assets,
- memory budgets,
- naming conventions,
- skin/vocabulary pairing.

## Deliverables

- `HUD-Skin-Package-Spec-v1.md`
- sample empty skin package
- schema or typed manifest
- validation checklist

## Exit criteria

- runtime can load a blank package
- all visual layers have defined ownership
- an artist or agent can produce a skin without reading C++ code
- multiple skins can use the same layout

## Best owner

HUD architecture/art pipeline owner, reviewed by code agent.

---

# Phase 12 — Build the Skin Loader

## Goal

Make the runtime load interchangeable skin packages through one implementation path.

## Tasks

- Parse skin manifest.
- Load full-screen layers.
- Load masks.
- Load palette.
- Load atlas.
- Validate dimensions.
- Validate alpha.
- Handle missing optional layers.
- Apply emission intensity.
- Bind vocabulary pack or vocabulary selection.
- Add skin selection in HUD Studio.
- Add screenshot comparison.
- Load only one active skin on Pi.
- Add fallback wireframe skin.

## Deliverables

- `HudSkinLoader`
- `HudSkin`
- manifest parser
- validation errors
- wireframe fallback skin

## Exit criteria

- skin switching changes appearance without changing layout
- no scene code references skin assets
- invalid packages fail gracefully
- only the active skin consumes runtime texture memory

## Best owner

HUD implementation code agent.

---

# Phase 13 — Dark Moss Skin Production

## Goal

Create the first production-quality skin and prove the complete art pipeline.

## Tasks

### Art direction

- Lock Dark Moss reference.
- Define material balance:
  - dark stone,
  - moss,
  - carved botanical details,
  - subtle metal,
  - green-yellow emission.
- Define weathering level.
- Define occlusion areas.
- Define the relation between shell and live content.

### Asset production

- Export blueprint wireframe.
- Generate material boards.
- Generate stone plates.
- Generate moss plates.
- Generate carved botanical details.
- Generate emissive line assets.
- Generate button and ornament atlas.
- Composite full shell.
- Clean malformed generated details.
- Remove fake text.
- Align all geometry to the blueprint.
- Extract:
  - back layer,
  - front layer,
  - emission layer,
  - media mask,
  - panel masks,
  - atlas.
- Create preview.

### Runtime validation

- Test all six scene profiles.
- Test all controls.
- Test loading/error states.
- Test Pi memory and draw cost.
- Compare screenshots against approved target.

## Deliverables

- complete Dark Moss skin package
- editable source assets
- production PNG layers
- generation notes
- validation screenshots
- Pi performance results

## Exit criteria

- visual target approved
- no baked fake data or text
- masks align pixel-perfectly
- all live content remains readable
- Pi performance remains acceptable
- production steps are documented well enough to repeat

## Best owner

HUD art-production agent, with implementation agent handling runtime packaging and validation.

---

# Phase 14 — Formalize the Art Production Pipeline

## Goal

Turn the Dark Moss production experience into a repeatable process for future skins.

## Tasks

Document the end-to-end workflow:

```text
approved layout
→ wireframe export
→ style brief
→ material boards
→ texture generation
→ ornament generation
→ shell composition
→ cleanup
→ layer extraction
→ manifest creation
→ runtime validation
→ Pi validation
→ visual QA
→ release package
```

Create reusable templates for:

- skin style brief,
- image-generation prompts,
- material-board prompts,
- ornament prompts,
- shell-composition prompts,
- cleanup checklist,
- layer-extraction checklist,
- manifest,
- asset-generation notes,
- visual QA report,
- Pi QA report.

Define what the LLM/art agent is responsible for and what must be programmatically aligned.

Create automated validation where possible:

- exact image dimensions,
- alpha presence,
- missing files,
- mask coverage,
- atlas metadata,
- naming,
- duplicate IDs,
- texture memory estimate.

## Deliverables

- `HUD-Art-Production-Pipeline-v1.md`
- reusable prompt templates
- empty skin source template
- automated skin validator
- QA checklist
- example based on Dark Moss

## Exit criteria

- a new skin can be started from the template
- no new runtime code is required
- the same output package structure is produced every time
- geometry remains identical across skins
- only theme/material/tone changes

## Best owner

HUD art-production/system-design owner plus a code agent for validation tooling.

---

# Phase 15 — Produce the Remaining Four Skins

## Goal

Use the finalized art pipeline to complete the full skin library.

## Production order

1. Plywood + Marble
2. Bioluminescent Tech
3. Botanical Drafting
4. Sunlit Solarpunk Glass

## Why this order

### Plywood + Marble

Closest to the physical sculpture and likely the canonical default skin.

### Bioluminescent Tech

Simplest raster/material complexity and a good test of emissive-layer flexibility.

### Botanical Drafting

Tests light surfaces, printed detail, and reduced glow.

### Sunlit Solarpunk Glass

Most demanding due to translucency, reflection, and layered organic detail.

## Per-skin process

- style brief
- material boards
- shell generation
- cleanup
- layer extraction
- packaging
- validation
- Pi profiling
- vocabulary tuning
- release approval

## Deliverables

- five total production skins
- previews
- editable sources
- runtime packages
- QA reports

## Exit criteria

- all skins use the same blueprint
- all skins use the same loader
- all skins support all six scenes
- no skin-specific C++ branches exist
- switching skins does not affect scene state

## Best owner

One art-production agent per skin, using the shared pipeline and reviewed by the HUD owner.

---

# Phase 16 — Final Integration and Release Hardening

## Goal

Prepare the system for reliable daily living-room operation.

## Tasks

- Set canonical default skin.
- Set default scene order.
- Set default media hold duration.
- Set default vocabulary pack.
- Set default quality profile.
- Add boot/loading state.
- Add media failure recovery.
- Add scene failure recovery.
- Add watchdog or safe fallback behavior.
- Add last-known-good configuration.
- Add config validation.
- Add startup logging.
- Add long-duration soak test.
- Add hard-power-loss recovery.
- Add automatic launch.
- Add display sleep prevention.
- Add final documentation.

## Deliverables

- release candidate
- deployment package
- startup service
- operating guide
- developer maintenance guide
- skin-production guide
- troubleshooting guide

## Exit criteria

- system boots unattended
- system recovers from missing media or failed scenes
- system runs for extended periods without memory growth
- controls remain responsive
- skin and vocabulary changes are straightforward
- new skins can be generated through the production pipeline

## Best owner

Runtime code agent, Pi agent, and HUD owner jointly.

---

# 6. Dependency Map

```text
Phase 0: Freeze Foundations
    ↓
Phase 1: Semantic Slot Model
    ├──────────────→ Phase 5: Wireframe HUD
    ├──────────────→ Phase 10: HUD Blueprint
    └──────────────→ Scene adapters

Phase 2: Shared Video Design
    └──────────────→ Scene migrations

Phase 3: Effect Knowledge Design
    └──────────────→ Scene migrations and reseed

Phase 4: ExperienceRuntime Skeleton
    ├──────────────→ Phase 5: Wireframe HUD
    └──────────────→ Phase 6: First Scene

Phase 5: Wireframe HUD
    ├──────────────→ Phase 6: First Scene
    ├──────────────→ Phase 9: Pi Baseline
    └──────────────→ Phase 10: Blueprint

Phase 6 + Phase 7
    └──────────────→ Phase 8: Remaining Scenes

Phase 9: Pi Baseline
    └──────────────→ Phase 11: Skin Package Budget

Phase 10: Blueprint
    └──────────────→ Phase 11: Skin Spec

Phase 11 + Phase 12
    └──────────────→ Phase 13: Dark Moss

Phase 13
    └──────────────→ Phase 14: Art Pipeline

Phase 14
    └──────────────→ Phase 15: Remaining Skins

All implementation streams
    └──────────────→ Phase 16: Release Hardening
```

---

# 7. Agent Ownership Summary

| Work | Best primary owner |
|---|---|
| Contract/header freeze | SceneManager code agent |
| Semantic slot model | HUD/system architecture agent |
| Video playback investigation | Codebase probe agent |
| Effect knowledge design | Shader/effects agent |
| ExperienceRuntime | Runtime/SceneManager agent |
| Wireframe HUD | HUD implementation agent |
| Scene adapters | Scene migration agents |
| Pi deployment/profiling | Raspberry Pi agent |
| HUD Blueprint | HUD product/design owner |
| Skin package spec | HUD architecture + art pipeline owner |
| Skin loader | HUD implementation agent |
| Dark Moss production | Art-production agent |
| Art-pipeline documentation | HUD/art-system owner |
| Skin validation tooling | Code agent |
| Remaining skins | One art agent per skin |
| Final hardening | Runtime + Pi + HUD owners |

---

# 8. Milestone Checklist

## Architecture milestone

- [x] Scene/HUD alignment
- [x] Scene/HUD Contract v1
- [x] scene scope
- [x] observability inventory
- [x] product-direction addendum
- [ ] contract header frozen
- [ ] semantic slot model approved

## Runtime milestone

- [ ] ExperienceRuntime skeleton
- [ ] SceneManager shell
- [ ] SceneFrame
- [ ] GL guard
- [ ] InputRouter
- [ ] RuntimeTelemetry

## Shared services milestone

- [ ] video playback plan
- [ ] media-root plan
- [ ] effect knowledge schema
- [ ] effect-selection design
- [ ] performance instrumentation extraction

## HUD milestone

- [ ] wireframe layout
- [ ] media mesh
- [ ] universal slots
- [ ] scene-specific region
- [ ] vocabulary map
- [ ] HUD validation studio
- [ ] skin loader

## Scene milestone

- [ ] Blob adapter
- [ ] Temporal adapter
- [ ] Blueprint adapter
- [ ] Contour adapter
- [ ] Fragment adapter
- [ ] Quadrant adapter
- [ ] scene switching
- [ ] curated reseed

## Pi milestone

- [ ] build config
- [ ] deployment
- [ ] kiosk boot
- [ ] telemetry
- [ ] baseline profiling
- [ ] quality profiles
- [ ] soak test

## Art milestone

- [ ] HUD Blueprint v1
- [ ] Skin Package Spec v1
- [ ] Dark Moss
- [ ] Art Production Pipeline v1
- [ ] Plywood + Marble
- [ ] Bioluminescent Tech
- [ ] Botanical Drafting
- [ ] Sunlit Solarpunk Glass

## Release milestone

- [ ] default configuration
- [ ] failure recovery
- [ ] startup sequence
- [ ] unattended boot
- [ ] final documentation
- [ ] production release

---

# 9. The Very Next Task

## Next phase

**Phase 1 — Define the HUD Semantic Slot Model**

This is the immediate next task because it is the smallest unresolved boundary that affects:

- the wireframe HUD,
- scene adapters,
- scene-specific widgets,
- vocabulary organization,
- shared data visualization,
- the future HUD Blueprint,
- and the skin pipeline.

The task should define structure, IDs, and data ownership—not final phrases or final visual layout.

## Who should tackle it

**Primary owner:** the HUD/system-architecture agent.

**Reviewers:**

- SceneManager agent, to ensure the model can be produced through the existing scene contract.
- Code agent, to ensure the types are practical and inexpensive on Raspberry Pi.
- Project owner, to approve the small universal control and telemetry set.

## Immediate output

The next agent should produce:

`HUD-Semantic-Slot-Model-v1.md`

It should include:

- stable universal slot IDs,
- scene-specific metric structure,
- semantic state IDs,
- normalized signal definitions,
- literal/normalized/derived/ambient classifications,
- vocabulary lookup organization,
- fallback rules,
- six initial scene mappings,
- required changes, if any, to `SceneHudStatus`,
- explicit non-goals,
- examples sufficient to build the wireframe HUD.

Once that document is approved, the runtime skeleton, wireframe HUD, video-service investigation, and effect-knowledge work can proceed in parallel.
