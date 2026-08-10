# Blob Scene Migration — First Production Scene Acceptance Review

**Date:** 2026-08-09  
**Owner:** Blob Scene Migration domain  
**Review type:** Mandatory first-production-scene Architecture checkpoint  
**Subject:** `blob-region-prototype` / `BlobProductionScene`  
**Status:** **REQUIRES NARROW BLOB PATCH**

## Purpose

Determine whether the completed Blob migration proves the first complete production `IEcopunkScene` running through the accepted Ecopunk Runtime and production HUD.

This review evaluates the coding-agent return against the accepted runtime, HUD, Shared Video, Shared Effects, lifecycle, semantic, and governance contracts. It does not treat a clean build or a continuous live run as a substitute for acceptance evidence that the migration prompt explicitly required.

## Authoritative evidence reviewed

Primary evidence:

- Architecture Acceptance — Infrastructure Convergence Complete
- Pre-Blob Cross-Domain Current-State Baseline
- Blob Scene Migration startup prompt
- Blob First Complete Production Migration — Coding-Agent Prompt
- Blob First Complete Production Migration — Completion Report
- Scene/HUD Contract v1
- HUD Semantic Slot Model v1
- Shared Effect Knowledge v1 Freeze Specification
- HUD Runtime — Final Narrow Acceptance Report
- ExperienceRuntime — Final Shared Effects Source-of-Truth Seam Review
- Decision Log, especially DEC-013, DEC-015, DEC-016, DEC-017
- Architecture Governance
- Cross-Domain Handoff Protocol

The coding-agent completion report is the source of truth for what was actually implemented and exercised in this migration. Where the report does not provide evidence for a requested property, this review marks that property partial or unverified rather than inferring it.

---

# 1. Executive disposition

## **REQUIRES NARROW BLOB PATCH**

The migration has crossed the most important implementation threshold: Blob is no longer merely an adapter that compiles. A real `BlobProductionScene` was installed into the production runtime, rendered through the runtime-owned scene target, consumed the canonical `VideoPlaybackService`, reached `HudFrameData`, and was visibly composited through the singular production HUD. `NextMedia` and `PreviousMedia` were exercised end-to-end with real media.

However, Architecture should **not yet accept Blob as the first complete production scene** because one explicit first-scene acceptance requirement remains unproven:

```text
setup
→ activate
→ update/draw
→ deactivate
→ activate
→ update/draw
→ shutdown
```

The completion report states that no Blob-specific lifecycle harness was added and that `activate()` / `deactivate()` / reactivation were not actually exercised as a repeated sequence. The approximately two-minute continuous run proves steady active operation and repeated media changes; it does **not** prove deactivation/reactivation correctness. Infrastructure convergence explicitly made repeated activate/deactivate correctness a Blob migration requirement, and the migration prompt required a repeated Blob lifecycle test with at least 20 cycles.

This is a **narrow implementation/test gap**, not an architecture defect. No frozen shared-contract change is required.

The narrow patch should also close two acceptance-evidence holes while the lifecycle harness is being added:

1. add deterministic assertions/documentation for the actual Blob semantic snapshot, including exact formulas/ranges/absence behavior for every populated universal signal and scene metric;
2. capture a paired standalone/runtime visual-parity artifact using the same media/settings so parity is reviewable rather than only described as “materially recognizable.”

Neither of those requires runtime/HUD/shared-service redesign.

The effect-activity gap is **not** a Blob acceptance blocker under current contracts: no approved Blob producer exists, so `HudFrameData.effects = std::nullopt` is the correct canonical fallback. Likewise, `Regenerate` is safer left unadvertised until curated variants exist.

---

# 2. Migration completeness

## Adapter and runtime registration

**Implemented.**

The migration added:

```text
sketches/blob-region-prototype/src/BlobSceneCore.{h,cpp}
sketches/experience_runtime/src/BlobProductionScene.{h,cpp}
```

