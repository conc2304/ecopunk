# HUD Semantic Slot Model v1

**Status:** Architecture draft for cross-domain review  
**Primary owner:** Architecture and Program Coordination  
**Required reviewers:** HUD Runtime and Validation Studio; ExperienceRuntime and SceneManager; Raspberry Pi Runtime and Performance  
**Intended destination:** `docs/hud-semantic-slot-model-v1.md`

---

# 1. Purpose

This document defines the stable semantic boundary between:

- scene/runtime state,
- shared services,
- HUD presentation,
- vocabulary,
- reusable widgets,
- scene-specific telemetry.

It allows the HUD runtime, ExperienceRuntime, scene adapters, shared video service, shared effects service, and future skin pipeline to proceed without finalizing every word, visual treatment, or widget arrangement.

The model intentionally separates:

```text
raw runtime state
→ semantic data
→ slot bindings
→ vocabulary
→ widget presentation
→ skin
```

The HUD must not read arbitrary scene internals or raw `ofxGui` values. Scenes and shared services expose typed semantic snapshots. The HUD binds those snapshots into stable slots and reusable widgets.

---

# 2. Goals

HUD Semantic Slot Model v1 must:

1. Represent all six approved runtime scenes.
2. Support one fixed HUD layout with flexible scene-specific telemetry regions.
3. Preserve the frozen Scene/HUD ownership model.
4. Keep runtime telemetry separate from scene telemetry.
5. Keep media status separate from scene-owned data where shared playback owns it.
6. Keep effect status separate from raw shader parameters.
7. Allow vocabulary to change without editing widget rendering code.
8. Allow scene-specific vocabulary without scene-specific HUD branches.
9. Distinguish literal, normalized, derived, and ambient data.
10. Support fake data in the HUD Validation Studio.
11. Remain inexpensive enough for Raspberry Pi 3B+.
12. Avoid committing the project to final cinematic wording.
13. Give coding agents enough structure to implement the wireframe HUD and scene adapters.

---

# 3. Non-goals

This model does not:

- define the final pixel layout,
- define the final HUD Blueprint,
- define final vocabulary,
- define skin artwork,
- define shader-effect preset formats,
- define video decoder ownership,
- define every possible future widget,
- expose raw `ofParameter` values,
- provide a generic reflection system over scene objects,
- create a visitor/curator/developer mode hierarchy,
- reintroduce camera input,
- add reset, restart, pause, or stop to the cinematic HUD,
- require runtime vocabulary hot reloading,
- require arbitrary user-authored scene widgets.

---

# 4. Approved Product Scope

## Runtime scenes

1. `blob-region-prototype`
2. `contour-portrait`
3. `temporal-fields`
4. `fragment-trail`
5. `quadrant-crosshair`
6. `blueprint_emergence`

## Runtime controls

Universal cinematic controls may include:

- previous scene,
- next scene,
- previous video,
- next video,
- reseed/reconfigure scene.

No cinematic control is exposed merely because a legacy sketch has a keyboard shortcut.

## Fixed presentation rule

The universal HUD layout remains stable across scenes.

Only these vary:

- semantic values,
- vocabulary,
- effect list,
- scene-specific telemetry bindings,
- optional scene-specific widget types,
- skin appearance.

---

# 5. Architectural Placement

The approved runtime relationship remains:

```text
ExperienceRuntime
├── SceneManager
├── HudCompositor
├── InputRouter
└── RuntimeServices
```

The semantic model is assembled from multiple owners:

```text
Active Scene
    └── SceneSemanticSnapshot

SceneManager
    └── SceneManagerStatus

Shared Video Service
    └── VideoPlaybackStatus

Shared Effect Service
    └── EffectActivityStatus

RuntimeServices
    └── RuntimeTelemetry

ExperienceRuntime / HUD adapter
    └── HudFrameData

HudCompositor
    └── Slot bindings → widgets → skin
```

No single scene owns the entire HUD snapshot.

---

# 6. Data Ownership

## 6.1 Active scene owns

The active scene owns semantic data derived from its own behavior:

- scene identity,
- current semantic state,
- normalized activity signals,
- scene timing,
- scene-specific metrics,
- curated reseed capability,
- scene-local effect context when not supplied by the shared effect service.

## 6.2 SceneManager owns

SceneManager owns:

- active scene ID,
- pending scene ID,
- scene transition phase,
- scene transition progress,
- scene loading/failure messages,
- scene index/count if exposed.

## 6.3 Shared video service owns

The shared video service should own:

- media ID,
- curated media title,
- clip playback progress,
- hold progress,
- hold time remaining,
- manual selection state,
- media health/error state.

Until that service exists, scene adapters may temporarily provide compatible values, but the HUD contract must not make them permanently scene-owned.

## 6.4 Shared effects service owns

The shared effects service should own, where applicable:

- active effect IDs,
- dominant effect ID,
- active preset IDs,
- transition progress,
- normalized effect intensity,
- effect availability or degraded status.

Scenes may supplement this with contextual effect-channel IDs, such as quadrant assignments.

## 6.5 RuntimeServices owns

RuntimeServices owns:

- FPS,
- frame time,
- memory,
- temperature,
- throttling,
- quality profile,
- runtime health.

These remain separate from `SceneSemanticSnapshot`.

## 6.6 HudCompositor owns

HudCompositor owns:

- slot binding,
- formatting,
- vocabulary resolution,
- short-term signal histories used only for presentation,
- ambient visual derivations,
- missing-data behavior,
- widget selection from presentation profiles.

The HUD does not mutate scene state except by dispatching approved commands through `InputRouter`.

---

# 7. Data Classification

```cpp
enum class HudDataClass : uint8_t {
    Literal,
    Normalized,
    Derived,
    Ambient
};
```

## Literal

A direct runtime value with minimal interpretation.

Examples:

- active pattern ID,
- fragment count,
- current media ID,
- transition progress,
- active effect ID.

## Normalized

A scene-specific value mapped into a documented `[0, 1]` semantic range.

Examples:

- activity,
- motion,
- density,
- variation,
- transition intensity.

## Derived

A value computed from literal or normalized data, often over time.

Examples:

- trend,
- stability,
- peak,
- change rate,
- rolling average,
- event density.

## Ambient

A display-oriented signal that is not a literal measurement but is deterministically influenced by real semantic state.

Examples:

- synthetic scan-line intensity,
- topographic drift,
- decorative pulse cadence,
- telemetry noise seeded by scene generation.

## Rule

A widget or slot must know the classification of its source. The HUD may present all four classes, but should not label Ambient data as if it were a literal measurement.

---

# 8. Core Semantic Types

The following are architecture-level target types. Exact C++ placement and naming may be refined by the implementation review, but the ownership and meaning should remain stable.

## 8.1 Normalized signal bundle

```cpp
struct SceneActivity {
    std::optional<float> overall;    // 0..1
    std::optional<float> motion;     // 0..1
    std::optional<float> density;    // 0..1
    std::optional<float> variation;  // 0..1
    std::optional<float> transition; // 0..1
};
```

### Semantics

- `overall`: combined visual/event activity.
- `motion`: spatial or temporal movement energy.
- `density`: occupied visual complexity or active-element concentration.
- `variation`: rate or degree of state change.
- `transition`: intensity/progress of reconfiguration.

### Rules

- Values are clamped to `[0, 1]`.
- Missing concepts use `std::nullopt`, not zero.
- Zero means present but inactive.
- Every scene documents how each populated signal is calculated.
- Signals must be cheap enough to calculate every frame or at a throttled cadence.
- A scene is not required to populate all five.

## 8.2 Semantic state

```cpp
struct SceneSemanticState {
    std::string primaryStateId;
    std::optional<std::string> secondaryStateId;
    std::optional<float> progress; // 0..1 when genuinely bounded
};
```

Examples:

- `scene.blueprint.state.building`
- `scene.temporal.state.transitioning`
- `scene.fragment.state.orbiting`
- `scene.quadrant.state.expanding`

`primaryStateId` is a stable machine ID, not final display text.

## 8.3 Scene timing

```cpp
struct SceneTimingStatus {
    float activeSeconds = 0.0f;
    std::optional<float> stateElapsedSeconds;
    std::optional<float> stateProgress;
    uint64_t generation = 0;
};
```

`generation` increments after a successful curated reseed or major scene regeneration. It is not required to match internal random seeds.

## 8.4 Scene metric value

