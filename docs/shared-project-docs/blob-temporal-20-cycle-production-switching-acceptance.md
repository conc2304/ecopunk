# Blob ↔ Temporal 20-Cycle Production Switching Acceptance

## Purpose

Execute the first real two-scene runtime milestone:

```text
Blob → Temporal → Blob
```

for **20 complete cycles** using the accepted production Blob and Temporal `IEcopunkScene` implementations, the accepted ExperienceRuntime/SceneManager infrastructure, the production HUD path, canonical Shared Video transport, and canonical Shared Effects transport.

This is a narrow acceptance harness session. It must not redesign runtime architecture or migrate additional scenes.

## Authoritative Inputs

- Temporal Production Migration / Two-Scene Runtime Acceptance Requirements
- Blob → Temporal → Blob HUD Acceptance Matrix
- Architecture Authorization — Temporal as Production Scene #2
- Scene/HUD Contract v1
- Shared Effect Knowledge v1 Freeze Specification
- DEC-013 through DEC-017
- Architecture Governance
- Cross-Domain Handoff Protocol

---

# 1. Execution Status in This Review Environment

The Ecopunk source repository and executable runtime are **not mounted in this environment**.

Therefore this manager review cannot truthfully report that the 20-cycle production acceptance run has already occurred.

No pass/fail result below should be treated as execution evidence until the coding/hardware environment returns the required logs, counters, screenshots, and resource observations.

Current Architecture disposition for the two-scene milestone:

```text
PENDING EXECUTION EVIDENCE
```

The remainder of this artifact is the exact acceptance session to run and the manager review gate to apply to the returned evidence.

---

# 2. Required Harness Target

Use the real production process:

```text
ExperienceRuntime
├── SceneManager
│   ├── BlobProductionScene
│   └── TemporalProductionScene
├── RuntimeServices
│   └── canonical VideoPlaybackService
├── canonical effect snapshot forwarding
└── one production HUD presentation path
```

Do not substitute:

- FakeScene;
- Validation Studio-only scene fixtures;
- scene process spawning;
- scene-local test renderers;
- HudCompositorStub;
- fixture-authored canonical effects.

Both real production scenes must be registered simultaneously.

---

# 3. Deterministic Startup

Configure the acceptance run to start in Blob.

Required proof at frame 0 / first active frame:

```text
activeSceneId == Blob canonical scene ID
Blob capabilities cached
Blob SceneHudStatus source active
Blob SceneFrame valid
HudFrameData.effects == nullopt
canonical HudFrameData.video source active
one production HUD path active
```

Record:

- registry contents;
- canonical scene IDs;
- display names;
- native render sizes;
- startup scene ID.

---

# 4. Minimum Run

Execute:

```text
20 complete Blob → Temporal → Blob cycles
```

Minimum dwell:

```text
Blob:     ≥ 60 completed active update/draw frames per activation
Temporal: ≥ 60 completed active update/draw frames per activation
```

A complete cycle is:

```text
Blob active
→ NextScene
→ static-frame transition
→ Temporal active
→ PreviousScene
→ static-frame transition
→ Blob active
```

Minimum active-scene frames:

```text
20 × 2 × 60 = 2,400
```

plus all transition frames.

Do not shorten the run because earlier single-scene lifecycle tests passed.

---

# 5. Instrumentation Required

Add or enable counters/logging for:

```text
runtimeCompletedFrameCount
sceneStatusPullCount
hudFrameDataAssemblyCount
productionHudDrawCount

blobUpdateCount
blobDrawCount
blobActivateCount
blobDeactivateCount
blobCapabilitiesQueryCount

temporalUpdateCount
temporalDrawCount
temporalActivateCount
temporalDeactivateCount
temporalCapabilitiesQueryCount

videoStatusCaptureCount
effectStatusCaptureCount

sceneFboAllocationCount
sceneFboReallocationCount
sceneFrameGeneration/frameNumber

listener/subscription counts where measurable
timer counts where measurable
decoder/history allocations
```

Record resource observations:

- RSS if available;
- total/outstanding allocations if existing instrumentation supports it;
- scene FBO dimensions;
- Temporal history allocation/capacity;
- Temporal decoder allocation state;
- playhead texture allocation state;
- Blob scratch-FBO high-water state;
- scene-owned container sizes where useful.

Instrumentation must not itself create a second scene-status/effect/video poll.

---

# 6. Per-Frame Invariants

For every ordinary active-scene frame, prove:

```text
one SceneHudStatus pull
one HudFrameData assembly
one production HUD draw
```

The active scene's capabilities must not be re-queried per frame.

Required:

```text
capabilities() == once per successful activation
```

Transition-only static-frame presentation frames may legitimately contain no new scene update/status pull. Classify those frames separately rather than distorting active-frame equality.

---

# 7. Blob → Temporal Acceptance

