# Temporal Production Migration / Two-Scene Runtime Acceptance Requirements

## Purpose

Define the final ExperienceRuntime/SceneManager acceptance requirements for authorizing Temporal Fields as production scene #2 and later validating the first real two-scene runtime sequence:

```text
Blob ↔ Temporal
```

This is a manager/validation-planning artifact. It does not authorize implementation changes outside the existing shared contracts and does not contain a Temporal migration coding prompt.

## Authoritative inputs

- Architecture Reconciliation — Post-Blob / Temporal Prerequisite Wave
- Architecture Acceptance — Infrastructure Convergence Complete
- Blob Scene Migration — Final Acceptance Handoff to Architecture
- Shared Video → Architecture Engineering Handoff — Temporal Specialized Adapter Seam
- Shared Effects — Production Selector / Eligibility Increment 2 Architecture Handoff
- Scene/HUD Contract v1
- HUD Semantic Slot Model v1
- Shared Effect Knowledge v1 Freeze Specification
- DEC-013
- DEC-014
- DEC-015
- DEC-016
- DEC-017
- Architecture Governance
- Cross-Domain Handoff Protocol

## Executive disposition

ExperienceRuntime is ready to accept Temporal as the second real production `IEcopunkScene`, subject to Temporal satisfying the requirements below.

Accepted prerequisites:

```text
Blob = accepted production scene #1
Infrastructure convergence = accepted
Shared Video ordinary playback = canonical
Temporal specialized Shared Video adapter seam = accepted
Temporal effect activity producer = canonical TFEffectPicker seam
Shared Effects selector Increment 2 = accepted
Production HUD boundary = accepted
```

The remaining Shared Effects authored-preset live observation is explicitly non-blocking for Temporal migration authorization.

---

# 1. Runtime Acceptance Matrix

| Area | Blob expectation | Temporal expectation | Switch acceptance requirement |
|---|---|---|---|
| Registration | Real `BlobProductionScene` implementing `IEcopunkScene` | Real Temporal production adapter implementing `IEcopunkScene` | Both registered simultaneously in SceneManager |
| Scene ID | Stable canonical Blob ID | Stable canonical Temporal ID | IDs stable across reactivation |
| Display name | Stable Blob display name | Stable Temporal display name | HUD title switches only on active ownership |
| Startup | Deterministic configured startup | Available as second scene | No random startup |
| Native render size | Blob native size | Temporal native size | Runtime FBO follows active scene size |
| Lifecycle | Accepted reusable lifecycle | Must prove reusable lifecycle | setup once; repeat activate/deactivate; shutdown final |
| Capabilities | Cached on activation | Cached on activation | outgoing set invalidated; incoming set authoritative |
| Scene status | One pull/frame | One pull/frame | Never poll both active-scene statuses |
| HudFrameData | One immutable aggregate/frame | One immutable aggregate/frame | No mixed-scene scene data |
| Video | Canonical `VideoPlaybackStatus` | Same canonical status; Temporal decoder follows selected media | No second status/selection authority |
| Effects | `nullopt` | Present canonical `EffectActivityStatus` when producer applies | Missing/present transitions immediately and cleanly |
| HUD | One production path | Same path | Exactly one HUD draw/frame |
| Render target | Runtime-owned FBO | Runtime-owned FBO | Scene never owns shared output target |
| SceneFrame | Read-only | Read-only | No stale pointer after FBO reallocation |
| Commands | Capability-gated Blob set | Capability-gated Temporal set | Unsupported commands absent/rejected |
| Shutdown | Clean | Must prove clean | Clean shutdown after repeated switches |

---

# 2. Runtime Registration Requirements

- Register Blob and Temporal as two real production `IEcopunkScene` implementations at the same time.
- No FakeScene, validation harness, process-spawn placeholder, or scene stub may stand in for either scene in the acceptance path.
- Use a deterministic startup scene; recommended acceptance baseline is Blob.
- Each scene must provide stable `sceneId()`, `displayName()`, and `nativeRenderSize()`.
- Capabilities are queried exactly once on every successful activation and cached for that activation.
- Do not combine Blob and Temporal capabilities.