`SceneManager` gained an internal production-scene installation path:

```text
installProductionScene(IEcopunkScene*)
activeScene()
```

and `ExperienceRuntime` gained explicit Blob installation. The non-harness `ofApp` installs Blob before runtime setup. The harness path intentionally remains on `FakeScene`.

This is a reasonable first-real-scene implementation increment. It does not alter the frozen `IEcopunkScene` contract or SceneManager ownership.

## Scene lifecycle wiring

The completion report states that `BlobProductionScene` implements lifecycle, HUD/semantic mapping, commands, and production rendering. The adapter exists and the runtime build/live launch proves `setup`, initial `activate`, `update`, and `draw` are functional.

`deactivate`, later `activate`, and final `shutdown` are implemented in code but are **not acceptance-proven as a complete sequence**.

## `nativeRenderSize()`

The adapter necessarily supplied the contract method and the real scene rendered correctly through the accepted viewport path, but the completion report does not state the exact returned dimensions or include an assertion for them.

**Assessment:** implementation present; acceptance evidence incomplete. The narrow lifecycle/integration test should assert the canonical expected native size.

## Stable identity

The production HUD resolved the pre-existing Blob presentation profile and vocabulary, including:

```text
scene.blob.state.fragmenting
scene.blob.card.label
scene.blob.metric.region_count
```

That is strong indirect evidence that the scene's canonical identity matches the accepted profile.

The completion report does not explicitly print the exact `sceneId()` or `displayName()` return strings.

**Assessment:** operationally demonstrated, but the narrow integration test should assert exact values to make the first-scene acceptance record complete.

## Capabilities

The production capability set is reported as:

```text
NextMedia
PreviousMedia
```

`Regenerate`, GUI, and debug actions are not advertised. `Reset` exists in `executeCommand()` but is not advertised.

The accepted runtime already caches capabilities on activation. Nothing in the migration reports a change to that ownership/caching rule.

## Typed commands

The migration uses the existing typed `SceneCommand` contract. `InputRouter` gained bindings:

```text
n/N → SceneCommand::NextMedia
p/P → SceneCommand::PreviousMedia
```

No string command channel or new command enum was created.

## Host ownership

Production Blob logic was extracted from standalone `ofApp` into `BlobSceneCore`, allowing the runtime adapter to drive scene behavior without depending on Blob's standalone host presentation.

The standalone `ofApp` remains as a **compatibility/developer shell** with:

- its own `VideoPlaybackService` instance for standalone execution;
- `ofxGui`;
- debug/timing presentation.

That shell is not the production runtime path and does not compete with `ExperienceRuntime` when Blob is installed there.

### Migration completeness result

**PARTIAL.**

The production structure is implemented and live, but the migration cannot be called *complete* until the required deactivate/reactivate/shutdown proof is executed.

---

# 3. Real runtime launch proof

## Required chain

The report provides real evidence for:

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

This is **not** compile-only evidence.

## Build evidence

Reported commands:

```text
cd sketches/blob-region-prototype && make Release -j4
cd sketches/experience_runtime && make Release -j4
cd sketches/experience_runtime/test && make -f Makefile.tests clean && make -f Makefile.tests test
cd sketches/blob-region-prototype/test && make -f Makefile.tests clean && make -f Makefile.tests test
```

Results:

- standalone Blob build: clean;
- ExperienceRuntime build: clean;
- existing runtime tests: run;
- existing Blob math tests: run.

The report does not provide exact test-case counts for these post-migration runs.

## Live launch evidence

Reported:

- two live windowed runs of `experience_runtime`;
- one live standalone Blob run;
- approximately two minutes of continuous production runtime observation;
- three automatic media changes with a 30-second hold duration;
- live `NextMedia`;
- live `PreviousMedia`;
- production HUD visible after Blob rendering.

## Real media evidence

Real media changed automatically and manually. The same `VideoPlaybackService` instance drove:

- Blob's rendered video;
- `VideoPlaybackStatus`;
- `HudFrameData.video`;
- HUD media identity.

A reported HUD value such as:

```text
media.auto.080b175e
```

is currently an uncurated fallback title/identifier, but it proves the data path is live.

## Frame count

**Not reported.**

The accepted HUD infrastructure separately proved long-frame behavior, but the Blob completion report does not give a Blob-active frame count. Approximately two minutes of live operation is meaningful runtime evidence, but not a substitute for the missing lifecycle sequence.

## Screenshots/parity captures

Runtime screenshots were captured. The report describes the standalone/runtime comparison as live/manual and explicitly says there was no frame-aligned or screenshot-diff parity baseline.

### Real launch result

**PASS for real runtime launch.**

The remaining rejection is lifecycle completeness, not lack of production launch.

---

# 4. Render ownership and visual parity

## Runtime target ownership

**PASS based on reported runtime behavior.**

Blob renders through `BlobProductionScene` into the target already bound by `ExperienceRuntime`. No evidence indicates Blob binds or presents the final window/default framebuffer in production.

## Final presentation ownership

**PASS.**

The production HUD remains:

```text
ExperienceRuntime
→ HudCompositorBridge
→ HudWireframeRenderer
```

No second production compositor was introduced.

## `SceneFrame`

**PASS / unchanged.**

No change to `SceneFrame` is reported. The accepted read-only ownership model remains intact.

## Competing Blob HUD

**PASS.**

The production adapter path excludes the standalone `ofxGui`/debug presentation. No competing cinematic Blob HUD is reported.

## `MediaViewportMesh`

**PASS for functional real-scene rendering.**

The live run showed correct media presentation, rounded viewport clipping, no bleed into the HUD shell, and correct composition after scene draw.

## Visual parity classification

### **unverified**

The report says the runtime result is “materially recognizable” and describes the same background cover-fit, fragment/shader treatment, and detector/tracker behavior.

That is encouraging but falls short of an acceptance-grade parity artifact. No matched standalone/runtime capture using the same media and parameter state was supplied, and no screenshot-diff baseline is available.

This does **not** imply a visible regression. It means the supplied evidence cannot honestly be upgraded to “visually equivalent.”

The narrow patch should capture a reproducible paired comparison. Pixel-perfect identity is not required.

---

# 5. GL-state isolation

## Evidence actually available

The completion report provides:

- pre-migration direct code inspection indicating Blob begin/end and push/pop usage was balanced;
- a live runtime run in which `SceneRenderGuard` remained healthy;
- no visible corruption in HUD drawing after Blob;
- correct `MediaViewportMesh` clipping;
- no observed bleed or black/garbled output.

No formal Blob-specific instrumentation counted OF FBO begin/end or shader begin/end pairs.

## FBO begin/end

**PARTIAL-PASS.** No imbalance was observed and code inspection reportedly found balanced paths.

## Shader bind/unbind

**PARTIAL-PASS.** No leaked shader state was observed in HUD output.

## Scissor/stencil

**PASS by runtime behavior.** No Blob-owned scissor/stencil leakage is reported.

## Blend state

**PASS functionally.** Fragment/background composition and HUD remained visually correct.

## Viewport

**PASS functionally.** Runtime viewport and `MediaViewportMesh` output remained correct.

## Matrix/style stacks

**PARTIAL-PASS.** No corruption was observed; no formal stack-balance instrumentation was added.

## Texture binds

**PARTIAL-PASS.** No leaked binding was reported and downstream HUD rendering remained valid.

## Internal Blob FBOs

The scene uses fragment/scratch FBO resources. The live run observed scratch FBO dimensions changing with scene content without runaway behavior.

## Overall GL isolation assessment

**PASS with evidence limitations.**

The real post-scene HUD stayed visually correct, which is the most important production symptom check. The lifecycle harness should preserve this assertion across deactivate/reactivate cycles.

