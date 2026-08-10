# Blob → Temporal → Blob HUD Acceptance Matrix

**Owner:** HUD Runtime & Validation Studio  
**Purpose:** Define the final HUD acceptance behavior for the first real two-scene production transition sequence.  
**Milestone:** Blob → Temporal → Blob  
**Task type:** Manager / validation planning only  
**Implementation status:** No HUD renderer change requested by this artifact  
**Readiness:** **HUD READY FOR TEMPORAL MIGRATION AUTHORIZATION**

## 1. Authoritative state

Architecture has established:

- Blob remains accepted as production scene #1.
- Blob has proven the production path:

```text
BlobProductionScene
→ SceneManager
→ ExperienceRuntime-owned scene FBO
→ SceneFrame
→ HudFrameData
→ HudCompositorBridge
→ HudWireframeRenderer
→ MediaViewportMesh
```

- Blob's accepted effect state is:

```cpp
HudFrameData.effects = std::nullopt;
```

- Temporal's Shared Video seam is accepted:

```text
VideoPlaybackService
= sole catalog / selection / session-history / Previous-Next / hold / status authority

TimeOffsetPlaybackAdapter
= follows canonical selected media

TimeOffsetVideoBuffer
= Temporal decoder/history/playhead execution only
```

- Temporal media changes follow canonical Shared Video hold/selection timing.
- Shared Effects Selector Increment 2 is accepted for Temporal migration planning.
- Temporal is expected to expose canonical `EffectActivityStatus` through `HudFrameData.effects` once migrated.
- The pending live authored-preset observation is non-blocking and does not alter HUD semantics.
- HUD Runtime is accepted: one production renderer, one immutable `HudFrameData` per frame, canonical 1280×720 geometry, profile-driven scene-specific content, canonical `effects.active`/`effects.dominant`, absent-vs-empty effect semantics, and no scene-specific renderer branches.
- The Scene/HUD transition contract remains:

```text
last outgoing frame
→ FadingOut / hold
→ Loading / scene replacement
→ first incoming frame
→ FadingIn
→ Idle
```

with transition state owned by `SceneManagerStatus`.

## 2. Acceptance principle

For every completed runtime frame:

```text
current active scene
+ current cached capabilities
+ current SceneManagerStatus
+ current VideoPlaybackStatus
+ current optional EffectActivityStatus
+ current scene semantic snapshot
→ one HudFrameData
→ one presentation profile
→ one HUD draw
```

The HUD must never solve switching by polling scenes/services directly, hard-coding scene IDs, retaining stale outgoing values, fabricating absent semantic values, or maintaining duplicate media/effect truth.

## 3. Transition-state table

| Stage | Active/pending ownership | Viewport | Identity/profile | Data expectation | Overlay |
|---|---|---|---|---|---|
| Blob steady | Blob active, manager Idle | Live Blob | Blob title/profile | Blob semantics + canonical video + `effects=nullopt` | None |
| Blob→Temporal FadingOut | Blob active, Temporal pending | Last/live Blob frame per runtime | Blob remains current | Coherent outgoing Blob aggregate; no early Temporal data | Manager FadingOut/progress/message |
| Loading | Manager replacing scene | Approved static/loading content | No fabricated incoming steady state | No mixed Blob+Temporal semantic ownership | Manager Loading |
| Temporal FadingIn | Temporal active | First valid Temporal render | Temporal title/profile | Temporal semantics + canonical video + canonical effects when valid | Manager FadingIn |
| Temporal steady, empty effects | Temporal active, Idle | Live Temporal | Temporal profile | `effects` present, `slots.empty()` | None |
| Temporal steady, active effects | Temporal active, Idle | Live Temporal | Temporal profile | Present canonical effect snapshot | None |
| Temporal media change | Temporal active | Valid Temporal output through history refill | Profile unchanged | HUD media follows canonical `VideoPlaybackStatus` only | None |
| Temporal→Blob FadingOut | Temporal active, Blob pending | Last/live Temporal frame | Temporal remains current | Temporal effects valid only while Temporal owns frame | Manager FadingOut |
| Loading | Manager replacing scene | Approved static/loading content | No mixed steady identity | Temporal semantic/effect state ceases with ownership | Manager Loading |
| Blob FadingIn | Blob active | First valid reactivated Blob frame | Blob title/profile restored | Blob semantics + video; `effects=nullopt` | Manager FadingIn |
| Blob restored | Blob active, Idle | Live Blob | Blob profile | No Temporal labels/effects/metrics remain | None |

