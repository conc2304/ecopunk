# Shared Effects — Canonical Activity Producer Seam Acceptance Review

**Review type:** Architecture acceptance review  
**Scope:** Canonical Shared Effects activity-producer seam only  
**Disposition:** **Requires Architecture ownership decision**

---

# 1. Executive disposition

## **Requires Architecture ownership decision**

The patch successfully proves a real, side-effect-free production snapshot accessor for Temporal Fields:

```cpp
videoeffects::EffectActivityStatus TFEffectPicker::activityStatus() const;
```

and the evidence supports that this accessor reflects the same `TFEffectPicker` state used by the rendered Temporal background effect.

However, the patch does **not** establish the frozen DEC-015 source-of-truth seam project-wide:

```text
Shared Effects owner
→ immutable EffectActivityStatus
→ ExperienceRuntime
→ HudFrameData.effects
→ HUD
```

The identified owner is:

```text
::TFEffectPicker
```

which is explicitly scene-owned and lives under:

```text
temporal-fields ofApp
→ TFBackgroundLayer
→ TFEffectPicker
```

It is not part of the shared `videoeffects` production infrastructure.

The patch also proves that:

- `videoeffects::VideoEffectService` is tooling/fixture-only today;
- Blob has no equivalent encapsulated production effect owner;
- no shared production object covering the runtime scenes was identified.

Therefore ExperienceRuntime is **not yet authorized** to treat `TFEffectPicker` as the canonical cross-domain Shared Effects owner for the final DEC-015 closure proof.

Architecture must first choose one ownership model:

1. **Per-scene production effect owners are valid canonical producers**, with scene adapters forwarding already-authored `EffectActivityStatus` snapshots without reconstructing them; or
2. **A shared production Shared Effects owner/service is required**, and scene-local owners must migrate beneath or expose themselves through that shared boundary.

No further Shared Effects coding prompt should be generated until Architecture makes that ownership decision.

---

# 2. Concrete canonical owner

## Owner identified by the patch

**Fully qualified type:**

```text
::TFEffectPicker
```

**Header:**

```text
sketches/temporal-fields/src/TFEffectPicker.h
```

**Source:**

```text
sketches/temporal-fields/src/TFEffectPicker.cpp
```

## Ownership chain

```text
temporal-fields::ofApp
    owns by value
        TFBackgroundLayer
            owns by value
                TFEffectPicker
```

## Why this is real production state

The report establishes:

```text
TFEffectPicker::update()
→ advances timer
→ may call pickNext()
→ updates currentEffect

TFEffectPicker::drawCurrent()
→ uses currentEffect + effect parameters
→ renders the actual Temporal background effect
```

Therefore `TFEffectPicker` is a genuine production state owner for Temporal Fields.

## Shared production infrastructure?

**No.**

It is scene-local:

- global namespace;
- stored under the Temporal sketch;
- owned by Temporal objects;
- not part of `videoeffects`;
- not shared by other production scenes.

This is the blocking ownership distinction.

## `videoeffects::VideoEffectService`

The patch correctly rejected it as canonical production owner.

Repository inspection found it only in:

- `shader-effect-debugger` — tooling;
- ExperienceRuntime `FakeScene` — fixture/test context.

It was not promoted merely because of its name.

---

# 3. Exact snapshot API

The exact accessor is:

```cpp
videoeffects::EffectActivityStatus activityStatus() const;
```

Declared in:

```text
sketches/temporal-fields/src/TFEffectPicker.h
```

Defined in:

```text
sketches/temporal-fields/src/TFEffectPicker.cpp
```

Confirmed properties:

- returns `EffectActivityStatus` by value;
- `const`;
- side-effect free;
- no file I/O;
- no selection;
- no randomization;
- no history/cooldown mutation;
- no evolution advancement;
- no dirty/event consumption.

The accessor performs only in-memory lookups against the active shader library and catalog registry.

**Technical API assessment:** suitable for immutable once-per-frame snapshot use.

**Architecture assessment:** owner scope remains unresolved.

---

# 4. Source-of-truth proof

## Canonical effect IDs

**Proven for Temporal.**

`effectId` comes directly from:

```text
currentEffect
```

which is the same value used by `drawCurrent()` to select the shader.

No scene/HUD compatibility string is used to reconstruct identity.

## Active slots/channels

**Proven for Temporal's background effect.**

Active status uses:

```text
slotId = "temporal_fields.background"
```

for its one background effect slot.

Raw/no-effect produces zero slots.

## Phase/evolution state

Reported:

```text
EvolutionPhase::Holding
```

This is semantically honest for `TFEffectPicker`, because it hard-switches effects and owns no interpolated transition controller.

## Transition progress

Reported:

```text
transitionProgress01 = 1.0f
```

This is honest for the same reason: no partial transition exists in this owner.

## Prominence

Reported:

```text
prominence = 1.0f
```

This is semantically valid for its single active slot and remains dominance-only.

## Effect-owned health

Health is derived from:

```cpp
shaderLib->has(currentEffect)
```

which is the same availability check used by the production draw path.

Thus:

```text
Ready
= Raw/no effect, or requested shader exists

Degraded
= selected effect is unavailable and rendering falls back
```

`Failed` is not fabricated because this owner has no distinct hard-failure state.

## Synthetic/reconstructed data

No snapshot field is derived from:

- `SceneHudStatus::activeEffects`;
- HUD data;
- FakeScene;
- Validation Studio.

The constant phase/progress/prominence values are derived from Temporal's real fixed semantics, not fixtures.

**Conclusion:** source-of-truth proof is strong for Temporal's owner only.

---

# 5. Competing-authority audit

| Path | Classification |
|---|---|
| `TFEffectPicker` | Scene-local production authority for Temporal background effect |
| Other scenes | Mixed / not yet canonicalized |
| Future scene adapters | Migration adapter only; may forward but must not reconstruct |
| `SceneHudStatus::activeEffects` | Compatibility-only |
| `shader-effect-debugger` | Tooling-only |
| Validation Studio | Tooling-only |
| ExperienceRuntime `FakeScene` | Test fixture |
| `videoeffects::VideoEffectService` | Shared helper/service used by tooling/fixtures; not production authority |
| catalog/knowledge/dominance helpers | Shared infrastructure; not current active-state owners |

The evidence does **not** establish:

```text
exactly one production activity authority project-wide
```

It establishes:

```text
one proven production activity owner for Temporal's current path
```

That is insufficient for final DEC-015 acceptance without an Architecture ownership ruling.

---

# 6. Present / missing semantics

## `std::nullopt`

ExperienceRuntime should use:

```cpp
std::nullopt
```

when no Architecture-approved canonical effect activity producer applies to the active scene.

This means:

```text
no authoritative producer
```

not:

```text
zero active effects
```

## Present-empty

When an approved canonical owner exists but currently has zero active effects:

```cpp
HudFrameData.effects = owner.activityStatus();
HudFrameData.effects->slots.empty() == true;
```

The patch proves this with Temporal's real Raw / No Effect outcome.

Health remains:

```text
Ready
```

## Zero active effects

Frozen rule:

```text
zero active effects != failure
```

These semantics are technically proven in Temporal, but ExperienceRuntime cannot apply them project-wide until the owner model is approved.

---

# 7. Health semantics

For `TFEffectPicker`, health comes from the same effect-renderer state used by the real draw path:

```cpp
shaderLib->has(currentEffect)
```

It is not inferred from:

- scene health;
- scene HUD status;
- empty slots;
- HUD state;
- ExperienceRuntime health.

Supported states for this owner:

```text
Ready
Degraded
```

`Failed` is part of the frozen type but not reachable from this owner's actual failure model.

That is acceptable; an owner should not fabricate a state it cannot observe.

---

# 8. Stability and side-effect proof

The patch provides:

```text
TFActivityStatusSelfTest: 25/25 passed
Shared Effects tests: 93/93 passed
video-effect drift check: clean
```

The self-test covers three deterministic cases:

1. Ready + present-empty Raw;
2. Ready + known-good active effect;
3. Degraded + unavailable effect.

## Repeated reads

Two calls with no update between them are compared for:

- schema version;
- health;
- message;
- slot count/order;
- slot ID;
- effect ID;
- display name;
- phase;
- transition progress;
- prominence.

**Result:** stable.

## Selection state

`getCurrentEffectName()` is unchanged before/after reads.

**Result:** no selection mutation.

## History/cooldown

`TFEffectPicker` owns no recent-history or cooldown state.

The accessor cannot mutate state that does not exist in this owner.

## Phase/progress

Unchanged across repeated reads.

## Health/load state

Unchanged across repeated reads; no load operation occurs.

## Present-empty

Stable and proven.

**Technical assessment:** sufficient for one read per frame.

---

# 9. Runtime cadence guidance

If Architecture approves the scene-local-owner model, ExperienceRuntime should capture in this order:

```text
active scene / effect-owner update completes
→ effect state for frame is finalized
→ activityStatus() called exactly once
→ HudFrameData assembled
→ HUD draws
```

For Temporal specifically:

```text
TFEffectPicker::update()
→ possible pickNext()
→ activityStatus()
→ render/presentation uses same post-update state
```

Capturing before `TFEffectPicker::update()` risks reporting the prior effect while the frame renders the newly selected effect.

If a scene adapter forwards the snapshot:

- the adapter may reach/reference the owner;
- the adapter must call the owner's accessor;
- the adapter must not synthesize `EffectActivityStatus`;
- `SceneHudStatus::activeEffects` remains compatibility-only.

If Architecture rejects scene-local producers as canonical, ExperienceRuntime must not wire this path.

---

# 10. Allocation / lifetime considerations

The accessor returns ordinary value-owned snapshot data:

```text
EffectActivityStatus
→ vector<EffectActivitySlot>
→ owned strings
```

It returns no borrowed pointers.

It creates no persistent:

- FBO;
- texture;
- shader;
- file handle;
- service;
- cache.

For `TFEffectPicker`, slot cardinality is 0 or 1, so expected per-call copying is small.

Potential per-frame allocation comes from:

- vector construction;
- slot string copies;
- display-name string copy;
- optional message storage.

Pi Runtime should measure this later. No Pi feasibility claim is implied.

---

# 11. Contract impact

## `EffectActivityStatus`

No frozen type change.

## Shared Effect Knowledge v1

No schema change.

## DEC-015

The accessor itself is additive and mechanically compatible.

However, accepting:

```text
scene-local TFEffectPicker
```

as the canonical meaning of:

```text
Shared Effects owner
```

would be an **ownership interpretation not yet established by DEC-015**.

## Canonical asset roots

No change.

## Effect ownership

This is the unresolved issue.

**Contract classification:**

```text
Architecture ownership clarification required
```

The code patch itself is not a frozen-schema change.

---

# 12. ExperienceRuntime handoff

## Current status

**Conditional — do not perform final seam proof yet.**

### Candidate Temporal owner

```text
::TFEffectPicker
```

### Exact accessor

```cpp
videoeffects::EffectActivityStatus activityStatus() const;
```

### Lifetime

Owned by Temporal's `TFBackgroundLayer`, which owns the active picker for the scene lifetime.

### Missing / present-empty

If no approved canonical owner applies:

```cpp
HudFrameData.effects = std::nullopt;
```

If approved owner applies but zero effects are active:

```cpp
HudFrameData.effects = owner.activityStatus();
HudFrameData.effects->slots.empty() == true;
```

### One-pull rule

Exactly one accessor call per frame while assembling immutable `HudFrameData`.

### ExperienceRuntime must not

- derive effect state from `SceneHudStatus::activeEffects`;
- inspect scene shader names directly;
- call `getCurrentEffectName()` and build its own status;
- infer health from scene health;
- convert empty slots to failure;
- query debugger state;
- read the knowledge pack to infer current activity;
- allow HUD Runtime to poll the owner;
- call the owner repeatedly for multiple consumers.

### ExperienceRuntime acceptance tests after ownership decision

1. owner update occurs before capture;
2. accessor called exactly once per frame;
3. returned value copied unchanged into `HudFrameData.effects`;
4. `nullopt` used only when no authoritative owner applies;
5. present-empty remains present-empty;
6. Ready-empty remains Ready;
7. canonical effect ID reaches HUD unchanged;
8. Degraded health reaches HUD unchanged;
9. `SceneHudStatus::activeEffects` cannot override canonical snapshot;
10. HUD does not poll scene/effect services directly;
11. frame assembly does not mutate owner state;
12. fixtures/tooling are absent from production source path.

---

# 13. Remaining Shared Effects work

## Blocking for ExperienceRuntime seam proof

### Architecture ownership decision

Architecture must choose:

#### Model A — scene-local canonical producers

```text
scene-local production effect owner
→ scene adapter forwards owner-produced EffectActivityStatus
→ ExperienceRuntime
```

Under this model, `TFEffectPicker` is acceptable for Temporal and no further Shared Effects snapshot patch is needed there.

#### Model B — shared production owner required

```text
shared Shared Effects owner/service
→ EffectActivityStatus
→ ExperienceRuntime
```

Under this model, this patch reached its explicit stop condition and another Shared Effects implementation increment would be required **after** Architecture defines the owner boundary.

Until Architecture chooses, ExperienceRuntime's final source-of-truth proof remains blocked.

## Non-blocking implementation work

Separate from this producer-seam decision:

- production selector adoption of canonical eligibility APIs;
- favored/weighting integration;
- recent-history/cooldown proof;
- Blob effect migration;
- Blueprint multi-instance aggregation;
- Fragment Trail fork reconciliation;
- Quadrant Crosshair fork/DebugMode migration;
- legacy JSON loader cleanup;
- global preset-ID collision hardening;
- build-system cleanup;
- Pi performance validation.

---

# 14. Final Architecture checklist

```text
[ ] exactly one production activity owner identified
[x] exact public snapshot accessor exists
[x] accessor returns frozen EffectActivityStatus
[x] accessor is const and side-effect free
[x] real rendered effect state drives snapshot
[x] canonical effect IDs proven
[x] real phase/progress proven
[x] real prominence proven
[x] effect health proven
[x] missing vs present-empty semantics proven
[ ] no competing production authority
[x] repeated reads stable
[x] one-pull-per-frame guidance documented
[ ] ExperienceRuntime handoff ready
```

The unchecked items are consequences of one unresolved question: **whether DEC-015 permits scene-local production effect owners to be canonical snapshot producers.**

The patch proves:

```text
real Temporal production owner
→ stable canonical snapshot
```

It does not prove:

```text
one project-wide Shared Effects production authority
→ ExperienceRuntime
```

---

ARCHITECTURE RECOMMENDATION: OWNERSHIP DECISION REQUIRED