---

# 6. Lifecycle proof

## Required sequence

```text
setup
→ activate
→ update/draw
→ deactivate
→ activate
→ update/draw
→ shutdown
```

## Evidence supplied

The production runtime proved:

```text
setup
→ activate
→ update/draw continuously
```

for approximately two minutes.

It also proved the scene remained stable while the canonical shared video service changed media repeatedly.

The code gained transient-state reset methods:

```text
BlobDetector::reset()
BlobTracker::reset()
VideoRegionController::reset()
```

which clear detections/tracks/fragments without changing tuning configuration.

## What was not proven

The report explicitly states:

- no automated Blob lifecycle harness was added;
- `deactivate()` was not exercised in a scripted real-scene sequence;
- a later `activate()` was not exercised after deactivation;
- repeated activation cycles were not run;
- shutdown after such cycles was not proven;
- no 20-cycle test was run.

Therefore there is no acceptance evidence yet for:

- duplicate callback/listener prevention across reactivation;
- timer duplication across reactivation;
- internal FBO validity after reactivation;
- stale tracker IDs across reactivation;
- shared video service reference validity after reactivation;
- resource retention/recreation policy across cycles;
- unbounded allocation growth caused specifically by repeated activation;
- shutdown after a deactivate/reactivate history.

## Video/effect resume/reset policy

### Video

The adapter uses the runtime-owned canonical `VideoPlaybackService`. Continuous operation proves service validity while active, but deactivation/reactivation with that reference has not been tested.

### Detection/tracking/fragments

Transient reset hooks exist and appear intentionally scoped.

### Effects

There is no canonical activity producer to resume/reset. Shader/render resources still need lifecycle exercise.

## Lifecycle result

**FAIL for the first-production-scene acceptance gate.**

This is the reason Architecture should not accept Blob yet.

---

# 7. Shared Video compliance

## Canonical service ownership

**PASS.**

The production adapter consumes the runtime-owned `VideoPlaybackService`.

The standalone compatibility sketch may instantiate its own service when run independently; that does not create a second production runtime authority.

## Canonical media root

**PASS based on preserved Shared Video migration.**

No alternate production media scan/root was reintroduced.

Canonical root remains:

```text
assets/shared/media/
```

## Playlist/shuffle/history authority

**PASS.**

No local production playlist, shuffle policy, session history, or hold authority was reintroduced.

## Previous/Next

**PASS.**

Both commands were exercised live through the real runtime path and immediately loaded the expected media.

## `HudFrameData.video`

**PASS.**

The media identity displayed in the HUD changed with Blob's rendered video, proving HUD media state remains the authoritative service snapshot.

## Blob-required pixels/texture/frame state

**PASS functionally.**

Blob detection/rendering worked with the canonical service over the live run. The report does not separately enumerate every API accessor used (`pixels`, `frameNew`, source size), so exact accessor-level assertions remain undocumented.

---

# 8. Blob effect ownership and canonical activity

## Real production execution path

The migrated scene core is reported as:

```text
ShaderLibrary
→ BlobDetector
→ BlobTracker
→ VideoRegionController
+ background VideoRegionEffectRenderer
```

Effect choice is currently represented by two bare `ofParameter<int>` / GUI-index-style values exposed through Blob core/config accessors.

Those indices feed the actual fragment/background effect execution paths, but no single Blob object owns a canonical effect-activity model with canonical IDs, activity slots, evolution/transition phase, bounded transition progress, prominence, or effect health.

## 1. What component owns Blob's active production effect state?

**No approved canonical owner exists.**

The execution configuration is split across Blob core parameters and the render/controller path. The renderers execute effects, but the current implementation has not established an object that owns canonical Shared Effects activity truth.

## 2. Does it author canonical `EffectActivityStatus`?

**No.**

## 3. Owner/accessor/slots/IDs/phase/progress/prominence/health