```cpp
enum class HudMetricValueType : uint8_t {
    Scalar,
    Count,
    Ratio,
    DurationSeconds,
    Identifier
};

struct SceneMetric {
    std::string metricId;
    HudMetricValueType valueType = HudMetricValueType::Scalar;
    HudDataClass dataClass = HudDataClass::Literal;

    std::optional<float> value;
    std::optional<float> normalizedValue; // 0..1
    std::optional<std::string> valueId;

    std::optional<std::string> unitId;
};
```

### Rules

- `metricId` is stable and vocabulary-addressable.
- `value` contains literal numeric data.
- `normalizedValue` is optional and may support gauges, bars, and ambient derivations.
- `valueId` is used for categorical values.
- Units use stable IDs such as `unit.seconds`, not embedded strings.
- A metric should not contain widget instructions.

## 8.5 Scene semantic snapshot

```cpp
struct SceneSemanticSnapshot {
    uint32_t schemaVersion = 1;

    std::string sceneId;
    std::string displayTitleId;

    SceneHealth health = SceneHealth::Ready;
    std::optional<std::string> messageId;

    SceneSemanticState state;
    SceneActivity activity;
    SceneTimingStatus timing;

    std::vector<std::string> activeEffectIds;
    std::vector<SceneMetric> metrics;
};
```

This is the proposed semantic extension to the existing scene status contract.

---

# 9. Relationship to Existing SceneHudStatus

The frozen `SceneHudStatus` already provides:

- scene ID,
- display name,
- health,
- message,
- media name,
- mode name,
- active effects,
- status lines,
- progress,
- motion energy,
- active item count,
- paused.

That shape was designed before the semantic-slot requirements were fully known.

## Recommendation

Do not delete or silently reinterpret `SceneHudStatus`.

Use a staged additive approach.

### Option recommended for v1 implementation

Add a semantic snapshot method or field without breaking existing status consumers:

```cpp
virtual SceneSemanticSnapshot semanticSnapshot() const = 0;
```

or:

```cpp
struct SceneHudStatus {
    // existing frozen fields
    ...
    std::optional<SceneSemanticSnapshot> semantic;
};
```

### Architecture preference

A separate method is cleaner because:

- `SceneHudStatus` remains a compact operational compatibility snapshot,
- the semantic model can evolve under its own schema version,
- scene migration can implement the semantic snapshot incrementally,
- the HUD Validation Studio can mock the semantic snapshot independently,
- `RuntimeTelemetry`, video status, and manager status remain separate.

### Required review

The ExperienceRuntime and SceneManager domain must decide whether adding `semanticSnapshot()` to `IEcopunkScene` is preferable to embedding an optional semantic field.

This is an additive shared-contract proposal and requires Architecture approval before code changes.

## Deprecated-for-cinematic-HUD fields

The cinematic HUD should not rely directly on:

- `paused`,
- opaque `statusLines`,
- raw `activeItemCount`,
- raw `motionEnergy`,
- scene-owned `mediaName` after shared video playback lands.

These fields may remain useful for migration or internal compatibility.

---

# 10. Aggregated HUD Frame Data

HudCompositor should consume one assembled immutable frame value.

```cpp
struct HudFrameData {
    uint32_t schemaVersion = 1;

    SceneFrame sceneFrame;
    SceneSemanticSnapshot scene;
    SceneManagerStatus sceneManager;
    std::optional<VideoPlaybackStatus> video;
    std::optional<EffectActivityStatus> effects;
    RuntimeTelemetry runtime;
    SceneCapabilities capabilities;
};
```

`HudFrameData` is assembled by `ExperienceRuntime` or a dedicated adapter layer. The HUD should not independently poll multiple mutable services during draw.

## Benefits

- deterministic validation,
- easier fake-data generation,
- cleaner fixed-dt screenshot testing,
- no partial cross-service snapshots,
- no direct HUD dependency on scene implementation classes.

---

# 11. Slot ID Rules

## 11.1 Format

Stable slot IDs use lowercase dot-separated namespaces:

```text
<owner>.<concept>[.<detail>]
```

Examples:

- `scene.title`
- `scene.state.primary`
- `media.hold.progress`
- `runtime.health`
- `control.scene.next`

## 11.2 Stability

After v1 approval:

- IDs may be added.
- Existing IDs may not change meaning.
- Renaming requires a compatibility alias and Architecture review.
- Skin code must not define slot IDs.
- Widget code must not invent scene-specific IDs.

## 11.3 Slot categories