## 4. Expected slot/profile behavior

### Blob

Accepted Blob states:

```text
scene.blob.state.analyzing
scene.blob.state.stable
scene.blob.state.fragmenting
```

Accepted universal signals:

```text
activity.overall
activity.density
```

Intentionally absent:

```text
activity.motion
activity.variation
activity.transition
```

Accepted scene metrics:

```text
scene.blob.metric.region_count
scene.blob.metric.fragment_count
scene.blob.metric.occupied_area
```

Blob acceptance requires the Blob title, current Blob state, Blob profile, and Blob flexible telemetry only. Missing optional Blob values remain missing.

### Temporal

Reserved Temporal states:

```text
scene.temporal.state.running
scene.temporal.state.transitioning
scene.temporal.state.evolving
scene.temporal.state.regenerating
```

Reserved Temporal metrics:

```text
scene.temporal.metric.pattern
scene.temporal.metric.temporal_depth
scene.temporal.metric.history_fill
scene.temporal.metric.field_activity
scene.temporal.metric.evolution_state
scene.temporal.metric.playhead_count
```

The semantic model still marks some Temporal formulas as unresolved. Therefore migration must not invent values merely to satisfy this matrix:

```text
approved migrated value exists
→ bind through existing Temporal profile

value unavailable / not honestly derivable
→ keep source missing
→ existing HUD missing-data behavior applies
```

### Profile handoff

```text
Blob profile → Temporal profile → Blob profile
```

must occur through the existing profile registry/compiler.

Pass only if no Blob label/value remains after Temporal activation and no Temporal label/value remains after Blob reactivation.

## 5. Media status

Canonical HUD media truth remains:

```text
VideoPlaybackService
→ VideoPlaybackStatus
→ HudFrameData.video
→ HUD
```

Temporal's `TimeOffsetVideoBuffer` never becomes a second HUD media source.

Across Blob and Temporal verify:

- title/identity comes only from `HudFrameData.video`;
- raw absolute file paths never appear;
- playback and hold progress remain distinct;
- media health remains distinct from scene/effect health;
- Previous/Next enablement follows canonical Shared Video status;
- Temporal pattern changes do not independently change HUD media identity;
- after a canonical Temporal media change, the HUD title/identity updates with no stale prior title;
- the specialized Temporal decoder/history may clear/refill internally without creating a second media identity.

## 6. Effect-state transition matrix

| State | Snapshot | Active slots | HUD expectation |
|---|---|---:|---|
| Blob steady | **Absent** | N/A | Effect data missing; do not pretend this is authoritative zero-effects |
| Temporal present-empty Ready | **Present** | 0 | Empty active set; health remains `Ready`; no fabricated dominant/intensity |
| Temporal active Ready | **Present** | 1+ | Complete `effects.active`; canonical `effects.dominant`; canonical health/phase/progress |
| Temporal Degraded | **Present** | 0 or 1+ | Activity derived from slots; health separately `Degraded` |
| Temporal Failed | **Present** | Contract-dependent | Health `Failed`; do not infer failure from empty slot list |
| Blob after return | **Absent** | N/A | All Temporal effect chips/dominance/health/transition state cleared |

Required invariants:

```text
effects.active
= complete canonical active effect-ID set

effects.dominant
= independent canonical deterministic dominance

effects.intensity
= absent unless an honest normalized source exists
```