**Not applicable because no approved producer exists.**

## 4. Is a snapshot forwarded without reconstruction?

**Not applicable.**

No snapshot exists to forward.

## 5. Correct fallback

**PASS.**

Blob uses:

```text
HudFrameData.effects = std::nullopt
```

This correctly means no approved canonical effect-activity producer exists for this scene. It is distinct from a present-empty snapshot.

## Non-authorities

The following are explicitly **not** canonical effect-activity authorities:

- GUI effect indices;
- display/effect-name strings;
- `SceneHudStatus::activeEffects`;
- the scene adapter itself.

No reconstruction from those sources was introduced.

## Effect acceptance result

**PASS.**

This is correct contract compliance, not an acceptance blocker.

---

# 9. Semantic data quality

The accepted semantic model defines Blob candidates such as region count, fragment count, occupied area, motion, variation, and transition. The migration report proves some real semantic values end-to-end but does **not** document the complete actual mapping with formulas/cadence/ranges.

Therefore this review distinguishes proven values from unsupported assumptions.

## Proven semantic values

| Semantic / metric ID | Source state | Normalization / derivation | Cadence | Expected range | Class | Acceptance status |
|---|---|---|---|---|---|---|
| `scene.state.primary` → `scene.blob.state.fragmenting` | Real Blob tracking/fragment runtime state | Exact state-selection formula not documented | Semantic snapshot/frame path | Identifier | Literal/semantic ID | Proven live, formula undocumented |
| `scene.activity.overall` | Real Blob region/fragment counts | HUD showed 80% activity with 8 active regions; exact weighting/clamp formula not reported | Semantic snapshot/frame path | `[0,1]` by contract | Normalized | Real value proven; exact derivation unverified |
| `scene.blob.metric.region_count` | `BlobTracker` track count | Direct count | Semantic snapshot/frame path | `>= 0` | Literal | Proven live (`8`) |
| active fragment count / equivalent | `VideoRegionController::getActiveFragmentCount()` or migrated equivalent | Direct count | Expected per snapshot; exact published metric ID not stated | `>= 0` | Literal | Source exists; publication not proven |

## Required values whose actual implementation is not documented

| Concept | Acceptance finding |
|---|---|
| overall activity | Present and real, but exact formula/range test missing |
| motion | Not documented as populated; treat as **absent/unverified**, not zero |
| density / occupied area | Not documented as populated; treat as **absent/unverified** |
| variation | Not documented as populated; treat as **absent/unverified** |
| transition | Not documented as populated. Blob has no structural transition model, so absence is the expected honest behavior; test should assert actual representation |
| active region/blob count | Proven through `scene.blob.metric.region_count` |
| fragment count | Cheap source exists; report does not prove exact published ID/value |
| tracking stability | Not reported; do not assume present |

## Fake/fixture/ambient values

None are reported in the Blob production semantic proof. The live HUD values were explicitly driven by real `BlobTracker` / `VideoRegionController` state.

## Semantic acceptance result

**PARTIAL.**

The migration proves that real Blob semantics reach the production HUD. It does not meet the migration prompt's documentation/test requirement for **every implemented value**.

### Required narrow-patch closure

Add one deterministic Blob semantic snapshot test/report recording for every populated value:

- exact ID;
- exact source;
- exact formula;
- clamp/normalization;
- cadence;
- range;
- data class;
- absence behavior.

Explicitly assert absence for unsupported motion/density/variation/transition values unless the code genuinely implements them.

No new semantic IDs should be introduced merely to satisfy this review.

---

# 10. Commands and capabilities

## `SceneCommand::NextMedia`

- existing typed enum;
- delegates to canonical `VideoPlaybackService`;
- bounded/reversible;
- exercised live.

**PASS.**

## `SceneCommand::PreviousMedia`

- existing typed enum;
- delegates to canonical `VideoPlaybackService`;
- bounded/reversible;
- exercised live.