- identity
- state
- signal
- timing
- media
- effects
- runtime
- controls
- scene-specific telemetry
- ambient

---

# 12. Universal Slot Catalog

## 12.1 Identity

| Slot ID | Source | Required | Value |
|---|---|---:|---|
| `scene.id` | scene | Yes | stable scene ID |
| `scene.title` | vocabulary via `displayTitleId` | Yes | text |
| `scene.index` | SceneManager | Optional | count/index |
| `scene.count` | SceneManager | Optional | count |
| `scene.health` | scene | Yes | enum |
| `scene.message` | scene | Optional | vocabulary/text |

## 12.2 Semantic state

| Slot ID | Source | Required | Value |
|---|---|---:|---|
| `scene.state.primary` | scene semantic state | Yes | vocabulary ID |
| `scene.state.secondary` | scene semantic state | Optional | vocabulary ID |
| `scene.state.progress` | scene semantic state | Optional | 0..1 |

## 12.3 Universal signals

| Slot ID | Source | Required | Value |
|---|---|---:|---|
| `scene.activity.overall` | scene | Optional | 0..1 |
| `scene.activity.motion` | scene | Optional | 0..1 |
| `scene.activity.density` | scene | Optional | 0..1 |
| `scene.activity.variation` | scene | Optional | 0..1 |
| `scene.activity.transition` | scene | Optional | 0..1 |

## 12.4 Timing

| Slot ID | Source | Required | Value |
|---|---|---:|---|
| `scene.time.active` | scene | Yes | seconds |
| `scene.time.state_elapsed` | scene | Optional | seconds |
| `scene.time.state_progress` | scene | Optional | 0..1 |
| `scene.generation` | scene | Yes | count |
| `scene.transition.phase` | SceneManager | Yes | enum |
| `scene.transition.progress` | SceneManager | Yes | 0..1 |

## 12.5 Media

These IDs are reserved now, even though the shared video-service schema is still being designed.

| Slot ID | Source | Required | Value |
|---|---|---:|---|
| `media.id` | video service | Optional | ID |
| `media.title` | video metadata/vocabulary | Optional | text |
| `media.playback.progress` | video service | Optional | 0..1 |
| `media.hold.progress` | video service | Optional | 0..1 |
| `media.hold.remaining` | video service | Optional | seconds |
| `media.selection.manual` | video service | Optional | bool |
| `media.health` | video service | Optional | enum |

## 12.6 Effects

| Slot ID | Source | Required | Value |
|---|---|---:|---|
| `effects.active` | effect service/scene | Optional | list of IDs |
| `effects.dominant` | effect service | Optional | ID |
| `effects.transition.progress` | effect service | Optional | 0..1 |
| `effects.intensity` | effect service | Optional | 0..1 |
| `effects.health` | effect service | Optional | enum |

## 12.7 Runtime telemetry

These are optional cinematic display inputs, not scene status.

| Slot ID | Source | Required | Value |
|---|---|---:|---|
| `runtime.fps` | RuntimeTelemetry | Optional | scalar |
| `runtime.frame_time` | RuntimeTelemetry | Optional | ms |
| `runtime.memory` | RuntimeTelemetry | Optional | bytes |
| `runtime.temperature` | RuntimeTelemetry | Optional | Celsius |
| `runtime.throttled` | RuntimeTelemetry | Optional | bool |
| `runtime.quality_profile` | RuntimeServices | Optional | ID |
| `runtime.health` | RuntimeServices | Optional | enum |

## 12.8 Controls

| Slot ID | Command owner | Visibility rule |
|---|---|---|
| `control.scene.previous` | RuntimeCommand | SceneManager ready |
| `control.scene.next` | RuntimeCommand | SceneManager ready |
| `control.media.previous` | Scene/shared video command | video available |
| `control.media.next` | Scene/shared video command | video available |
| `control.scene.reseed` | SceneCommand | capability advertised |

Unsupported controls are hidden.

---

# 13. Scene-Specific Metric IDs

Scene-specific IDs are namespaced:

```text
scene.<scene-id>.metric.<metric-name>
```

Examples:

- `scene.blob.metric.region_count`
- `scene.temporal.metric.temporal_depth`
- `scene.quadrant.metric.channel_0_state`

Scene-specific metrics may populate the flexible telemetry region but must not alter universal slot meanings.

---

# 14. Vocabulary Model