---

# 3. Lifecycle / Switch Sequence

The accepted transition model remains:

```text
last outgoing frame
→ transition/static-frame phase
→ active ownership handoff
→ first incoming frame
→ transition completion
```

No live dual-scene crossfade is required.

## Blob → Temporal

1. Blob completes its final active update/draw.
2. Runtime retains the approved outgoing static frame for transition presentation.
3. SceneManager enters a non-Idle transition phase.
4. New Blob SceneCommands are no longer accepted once deactivation starts.
5. `Blob.deactivate()` executes.
6. Blob live `SceneHudStatus`, semantics, capabilities, and scene-owned effect source are invalid as current active data after ownership handoff begins.
7. Temporal is set up once if needed.
8. `Temporal.activate()` executes.
9. Temporal becomes the active owner only after activation succeeds.
10. Temporal capabilities are queried once and cached.
11. Temporal becomes the sole active scene-status/semantic source.
12. Canonical `VideoPlaybackService` remains continuous and authoritative.
13. Temporal specialized decoder/history synchronizes to the service-selected media.
14. Temporal's canonical effect producer becomes the active effect snapshot source.
15. Temporal updates.
16. SceneManager captures Temporal `SceneHudStatus` exactly once.
17. Temporal draws into the ExperienceRuntime-owned FBO.
18. Runtime constructs a Temporal `SceneFrame`.
19. ExperienceRuntime assembles one immutable `HudFrameData`.
20. Production HUD renders exactly once.
21. Transition returns to Idle.

## Temporal → Blob

The inverse applies:

1. Temporal completes final active update/draw.
2. Runtime retains the outgoing static frame.
3. SceneManager enters transition.
4. Temporal commands stop once deactivation begins.
5. `Temporal.deactivate()`.
6. Temporal live scene status/capabilities/semantic/effect data cease being current.
7. `Blob.activate()`.
8. Blob becomes active only after activation succeeds.
9. Blob capabilities are queried once and cached.
10. Blob becomes sole active scene-status source.
11. Blob updates and status is captured once.
12. Blob draws into runtime-owned FBO.
13. Runtime creates new Blob `SceneFrame`.
14. `HudFrameData.effects` becomes `std::nullopt`.
15. No Temporal effect health/chips/IDs remain.
16. HUD draws once and transition returns Idle.

---

# 4. Ownership Transfer Rules

## Active scene ownership

SceneManager changes active ownership only after incoming activation succeeds.

Before activation succeeds:
- outgoing/static transition content may remain visible;
- `pendingSceneId` may identify the target;
- manager transition state is authoritative;
- incoming capabilities/status must not be presented as active.

After successful activation:
- `activeSceneId` becomes incoming scene;
- incoming capabilities become authoritative;
- incoming `SceneHudStatus` becomes the only active scene status;
- outgoing live semantics/profile data are invalidated.

## Outgoing status invalidation

Static visual transition content may retain the outgoing `SceneFrame`, but not stale outgoing:
- `SceneHudStatus`;
- `SceneSemanticData`;
- `SceneCapabilities`;
- flexible telemetry;
- effect snapshot source.

## Failure behavior

If incoming activation fails:
- do not publish partially initialized incoming status/capabilities;
- use existing manager failure state/message;
- preserve a valid outgoing static frame where available;
- do not fabricate a hybrid frame.

---

# 5. Two-Scene Lifecycle Proof

## Minimum repeated-switch proof

Require:

```text
20 complete Blob → Temporal → Blob cycles
```

with a minimum of:

```text
60 completed update/draw frames per scene per activation
```

This yields at least 2,400 active scene frames plus transition frames.

The 20-cycle requirement matches Blob's already accepted lifecycle scale and is large enough to expose duplication or resource-retention faults without becoming a full soak test.

## Required assertions