**PASS.**

## `SceneCommand::Regenerate`

- not advertised;
- no curated/proven-safe Blob variant pool exists.

**NOT APPLICABLE / deferred.**

## `SceneCommand::Reset`

- not advertised in production capabilities;
- adapter reportedly handles it internally;
- current `executeCommand(Reset)` lacks lifecycle-state enforcement.

**Assessment:** hidden correctly; lifecycle gating is a low-risk implementation issue that should be covered/fixed by the narrow lifecycle patch.

## Forbidden controls

No production capability is reported for pause, stop, restart, camera, debug, raw parameter editing, GUI visibility, fullscreen, or exit.

**PASS.**

---

# 11. Curated reseed

`Regenerate` is **not implemented/advertised**.

No safe Blob preset/range table currently exists. The coding agent correctly declined to fabricate one from raw GUI values.

## Does this block acceptance?

**No, not by itself.**

The migration prompt explicitly allowed `Regenerate` to remain unadvertised if safe implementation would require inventing effect compatibility, randomizing raw GUI parameters, or changing shared contracts.

This is safer than exposing an uncurated reseed and preserves the product rule that any visible reseed must be curated and safe.

It remains future Blob product work.

---

# 12. Camera and developer tooling

## Camera

**PASS.** The production path uses prerecorded media only.

## `ofxGui`

**PASS.** `ofxGui` remains in the standalone developer sketch and is not part of the production cinematic HUD.

## Debug/timing overlays

**PASS.** They remain developer-only and are excluded from `BlobProductionScene` production drawing.

## Standalone tooling

**PASS.** The standalone Blob app remains useful as a developer/compatibility shell and does not become a second production host or HUD.

---

# 13. HUD integration

## Scene-specific renderer branch

**PASS.** The report uses the pre-existing Blob presentation profile/vocabulary. No Blob conditional was added to shared HUD widget rendering.

## Profile binding

**PASS.** The live HUD resolved:

```text
scene.blob.state.fragmenting
scene.blob.card.label = "REGION FIELD"
scene.blob.metric.region_count
```

with real Blob data.

## Scene title

**PASS functionally.** The correct Blob profile was selected. Exact displayed scene-title text is not quoted in the completion report.

## Media title

**PARTIAL.** The slot works end-to-end, but the observed value `media.auto.080b175e` is an uncurated fallback identifier rather than a finished curated media title.

That is Shared Video/catalog metadata debt, not a Blob runtime architecture defect.

## Shared controls

**PASS for Blob's advertised controls.** Previous/next media were exercised live.

## Flexible telemetry region

**PASS for a real Blob binding.** The HUD displayed real region telemetry and normalized activity.

## Missing data

The profile system hides optional missing values and no fake canonical effect snapshot was created. A Blob-specific missing-data screenshot matrix was not supplied.

## Legacy local overlays

**PASS.** Standalone GUI/debug overlays are suppressed from the production scene path.

## HUD change classification

No shared HUD change is recommended.

- curated media title cleanup: **config/data-only / Shared Video metadata**
- richer Blob semantics: **existing-slot/profile use where possible**
- any new frozen semantic slot: **Architecture-triggering and not currently justified**

---

# 14. Performance / allocation evidence

## Desktop frame rate

**Not measured/reported for the Blob-active run.**

## Per-frame allocations

**Not instrumented for Blob.**

The report only observed scratch-FBO reallocations when fragment count/size changed.

## Long-run resource growth

Approximately two minutes of continuous activity showed no crash, no obvious runaway growth, and fragment/scratch-FBO sizes changing with content.

This does not test reactivation-induced growth.

## Activation/deactivation allocation behavior

**Not measured**, because the repeated lifecycle sequence was not run.

## Obvious leaks

None observed during the active two-minute run.

## Runtime/HUD regressions

No visible HUD/runtime regression was reported while Blob was active.