## 14.1 Purpose

Vocabulary maps stable IDs to display strings. It is not responsible for layout, behavior, color, or data calculation.

## 14.2 Storage

Runtime hot reload is not required.

Acceptable implementation choices:

- compiled C++ tables,
- JSON loaded during setup,
- generated C++ from a source JSON file.

Architecture preference: author vocabulary in data files and load once at setup, because it keeps text centralized and allows multiple packs without widget-code changes.

## 14.3 Vocabulary pack

```cpp
struct VocabularyPack {
    std::string packId;
    std::unordered_map<std::string, std::string> strings;
};
```

Example keys:

```text
slot.scene.title.label
slot.media.title.label
control.scene.next.label
scene.blueprint.title
scene.blueprint.state.building
metric.temporal_depth.label
effect.chromatic_aberration.label
unit.seconds.short
```

## 14.4 Resolution order

```text
scene-specific override
→ selected vocabulary pack
→ canonical default vocabulary
→ stable ID
```

## 14.5 Skin relationship

A skin may nominate a preferred vocabulary pack, but:

- skin loading must not change semantic IDs,
- vocabulary packs can be reused across skins,
- skins must not contain hardcoded scene branches,
- vocabulary remains separately replaceable.

## 14.6 Minimum vocabulary categories

Vocabulary slots are required for:

- scene titles,
- scene state names,
- scene mode/pattern names,
- metric labels,
- units,
- media titles,
- effect names,
- runtime health,
- media health,
- controls,
- transition phases,
- loading/failure messages,
- ambient flavor labels if used.

---

# 15. Presentation Profiles

A presentation profile binds semantic data to reusable widgets.

It does not calculate scene data.

```cpp
enum class HudWidgetType : uint8_t {
    Text,
    StatusBadge,
    NumericValue,
    ProgressBar,
    ProgressRing,
    Sparkline,
    Gauge,
    EffectList,
    ChannelStrip,
    Timeline,
    AmbientField
};

struct HudSlotBinding {
    std::string regionId;
    HudWidgetType widgetType;
    std::string sourceId;
    std::optional<std::string> labelVocabularyId;
    bool visibleWhenMissing = false;
};

struct ScenePresentationProfile {
    std::string sceneId;
    std::vector<HudSlotBinding> bindings;
};
```

## Rules

- Profiles may be data or compiled configuration.
- `HudCompositor` selects a profile by scene ID.
- Widget implementations bind by `sourceId`.
- No code path such as `if (sceneId == "temporal-fields")` is allowed inside shared widget rendering.
- Scene-specific widget types require Architecture review if they cannot be expressed by the approved reusable widget set.
- The layout owns region IDs.
- The scene profile owns which supported source fills each flexible region.

---

# 16. History and Derived Signals

The HUD may maintain lightweight histories for numeric semantic slots.

```cpp
struct HudSignalHistoryConfig {
    std::string sourceId;
    size_t sampleCount = 64;
    float sampleRateHz = 8.0f;
};
```

## Recommended v1 limits

- 32–64 samples per signal.
- 5–10 Hz sample rate.
- Only signals actually bound to widgets allocate history.
- No per-frame heap allocations.
- No scene owns HUD presentation history.
- History resets or marks an event boundary on scene changes.

## Standard derived values

The HUD may derive:

- rolling average,
- recent minimum,
- recent maximum,
- normalized slope,
- stability,
- event pulse,
- peak hold.

These calculations should be generic and slot-driven.

---

# 17. Ambient Data Rules

Ambient widgets are allowed when real scene data is insufficient for the desired cinematic density.

## Requirements

Ambient data must:

- be deterministic from a stable seed where practical,
- be influenced by at least one real semantic input,
- never impersonate a precise literal measurement,
- stop updating cleanly during loading/failure states,
- remain inexpensive.

## Examples

```text
scene.activity.motion
→ controls scan speed

scene.activity.density
→ controls contour density

scene.activity.transition
→ controls event tick frequency

scene.generation
→ reseeds ambient field pattern
```

## Ambient source IDs

Ambient sources use:

```text
ambient.<name>
```

Examples:

- `ambient.scan_phase`
- `ambient.field_noise`
- `ambient.event_density`
- `ambient.topography`
- `ambient.signal_jitter`

Ambient IDs are HUD-owned, not scene-owned.

---

# 18. Six Initial Scene Mappings