Across all cycles prove:
- no duplicate callbacks/listeners/timers;
- no duplicated event subscriptions;
- Blob scene work does not continue while Temporal is active;
- Temporal history/effect execution does not continue while Blob is active;
- no invalid post-deactivation commands;
- Blob reactivation resumes real detection/rendering;
- Temporal reactivation resumes real pattern/history/effect execution;
- no stale scene-owned resources;
- no invalid FBO/texture references;
- no monotonic cycle-proportional resource growth;
- clean final shutdown.

---

# 6. HudFrameData Invariants

For each ordinary active-scene frame:

```text
one SceneHudStatus pull
→ one SceneFrame
→ one HudFrameData assembly
→ one production HUD draw
```

Required instrumentation:
- scene status pulls;
- HudFrameData assemblies;
- production HUD draws;
- capability queries per activation.

Transition-only static-frame presentations may not perform a scene update/status pull; classify those frames explicitly instead of forcing false equality across transition frames.

A single `HudFrameData` must never combine:
- Blob status with Temporal capabilities;
- Temporal status with Blob capabilities;
- Blob semantics with Temporal profile;
- Temporal effect snapshot with Blob active scene.

---

# 7. Video Snapshot Transition Rules

Canonical ownership remains:

```text
VideoPlaybackService
= sole catalog / selection / session-history / Previous-Next / hold / status authority
```

`HudFrameData.video` always comes from this service.

## Blob → Temporal

```text
canonical selected media
→ Temporal activates
→ TimeOffsetPlaybackAdapter follows selected media/path
→ TimeOffsetVideoBuffer explicitly loads it
→ old Temporal history clears immediately on media identity change
→ history refills from new media only
```

Temporal's dedicated decoder/history is scene-local execution state only.

## Temporal → Blob

Temporal specialized decoder/history stops or is retained only according to scene lifecycle policy. Blob resumes consuming the canonical service texture/pixels. `HudFrameData.video` remains the service snapshot.

## Media commands

Previous/Next must use the canonical Shared Video route for both scenes.

For Temporal:

```text
service selection changes
→ adapter follows
→ history clears/refills
```

No Temporal-local playlist, index, Previous/Next history, shuffle, or hold authority.

## Cadence

Do not restore Temporal's former pattern-triggered media selection. Canonical Shared Video timing is accepted behavior.

---

# 8. Effect Snapshot Transition Rules

## Blob

Accepted:

```cpp
HudFrameData.effects = std::nullopt;
```

Meaning: no approved canonical Blob effect producer applies.

## Temporal

When its approved producer applies:

```cpp
HudFrameData.effects.has_value() == true;
```

from canonical `TFEffectPicker` activity status, forwarded without reconstruction.

## Blob → Temporal

Acceptance:

```text
last Blob active frame: effects = nullopt
first valid Temporal active frame: effects = present
```

Temporal may initially be present-empty if zero effects are active.

## Temporal → Blob

Acceptance:

```text
last Temporal active frame: effects = present
first valid Blob active frame: effects = nullopt
```

No stale Temporal:
- active effect chips;
- dominant effect;
- health;
- phase;
- transition progress;
- canonical effect labels

may remain in the first Blob active frame.

## Presence matrix

| Scene/state | Snapshot | Slots | Meaning |
|---|---|---:|---|
| Blob | missing | n/a | no approved canonical producer |
| Temporal raw/no-effect | present | 0 | authoritative producer, zero active effects |
| Temporal active | present | ≥1 | authoritative active effect state |

`SceneHudStatus::activeEffects` remains compatibility-only. Add a conflict test confirming canonical `HudFrameData.effects` wins when present.

No reconstruction from strings, shader names, HUD labels, or local indices.

---

# 9. Semantic / Profile Transition Rules

Blob's accepted semantics include:
- `activity.overall`;
- `activity.density`;
- `scene.blob.metric.region_count`;
- `scene.blob.metric.fragment_count`;
- `scene.blob.metric.occupied_area`;
- Blob primary state IDs.

Temporal must provide its approved semantic mapping through `SceneHudStatus.semantic`.

