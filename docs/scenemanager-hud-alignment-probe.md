# SceneManager ↔ HUD Initiative Alignment Report

**Role of this document**: produced by the SceneManager consolidation side, for the parallel HUD compositor/skin initiative, to converge both efforts on the same ownership boundaries before either commits production code. No code was written or modified to produce this report. Facts found directly in code or existing docs are marked as such; everything else is a recommendation or an open question, per the task's explicit instruction to keep those separated.

**Inputs treated as settled ground truth** (per the brief, and cross-checked against the repo where possible — conflicts are flagged explicitly where found): v1 scene scope is `blueprint_emergence`, `temporal-fields`, `quadrant-crosshair`, `blob-region-prototype`, `fragment-trail`; `radar-pulse`/`radar-effects-gallery` are cut from scope (product decision, not technical); `shader-effect-debugger`/`hud_validation_harness` are not scenes and never will be; `contour-portrait` is an open second-wave question, not in scope; `CrosshairSystem` is graduating to `shared/src/crosshair/` using fragment-trail's version as base (decided, not implemented); no sketch has ever run on real Pi 3B hardware; `exit()`/teardown is empty or unimplemented across every audited sketch; the video-player-wrapper standardization question is explicitly open, not decided.

---

## Executive Alignment Recommendation

**No SceneManager architecture plan currently exists in this repository.** This is a direct, verified fact, not an inference: `docs/scene-consolidation-decisions.md` itself states that "the original SceneManager consolidation engineering prompt... could not be located anywhere in this repo." What actually exists is three artifacts: `docs/scene-consolidation-probe.md` (a factual audit, explicitly not a design document — its own header states "no refactor suggestions appear anywhere below"), `docs/scene-consolidation-decisions.md` (exactly two decisions: CrosshairSystem graduation and the radar-*/scope cut — both scope/product calls, not architecture), and `docs/implement-crosshairsystem-graduation.md` (a task outline, explicitly "not started"). **No scene interface, render-ownership model, lifecycle model, or transition model has been designed anywhere in this repo prior to this report.**

Given that, most of the questions below cannot be answered as "here is what SceneManager decided" — they are answered as **grounded recommendations**, reconciled against the given scope decisions and the actual per-scene code facts already gathered in `scene-consolidation-probe.md` and the companion `docs/hud-layout-integration-probe.md`. This report's own recommendations are therefore themselves inputs for a joint decision, not a ratified plan — see §14's "Stable now?" column, which is honest that almost nothing here is yet "Committed."

**One direct conflict with the HUD probe, flagged as instructed**: `docs/hud-layout-integration-probe.md` recommends prototyping the HUD's first integration against `shader-effect-debugger`, specifically because it already exercises a caller-owned-FBO (`destinationFbo`) rendering contract cleanly. That recommendation predates — and is superseded by — the settled fact that `shader-effect-debugger` is not a scene and never will be. It was a reasonable choice under the assumption that any clean, FBO-contract-honoring sketch would do; that assumption didn't account for the product decision that this specific sketch will never be live-cycled. §12 below recommends a different first-pair of scenes that actually exercises the capability ("switching between two live scenes") that matters for SceneManager, which `shader-effect-debugger` — a single always-on effect-tuning tool — cannot prove regardless of its clean render contract.

**Most cost-effective direction**: keep both initiatives parallel, as the HUD probe itself already recommended, but ratify a small shared contract (render-target shape, status shape, command shape) *before* either side writes production code against it — not after. §15 proposes a concrete first artifact for this.

---

## Current SceneManager Plan Summary