The mappings below establish candidate semantic coverage. Scene migration agents must verify formulas against the code before implementation.

---

## 18.1 Blob Region Prototype

### Primary state IDs

- `scene.blob.state.analyzing`
- `scene.blob.state.fragmenting`
- `scene.blob.state.stable`
- `scene.blob.state.degraded`

### Universal activity mapping

| Signal | Candidate source |
|---|---|
| overall | weighted region count + fragment count |
| motion | aggregate tracked-region velocity or frame-difference energy |
| density | occupied region area / frame area |
| variation | region creation/removal rate |
| transition | usually absent |

### Scene metrics

- `scene.blob.metric.region_count`
- `scene.blob.metric.fragment_count`
- `scene.blob.metric.occupied_area`
- `scene.blob.metric.tracking_stability`

### Candidate flexible widgets

- region-count value,
- occupied-area gauge,
- motion sparkline,
- compact region activity field.

### Data gaps

- exact aggregate motion formula,
- tracking-stability measure,
- media identity until video service lands,
- curated reseed semantics.

---

## 18.2 Contour Portrait

### Primary state IDs

- `scene.contour.state.tracing`
- `scene.contour.state.displacing`
- `scene.contour.state.breaking`
- `scene.contour.state.reforming`

### Universal activity mapping

| Signal | Candidate source |
|---|---|
| overall | displacement amount + breakup activity |
| motion | temporal change in displacement field |
| density | active line/vertex density normalized to configured range |
| variation | change in line displacement or breakup region |
| transition | breakup progress when bounded |

### Scene metrics

- `scene.contour.metric.line_density`
- `scene.contour.metric.displacement_energy`
- `scene.contour.metric.contour_complexity`
- `scene.contour.metric.breakup_progress`
- `scene.contour.metric.preset`

### Candidate flexible widgets

- contour-complexity gauge,
- displacement sparkline,
- breakup progress ring,
- preset/status badge.

### Data gaps

- exact complexity calculation,
- semantic preset IDs,
- video-only source simplification,
- safe curated reseed preset pool.

---

## 18.3 Temporal Fields

### Primary state IDs

- `scene.temporal.state.running`
- `scene.temporal.state.transitioning`
- `scene.temporal.state.evolving`
- `scene.temporal.state.regenerating`

### Universal activity mapping

| Signal | Candidate source |
|---|---|
| overall | pattern-specific activity aggregate |
| motion | active pattern motion estimate |
| density | active fragments/elements normalized by pattern |
| variation | evolution or regeneration delta |
| transition | pattern transition progress |

### Scene metrics

- `scene.temporal.metric.pattern`
- `scene.temporal.metric.temporal_depth`
- `scene.temporal.metric.history_fill`
- `scene.temporal.metric.field_activity`
- `scene.temporal.metric.evolution_state`
- `scene.temporal.metric.playhead_count`

### Candidate flexible widgets

- temporal-history band,
- pattern badge,
- field-activity sparkline,
- transition timeline,
- temporal-depth gauge.

### Data gaps

- cross-pattern normalized activity definition,
- temporal-depth summary formula,
- whether playhead count is meaningful enough to display,
- interaction between preset timeline and scene state.

---

## 18.4 Fragment Trail

### Primary state IDs

- `scene.fragment.state.drifting`
- `scene.fragment.state.scanning`
- `scene.fragment.state.hunting`
- `scene.fragment.state.nervous`
- `scene.fragment.state.orbiting`
- `scene.fragment.state.decaying`

### Universal activity mapping

| Signal | Candidate source |
|---|---|
| overall | active fragments + spawn activity |
| motion | crosshair speed normalized to allowed range |
| density | active fragment count / configured maximum |
| variation | mode/preset change rate or fragment churn |
| transition | mode transition if explicitly modeled |

### Scene metrics

- `scene.fragment.metric.movement_preset`
- `scene.fragment.metric.fragment_count`
- `scene.fragment.metric.trail_persistence`
- `scene.fragment.metric.spawn_cadence`
- `scene.fragment.metric.content_mode`

### Candidate flexible widgets

- movement preset strip,
- fragment-density gauge,
- spawn-cadence trace,
- trail-persistence bar.

### Data gaps

- named preset accessor,
- safe normalization ranges,
- local video/effect fork migration,
- CrosshairSystem graduation dependency.