At every switch:
- outgoing semantics are invalidated;
- flexible telemetry source/cache is rebound;
- incoming semantic data becomes authoritative only after incoming update/capture;
- unsupported incoming values are absent/`nullopt`;
- unsupported values must not be zero-filled merely to clear old widgets;
- no stale scene-specific card labels or metrics remain.

No scene-ID branch may be added to shared HUD rendering.

---

# 10. Render / FBO / GL Requirements

## Runtime FBO

ExperienceRuntime remains sole owner of the scene output FBO.

If scene native sizes differ:

```text
active scene changes
→ runtime safely reallocates FBO
→ old SceneFrame texture reference invalidated
→ new SceneFrame created only after new FBO is valid
```

HUD must not retain the old texture pointer.

## Static transition frame

The outgoing static frame must use the approved runtime-owned transition/render boundary.

Not acceptable:
- drawing a deactivated outgoing scene again solely for transition;
- retaining a pointer to destroyed scene-owned FBO;
- introducing live dual-scene render/crossfade without Architecture review.

## GL isolation

For both scenes, preserve the established `SceneRenderGuard` baseline:
- scissor;
- stencil;
- blend;
- shader;
- manual texture bindings;
- viewport;
- matrices;
- style;
- framebuffer.

Scenes must still balance their own OF-managed begin/end and push/pop stacks.

## Cross-scene corruption

After Temporal → Blob:
- Blob tracker/detector/rendering valid;
- Blob scratch FBO valid;
- no Temporal playhead/history texture leaks into Blob.

After Blob → Temporal:
- Temporal history starts from canonical current media;
- six playheads become valid according to refill policy;
- no Blob fragment state leaks into Temporal.

---

# 11. Command / Capability Requirements

## Runtime commands

Require real acceptance for:
- `NextScene`;
- `PreviousScene`.

Scene-switch commands remain ignored/rejected while a transition is active under the existing contract.

## Scene capabilities

On each activation:

```text
outgoing capabilities invalidated
incoming capabilities queried once
HUD controls rebound
```

Unsupported commands:
- not advertised;
- not rendered as supported controls;
- rejected if dispatched.

## Media commands

Both scenes use canonical Shared Video Previous/Next.

## Reset / Regenerate

Do not expose Blob Reset.

Do not introduce a new Temporal command meaning. If Temporal requires a behavior that does not fit the existing typed command contract, stop and return to Architecture.

---

# 12. Temporal-Specific Prerequisites — Already Accepted

## Shared Video

Accepted:
- `VideoPlaybackService` as sole selection/catalog/history/hold/status authority;
- `TimeOffsetPlaybackAdapter` as follower;
- `TimeOffsetVideoBuffer` as dedicated decoder/history/playhead execution;
- `VideoPlaybackService::currentAbsolutePath() const` as additive canonical read-only API;
- canonical Shared Video media cadence replacing pattern-triggered local advance.

## Shared Effects

Accepted:
- `TFEffectPicker` canonical activity producer;
- frozen `EffectActivityStatus`;
- compatibility precedence;
- Unclassified exclusion;
- explicit incompatibility exclusion;
- hard blocked-preset exclusion;
- blocked overrides favored/whitelist;
- stable preset IDs;
- canonical authored pack/sync/import path.

## Non-blocking follow-up

Still desirable but not a migration gate:

```text
Temporal picker naturally selects
the newly authored canonical dither preset
→ applies it
→ renders it live
```

Do not force a permanent selector-policy change merely to create this observation.

---

# 13. Repeated-Switch Test Matrix

For each of 20 cycles:

### Blob dwell
- Blob active ID/status/capabilities correct;
- `effects = nullopt`;
- video snapshot canonical;
- real Blob activity visible;
- no Temporal updates.

### Blob → Temporal transition
- manager phase non-Idle;
- outgoing static frame valid;
- no incoming live status before activation;
- no stale Blob semantics after handoff;
- one HUD presentation;
- no invalid texture pointer.