## Pi work still required

Raspberry Pi Runtime must measure on real Pi 3B+ hardware:

- sustained FPS/frame time;
- resident memory;
- thermal/throttle behavior;
- decoder + OpenCV + fragment/effect cost;
- scratch-FBO/texture residency;
- allocation behavior;
- quality headroom;
- future skinned-HUD texture budget.

No Pi feasibility conclusion is made here.

---

# 15. Tests/builds

```text
Blob standalone build:                    PASS
ExperienceRuntime build:                  PASS
existing runtime tests:                   PASS as run
existing Blob math tests:                 PASS as run
Blob production lifecycle test:           FAIL / missing
runtime integration test:                 PARTIAL
GL-state test:                            PARTIAL-PASS
Shared Video regression:                  PASS functionally
Shared Effects regression:                NOT APPLICABLE for Blob producer transport
semantic snapshot test:                   FAIL / missing
command/capability test:                  PARTIAL-PASS
canonical 1280×720 runtime screenshots:   PARTIAL
standalone/runtime parity artifact:        PARTIAL / manual only
long-frame / lifecycle soak:              PARTIAL
```

Runtime screenshots and a live standalone comparison exist, but not a matched acceptance-grade parity pair.

---

# 16. Shared-contract drift

| Contract / ownership area | Drift? | Review |
|---|---:|---|
| `IEcopunkScene` | No | Existing interface implemented |
| `SceneFrame` | No | Read-only runtime frame preserved |
| lifecycle semantics | No | Existing semantics used; proof incomplete |
| command ownership | No | Existing typed `SceneCommand` used |
| SceneManager ownership | No | Internal installation/indirection added; runtime remains owner |
| `SceneHudStatus` | No | No reported public shape change |
| semantic slot IDs | No | Existing Blob profile/IDs used |
| `HudFrameData` | No | Existing aggregate preserved |
| `VideoPlaybackStatus` | No | Canonical service/status preserved |
| Shared Effect Knowledge v1 | No | Frozen contract preserved |
| `EffectActivityStatus` | No | No Blob-local duplicate created |
| canonical HUD geometry | No | Existing wireframe/viewport preserved |
| production HUD ownership | No | Singular bridge/renderer path preserved |

`SceneManager::installProductionScene()` and `activeScene()` are runtime implementation additions required to move from a hardcoded fake scene to the first real scene. They do not change the frozen public scene contract or ownership model.

## Conclusion

**no shared-contract drift**

No Architecture contract-review trigger is required for the narrow patch.

---

# 17. Remaining Blob work

## Blocking acceptance

### 1. Prove repeated lifecycle/reactivation

Add a Blob-specific GL-capable lifecycle/integration harness that executes at minimum 20 activation/deactivation cycles and then clean shutdown.

Prove:

- no duplicate callbacks/listeners/timers;
- transient track/fragment state resets according to policy;
- no invalid shared-video reference;
- no stale texture/FBO reference;
- no monotonic internal FBO/texture/container growth;
- HUD still renders correctly after every reactivation;
- command/capability behavior remains valid;
- shutdown is clean.

### 2. Close semantic snapshot acceptance evidence

Add deterministic assertions and a short mapping table for every populated Blob semantic value.

Do not invent missing values.

### 3. Close parity evidence

Capture a matched standalone/runtime comparison with:

- same media;
- same parameter state;
- debug UI hidden;
- representative multi-region activity.

Pixel-perfect diff is optional; a reviewable paired artifact is required.

These three items can be one narrow test/evidence patch. They do not require architecture redesign.

## Non-blocking cleanup

- centralize/clean symlink-based shared-source integration before broad scene migration/Pi deployment;
- replace copied shader asset directories with a documented canonical distribution mechanism;
- clean working-tree/commit scoping;
- lifecycle-gate hidden `Reset` or return `false` when command state is invalid;
- add exact test-count reporting;
- add longer desktop soak;
- remove stale compatibility/debug assets when no longer needed;
- curate media display titles.