---

## 18.5 Quadrant Crosshair

### Primary state IDs

- `scene.quadrant.state.online`
- `scene.quadrant.state.quiet`
- `scene.quadrant.state.standby`
- `scene.quadrant.state.expanding`
- `scene.quadrant.state.reconfiguring`

### Universal activity mapping

| Signal | Candidate source |
|---|---|
| overall | combined per-quadrant activity |
| motion | crosshair motion + motion extraction energy |
| density | number/area of active quadrants |
| variation | effect/state changes across quadrants |
| transition | ExpansionDirector progress |

### Scene metrics

- `scene.quadrant.metric.crosshair_preset`
- `scene.quadrant.metric.expansion_state`
- `scene.quadrant.metric.expansion_progress`
- `scene.quadrant.metric.channel_0_state`
- `scene.quadrant.metric.channel_1_state`
- `scene.quadrant.metric.channel_2_state`
- `scene.quadrant.metric.channel_3_state`
- `scene.quadrant.metric.channel_0_effect`
- `scene.quadrant.metric.channel_1_effect`
- `scene.quadrant.metric.channel_2_effect`
- `scene.quadrant.metric.channel_3_effect`

### Candidate flexible widgets

- four-channel telemetry strip,
- effect chips per quadrant,
- expansion progress ring,
- crosshair movement badge.

### Data gaps

- ExpansionDirector re-entrancy,
- effect ID canonicalization,
- motion normalization,
- CrosshairSystem graduation,
- replacement of DebugMode responsibilities.

---

## 18.6 Blueprint Emergence

### Primary state IDs

- `scene.blueprint.state.blank`
- `scene.blueprint.state.placing`
- `scene.blueprint.state.building`
- `scene.blueprint.state.settling`
- `scene.blueprint.state.dissolving`
- `scene.blueprint.state.hold`

### Universal activity mapping

| Signal | Candidate source |
|---|---|
| overall | fragment population + placement activity |
| motion | fragment drift or motion extraction energy |
| density | zone occupancy / target occupancy |
| variation | placement/dissolve event rate |
| transition | current cycle-phase progress |

### Scene metrics

- `scene.blueprint.metric.cycle_mode`
- `scene.blueprint.metric.fragment_count`
- `scene.blueprint.metric.zone_a_density`
- `scene.blueprint.metric.zone_b_density`
- `scene.blueprint.metric.zone_balance`
- `scene.blueprint.metric.phase_progress`

### Candidate flexible widgets

- structural-phase timeline,
- zone-balance gauge,
- fragment-density sparkline,
- build/dissolve progress ring.

### Data gaps

- media title accessor,
- normalized zone-density ranges,
- exact phase-progress calculation,
- curated pacing/reseed profiles.

---

# 19. Default Wireframe Profile

The HUD Validation Studio should initially support this universal profile:

## Fixed universal regions

1. Scene title
2. Media title
3. Primary semantic state
4. Active effect summary
5. Scene active time or generation
6. Previous/next scene
7. Previous/next media
8. Reseed
9. Overall activity trace
10. Motion or density trace

## Flexible scene-specific region

Supports up to:

- one primary scene-specific widget,
- two compact secondary metrics,
- or one multi-channel widget.

## Missing-data behavior

- Required identity slot missing: show canonical fallback.
- Optional slot missing: hide widget.
- Flexible region empty: display ambient field bound to available universal signals.
- Loading: freeze or dim last valid semantic values and show manager state.
- Failed: suppress misleading live traces and show failure vocabulary.

---

# 20. Performance Constraints

The semantic model itself should be lightweight.

## Requirements

- Snapshot values returned by value must remain compact.
- Avoid unbounded strings and vectors.
- Cap active effects and metrics.
- Avoid per-frame JSON parsing.
- Vocabulary resolves at setup or through cached lookups.
- Presentation profiles resolve once per scene activation.
- Histories allocate fixed-size buffers.
- Semantic calculation should reuse values scenes already compute.
- Derived HUD data should run at 5–10 Hz where possible.
- Ambient data must not require full-resolution FBOs by default.

## Recommended v1 caps

- maximum active effect IDs: 8,
- maximum scene metrics: 16,
- maximum vocabulary string length: implementation-defined but bounded in authored data,
- maximum active history signals: 8,
- maximum samples per signal: 64.

These limits require Pi-domain review before freeze.