### Temporal first active frame
- Temporal active ID;
- capabilities queried once;
- Temporal status captured once;
- Blob metrics cleared;
- video status canonical;
- effect snapshot present;
- native render size correct;
- no GL leak.

### Temporal dwell
- at least 60 active frames;
- specialized decoder/history valid;
- six playheads recover after media refill;
- effect activity canonical;
- no Blob updates.

### Temporal → Blob transition
- outgoing static frame valid;
- Temporal scene status invalidated;
- Temporal effects removed at Blob handoff;
- one HUD presentation.

### Blob reactivation
- Blob capabilities recached once;
- Blob status captured once/frame;
- `effects = nullopt`;
- no stale Temporal telemetry/effect state;
- Blob renders real activity.

### End of cycle
- allocations/resources bounded;
- no duplicated callbacks/timers/listeners.

After cycle 20:
- final Blob validation;
- clean shutdown;
- no observable active scene-owned resources after shutdown.

---

# 14. Performance / Resource Observations

Desktop evidence only; no Pi conclusions.

Record per scene:
- mean frame time;
- p95 if available;
- FPS;
- allocations/frame;
- outstanding allocation trend;
- scene-FBO dimensions;
- internal FBO/texture high-water marks.

Record switch spikes:
- activation/deactivation time;
- runtime FBO reallocation;
- video synchronization/reload;
- Temporal decoder reload;
- Temporal history clear/refill allocation;
- first playhead ready delay;
- profile/HUD rebinding cost.

Temporal-specific:
- dedicated decoder allocation;
- history resolution/capacity;
- history bytes where measurable;
- six playhead texture allocation;
- repeated activation high-water behavior.

Blob-specific:
- tracker/fragment counts;
- scratch-FBO high-water;
- fragment cap behavior.

Acceptance requires no obvious leak, cycle-proportional growth, resource multiplication, or stale resource ownership.

---

# 15. Architecture Stop Conditions

Stop and return to Architecture if migration requires:

- changing `IEcopunkScene`;
- changing lifecycle semantics;
- changing `SceneFrame`;
- changing `HudFrameData`;
- changing SceneManager active ownership semantics;
- adding scene-specific lifecycle bypasses;
- changing command enums/ownership;
- changing canonical video ownership/status/root/Previous-Next behavior;
- restoring Temporal-local playlist or pattern-triggered selection;
- changing frozen `EffectActivityStatus`;
- reconstructing canonical effect state from compatibility fields;
- adding another effect authority;
- changing effect health/prominence semantics;
- renaming/adding frozen semantic slot IDs;
- adding scene-ID-specific branches to shared HUD rendering;
- making HUD poll scenes/services directly;
- adding a second production HUD path;
- introducing live dual-scene rendering without review.

Use a written Cross-Domain Handoff proposal if any stop condition occurs.

---

# 16. Required Acceptance Evidence Package

Temporal migration completion must return:
1. two real scene registrations;
2. deterministic startup proof;
3. lifecycle trace both directions;
4. 20-cycle switch result;
5. capability query counts;
6. SceneHudStatus pull counts;
7. HudFrameData assembly count;
8. HUD draw count;
9. video ownership proof;
10. Temporal history clear/refill proof under ExperienceRuntime;
11. effect missing/present/present-empty transition proof;
12. semantic/profile stale-data clearing proof;
13. static-frame transition texture/FBO proof;
14. GL restoration result for both scenes;
15. resource/allocation trends;
16. clean shutdown;
17. builds/tests run;
18. deviations;
19. newly discovered risks;
20. contract changes requested.

Use the Cross-Domain Handoff Protocol format.

---

# 17. Final Readiness Statement

Blob is accepted. Infrastructure convergence is accepted. The Temporal Shared Video prerequisite is closed. Shared Effects is sufficiently closed for migration, with only a non-blocking live authored-preset observation pending.

No unresolved ExperienceRuntime shared-contract decision blocks beginning the Temporal production migration.

# READY FOR TEMPORAL MIGRATION AUTHORIZATION