## Future product enhancements

- curated Blob `Regenerate` variants;
- a real Blob-local canonical effect-activity owner, if product value justifies it;
- richer motion/density/variation semantics using existing cheap state only;
- occupied-area telemetry;
- fragment-count flexible-region presentation;
- effect preset curation;
- Pi quality tuning.

---

# 18. Cross-domain handoffs

## ExperienceRuntime

**Classification:** required action for acceptance patch.

Support or host the Blob lifecycle harness in the existing GL/runtime test environment. Preserve runtime-owned FBO, one `HudFrameData`, one HUD draw, capability caching, and canonical service ownership.

## HUD Runtime

**Classification:** informational / validation support.

No HUD code change is currently required. Assist with parity capture, post-reactivation HUD correctness assertion, and flexible-region semantic verification.

## Shared Video

**Classification:** informational + non-blocking metadata action.

No Blob playback change required. Optional follow-up: replace `media.auto.<id>` fallback with curated media title metadata.

## Shared Effects

**Classification:** informational / future required action, non-blocking for current acceptance.

Record:

```text
Blob has no approved EffectActivityStatus producer.
HudFrameData.effects remains std::nullopt.
```

Future Blob effect-owner work should occur under frozen v1 semantics.

## Raspberry Pi Runtime

**Classification:** required next-milestone action after narrow acceptance patch.

Prepare real Pi profiling for:

```text
Blob + VideoPlaybackService + production HUD
```

## HUD Art Pipeline

**Classification:** informational.

Blob confirms the accepted runtime geometry can present a real opaque scene. No Blob-specific art/HUD geometry change is requested.

## Architecture

**Classification:** blocking checkpoint, not a contract proposal.

Withhold “first complete production scene accepted” until the narrow lifecycle/semantic/parity evidence patch returns.

No architecture redesign or shared-contract decision is requested.

---

# 19. Readiness for next system milestone

## A. Is Blob accepted as the first complete production scene?

**No.**

The real production chain is proven, but repeated deactivate/reactivate correctness remains unproven.

## B. Can Architecture plan the second production scene migration?

**Yes, planning can proceed; implementation should not use Blob acceptance as a completed dependency until the narrow patch passes.**

Temporal prerequisite work can continue in parallel.

## C. Is runtime ready for a real two-scene switching milestone once scene two is migrated?

**Architecturally yes; acceptance-wise not yet fully demonstrated.**

Blob must first prove its own deactivate/reactivate behavior because that is directly prerequisite to trusting it as one side of a two-scene switch.

## D. Should Raspberry Pi profiling now include real Blob + production HUD?

**Yes, after or alongside the narrow patch, but the canonical measured baseline should be taken from the accepted post-patch build.**

Exploratory Pi measurement may start now; do not freeze budgets against an unaccepted Blob lifecycle build.

---

# 20. Final acceptance matrix

```text
IEcopunkScene integration:                 PASS
real ExperienceRuntime launch:            PASS
render/FBO ownership:                     PASS
visual parity:                            PARTIAL
GL isolation:                             PASS
lifecycle/reactivation:                   FAIL
canonical video:                          PASS
canonical effect activity/fallback:       PASS
real semantic data:                       PARTIAL
commands/capabilities:                    PASS
curated reseed:                           NOT APPLICABLE
camera disabled:                          PASS
developer tooling isolated:               PASS
production HUD integration:               PASS
allocation/resource behavior:             PARTIAL
shared-contract drift:                    PASS
```

The failing lifecycle/reactivation row is acceptance-blocking. The partial semantic/parity/resource rows are small enough to close in the same narrow test/evidence patch and do not justify a broader engineering session.

ARCHITECTURE RECOMMENDATION: DO NOT ACCEPT — NARROW PATCH REQUIRED
