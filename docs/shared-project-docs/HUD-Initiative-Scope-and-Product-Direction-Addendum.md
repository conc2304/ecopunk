# HUD Initiative Scope and Product-Direction Addendum

**Status:** Approved direction to be used alongside the Scene Observability Profile and Scene/HUD Contract v1.

**Purpose:** Correct and narrow the assumptions used by the earlier observability work before creating the Cinematic Systems Language Specification, HUD Information Architecture, HUD Blueprint, or implementation handoff.

## 1. Product Context

This system is not being designed for a museum, gallery, or public installation.

It will live in a private living room as persistent, animated living decor. The primary user is also the developer, curator, and owner of the system.

The HUD should therefore prioritize cinematic atmosphere, visual coherence, ambient technical storytelling, satisfying interaction, long-running autonomous behavior, and compatibility with the physical plywood, marble, moss, and light-based sculpture.

It should feel like a believable systems interface from a science-fiction film while remaining grounded in real runtime state.

## 2. Supported Runtime Scenes

The active SceneManager and HUD scope contains six scenes:

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
- FireplaceWaterfall and its sketches

`radar-effects-gallery` and `radar-pulse` are excluded because they are not strong enough candidates for the final artwork collection.

## 3. Shader Effect Debugger Role

`shader-effect-debugger` is a developer tool, not a runtime scene and not part of the cinematic HUD.

Its long-term role is to replace or absorb the useful responsibilities of `quadrant-crosshair`'s current `DebugMode`.

It should support:

- previewing every shared video shader and video effect
- exposing only the active effect's parameters
- creating favored effect-parameter presets
- assigning selection weights or preference levels
- marking visually poor parameter combinations as blocked
- marking presets as unsafe or unsuitable for Raspberry Pi use
- testing evolution and drift behavior
- saving reusable knowledge consumed by production scenes

Production scenes should not independently invent or drift in how they randomize effect parameters.

## 4. Shared Effect Behavior

Every scene that uses shaders or video effects should draw from the canonical shared video-effects library wherever technically compatible.

When multiple effects are available, scenes should be capable of using the broader catalog rather than maintaining independent scene-local effect lists without a documented reason.

The eventual shared effect-selection system should understand:

- effect availability
- scene compatibility
- favored presets
- blocked presets
- performance profile
- recent-selection history
- selection weights
- active effect names
- transition state
- parameter ranges approved by the debugger

The HUD may display effect activity and names where useful, but must not expose raw shader parameters.

## 5. Video Input Policy

No runtime scene will use a live camera.

Any camera modes currently present in code should be disabled, excluded from runtime capabilities, or removed during migration where safe.

This applies especially to `contour-portrait`.

`blob-region-prototype` will process prerecorded video content. Its terminology should describe regions, fields, movement, or fragments—not visitors, people, surveillance, or camera tracking.

The runtime and HUD should not be designed around human-presence detection.

## 6. Standardized Video Playback

Video cycling and playback should be standardized across all six scenes through a shared service or shared playback contract.

Desired baseline behavior:

1. Select a video from one canonical media directory.
2. Play that video on repeat.
3. Keep it active for at least a configurable hold duration.
4. After the hold duration, select another random video.
5. Avoid immediate repetition where possible.
6. Continue autonomous playback indefinitely.

Manual previous/next video commands should:

1. switch immediately,
2. reset the hold timer,
3. loop the manually selected video,
4. return to normal automatic random selection after the hold interval.

The shared playback system should expose:

- current media ID
- curated display title
- clip playback progress
- current hold progress
- hold time remaining
- manual-selection state
- media-ready/error state

All scenes should use one standardized media root. The project should not require repeated copies or symlinks into separate sketch directories.

A shared video-playback consolidation initiative may proceed in parallel with the HUD work, but its status contract must align with the HUD's needs.

## 7. Runtime Control Scope

The cinematic HUD may expose:

- previous scene
- next scene
- previous video
- next video
- reseed or reconfigure scene parameters
- current scene title
- current video title

The HUD will not expose:

- pause
- stop
- restart
- reset
- developer GUI visibility
- raw debug modes
- shader parameter sliders
- CV thresholds
- performance-tuning controls
- camera controls
- installation shutdown or exit as a normal visible action

Previous/next scene remains gated by SceneManager implementation readiness.

## 8. Scene Randomization

The visible randomization action must not randomize arbitrary `ofxGui` values.

It should request a curated, safe variation from the active scene.

Recommended behavior:

- choose from approved presets and ranges
- preserve performance constraints
- avoid blocked shader-effect parameter combinations
- avoid visually broken scene states
- support deterministic reproduction internally if needed
- produce an immediate but coherent visual reconfiguration