---

# 21. Validation Requirements

The HUD Validation Studio must be able to:

- load fake snapshots for all six scenes,
- switch vocabulary packs,
- simulate missing optional data,
- simulate every SceneHealth state,
- simulate SceneManager transitions,
- show data-class badges in debug mode,
- inspect resolved source IDs,
- test profile bindings,
- run deterministic fixed-dt histories,
- capture screenshot baselines,
- verify that no shared widget branches on scene ID,
- estimate active history memory,
- validate that ambient widgets react to semantic inputs.

---

# 22. Required Contract Changes

## Proposed additive change

Add semantic snapshot access to the scene contract.

Preferred form:

```cpp
virtual SceneSemanticSnapshot semanticSnapshot() const = 0;
```

## Proposed runtime aggregation

Define `HudFrameData` outside the scene contract, likely under the HUD/runtime boundary.

## Deferred types

The exact definitions of:

- `VideoPlaybackStatus`,
- `EffectActivityStatus`,
- runtime quality profile,
- presentation-profile serialization,

remain owned by their domains and should be integrated without changing universal slot meanings.

---

# 23. Open Questions for Cross-Domain Review

## ExperienceRuntime and SceneManager

1. Separate `semanticSnapshot()` method or optional field in `SceneHudStatus`?
2. Where should `HudFrameData` be assembled?
3. Should scene capabilities retain labels, or should vocabulary IDs replace raw labels?
4. How should active-scene index/count be exposed?
5. Should semantic snapshots be pulled once per frame or cached by SceneManager?

## HUD Runtime and Validation Studio

1. Is the proposed widget set sufficient for the wireframe?
2. Are universal slot IDs granular enough?
3. Which histories should be HUD-owned?
4. Should presentation profiles be C++ or data files in v1?
5. What region-ID contract is required before the Blueprint is frozen?
6. Can multi-channel quadrant telemetry fit without a custom scene branch?

## Shared Video Playback

1. Confirm reserved media slot IDs.
2. Define status ownership and health enum compatibility.
3. Confirm whether clip playback progress is reliable for every supported backend.
4. Confirm manual-selection semantics.

## Shared Effects

1. Confirm canonical effect-ID shape.
2. Define dominant effect and transition semantics.
3. Confirm maximum simultaneous active effects.
4. Confirm whether per-channel effect assignments belong in scene metrics.

## Raspberry Pi Runtime and Performance

1. Review snapshot/vector caps.
2. Review history sample limits.
3. Review update cadence.
4. Identify any C++/STL allocation concerns.
5. Confirm whether data-file vocabulary loaded at setup is acceptable.

---

# 24. Approval Criteria

The model is ready to freeze when:

- all six scenes are representable,
- HUD Runtime can build the wireframe without scene-specific renderer branches,
- ExperienceRuntime can assemble one immutable `HudFrameData`,
- SceneManager accepts the additive semantic snapshot path,
- Video and Effects domains accept their reserved slot namespaces,
- Pi domain approves the size/cadence assumptions,
- vocabulary can change without widget-code edits,
- optional data has explicit fallback behavior,
- no raw `ofxGui` parameter enters the cinematic HUD automatically.

---

# 25. Recommended Next Actions

## Immediate

1. Send this document to HUD Runtime for presentation-model review.
2. Send it to ExperienceRuntime/SceneManager for contract compatibility review.
3. Send the performance section to Raspberry Pi Runtime.
4. Send reserved media/effects namespaces to their respective domains.
5. Collect review comments as written handoffs.

## After review

1. Revise and freeze v1.
2. Add the approved semantic types to the shared contract or adjacent shared headers.
3. Begin the wireframe HUD and Validation Studio.
4. Begin the ExperienceRuntime skeleton with fake `HudFrameData`.
5. Begin Blob and Temporal semantic-adapter planning.
6. Update the Decision Log with the frozen slot IDs and semantic ownership.

---

# 26. Architecture Decision Summary

This model establishes:

- one immutable aggregated HUD frame,
- one semantic snapshot per scene,
- stable universal slot IDs,
- namespaced scene metrics,
- centralized vocabulary,
- fixed layout with profile-driven flexible regions,
- real-data-first visualization,
- explicit ambient-data rules,
- no scene-specific branches in shared widget rendering,
- additive compatibility with the frozen Scene/HUD Contract.