`prominence` is never intensity.

On Temporal→Blob, the first Blob-owned frame with `effects=nullopt` must clear all Temporal effect presentation. `SceneHudStatus::activeEffects` must not override/merge with a present canonical sibling snapshot.

## 7. Capability/control transition matrix

| Control | Source | Acceptance |
|---|---|---|
| Previous Scene | Runtime/SceneManager | Enabled/visible only when manager is ready; ignored/disabled during transition |
| Next Scene | Runtime/SceneManager | Same |
| Previous Media | Current capabilities + canonical video | Follows active scene support and `VideoPlaybackStatus` availability |
| Next Media | Current capabilities + canonical video | Same |
| Regenerate/Reseed | Scene capability | Blob remains hidden under current accepted capability; Temporal shown only if migrated Temporal advertises the existing typed command |
| Reset | Product policy / scene command | Hidden unless separately approved |

Capabilities must be replaced/cached on incoming activation. No outgoing command descriptor may persist. Unsupported controls are hidden in production. No scene-ID control branch is allowed.

## 8. Flexible telemetry expectations

Blob may present accepted region/fragment/occupied-area and universal activity/density signals.

Temporal may present only migrated, honest values from the already-reserved Temporal semantic IDs. The HUD does not require every candidate Temporal metric to exist.

At ownership replacement, outgoing scene-specific bindings/histories must not feed the incoming profile. Missing incoming optional values use existing missing-data behavior rather than stale outgoing values.

## 9. MediaViewportMesh requirements

Canonical viewport remains:

```text
media_viewport = (25.6, 115.2, 768.0, 504.0)
corner radius = 18 px
lower-right bevel = 42 px at 45°
```

Acceptance covers:

- Blob opaque render;
- Blob outgoing/static frame;
- Temporal first frame;
- Temporal steady render;
- Temporal media change/history refill;
- Temporal outgoing/static frame;
- Blob first reactivated frame;
- Blob restored steady.

Fail for black/garbled viewport, stale scene texture, ARB sampling regression, wrong crop, changed bevel/radius, geometry shift, texture outside the mesh, or a second scene-owned viewport/HUD.

## 10. Transition overlay

Use the existing `SceneManagerStatus` model only:

```cpp
Idle
FadingOut
Loading
FadingIn
Failed
```

During FadingOut/FadingIn show the existing manager phase/progress/message presentation. During Loading show existing Loading behavior. `Failed` uses manager Failed/message. Do not encode manager transition failure as a scene health state.

## 11. Stale-state checks

| State | Blob→Temporal | Temporal→Blob |
|---|---|---|
| Scene title | Blob title disappears | Temporal title disappears |
| Primary state | Blob state disappears | Temporal state disappears |
| Profile | Blob bindings inactive | Temporal bindings inactive |
| Scene metrics | Blob region/fragment values cleared | Temporal pattern/history values cleared |
| HUD histories | No outgoing scene-specific samples in incoming history | Same |
| Media | Current canonical snapshot only | Same |
| Effects | Missing→present as Temporal owns frame | Present→missing immediately when Blob owns frame |
| Effect health | Temporal only | Cleared on Blob |
| Controls | Recomputed from Temporal capabilities/video | Blob capabilities restored |
| Flexible widget composition | Temporal profile | Blob profile |
| Scene health | Current scene only | No outgoing scene health |
| Legacy scene HUD | None | None |

Strong stale-state test: use non-zero Blob metrics, a clearly named Temporal pattern/metric, and at least one clearly named Temporal active effect. After switching back, the first stable Blob frame must contain none of those Temporal-specific values.

## 12. One-HUD-path invariants

Across the full sequence require:

```text
one completed runtime frame
→ one immutable HudFrameData
→ one production HUD update
→ one production HUD draw
```

Production path remains:

```text
ExperienceRuntime
→ HudCompositorBridge
→ HudWireframeRenderer
```

Require:

- one production renderer;
- inactive stub;
- no Blob local cinematic HUD;
- no production Temporal `TFHudLayer`/legacy HUD;
- no debug parameter panels/overlays;
- no HUD polling of scenes, SceneManager, Shared Video, or Shared Effects;
- no scene-ID branch in shared renderer/widgets;
- scene-specific composition only through presentation profiles.

## 13. Screenshot / capture matrix

All acceptance screenshots are exactly **1280×720**.

| ID | Capture | Evidence |
|---|---|---|
| B1 | Blob steady active | Blob title/state/profile/telemetry, opaque viewport, controls |
| B2 | Blob effect snapshot absent | Missing-snapshot effect behavior |
| BT1 | Blob→Temporal FadingOut | Manager overlay + coherent outgoing Blob |
| BT2 | Blob→Temporal Loading | No mixed Blob/Temporal semantics |
| BT3 | Temporal FadingIn | Temporal identity/profile + manager overlay |
| T1 | Temporal effects present-empty Ready | Empty authoritative state distinct from Blob missing |
| T2 | Temporal active effects | Complete active set, canonical dominant, health |
| T3 | Temporal effect transition | Existing phase/progress projection |
| T4 | Temporal Degraded effect health, if existing deterministic seam permits | Degraded distinct from Failed/empty |
| T5 | Temporal steady semantic telemetry | Pattern/state + at least one valid flexible presentation |
| T6 | Temporal media before change | Canonical media title/timing |
| T7 | Temporal media after canonical change | New identity, no raw path, correct controls |
| T8 | Temporal capability/control state | Controls reflect cached capability set |
| TB1 | Temporal→Blob FadingOut | Temporal coherent + manager overlay |
| TB2 | Temporal→Blob Loading | No mixed ownership |
| TB3 | Blob FadingIn | Blob profile restored; no stale Temporal effects/metrics |
| B3 | Blob restored steady | Blob semantics restored; `effects=nullopt` |
| B4 | Blob restored controls | Blob capability state restored |

If real Temporal Degraded health cannot be produced without changing production effect behavior, cross-reference the already-accepted deterministic HUD Degraded test instead of inventing a production seam. That is non-blocking.

## 14. Effect snapshot evidence

Where Temporal effects are active, record:

```text
schemaVersion
health
slot IDs
effect IDs
display names
phase
transitionProgress01
prominence
resolved effects.active
resolved effects.dominant
resolved effects.intensity presence
```

Pass when active IDs are complete, dominance is canonical and order-independent, health is Shared Effects-owned, transition semantics remain frozen, and intensity stays absent without an honest source.

The outstanding live authored-dither observation is separate and non-blocking for HUD acceptance.

## 15. Media evidence

Record before/after at least one Temporal canonical media change:

```text
mediaId/title
playback progress
hold progress/remaining
canSelectPrevious
canSelectNext
```

Never log/display raw `currentAbsolutePath()` in the visitor HUD.

A failed Temporal specialized-decoder load must not mutate canonical HUD media state.

## 16. Generic HUD harness gap assessment

**No generic HUD Runtime harness gap is identified.**

Accepted HUD infrastructure already covers:

- profile switching;
- absent/empty/active effects;
- manager transition states;
- canonical 1280×720 capture;
- MediaViewportMesh;
- missing-data behavior;
- singular HUD draw proof.

The new evidence is inherently the real Blob/Temporal integration sequence and should be gathered by the Temporal/two-scene runtime migration harness, not by another HUD architecture implementation session.

## 17. Architecture stop conditions

Stop and escalate if validation would require:

```text
new semantic slot ID
new HUD region ID
canonical geometry or MediaViewportMesh change
scene-ID condition in HudWireframeRenderer/reusable widgets
second production HUD/compositor
direct HUD polling of a scene, SceneManager, Shared Video, or Shared Effects
second media truth
second effect activity type/truth
compatibility activeEffects overriding present HudFrameData.effects
new command enum solely for Temporal HUD
raw file path in HUD
raw shader parameters in HUD
fabricated normalized Temporal metric
prominence mapped to effects.intensity
HudFrameData change solely for presentation
```

