# Ecopunk Runtime — Shared Project Context

## Product

A Raspberry Pi 3B+-targeted openFrameworks living-room art system that cycles among six Ecopunk Video Collage scenes and presents them inside a cinematic sci-fi HUD.

The final runtime should support:

- one canonical 1280×720 HUD layout,
- a scene viewport with rounded corners and a 45-degree lower-right bevel,
- an L-shaped control/telemetry shell,
- previous/next scene,
- previous/next video,
- curated scene reseed/reconfigure,
- scene title and curated video title,
- real and derived data visualizations,
- one flexible scene-specific telemetry region,
- interchangeable HUD skins,
- a repeatable art-production pipeline for future skins.

## Runtime scene scope

Included:

1. `blob-region-prototype`
2. `contour-portrait`
3. `temporal-fields`
4. `fragment-trail`
5. `quadrant-crosshair`
6. `blueprint_emergence`

Excluded from the runtime scene cycle:

- `radar-effects-gallery`
- `radar-pulse`
- `shader-effect-debugger`
- `hud_validation_harness`
- `hud_elements`
- FireplaceWaterfall

## Important product decisions

- No live camera input will be used.
- `contour-portrait` must run in prerecorded-video mode.
- `blob-region-prototype` processes prerecorded video, not visitors.
- `ofxGui` is developer-only.
- No visitor-facing pause, stop, reset, restart, debug, camera, or raw parameter controls.
- Scene randomization must use curated safe presets/ranges.
- Video playback must be standardized across scenes.
- Media should live under one canonical shared root.
- Shader-capable scenes should use the canonical shared effect library where technically compatible.
- `shader-effect-debugger` is developer tooling for testing effects and authoring favored/blocked presets.
- The HUD is cinematic and technically believable, not a museum label or debug dashboard.
- Vocabulary may be compiled or loaded at setup; hot reloading is not required.
- Layout stays fixed across scenes and skins; only scene-specific telemetry slots vary.

## Frozen architecture direction

The approved model is:

```text
ExperienceRuntime
├── SceneManager
├── HudCompositor
├── InputRouter
└── RuntimeServices
```

- `ExperienceRuntime` owns the scene output FBO.
- `HudCompositor` consumes a read-only `SceneFrame`.
- Scenes render at native resolution for v1.
- Scenes implement `IEcopunkScene`.
- Scene status is immutable and pull-based.
- Runtime and scene commands are separate.
- Unsupported controls are hidden.
- Runtime telemetry is separate from scene status.
- The HUD and SceneManager are sibling systems.

## Data philosophy

Prefer data in this order:

1. Literal runtime data
2. Normalized scene data
3. Derived historical data
4. Ambient display data influenced by real scene state

The HUD should never expose raw developer tuning values automatically.

## Collaboration model

Each domain chat is a project-management and architecture thread for its domain.

Domain chats:

- do not implement code directly,
- inspect plans and probe results,
- produce domain decisions,
- generate coding-agent prompts,
- review coding-agent findings and implementation results,
- escalate cross-domain contract changes to the Architecture chat.

Coding agents:

- inspect and modify the codebase,
- execute tests/builds,
- report deviations and blockers,
- do not unilaterally change shared contracts.

## Required source documents

All domain chats should have access to:

- Scene/HUD Contract v1
- Scene Observability Profile
- HUD Scope and Product-Direction Addendum
- Ecopunk HUD System Master Roadmap
- Architecture Governance
- Cross-Domain Handoff Protocol
- Decision Log