Possible HUD language:

- `RESEED`
- `RECONFIGURE`
- `GENERATE VARIANT`
- `SHIFT STATE`

The final language will be decided in the Cinematic Systems Language Specification.

## 9. ofxGui Policy

`ofxGui` is exclusively a developer-tuning interface.

It is used to:

- explore parameter ranges
- identify visually strong settings
- validate performance
- tune pacing
- author presets
- debug scenes

It is not the source of truth for cinematic HUD controls.

```text
ofxGui developer tuning
→ approved ranges and presets
→ runtime scene behavior
→ normalized semantic state
→ cinematic HUD
```

No raw `ofParameter` group should automatically appear in the runtime HUD.

## 10. HUD Product Definition

Only one runtime HUD product is being built.

It is a cinematic ambient control and telemetry interface for the living-room artwork.

Separate curator and visitor products are not required.

Developer tooling remains outside the runtime HUD in:

- `ofxGui`
- `shader-effect-debugger`
- validation harnesses
- logs and performance instrumentation

The runtime HUD should look information-rich and technically believable while exposing only a small number of direct controls.

Visual density can come from real derived signals, histories, state traces, event markers, effect indicators, and decorative systems graphics—not from exposing dozens of editable parameters.

## 11. Shared Semantic Runtime Data

The six scenes should converge on a small shared set of normalized semantic data while retaining scene-specific telemetry.

Candidate concepts:

```cpp
struct SceneActivity {
    float overall;
    float motion;
    float density;
    float variation;
    float transition;
};

struct SceneSemanticState {
    std::string primaryState;
    std::optional<std::string> secondaryState;
    std::optional<float> progress;
};

struct SceneTimingStatus {
    float activeSeconds;
    std::optional<float> stateProgress;
    uint64_t generation;
};
```

Exact structures are not approved by this addendum. They are inputs to the upcoming Cinematic Systems Language Specification and data-model work.

Values should be normalized where possible, cheap to calculate, grounded in real scene behavior, stable enough to drive graphs and animation, and semantically consistent across scenes.

Each scene may calculate the shared values differently.

## 12. Scene-Specific Telemetry

The canonical HUD layout may include one flexible scene-specific telemetry region.

Examples:

### Blob
- region activity
- occupied area
- movement field
- fragment count

### Contour
- line density
- displacement energy
- contour complexity
- breakup phase

### Temporal Fields
- active pattern
- temporal depth
- field activity
- evolution state

### Fragment Trail
- movement preset
- active fragments
- trail persistence
- spawn cadence

### Quadrant Crosshair
- four quadrant states
- active effect per quadrant
- expansion state
- crosshair movement mode

### Blueprint Emergence
- structural phase
- fragment density
- zone balance
- build/dissolve progress

These are candidates, not final HUD requirements.

## 13. Techy Data Visualization Direction

The HUD should support the visual language of a science-fiction systems display.

Appropriate data-visualization material may include:

- rolling activity traces
- motion and density sparklines
- temporal-history bands
- effect-stack indicators
- bounded phase progress
- event ticks
- signal pulses
- four-channel quadrant telemetry
- state-transition diagrams
- small topographic or field visualizations
- decorative telemetry derived from real normalized signals

Data visualizations do not need to be literal scientific instruments, but should be driven by real state rather than arbitrary looping decoration wherever practical.

The forthcoming specification must distinguish:

- literal data
- normalized semantic data
- derived display data
- decorative flavor data

## 14. Impact on Prior Observability Report

The Scene Observability Profile remains useful as a factual code inventory, but these conclusions are superseded:

- museum/visitor interpretation is no longer the primary framing
- camera-based visitor detection is out of scope
- radar scenes are not runtime candidates
- `shader-effect-debugger` is developer tooling only
- curator and visitor HUDs will not be developed as separate products
- reset/restart will not be exposed
- raw filenames should be replaced by curated media titles
- scene-specific telemetry may be visually prominent when it supports the cinematic HUD
- graphs and signal visualizations are desirable when driven by real or derived scene state

## 15. Next Deliverable

The next task is to produce a:

# Cinematic Systems Language Specification

It should define:

- the universal HUD vocabulary
- naming rules for scene states
- naming rules for effects and media
- normalized shared signals
- derived display signals
- scene-specific telemetry vocabulary
- runtime control language
- data-visualization semantics
- distinctions between literal, semantic, derived, and decorative data
- candidate data-model changes needed beyond `SceneHudStatus`
- mappings for all six supported scenes

The specification must use this addendum, the Scene Observability Profile, and Scene/HUD Contract v1 as source documents.

It should not yet design exact panel geometry or produce implementation code.