Also escalate if Temporal cannot suppress its legacy `TFHudLayer` in production without a shared-contract/lifecycle redesign. Ordinary scene-local removal/suppression during Temporal migration is expected and is not itself an Architecture change.

## 18. Final acceptance checklist

### Identity/profile
- [ ] Blob title/state/profile correct before switch.
- [ ] Temporal identity/profile replaces Blob cleanly.
- [ ] No Blob flexible labels/values remain.
- [ ] Blob profile restores cleanly.
- [ ] No Temporal labels/values remain.

### Media
- [ ] HUD media always from `HudFrameData.video`.
- [ ] No raw path displayed.
- [ ] Playback/hold remain distinct.
- [ ] Previous/Next canonical.
- [ ] Temporal media change follows Shared Video timing.
- [ ] Specialized decoder/history never becomes second HUD media truth.

### Effects
- [ ] Blob absent differs from Temporal present-empty.
- [ ] Temporal present-empty passes.
- [ ] Temporal active passes.
- [ ] `effects.active` complete.
- [ ] `effects.dominant` canonical.
- [ ] Health canonical.
- [ ] Transition semantics canonical.
- [ ] No prominence→intensity.
- [ ] No Temporal effect state after Blob return.
- [ ] Compatibility data never overrides canonical status.

### Controls
- [ ] Scene Previous/Next follows manager readiness.
- [ ] Media Previous/Next follows current video/capability state.
- [ ] Blob reseed hidden under current accepted capabilities.
- [ ] Temporal commands appear only if advertised.
- [ ] Unsupported controls hidden.
- [ ] Scene commands unavailable/rejected during transitions.
- [ ] No scene-ID branch.

### Semantics
- [ ] Blob values honest; absent values remain absent.
- [ ] Temporal values honest; optional gaps remain missing.
- [ ] Profile switch replaces flexible composition.
- [ ] Scene-specific HUD histories do not mix across ownership replacement.

### Viewport
- [ ] Blob opaque render passes.
- [ ] Outgoing/static frames pass.
- [ ] Temporal first/steady/media-change frames pass.
- [ ] Blob return passes.
- [ ] No stale/black/garbled texture.
- [ ] Rounded corners/bevel/crop unchanged.

### Transition overlay
- [ ] FadingOut/Loading/FadingIn/Failed driven by SceneManagerStatus.
- [ ] Scene health not used as manager transition state.

### Production boundary
- [ ] One immutable `HudFrameData` per frame.
- [ ] One HUD update/draw per frame.
- [ ] One renderer path.
- [ ] No Blob local HUD.
- [ ] No Temporal legacy HUD in production.
- [ ] No direct service/scene polling.
- [ ] No scene-ID renderer branch.

### Captures
- [ ] Minimum matrix completed at exactly 1280×720.
- [ ] Blob before/after captures directly comparable.
- [ ] Temporal empty/active effects captured.
- [ ] Controls before/after capability change captured.
- [ ] Optional untestable Degraded case is cross-referenced to existing deterministic HUD tests rather than fabricated.

## 19. Final readiness statement

Architecture's post-Blob prerequisite reconciliation states that Temporal migration is authorized once the ExperienceRuntime two-scene acceptance requirements and this HUD acceptance matrix are ready.

This planning review finds:

- no missing HUD architecture;
- no renderer implementation required before Temporal migration;
- no need for new semantic slots;
- no need for new transition semantics;
- no need for a new HUD-specific harness;
- sufficient accepted HUD behavior to define all required Blob → Temporal → Blob checks.

Actual pass/fail execution of this matrix occurs during Temporal migration and the first real two-scene runtime validation.

# HUD READY FOR TEMPORAL MIGRATION AUTHORIZATION