## Before switch

Require:

```text
activeScene == Blob
effects == nullopt
Blob status current
Blob capabilities current
canonical video snapshot current
Blob SceneFrame valid
```

## Command

Issue the real:

```text
RuntimeCommand::NextScene
```

through the production InputRouter/SceneManager path.

Do not call scene-switch internals directly in the acceptance run unless a separate unit test explicitly targets them.

## Transition

Prove ordering consistent with the accepted lifecycle:

```text
Blob final active frame
→ capture outgoing static frame
→ transition phase non-Idle
→ Blob deactivate
→ outgoing live scene ownership invalidated
→ Temporal activate
→ Temporal capabilities replace Blob capabilities
→ Temporal becomes active
→ Temporal update/status/draw
→ new Temporal SceneFrame
→ new HudFrameData
→ one HUD presentation
```

Required assertions:

- no new Blob update after Blob deactivation;
- no stale Blob capabilities after Temporal activation;
- no stale Blob semantic/profile values in first valid Temporal active frame;
- no invalid outgoing texture pointer;
- no second HUD render path;
- no SceneManager ownership ambiguity.

---

# 8. Temporal → Blob Acceptance

Issue the real:

```text
RuntimeCommand::PreviousScene
```

through the production path.

Prove inverse ordering:

```text
Temporal final active frame
→ static-frame transition
→ Temporal deactivate
→ Temporal live status/effect ownership invalidated
→ Blob activate
→ Blob capabilities recached
→ Blob update/status/draw
→ Blob SceneFrame
→ HudFrameData.effects = nullopt
→ one HUD presentation
```

Required first-Blob-frame assertions:

- no Temporal effect chips;
- no Temporal effect health;
- no Temporal pattern/state ID;
- no Temporal scene telemetry;
- no Temporal control availability;
- no Temporal specialized media/history state in HUD status;
- Blob video status remains canonical Shared Video status.

---

# 9. Effect Snapshot Transition Proof

## Blob

Required:

```cpp
HudFrameData.effects == std::nullopt;
```

This means no Architecture-approved canonical Blob producer applies.

## Temporal

Required when Temporal's canonical producer applies:

```cpp
HudFrameData.effects.has_value() == true;
```

Canonical source must remain Temporal's production effect owner and frozen `EffectActivityStatus`.

## Explicit cases

### Blob → Temporal present-empty

At least once, prove:

```text
Blob final active frame: effects = missing
Temporal active raw/no-effect frame: effects = present, slots = 0
```

These states must remain distinct.

### Temporal present-active

At least once, capture:

- canonical effect ID;
- slot ID;
- phase;
- transition progress;
- prominence;
- effect-owned health.

### Temporal → Blob

First valid Blob active frame must return to:

```text
effects = nullopt
```

with no one-frame stale Temporal effect presentation.

## Compatibility

Deliberately verify that:

```cpp
SceneHudStatus::activeEffects
```

cannot override canonical `HudFrameData.effects`.

No canonical effect reconstruction from compatibility strings is permitted.

---

# 10. Shared Video Continuity

Across all 20 cycles prove that:

```text
VideoPlaybackService
```

remains the sole media:

- catalog;
- selection;
- Previous/Next;
- session history;
- hold;
- status authority.

Required checks:

## Blob → Temporal

```text
canonical selected media
→ Temporal activates
→ specialized adapter follows currentAbsolutePath/current selection
→ Temporal dedicated decoder loads selected media
→ Temporal history resets if media identity changed
→ history refills from current selected media only
```

## Temporal → Blob

```text
Blob resumes canonical service texture/pixels
HudFrameData.video remains service-owned
Temporal history never becomes HUD media authority
```

Run at least one media Previous/Next operation during Blob dwell and one during Temporal dwell.

For Temporal verify:

```text
service selection changes
→ adapter follows
→ history clears/refills
```

No Temporal-local playlist or shuffle authority may appear.

---

# 11. Semantic / Profile / Control Clearing

At each ownership handoff, inspect the first valid incoming active `HudFrameData`.

## Blob → Temporal

Verify disappearance of Blob-specific:

- title/profile;
- primary/secondary state;
- Blob region metrics;
- Blob-specific flexible telemetry;
- unsupported Blob command controls.

Verify appearance of Temporal:

- scene title;
- Temporal state/pattern;
- Temporal activity;
- Temporal scene metrics;
- Temporal command capabilities.

Unsupported Temporal values must be absent, not zero-filled.

## Temporal → Blob

Perform the inverse validation.

No stale flexible telemetry may survive the ownership switch.

No shared HUD scene-ID branch may be added to achieve clearing.

---

# 12. Capability Replacement

On each activation:

```text
outgoing capability set invalidated
incoming capabilities queried once
incoming capabilities cached
HUD controls rebound
```