| Artifact | What it actually contains | What it does not contain |
|---|---|---|
| `docs/scene-consolidation-probe.md` | Exhaustive, code-grounded inventory of all 11 original sketches — draw order, FBO ownership, build status, shared-infrastructure usage | Zero architecture recommendations (explicit, by its own header) |
| `docs/scene-consolidation-decisions.md` | Decision 1 (CrosshairSystem graduates to `shared/src/crosshair/`, fragment-trail's version as base, line width becomes configurable); Decision 2 (radar-pulse and radar-effects-gallery cut from SceneManager scope entirely — product call, both sketches build/run fine) | No scene interface, no render-ownership model, no lifecycle model |
| `docs/implement-crosshairsystem-graduation.md` | A task outline for Decision 1, status "outline only — not started" | Not itself part of "the SceneManager implementation" — explicitly scoped as prerequisite cleanup only |

No implementation of any SceneManager code exists in the repository as of this probe (confirmed: no `SceneManager.h`/`.cpp`, no `IScene`/`IEcopunkScene` file, anywhere under `shared/src/` or any sketch).

---

## 1. Proposed Shared Scene/HUD Contract

Evaluating the given `IEcopunkScene` example against the actual codebase, method by method:

| Method | Verdict | Basis |
|---|---|---|
| `setup(const SceneServices&)` | **Adopt shape; contents Open** | No existing sketch takes an injected services struct — each `ofApp::setup()` does everything itself. This is a clean additive shape, not a rewrite of any scene's internals; exact `SceneServices` contents depend on §10's shared-services decisions, which are themselves open. |
| `activate()` / `deactivate()` | **Adopt, and treat as required, not optional** | This lifecycle stage does not exist in any of the 5 in-scope sketches today — each only has `setup()` (once) and an empty/absent `exit()`. Given the stated fact that teardown is unimplemented everywhere despite several scenes owning per-instance FBOs (blueprint_emergence's per-fragment scratch FBO pairs, quadrant-crosshair's per-quadrant ping-pong pairs, temporal-fields' transition snapshot pair, blob-region-prototype's two scratch pairs), `deactivate()` must be real, working teardown — this is new engineering work the contract obligates, not a documentation exercise. |
| `update(float dt)` | **Adopt as-is** | Matches every in-scope sketch's existing pattern exactly — all 5 already take a `dt`-based `update()`. Zero friction here. |
| `drawToCurrentTarget()` | **Adopt for v1, with the constraint that "current target" means a native-resolution FBO the host allocates and clears — not an arbitrary caller-supplied rect** | See §3's Option A recommendation. A bare `draw(viewport)` bounds-aware contract is *not* recommended for v1 given concrete counter-evidence: `fragment-trail`'s and `quadrant-crosshair`'s `glScissor` Y-flip math is hardcoded against the current backbuffer/canvas height, not derived from a passed-in rect — making genuine bounds-aware rendering real, scene-specific work, not a free host-side parameter. |
| `nativeRenderSize()` | **Adopt, required** | Costs nothing to add and is truthful — every in-scope sketch already assumes a fixed 1280×720 canvas via either a named constant or a bare literal. |
| `hudStatus()` / `capabilities()` / `executeCommand()` | **Adopt shape; contents detailed in §7/§8** | Reasonable shape overall; the example enum itself has real gaps (see §8). |
| `reset()` | **Adopt, but must be capability-gated, not universal** | Exists cleanly in 2 of 5 scenes today (`blueprint_emergence`'s `r` → `composition.startCycle()`; `temporal-fields`' `r` → `composition.startCycle()`); absent in the other 3 (`blob-region-prototype`, `quadrant-crosshair`, `fragment-trail` have no restart/reseed command at all). Must be discoverable via `capabilities()`, exactly like every other command — see §8. |
| `shutdown()` | **Adopt, required, and treat as genuinely new work** | Must actually free FBOs and stop any timers/callbacks. Per the given ground truth, no sketch does this today even where FBOs exist that would leak. |
| Raw input handling | **Not present in the example — recommend it stay absent from the interface entirely** | See §8/§9: recommend the host/runtime owns all keyboard-to-command translation exclusively; no scene reads raw key codes once hosted. This is a design change from every current sketch (all 9 originally-surveyed sketches, including all 5 in v1 scope, implement their own `keyPressed()` today) — flagged as a recommendation, not yet decided. |

**Overall verdict**: the example interface's shape is sound and worth adopting largely as given, with three amendments: (1) make `nativeRenderSize()` + `drawToCurrentTarget()` the *only* render-target-sizing mechanism for v1 — do not add a second, arbitrary-bounds `draw(viewport)` overload yet; (2) do not add any raw-input virtual method; (3) treat `activate()`/`deactivate()`/`shutdown()` as mandatory, working teardown, not optional lifecycle hooks — this is the one place where "match the existing pattern" is explicitly wrong, because the existing pattern (empty `exit()`) is exactly what's broken.

---

## 2. Final Render Ownership & Draw-Order Diagram

Direct answers to the enumerated questions:

- **Does each scene own its output FBO?** No — recommend against this. Per-scene-owned, permanently-allocated output FBOs compound the Pi memory-residency risk flagged in §5, for no benefit v1 needs.
- **Does SceneManager own one reusable scene FBO?** Recommend SceneManager and the HUD Compositor share access to *one* host-allocated FBO (sized to the active scene's `nativeRenderSize()`), rather than each independently believing it owns a target — see the third bullet below for why this must be a single, agreed owner.
- **Does the HUD compositor own the scene FBO?** Recommend yes, structurally — the Host Runtime allocates it, but the HUD Compositor is the consumer that reads its texture, so vesting allocation/lifecycle responsibility there (not duplicated in SceneManager) avoids the two systems each assuming they control it.
- **Does the active scene draw into whatever target is currently bound?** Yes — this is `drawToCurrentTarget()`'s whole point (§1). The scene does not know or care that it's inside an FBO.
- **Who clears the scene target?** The Host, once per frame, before the active scene's `update()`/`draw()` — not the scene itself. Several scenes currently call `ofBackground(...)` inside their own `draw()` (§9's violation table) — under this model that becomes redundant, not authoritative, and should be removed from scenes during migration for cleanliness (harmless if left, since it's an unnecessary re-clear of a color that should already match).
- **Who restores viewport, matrix, scissor, blend, shader, and FBO state?** The Host/adapter layer, *after* the scene's `drawToCurrentTarget()` call returns — this is **new work that does not exist in any scene today**. No sketch was found wrapping its own `draw()` in `ofPushStyle()`/`ofPopStyle()` or an equivalent state guard; every sketch today assumes it draws directly to the window with no downstream consumer to protect.
- **Where does the final scale-and-clip operation happen?** The HUD Compositor — matches the HUD probe's own recommendation exactly (render at native resolution, then scale + mesh-clip into the viewport one layer up).

### Draw-order diagram

```
Host Runtime, once per frame:
 1. Host binds the shared outer SceneFbo (allocated at the active scene's nativeRenderSize())
 2. Host clears SceneFbo
 3. Input Router has already translated raw input into SceneCommand values this frame (§8/§9)
 4. SceneManager routes any pending commands to the active scene / itself, then calls
        activeScene->update(dt)
 5. SceneManager calls
        activeScene->drawToCurrentTarget()
    — the scene may use its own internal FBOs/scissor here; it must leave GL state
      restorable, but is not required to restore it itself (see step 6)
 6. Host/adapter layer restores viewport, matrix, scissor, blend mode, bound shader,
    and bound FBO/texture to its own known-good state — NEW responsibility, not
    provided by any scene today
 7. Host unbinds SceneFbo
 8. HUD Compositor draws SceneFbo's texture through the mesh-clip (rounded corners +
    45° bevel, per docs/hud-layout-integration-probe.md §5) into the media-viewport
    region of the final window-space composite
 9. HUD Compositor draws the L-panel chrome/skin + widgets on top, using this frame's
    pulled hudStatus() (§7)
10. Host presents the composite to the default framebuffer (the window)
```

This ownership model *defines who is responsible* for the nested-FBO-under-outer-FBO hazard (the scene's own adapter, at step 5/6) — it does not by itself prove any given scene is safe. That classification is §4's job.

---

## 3. Native Render Size vs. Arbitrary Destination Bounds

**Recommendation: Option A for v1** (native-resolution scenes; SceneManager/HUD scales and clips the result), explicitly framed as the first stage of a longer-term path toward Option C (optional, scene-by-scene bounds-awareness later) — not Option B as a v1 requirement.

**Why not Option B (mandatory bounds-aware rendering for every scene)**: concrete, already-found evidence rules this out as free. Two of the five in-scope scenes (`quadrant-crosshair`, `fragment-trail`) compute `glScissor` rectangles using a Y-flip formula hardcoded against the current canvas/backbuffer height (`canvasH - destRect.y - destRect.height`, confirmed at `FTFragment.cpp:56` and mirrored in `Quadrant.cpp`). Making this genuinely bounds-aware means threading the actual destination height through every such call site — real, scene-specific refactor work, not a host-side parameter flip.

**Effect on migration cost**: under Option A, `blob-region-prototype` (no ping-pong FBO, no scissor math) needs close to zero internal changes — a pure adapter. `blueprint_emergence`/`quadrant-crosshair`/`temporal-fields` need their internal FBO-nesting behavior verified under an *outer* host FBO (§4) but not rewritten. `quadrant-crosshair`/`fragment-trail` additionally need the scissor-height constant parameterized *only if/when* they're later asked to render at something other than their native 1280×720 — which Option A defers indefinitely, keeping v1 migration cost lower across the board.

**Effect on Pi performance**: unmeasured, and this report adds no invented number, per the same constraint the HUD probe already observed. Option A's added cost is one extra full-canvas FBO copy-and-scale per frame on top of whatever a scene's own internal FBOs already cost — flagged in the HUD probe's own Performance Risk Model as the single biggest open cost question, inherited here unresolved, not re-estimated.

---

## 4. Nested FBO and OpenGL State Safety — Per-Scene Classification

No existing document addresses this anywhere in the repo — the classification below is derived directly from `scene-consolidation-probe.md`'s per-sketch inventories and the HUD probe's render-boundary matrix, restricted to the 5 v1 scenes. `radar-pulse`/`radar-effects-gallery` are excluded per Decision 2; `contour-portrait` is excluded as out-of-v1-scope; `shader-effect-debugger`/`hud_validation_harness` are excluded as non-scenes.

| Scene | Classification | Basis |
|---|---|---|
| **blob-region-prototype** | **Directly compatible / adapter-only** | No top-level ping-pong FBO (only small, non-decaying per-region scratch crop FBOs via `VideoRegionEffectRenderer`); no scissor math found. Lowest-risk migration in v1 scope. |
| **temporal-fields** | **Requires FBO-nesting validation**, narrow exposure | `TFComposition`'s `outgoingSnapshotFbo`/`incomingRenderFbo` pair is used *only* during `PATTERN_TRANSITION` — steady-state `RUNNING` phase has no FBO capture at all, narrowing the window that needs verification relative to the other FBO-owning scenes. Best `windowResized()` coverage of the repo (propagates to 5 owned systems) reduces adapter risk elsewhere. |
| **blueprint_emergence** | **Requires FBO-nesting validation + draw-pipeline note** | `ErosionFBO` is bypassed by default (`bypassErosion=true`) but real when enabled. Its own code already documents overlay draws happening *after* the FBO closes specifically to avoid a nesting hazard — the adapter must preserve that ordering (or re-verify it's safe) once this sketch is no longer the outermost draw target itself. |
| **fragment-trail** | **Requires scissor parameterization; no FBO-nesting risk** | Uses `glScissor` + direct texture draw per fragment exclusively — no ping-pong or internal FBO found at all, so it does *not* need FBO-nesting validation. It does carry compounding, non-GL migration risk: 4 local forks of shared classes (`ShaderLibrary`, `LFOBank`, `TriggerBus`, `TimeOffsetVideoBuffer`) plus its own still-forked `CrosshairSystem` (graduation not started) — this is the most plausible reading of the given "needs rework" note, flagged here as an inference, not a directly-stated fact. |
| **quadrant-crosshair** | **Requires both FBO-nesting validation *and* scissor parameterization simultaneously** | Per-quadrant ping-pong erosion FBO pairs (`fbo_read`/`fbo_write`) *and* the same backbuffer-height-relative `glScissor` Y-flip as fragment-trail, both present at once. Highest render-boundary difficulty of the 5 v1 scenes — matches the HUD probe's independent "Moderate–High" rating for this exact sketch. |

**Which need special adapters**: `quadrant-crosshair` is the highest-effort case (both hazards, plus it's the largest global-OF-ownership violator — §9). `blueprint_emergence` and `temporal-fields` need FBO-nesting verification only. `fragment-trail` needs scissor parameterization only, compounded by unrelated local-fork cleanup. `blob-region-prototype` needs neither GL-state adapter.

---

## 5. Scene Lifecycle and Residency Model

No decision exists in the repo. Recommendations, with explicit "must be measured" call-outs per the task's constraint against inventing numbers:

- **Not all scenes constructed at startup.** Five scenes' worth of simultaneous video decoders + FBOs + shader programs resident at once is a real memory-pressure concern on a 1GB-RAM Pi 3B — but this is **unmeasured**, not established. Recommend lazy construction on first activation.
- **One next scene may be preloaded** ahead of a transition, as an explicit, opt-in capability — not required for the first working milestone.
- **Scenes are destroyed on deactivation for v1** (not retained in memory) — this is the conservative choice specifically *because* teardown is currently unimplemented and unverified everywhere (given fact). Keeping multiple scenes simultaneously live would compound an unproven teardown risk rather than reduce it; this is a risk-driven choice, not a performance-optimal one, and should be revisited once teardown is proven clean.
- **Only one scene updates/draws at a time in v1** — no simultaneous outgoing+incoming update/draw. A true live cross-fade (both scenes rendering at once) is recommended as a *later* phase, gated on first proving single-scene teardown works, not a v1 requirement.
- **Scene activation can fail**, and there is existing in-repo precedent for graceful handling worth reusing conceptually: `shader-effect-debugger`'s `switchToEffect()` already implements a "walk the whole ring once, skip broken entries, never crash" retry pattern for a failed shader compile — not a scene itself, but a directly reusable *pattern* for SceneManager to adopt when a scene fails to activate (skip to the next scene, or hold the current one, log the failure, surface it via HUD status, never crash the installation).
- **What must be measured, not invented**: peak resident memory per scene (video decode buffers + all FBOs + shader programs) on real Pi 3B+ hardware; whether the GStreamer-backed Linux video decoder actually releases previous buffers promptly on scene switch; actual teardown completeness once built (no leak-detection tooling exists anywhere in this repo today).

---

## 6. Scene Switching and Transitions

**In-process vs. process restart**: no decision exists. In-process switching between the 5 v1 scenes is **not yet buildable at all** without first resolving the real class-name collisions already documented (`ShaderLibrary`/`LFOBank`/`TriggerBus`/`GridState` each independently forked and colliding at `shared/src`'s top level across `blueprint_emergence`, `quadrant-crosshair`, and `fragment-trail`, handled today only by fragile, order-sensitive `PROJECT_EXCLUSIONS` wildcard matching per sketch). The CrosshairSystem graduation task is one instance of exactly this problem, and it is itself still "not started." Recommend in-process switching remain the end goal, with a fallback first milestone of process-boundary switching between exactly two scenes if collision resolution takes longer than expected — flagged as a fallback, not the primary plan.

**Transition ownership**: recommend the Host/HUD Compositor owns the transition *visual* (crossfade, wipe, etc., operating on the composited texture), not any individual scene — and, consistent with §5's conservative residency model, v1 transitions should be **hold-last-frame-of-outgoing → cut/fade → first-frame-of-incoming**, using the outer `SceneFbo`'s last captured texture as a still frame, not a live dual-scene crossfade. `temporal-fields`' own internal `outgoingSnapshotFbo`/`incomingRenderFbo` pattern already does exactly this "snapshot the outgoing side, don't keep it live" trick one level down, inside a single sketch — reasonable prior art to mirror one level up rather than inventing a different transition model.

**HUD stability during transitions**: recommend the HUD chrome/skin remains stable; only the media-viewport content changes. Skin and scene are orthogonal by design (matches the HUD probe's own framing).

**Input suppression during transitions**: recommend yes — reject/ignore commands mid-transition (ties to §8). `temporal-fields` already demonstrates the cost of *not* doing this: forcing a pattern switch mid-transition re-enters `switchToPattern()` against an already-mid-transition frame, a real (if non-crashing) visual edge case documented in the HUD probe.

**Minimum state the HUD needs for transition progress**: an `Idle`/`Transitioning` enum plus a `[0,1]` progress float — deliberately minimal.

**Direct answer to the explicit question — can the HUD safely expose previous/next scene in its first release?** **No.** This must stay disabled. In-process switching is not yet buildable (collision resolution incomplete), and even the process-restart fallback has no implementation, decision, or owner yet. This is a **Blocked**, not merely **Deferred**, item — see §14.

---

## 7. Read-Only HUD Status Contract

Evaluated against the 5 v1 scenes specifically (the earlier per-sketch accessor inventory covered all 9 originally-surveyed sketches; this narrows it and adds v1-specific gaps not previously surfaced).

- **Universal**: `sceneId`/`displayName` (trivial to add, doesn't exist as a field today but costs nothing); `fps` (already universal and safe — `ofGetFrameRate()` is a global, not sketch state).
- **`paused`**: **not** recommended as universal. True pause/resume, in the general "pause the whole scene" sense the field implies, exists in **none** of the 5 v1 scenes today (the only two sketches with anything pause-like in the wider 9-sketch survey — `shader-effect-debugger`, `temporal-fields`' timeline-scoped pause — are either out of scope entirely or narrower than the field suggests). Recommend optional, default `false`, not a field every scene must honor.
- **`mediaName`**: exists via a real public getter in `blueprint_emergence` (though `VideoSampler` itself lacks a filename accessor — a genuine gap, not yet built), `temporal-fields` (`TimeOffsetVideoBuffer::getCurrentMediaFilename()`), and `quadrant-crosshair` (`VideoSystem::getCurrentFilename()`). **`blob-region-prototype` and `fragment-trail` were not confirmed to expose an equivalent accessor in prior passes — mark Open/Blocked by code investigation for these two, not assumed present.**
- **`modeName`/`activeEffects`**: partial. `quadrant-crosshair`'s `QuadrantManager::getTelemetry()` gives per-quadrant active shader name; `temporal-fields` gives active pattern type/name; `blueprint_emergence` gives cycle mode/phase. `blob-region-prototype` and `fragment-trail` have GUI-parameter-driven "current effect" state that is currently private — needs new getters, not free.
- **`progress`**: `temporal-fields` has real phase-elapsed/transition timing; `blueprint_emergence` has phase state. The other three have no obvious `[0,1]` concept today — keep strictly optional.
- **`motionEnergy`**: `MotionExtraction` is used by `blueprint_emergence`, `temporal-fields`, and `quadrant-crosshair` (each via their own fork or shared instance), but none was confirmed to expose a clean `[0,1]` "energy" getter — this is new-getter work across the board, not a free read.
- **`audioEnergy`**: **recommend dropping this field for v1 entirely**, not just marking it optional. No sketch in the repo has any audio input at all (confirmed: no `ofSoundStream`/audio-analysis code found anywhere during the HUD probe). Carrying a permanently-empty optional field implies a capability that doesn't exist anywhere in this codebase.
- **`interactionEnergy`**: same recommendation, same reason — no sketch has meaningful pointer/physical interaction input (no functioning mouse handlers, no GPIO/OSC/MQTT anywhere). Drop for v1.
- **`activeItemCount`**: exists, and is semantically different per scene (blob-region-prototype: tracked-region count; fragment-trail: active fragment count; blueprint_emergence: fragment count) — keep as one generic optional int exactly as the example already frames it, not several named fields per sketch-specific concept.
- **A real polling hazard, found and worth restating here**: `quadrant-crosshair`'s `VideoSystem::fileChanged()` clears its own internal flag on read — it must **never** be exposed directly to the HUD. Recommend the HUD not need a "did it just change" signal at all: since `mediaName` is a plain string, the HUD can detect a change itself by diffing the value frame-to-frame, which is strictly non-destructive and requires **no** new scene-side getter.
- **Who takes the snapshot**: the scene itself, building and returning its own `hudStatus()` struct **by value** each call — matching the already-proven `getState()`-by-value pattern several scenes already use internally (e.g. `BEComposition::getState()`). SceneManager/HUD never reaches into scene internals directly, satisfying the task's explicit constraint against that.
- **Push, pull, or event-backed?** **Pull**, called once per frame by the host after `update()` — every existing "status-shaped" accessor in this codebase today is pull-based; none is event/callback-driven at this specific layer (even though scenes use internal callbacks for their *own* logic, e.g. `TFComposition::onPatternChanged`).
- **Opaque `statusLines`**: yes, recommend exactly as the task frames it — sketch-specific rows stay opaque strings, never modeled as first-class typed fields.

---

## 8. Command and Capability Contract

**SceneManager-owned vs. scene-owned, and a real gap in the example enum**: `PreviousScene`/`NextScene` are not in the given `SceneCommand` enum at all. These belong to SceneManager, not `executeCommand()` on any individual scene, and per §6 must stay unsupported/hidden in v1 pending the in-process-switching prerequisite.

**Scene-owned commands, checked against actual v1-scope capability data** (restated and narrowed from the full 9-sketch survey in the companion HUD probe):

| Command | Present in v1 scope | Detail |
|---|---|---|
| `PauseResume` | **None of the 5** | The only pause-like concepts in the wider 9-sketch survey (`shader-effect-debugger`, `temporal-fields`' timeline-scoped pause) are either out of scope or narrower than a general scene pause. `capabilities()` must not claim this for any v1 scene today. |
| `Regenerate`/`Reset` | 2 of 5 (`blueprint_emergence`, `temporal-fields`, both `r` → `startCycle()`) | Absent in `blob-region-prototype`, `quadrant-crosshair`, `fragment-trail`. |
| `NextMedia`/`PreviousMedia` | Sparsest of the set — `quadrant-crosshair` has `NextMedia` only (`n`/`N`), no `Previous` | `blueprint_emergence`/`temporal-fields` advance media only as a *side effect* of pattern-switch logic, not as a standalone command; `blob-region-prototype`/`fragment-trail` have no such command at all. |
| `NextMode`/`PreviousMode` | **Semantic mapping gap, not just a missing-command gap** | `blueprint_emergence` has no direct mode command; `quadrant-crosshair` has crosshair-preset cycling (`1`-`5`/`TAB`) that loosely maps to `NextMode` but has no clean `Previous`; `temporal-fields` has `forceNextPattern`/TAB-preset-load with no clean previous either; `fragment-trail` has direct `1`/`2`/`3` mode-select plus a separate `m`-key strategy cycle that doesn't fit a simple next/previous pair at all. None of these map cleanly 1:1 onto a generic `NextMode`/`PreviousMode` pair. **Recommend this specific pairing stay Open**, pending a closer per-scene design pass, rather than baking in an enum that most scenes would have to awkwardly approximate. |
| `ToggleAutoAdvance` | **None found as a direct player-facing toggle** | `temporal-fields`' closest analogue (`composition.setAutoCycleSuspended()`) is currently driven internally by whether a timeline preset is active, not exposed as a standalone toggle. Open — needs a new getter/setter if wanted. |

**Unsupported controls: hidden, not disabled** — matches both the background brief and the HUD probe's own identical recommendation. A visible-but-disabled control implies "might work later," which isn't accurate for e.g. `PauseResume` on `blob-region-prototype` today.

**Command rejection during transitions**: recommend SceneManager-level commands (`Previous`/`NextScene`) are silently ignored (not queued) while mid-transition; active-scene commands route through, but the scene's own `executeCommand()` is responsible for its own internal guards. **At least one such guard is unverified today**, carried forward unresolved from the HUD probe: `quadrant-crosshair`'s `ExpansionDirector::trigger()`/`triggerQuadrant()` is called unconditionally from `keyPressed` with no visible re-entrancy guard at the call site — whether `ExpansionDirector` itself guards internally needs a direct read of `ExpansionDirector.cpp`, not yet done in either probe.

**Immediate vs. queued**: recommend immediate execution only for v1 — no command queue. Matches how every existing `keyPressed()` handler in the codebase already behaves (synchronous).

**Should keyboard input be translated into commands by the runtime, rather than handled inside each scene?** **Recommend yes.** This directly resolves §1's flagged gap (no raw-input method on the scene interface) and is a real design change from today's code — all 9 originally-surveyed sketches, including all 5 in v1 scope, currently implement their own `keyPressed()`. Recommended, not yet decided.

**Sketch-local private methods needing thin public adapters** (v1 scope only, from the HUD probe's direct findings): `quadrant-crosshair`'s `DebugMode::active` is a public field on `DebugMode`, but reached only through `ofApp`'s **private** `debug` member — needs a thin public wrapper. `blob-region-prototype`'s `RollingAverage` timers and its GUI `ofParameter<T>`s are private `ofApp` members — need new public accessors if surfaced via `hudStatus()`. The other three v1 scenes (`blueprint_emergence`, `temporal-fields`, `fragment-trail`) route their meaningful commands through already-public methods on owned objects — lower adapter cost on this specific axis.

---

## 9. Global openFrameworks Ownership

**Recommended rule set** (not yet approved):

1. Only the Host configures the window: size, fullscreen, title, vsync, target frame rate.
2. Scenes may not call `ofExit()`, `ofToggleFullscreen()`, `ofHideCursor()`, `ofSetFrameRate()`, or `ofSetVerticalSync()` — these become host-exclusive.
3. Scenes may *request* exit/fullscreen-toggle/etc. only via a runtime command — **and the given `SceneCommand` enum has no such host-level actions in it at all**, a real gap in the example worth flagging: it only lists scene-action commands, not host-action ones.
4. `ofDisableArbTex()` is called exactly once, by the Host, before any scene is constructed.
5. Background clearing is the Host's responsibility each frame (§2); scenes should stop asserting their own `ofBackground()` during migration (a redundant scene-side call is not a hard failure, just clutter to retire).
6. Blend mode, scissor state, matrix stack, and bound FBO/shader/texture are restored by the scene-hosting adapter after each scene's draw call returns — new work, per §2/§4.
7. Global keyboard input is captured only by the Host/runtime and translated into `SceneCommand` values before reaching any scene (§8) — recommended, not decided.

**Violation table, v1 scope only** (direct facts from `scene-consolidation-probe.md`'s per-sketch `setup()`/`keyPressed()` inventories):

| Scene | `ofSetFrameRate` | `ofSetVerticalSync` | `ofHideCursor` | `ofToggleFullscreen` | `ofExit` | Per-frame `ofBackground` |
|---|---|---|---|---|---|---|
| blueprint_emergence | Yes (24) | No | No | No | No | Not confirmed in available notes |
| blob-region-prototype | No (vsync-limited only) | Yes (true) | No | No | No | Not confirmed |
| **quadrant-crosshair** | **Yes (24)** | **Yes (true)** | **Yes** | **Yes (`f`/`F`)** | **Yes (`ESC`)** | Background clear happens every `draw()` call per the probe's own description |
| temporal-fields | Yes (24) | No | No | No | No | Yes (`GROUND_DARK`, every frame) |
| fragment-trail | Yes (24, "Pi 3B render target" comment) | Yes (true) | No | No | No | Yes (`8,10,9`, every frame) |

**`quadrant-crosshair` is the single largest violator of the seven listed rules** (5 of 6 columns), consistent with it also carrying the highest GL-state risk in §4 — this compounds, rather than offsets, its migration difficulty. `ofExit()` bound directly to a keypress is the clearest and most serious individual violation found anywhere in v1 scope: a scene must never be able to terminate the host process.

---

## 10. Shared Services

| Service | Current owner | Target owner | Migration timing | HUD dependency | Centralize for v1? |
|---|---|---|---|---|---|
| Video player/media playlist | Per-scene, 4 different wrapper shapes (`VideoSampler`/blueprint_emergence, `TimeOffsetVideoBuffer`/blob-region-prototype+temporal-fields already shared, `VideoSystem`/quadrant-crosshair local fork, forked `TimeOffsetVideoBuffer`/fragment-trail) | **Open — explicitly not decided per the given ground truth.** Leading recommendation: standardize on one shared wrapper (likely `TimeOffsetVideoBuffer`, already genuinely shared across 2 of 5 v1 scenes) | Real behavior-preserving migration per scene, not a mechanical rename — `VideoSampler`'s live-continuous-crop semantics and `VideoSystem`'s playlist-with-loop-counts semantics are not drop-in interchangeable | Only indirectly — the HUD needs each scene to expose `mediaName` via `hudStatus()` (§7), which does **not** require player unification first. **Recommend not blocking §7's `mediaName` field on this decision landing.** | No |
| Video effect service | Already `shared/src/video-effects/`; migration status varies (blueprint_emergence/temporal-fields/blob-region-prototype: migrated; quadrant-crosshair: production path only; fragment-trail: deliberately excluded, local `ShaderLibrary` fork) | Already correct owner | N/A for 4 of 5 scenes; `fragment-trail`'s fork is part of its "needs rework" status | Yes, for `activeEffects` (§7) — needs a per-scene getter regardless of which service instance backs it | Already done where it matters |
| Audio input | None — doesn't exist | N/A | N/A | No | No |
| Sensor input | None — doesn't exist (no GPIO/OSC/MQTT/serial anywhere) | N/A | N/A | No | No |
| Random seeds | Per-scene (`ofRandom()` global state, each composition's own seed field) | **Open** | N/A | No known dependency | **No — avoid centralizing without a proven need**, per the task's own instruction against neatness-only centralization |
| Asset-path resolver | Per-sketch, ad hoc (see §11) | Host-owned | Should happen alongside §11's recommendation | Yes, indirectly (skin-package storage should follow the same convention) | Recommended yes |
| Performance instrumentation | None centrally — `blob-region-prototype` and (out-of-scope) `radar-pulse` independently hand-rolled the identical `RollingAverage`/timing-bracket pattern with zero shared code | `shared/src/PerfInstrumentation.h` | **Can happen now, independent of everything else** — a low-risk extraction of already-working code, not a new design | Yes — directly serves both initiatives' validation plans | **Yes — one of the few clean "centralize now" recommendations in this report** |
| System telemetry (Pi thermal/memory) | None — doesn't exist anywhere | New, host-owned once built | Not required for this report's architecture decisions, but required before any Pi go/no-go number is trustworthy | Indirectly | Not required for v1 architecture; required before Pi validation is meaningful |
| Logging | Scattered `ofLogNotice`/`ofLogError`, no central sink beyond stdout | No strong evidence a central service is needed | Low priority | No | No |
| Preset persistence | Scene-local (`temporal-fields`' `TFPresetTimeline`/`TFParameterPanel`; no other v1 scene has a preset concept) | **Scene-local — do not centralize** | N/A | No | No — this is scene-specific content, not a cross-scene concern |
| HUD status aggregation | N/A | HUD Compositor itself | N/A | This *is* the HUD's own responsibility | Inherently HUD-side, not a SceneManager-owned service — flagged to avoid scope creep in the wrong direction |

---

## 11. Asset and Data-Path Resolution

**Current state** (facts): each sketch resolves assets via `ofToDataPath(...)` relative to its own `bin/data/` by default. Several scenes use ad hoc symlinked shared-media directories rather than a real shared-asset mechanism (`contour-portrait`'s `bin/data/sharedMedia` → `blueprint_emergence`'s media; `fragment-trail`'s per-file symlinks into `blueprint_emergence`'s media — itself documented in `fragment-trail`'s own `HANDOFF.md` as a workaround, not a design, forced by an `rmdir` permission issue). Hardcoded `/home/pi/blueprint/media/` literals exist in 4 sketches total (per the HUD probe's build-blocker findings); of those, `quadrant-crosshair` and `blueprint_emergence` (plus its orphaned `BESettings_presets.h`) are **in** v1 scope, `radar-effects-gallery`/`radar-pulse` are now out of scope entirely. All are confirmed dead code today, since `PLATFORM_PI` is never defined by any build file (given fact).

**Recommendation — a deterministic scene asset-root API**: each scene receives one or more resolved paths (`mediaRoot`, `presetRoot`, `knowledgeRoot`) via `SceneServices` at `setup()` time, resolved **once** by the Host based on dev-vs-Pi environment detection — replacing today's per-sketch `#ifdef PLATFORM_PI`/hardcoded-absolute-path pattern entirely rather than leaving it to bit-rot further.

**Shared media**: recommend one canonical shared-media directory, resolved by the Host — scenes never hardcode a path into a sibling sketch's `bin/data/` folder, replacing today's ad hoc symlink pattern.

**Collision avoidance**: recommend each scene's resolved asset root already be scene-namespaced (e.g. `assets/scenes/<sceneId>/`), so two scenes can each own a `preset.json` without colliding — this mirrors `shared/src/video-effects/`'s existing per-sketch `bin/data/shared-video-effects/` sync convention, which already avoids exactly this collision class.

**Knowledge-base files**: the underlying `shared/src/video-effects/knowledge/` service is used indirectly by in-scope scenes via the effect catalog, but no v1 scene exercises its whitelist/blacklist write path today (confirmed: the directory doesn't exist on disk in this checkout). No action required for v1.

**Dev vs. Pi paths**: this recommendation can be *designed* now but cannot be truly validated until a Pi build path exists at all — the same "no Pi build path exists" blocker the HUD probe already documented applies identically here, not a new problem this report introduces.

---

## 12. Supported-Scene Scope and Migration Order

| Scene | Migration phase | Adapter/refactor strategy | Major blocker | Render-boundary difficulty | Status-contract work | Command-contract work | Relative risk |
|---|---|---|---|---|---|---|---|
| **blob-region-prototype** | Phase 1 | Thin native-resolution adapter, minimal internal changes | Generic teardown-building only | Low | Needs new getters (`RollingAverage` timers, GUI params are private today), but underlying state is simple | No `Reset`/`PauseResume`/`NextMedia` exists today — `capabilities()` would report almost nothing beyond fps | **Low** |
| **temporal-fields** | Phase 1 | Native-resolution adapter; FBO-nesting validation needed only during the narrow `PATTERN_TRANSITION` window | None major beyond the general FBO-nesting question | Moderate (best `windowResized()` coverage in the repo lowers adapter risk elsewhere) | Richest existing status surface of the 5 (pattern name, phase, cycle seed, playhead depth, background-layer mode, timeline pause state) | `r` (Reset) and `TAB`/`P`/`]` (timeline controls) map reasonably cleanly; no general `PauseResume`, no direct `NextMedia` | **Moderate** |
| **blueprint_emergence** | Phase 1 candidate | Native-resolution FBO adapter, preserve existing overlay-drawn-after-FBO ordering | FBO-nesting unverified under an *outer* host FBO; `VideoSampler` lacks a filename getter | Moderate | `getState()`-style accessors already exist for cycle mode/phase (good); media filename is a real gap | `r` (Reset) exists and maps cleanly; no `PauseResume`/`NextMedia` | **Moderate** |
| **fragment-trail** | Phase 2 | Scissor-height parameterization; also depends on CrosshairSystem graduation landing (not started) and the fate of its other 3 local forks (`ShaderLibrary`/`LFOBank`/`TriggerBus`/`TimeOffsetVideoBuffer`), none of which is decided | Depends on prerequisite work outside this sketch's own control | Moderate (scissor only — no FBO-nesting risk) | Rich internal state (`FTModeController` mode/strategy, `CrosshairSystem` state), several accessors need public-reachability checks | `1`/`2`/`3` direct mode-select + `m`-key strategy cycle + `h`-key panel toggle exist; no `PauseResume`/`Reset` | **Moderate–High** (mostly from open shared-service dependencies, not render-boundary difficulty itself) |
| **quadrant-crosshair** | Phase 2 (after CrosshairSystem graduation and the related `LFOBank` fork question are settled) | Needs *both* FBO-nesting validation and scissor-height parameterization | Two compounding GL-state hazards (§4) + largest global-OF-ownership violator (§9, 5/6 categories) + depends on in-flight CrosshairSystem graduation | Moderate–High | `QuadrantManager::getTelemetry()` already gives strong per-quadrant status — best status-contract fit of the 5 despite the hardest render-boundary case | `NextMedia` (`n`/`N`) exists; crosshair-preset cycling loosely maps to `NextMode` but not cleanly (flagged §8 gap); `ofExit`/`ofToggleFullscreen`/`ofHideCursor` all need removal | **High** |

**Radar-pulse and radar-effects-gallery** are correctly excluded from this matrix per Decision 2 — noted here explicitly so a reader of this table alone doesn't wonder why 9 originally-surveyed sketches became 5.

**First two scenes to prove real SceneManager switching**: recommend **`blob-region-prototype`** (lowest render-boundary difficulty, cleanest build, fewest local forks — "deliberately thin" per its own code) paired with **`temporal-fields`** (richest status/lifecycle surface, best resize handling, and its own internal snapshot-FBO transition pattern is directly useful prior art for §6's transition model). This is deliberately **not** the two hardest cases (`quadrant-crosshair`, `fragment-trail`), and — per the Executive Alignment Recommendation — deliberately **not** `shader-effect-debugger`, which cannot prove "switching between two live scenes" regardless of how cleanly it honors a render-target contract, because it was never meant to be one of the things being switched between.

---

## 13. HUD Compositor Relationship

**Recommendation: Option C (sibling systems under a Host Runtime), with one explicit amendment** — the Host Runtime owns both the shared outer `SceneFbo` (§2) and input routing (§8/§9); neither `SceneManager` nor `HUD Compositor` unilaterally owns the render target, since none of A/B/C as literally drawn resolves that question by itself.

**Why not A (SceneManager owns HUD)**: couples the HUD's release/versioning/skin-swapping cycle to SceneManager's, undermining the HUD's own stated design premise — capability-driven, hides unsupported actions per scene — which requires it to *query* scene/SceneManager state, not be owned by it.

**Why not B (HUD owns SceneManager)**: inverts a dependency that shouldn't exist. Ownership implies lifecycle control the HUD doesn't need and directly conflicts with the constraint against the HUD reaching into sketch-private state.

- **Update order**: Input Router (raw input → command) → `SceneManager.update(activeScene, dt)` → `HUD Compositor.update(status pulled from active scene)` — status is always as fresh as the frame that just ran, never more than one frame stale.
- **Draw order**: exactly §2's diagram — SceneManager draws into the shared `SceneFbo`, HUD Compositor composites (clip + skin + widgets) from its texture, Host presents.
- **Command flow**: Input Router → `SceneCommand` → dispatched to either SceneManager (`Previous`/`NextScene`) or the active scene's `executeCommand()` (everything else), gated by `SceneCapabilities` in both directions.
- **Status flow**: Active Scene → `hudStatus()` (pull, once/frame) → HUD Compositor; SceneManager also exposes its *own* status (which scene is active, transition state) as a second, distinct source the HUD combines — not folded into per-scene status.
- **Transition ownership**: SceneManager decides *when* a transition happens and *which* scene is next; Host/HUD Compositor decides *how it looks* (§6) — deliberately split so HUD visual work never touches SceneManager code, and vice versa.
- **Skin switching**: entirely independent of SceneManager, lives wholly inside HUD Compositor — matches the "raster-based interchangeable skins with shared geometry" brief and §6's finding that skin and scene are orthogonal.
- **Testability**: highest of the three options. SceneManager can be exercised headlessly using exactly the pattern `hud_validation_harness` already proves works in this repo (fixed-`dt`-step + capture); HUD Compositor can be exercised against a stub `IEcopunkScene` without a real SceneManager at all.
- **Pi resource management**: the sibling model lets the Host make **one** allocation decision for the shared `SceneFbo` rather than two competing owners each assuming control — directly reduces the double-FBO-allocation risk flagged repeatedly in §2/§4.

---

## 14. Decisions the HUD Initiative Can Rely On Now

| Concern | Decision | Stable now? | Owner | HUD impact |
|---|---|---|---|---|
| Native scene size | 1280×720; Host scales + clips (Option A, §3) | Recommended, not approved | SceneManager | HUD can build its scale/clip pipeline against this now, but should not treat it as permanent |
| Scene FBO owner | Single shared, host-allocated outer FBO; scene draws to whatever's bound | Recommended, not approved | Host Runtime | HUD can prototype its mesh-clip against one shared FBO texture handle |
| Final compositor owner | HUD Compositor, sibling to SceneManager under a Host Runtime (§13) | Recommended, not approved | Host Runtime design | Confirms HUD is not subordinate to SceneManager — safe to proceed on that assumption |
| Scene status format | `hudStatus()`, scene-built by value, pulled once/frame, opaque `statusLines` for scene-specific rows | Recommended, not approved | Joint | HUD can design status-rendering widgets against this shape now |
| Command format | `executeCommand(SceneCommand)` + `Previous`/`NextScene` split out to SceneManager | Recommended, not approved; `NextMode`/`PreviousMode` semantics explicitly Open | Joint | HUD should build capability-gated controls now, treating `NextMode`/`PreviousMode` as unstable |
| Capability discovery | `SceneCapabilities{supportedCommands}` — hide, don't disable, unsupported controls | Recommended, not approved | Joint | HUD can build hide-based UI now |
| Scene activation model | Lazy construction, one-scene preload, destroy on deactivate | Recommended, not approved; Pi memory ceiling unmeasured | SceneManager | HUD transition UI should assume no live dual-scene crossfade in v1 |
| Transition ownership | SceneManager decides when/which; Host/HUD decides how it looks; single-live-scene model | Recommended, not approved | Joint | HUD can design transition visuals independent of SceneManager internals |
| Asset-root resolution | Host-resolved, scene-namespaced roots via `SceneServices` | Recommended, not approved | Host Runtime | HUD skin-package storage should follow the same namespaced-root convention |
| Global OF state policy | Host-exclusive for frame rate/vsync/cursor/fullscreen/exit | Recommended, not approved; `quadrant-crosshair` is the largest current violator | Host Runtime | HUD's own fullscreen/exit controls, if any, must route through the Host, not call OF globals directly |
| In-process scene switching itself | **Blocked** — real class-name collisions unresolved, CrosshairSystem graduation not started | Not stable | SceneManager (prerequisite work) | **HUD must keep Previous/Next-Scene controls disabled in v1** |
| v1 scope (5 scenes; radar-* cut; debugger/harness excluded) | **Committed** | Stable | Product/scope decision, recorded in `scene-consolidation-decisions.md` | HUD's capability matrix should target exactly these 5, not the originally-audited 9 |

---

## Risks of Proceeding Without Alignment

- HUD building `drawInto`/FBO assumptions before Option A vs. B vs. C (§3) is jointly ratified risks rework if a genuinely bounds-aware model is chosen later.
- HUD retaining `shader-effect-debugger` as its PoC host — the prior probe's recommendation — would prove a capability (`destinationFbo` redirection) that matters, but not the capability (switching between live scenes) SceneManager actually needs proven first; wasted PoC effort if not corrected.
- HUD shipping live previous/next-scene controls before in-process switching exists would ship controls that cannot work — must stay disabled per §6/§14.
- HUD assuming any status field is safe to poll without confirming it against known hazards (e.g. `quadrant-crosshair`'s flag-clearing `fileChanged()`) risks state races.
- Both initiatives independently deciding "who restores GL state after a scene draws" without agreement risks either wasteful double-restoration or — worse — neither side doing it, a real blend/scissor leak into HUD chrome.
- Independently-designed skin-asset-root and scene-asset-root conventions (§11) risk two incompatible path-resolution schemes needing reconciliation later.

## Recommended Next Coordination Milestone

A short, joint design pass — not a full spec — to ratify exactly the highest-cost-if-wrong items from §14 as **Committed** rather than **Recommended**: the render-ownership model (§2), the status/command struct shapes (§7/§8), and the global-OF-ownership rule set (§9). Recommend this happen *before* either initiative writes production code against `IEcopunkScene` or `SceneHudStatus`, even though both sides can continue exploratory/adapter-shape work in parallel until then.

**Concrete first artifact**: a small shared header (e.g. `shared/src/scene/SceneContract.h`) containing only the agreed struct/interface shapes from this report, with no implementation. Both initiatives build against it; any future change to it is a two-team conversation, not a unilateral edit — a concrete version of the "small shared contract before either commits to incompatible ownership" instinct the HUD probe already raised.