Over 20 cycles, expected minimum successful activation counts should match actual lifecycle policy.

At minimum prove:

```text
Blob capabilities queries == Blob successful activation count
Temporal capabilities queries == Temporal successful activation count
```

and not active-frame count.

Unsupported commands must:

- not be advertised;
- not appear enabled;
- return false/reject if dispatched.

---

# 13. NextScene / PreviousScene

Prove both production runtime commands:

```text
NextScene
PreviousScene
```

through the production routing path.

During non-Idle transition, repeated scene-switch commands must be ignored/rejected according to the frozen SceneManager contract.

Do not queue surprise second switches.

Record at least one deliberate attempted switch command during a transition and verify the approved suppression behavior.

---

# 14. Static-Frame Transition

For both directions prove:

- outgoing final `SceneFrame` is captured before live outgoing ownership becomes invalid;
- transition presents approved static outgoing content;
- deactivated scene is not asked to live-render for transition;
- incoming active live frame replaces static content only at the intended stage;
- no destroyed scene-owned FBO or texture pointer is retained;
- no dual-live-scene rendering is introduced.

Record manager transition phase/progress through at least one complete switch each direction.

---

# 15. GL / FBO Isolation

Required every switch:

```text
scene draw
→ SceneRenderGuard defensive baseline
→ production HUD draw
```

Verify established baseline:

- scissor disabled;
- stencil disabled;
- standard alpha blend restored;
- no stale shader bound;
- no stale manually-bound texture;
- viewport restored;
- matrices restored;
- style restored;
- expected framebuffer bound.

## Blob after Temporal

Verify:

- Blob detector/tracker output remains real and valid;
- Blob fragments render;
- Blob scratch FBO remains valid;
- no Temporal history/playhead texture leaks;
- media viewport remains correct.

## Temporal after Blob

Verify:

- Temporal history uses current canonical media;
- playheads recover according to refill policy;
- Blob scratch state does not leak;
- no stale Blob GL state remains.

---

# 16. Required Mid-TFFragmentTransition Capture

Capture at least one **real mid-`TFFragmentTransition` frame** under ExperienceRuntime.

The captured frame must come from the real Temporal production scene, not a standalone Temporal sketch/harness.

Record:

- runtime frame number;
- Temporal pattern/state;
- `TFFragmentTransition` progress;
- scene FBO native dimensions;
- window viewport;
- active framebuffer;
- SceneRenderGuard post-state;
- HUD draw count for that frame.

Verify visually and programmatically:

- HUD geometry unchanged;
- media viewport bounds unchanged;
- no viewport shift;
- no clipping/scissor leak;
- no stale shader/texture corruption;
- no framebuffer nesting corruption;
- HUD text/widgets remain correctly positioned;
- next frame remains valid.

Save screenshot/reference image and relevant state log.

This is a mandatory acceptance artifact.

---

# 17. Authored Shared Effects Preset Observation

During the natural 20-cycle run, observe whether the newly authored canonical Shared Effects preset is naturally selected by Temporal.

If it appears:

capture:

```text
canonical preset/effect identity
selection time/frame
application
rendered frame
EffectActivityStatus
```

Do not modify:

- selector weights;
- whitelist/favored status;
- compatibility;
- cooldown;
- policy

solely to force the observation.

If it does not appear naturally, report:

```text
NOT OBSERVED — NON-BLOCKING
```

This does not fail the two-scene acceptance run.

---

# 18. Resource / Allocation Acceptance

Record at least once per cycle:

- RSS where available;
- outstanding allocations;
- Blob resource high-water values;
- Temporal decoder/history high-water values;
- scene FBO allocations/reallocations.

Acceptance requires no:

```text
cycle-proportional monotonic growth
duplicate decoder accumulation
duplicate timer/listener accumulation
scene FBO multiplication
stale scene resource retention after shutdown
```

Bounded high-water retention is acceptable if explained.

Do not make Pi feasibility conclusions from this desktop run.

---

# 19. Clean Shutdown

After cycle 20:

1. leave Blob active for final validation;
2. verify:
   - Blob status valid;
   - Blob rendering valid;
   - `effects = nullopt`;
   - video status valid;
   - HUD valid;
3. invoke normal runtime shutdown;
4. verify:
   - active scene deactivated as expected;
   - `shutdown()` called under current residency/lifecycle policy;
   - no scene callbacks/timers continue;
   - decoder/history resources cleaned;
   - runtime scene FBO released;
   - HUD/runtime teardown completes without invalid access.

---

# 20. Required Results Table

Return a table with at least:

| Requirement | Result | Evidence |
|---|---|---|
| Two production scenes registered | PASS/FAIL | IDs/types/log |
| Deterministic Blob startup | PASS/FAIL | startup log |
| NextScene | PASS/FAIL | trace |
| PreviousScene | PASS/FAIL | trace |
| 20 complete cycles | PASS/FAIL | cycle log |
| ≥60 active frames/scene/activation | PASS/FAIL | counters |
| Lifecycle ordering | PASS/FAIL | trace |
| Capability replacement | PASS/FAIL | counts |
| One SceneHudStatus pull/active frame | PASS/FAIL | counters |
| One HudFrameData/presented frame | PASS/FAIL | counters |
| One HUD draw/presented frame | PASS/FAIL | counters |
| Blob effects missing | PASS/FAIL | snapshots |
| Temporal present-empty | PASS/FAIL | snapshots |
| Temporal present-active | PASS/FAIL | snapshot |
| No stale Temporal effects on Blob | PASS/FAIL | first-frame snapshot/screenshot |
| No stale scene semantics/profile | PASS/FAIL | switch matrix |
| Canonical video continuity | PASS/FAIL | service/adapter logs |
| Temporal history resync | PASS/FAIL | history log |
| GL/FBO isolation | PASS/FAIL | GL checks |
| Mid-TFFragmentTransition frame | PASS/FAIL | screenshot/log |
| Bounded resources | PASS/FAIL | trend table |
| Clean shutdown | PASS/FAIL | teardown log |
| Authored preset natural observation | OBSERVED / NOT OBSERVED | non-blocking evidence |

---

# 21. Architecture Stop Conditions

Stop and return to Architecture if execution requires changing:

- `IEcopunkScene`;
- lifecycle semantics;
- `SceneFrame`;
- `HudFrameData`;
- SceneManager ownership;
- command ownership;
- canonical Shared Video ownership;
- canonical Shared Effects ownership/type;
- frozen semantic slot IDs;
- production HUD ownership/path;
- accepted Temporal dual-decoder/history boundary.

Also stop if the harness reveals a real requirement for live dual-scene rendering rather than the approved static-frame transition.

Do not silently patch a shared contract to make the 20-cycle test pass.

---

# 22. Required Coding-Agent Return

Use the standard Cross-Domain Handoff report:

1. Summary
2. Files inspected
3. Files changed
4. Tests/builds run
5. Results
6. Deviations from prompt
7. Newly discovered risks
8. Contract changes requested
9. Recommended next step

Additionally attach or include:

- full 20-cycle lifecycle trace or summarized cycle table;
- counter totals;
- per-cycle resource table;
- screenshots for:
  - Blob normal frame;
  - transition Blob→Temporal;
  - Temporal normal frame;
  - required mid-`TFFragmentTransition`;
  - transition Temporal→Blob;
  - Blob reactivated frame;
- video/history resync log;
- effect presence transition log;
- semantic/profile stale-data matrix;
- GL/FBO validation;
- clean shutdown log;
- authored preset observation if it occurred naturally.

---

# 23. Domain-Manager Architecture Acceptance Review Gate

After the engineering return, the ExperienceRuntime manager must answer:

1. Were two real production scenes simultaneously registered?
2. Did 20 complete Blob→Temporal→Blob cycles pass?
3. Did every activation obey the approved lifecycle?
4. Were capability sets replaced exactly at activation?
5. Did scene status remain one pull per active frame?
6. Was exactly one immutable `HudFrameData` assembled per presented frame?
7. Was exactly one production HUD draw performed per presented frame?
8. Did Blob maintain `effects = nullopt`?
9. Did Temporal preserve present-empty versus present-active canonical effects?
10. Did all stale effect/semantic/profile/control/media state clear at each handoff?
11. Did canonical Shared Video remain the sole media authority?
12. Did Temporal history resynchronize correctly?
13. Did GL/FBO isolation pass both directions?
14. Did Blob and Temporal reactivate repeatedly without duplicated callbacks/resources?
15. Did the required mid-`TFFragmentTransition` ExperienceRuntime capture pass without HUD/GL/viewport corruption?
16. Were resources bounded across cycles?
17. Did clean shutdown pass?
18. Was any shared-contract change required?

## Acceptance disposition

Choose exactly one:

```text
ACCEPT — FIRST REAL TWO-SCENE MILESTONE
```

or:

```text
REJECT / HOLD — <specific failing requirement>
```

The optional authored-preset observation may be reported separately as:

```text
OBSERVED
```

or:

```text
NOT OBSERVED — NON-BLOCKING
```

---

# 24. Current Manager Return

Because the actual repository/runtime executable is unavailable in this review environment, the required production run has not been executed here.

No architecture acceptance claim can be made without the returned 20-cycle evidence.

Current disposition:

```text
HOLD — 20-CYCLE PRODUCTION EXECUTION EVIDENCE REQUIRED
```

This is not an architecture-design blocker. It is an evidence gate.

Once the exact harness above is executed against the accepted Blob and Temporal production scenes, return the coding-agent evidence for the final Architecture acceptance review.
