# Scene/HUD Contract v1

**Status**: Revised per HUD Architecture Review. All 11 requested changes (§14 of the review) are incorporated below. Ready for freeze as the shared implementation boundary pending final sign-off from both initiatives.

**Naming note**: adopts the HUD review's terminology throughout — `ExperienceRuntime` (was "Host Runtime" in the draft), `HudCompositor` (was "HUD Compositor"), `InputRouter`, `RuntimeServices`. One sibling-system diagram, one set of names, used consistently by both initiatives from here on.

---

## Revision Log (Draft → v1)

| # | Change | Section |
|---|---|---|
| 1 | `SceneServices` simplified to one `sceneAssetRoot` instead of separate `presetRoot`/`knowledgeRoot`; added `PerfInstrumentation*`; confirmed no HUD/skin/video-player references | §3 |
| 2 | `ExperienceRuntime` made the unambiguous owner of the scene output FBO; `HudCompositor` consumes a new read-only `SceneFrame` view, not the FBO directly | §4 |
| 3 | GL-state baseline made explicit; scene vs. adapter obligations distinguished; `SceneRenderGuard` noted as a runtime implementation detail, not part of the public interface | §5 |
| 4 | Lifecycle contradiction resolved — `deactivate()` no longer implies destruction; `shutdown()` is the only resource-releasing step; SceneManager may call `shutdown()` immediately after `deactivate()` in v1 as a residency *policy*, not a contract requirement | §6 |
| 5 | `SceneHealth` enum and `message` field added to `SceneHudStatus`; `schemaVersion` added | §7 |
| 6 | `SceneCapabilities` changed from a bare `std::set<SceneCommand>` to a vector of `SceneCommandDescriptor` (adds human-readable labels) | §8 |
| 7 | `SceneCommand` (scene-owned) and `RuntimeCommand` (runtime-owned: `NextScene`/`PreviousScene`/`ToggleHud`/`ToggleFullscreen`/`Exit`) split into two enums, dispatched by `InputRouter` to different owners — this also answers the draft's open "`HostCommand` channel" question | §9 |
| 8 | Unsupported-control debug/placeholder mode confirmed as HUD-tooling only, no contract change | §9 |
| 9 | `SceneTransitionPhase` and `SceneManagerStatus` added, kept separate from `SceneHudStatus` — transition/loading state is never part of an individual scene's snapshot | §10 |
| 10 | Mesh-clip viewport shape confirmed as a new HUD-owned class (`shared/src/hud-compositor/MediaViewportMesh.*`), not an extraction of `TFShapeFragmentRenderer` — revisit extraction later if warranted | §11 |
| 11 | HUD/skin asset paths confirmed fully separate from `SceneServices`, delivered via a distinct `HudAssetPaths`/`HudServices` object; canonical `assets/` tree defined | §12 |
| — | `fps` and all runtime/system metrics (frame time, CPU temp, memory, throttling) removed from `SceneHudStatus` entirely, replaced by a separate `RuntimeTelemetry` source | §13 |
| — | First-integration sequencing reframed as Stage A (HUD mechanism validation, independent of SceneManager) / Stage B (SceneManager validation via `blob-region-prototype` + `temporal-fields`) | §14 |

All seven questions posed to the HUD initiative in the draft's closing section were answered directly by the review; each answer is folded into its relevant section below rather than kept as a separate open-questions list.

---

## 1. Goals

*(unchanged from draft)* Let both initiatives build against a stable set of types/interfaces for weeks without either side needing to read the other's implementation; extend the proven `destinationFbo`-style pattern one layer up; make status non-destructive and pull-based; make commands capability-gated; give `ExperienceRuntime` unambiguous ownership of window/frame-rate/cursor/fullscreen/exit; preserve incremental build/run capability throughout migration.

## 2. Non-goals

*(unchanged from draft, plus one addition)* Not defining bounds-aware rendering for v1; not specifying skin geometry or the five skins' visual treatment; not committing to an in-process scene-switching timeline; not centralizing the video-player wrapper or other services without proven need; not including `audioEnergy`/`interactionEnergy`; excludes `FireplaceWaterfall`, `shader-effect-debugger`, `hud_validation_harness`, `radar-pulse`/`radar-effects-gallery`; not validating anything on real Pi hardware. **Added**: not defining runtime/system telemetry inside the scene status contract — that's `RuntimeTelemetry` (§13), a `RuntimeServices`-owned concern, never scene-owned or scene-adjacent.

## 3. Proposed Scene Interface

```cpp
class IEcopunkScene {
public:
    virtual ~IEcopunkScene() = default;

    virtual void setup(const SceneServices& services) = 0;
    virtual void activate() = 0;
    virtual void deactivate() = 0;

    virtual void update(float dt) = 0;
    virtual void drawToCurrentTarget() = 0;

    virtual glm::ivec2 nativeRenderSize() const = 0;

    virtual std::string sceneId() const = 0;
    virtual std::string displayName() const = 0;

    virtual SceneHudStatus hudStatus() const = 0;
    virtual SceneCapabilities capabilities() const = 0;
    virtual bool executeCommand(SceneCommand command) = 0;

    virtual void reset() = 0;
    virtual void shutdown() = 0;
};
```

Interface itself is unchanged from the draft — the review's revisions land in the *types* (`SceneServices`, `SceneHudStatus`, `SceneCapabilities`, the command split), not the method signatures.

### `SceneServices` — revised

```cpp
struct SceneServices {
    glm::ivec2 canvasSize;

    std::string sceneAssetRoot;         // e.g. assets/scenes/<sceneId>/ —
                                         // scenes resolve their own
                                         // subdirectories beneath this
    std::string sharedMediaRoot;
    std::string sharedEffectAssetRoot;

    PerfInstrumentation* perf = nullptr;
};
```

**Changed from draft**: the draft's separate `presetRoot`/`knowledgeRoot` collapse into one `sceneAssetRoot` — scenes resolve their own subdirectories beneath it. Shared media and shared effect assets stay explicit, separate fields, since they're genuinely cross-scene resources, not scene-namespaced ones. `PerfInstrumentation*` is added per §10's "centralize now" call. **Confirmed absent, per review**: no HUD/skin/widget/compositor reference of any kind, and no shared video-player interface — that channel doesn't get added until the video-wrapper unification question (still Open) has an approved target.

## 4. Render Ownership

**Ownership, resolved per review**: `ExperienceRuntime` is the unambiguous owner of the scene output FBO — not "Host allocates, HUD structurally owns," which the review correctly flagged as ambiguous in the draft. `HudCompositor` never touches the FBO directly; it receives a read-only view:

```cpp
struct SceneFrame {
    const ofTexture* texture = nullptr;
    glm::ivec2 nativeSize{0, 0};
    uint64_t frameNumber = 0;
};
```

`frameNumber` lets `HudCompositor` detect a stale/unchanged frame without needing to compare texture contents — useful if the HUD ever throttles its own redraw independent of scene update cadence (see §7's cadence note).

**Sibling-system diagram** (confirmed, using the review's naming):

```
ExperienceRuntime
├── SceneManager
├── HudCompositor
├── InputRouter
└── RuntimeServices
```

Neither `SceneManager` nor `HudCompositor` owns the other system's resources — `ExperienceRuntime` owns the shared `SceneFbo`; `HudCompositor` only ever sees a `SceneFrame`.

**Draw-order diagram, updated**:

```
ExperienceRuntime, once per frame:
 1. Runtime binds the shared SceneFbo (sized to the active scene's
    nativeRenderSize(), currently always 1280x720)
 2. Runtime clears SceneFbo
 3. InputRouter has already translated raw input into SceneCommand/
    RuntimeCommand values this frame (§9)
 4. SceneManager routes any pending SceneCommands to the active scene,
    handles any pending RuntimeCommands itself, then calls
    activeScene->update(dt)
 5. SceneManager calls activeScene->drawToCurrentTarget()
    — the scene may use its own internal FBOs/scissor here; it must
      not intentionally leave unsafe state enabled (§5), but is not
      required to restore a full baseline itself
 6. Runtime's adapter layer restores the known-good GL baseline (§5)
 7. Runtime unbinds SceneFbo, constructs this frame's SceneFrame
    (texture + nativeSize + frameNumber)
 8. HudCompositor draws the SceneFrame's texture through the mesh clip
    (§11) into the media-viewport region of the window-space composite
 9. HudCompositor draws the L-panel chrome/skin + widgets on top, using
    this frame's pulled hudStatus() (§7) and SceneManagerStatus (§10)
10. Runtime presents the composite to the default framebuffer
```

Everything else from the draft's render-ownership answers (who clears, who restores, where scale-and-clip happens) is unchanged — only the FBO-ownership ambiguity and the FBO-vs-`SceneFrame` access pattern were revised.

## 5. GL State Restoration

**New section**, per review — the draft covered *that* restoration happens but not the baseline or the obligation split.

**Scene obligations vs. adapter obligations, distinguished**:
- **Scenes** must not *intentionally* leave unsafe state enabled after `drawToCurrentTarget()` returns (e.g. don't deliberately leave scissor-test on with an application-specific rect and assume the next consumer will clean it up).
- **The adapter** restores a known baseline *defensively*, regardless of whether a given scene actually violated it — because verifying every scene's internal GL discipline by inspection is not reliable enough to skip the defensive restore.

**The known-good baseline, defined explicitly**:
- Scissor disabled
- Stencil disabled
- Alpha blending enabled, runtime's standard blend mode
- No shader bound
- No texture manually bound
- Viewport restored
- Projection/model-view matrices restored
- Style restored (`ofPushStyle`/`ofPopStyle` equivalent)
- Expected framebuffer rebound

**`SceneRenderGuard`**: a reusable RAII-style guard implementing the above is runtime *implementation*, not part of the public `IEcopunkScene` interface — scenes never see it or interact with it directly; it wraps the adapter's call into `drawToCurrentTarget()` from the outside.

## 6. Lifecycle — revised to resolve the draft's contradiction

**The draft's contradiction, as flagged**: it said scenes are destroyed on deactivation, while separately defining `deactivate()` and `shutdown()` as distinct steps and allowing for future reactivation. Those don't both hold. Resolved:

```
construct → setup() → activate() → update()/draw() → deactivate()
    → [optional future activate()] → shutdown() → destroy
```

**Semantics, per review**:
- **`setup()`** — one-time creation and service binding.
- **`activate()`** — resume/start active behavior.
- **`deactivate()`** — stop active behavior and detach transient callbacks. **Does not imply resource destruction.** A scene may release optional, large transient resources here (e.g. an unused FBO it can cheaply reallocate) but must remain valid for a later `activate()` unless `shutdown()` has since been called.
- **`shutdown()`** — release owned resources permanently. This is the only step that must actually free FBOs/textures/timers for good.
- **Destructor** — final safety net only, not where teardown logic should live.

**Residency policy vs. contract**: for v1, `SceneManager` *may choose* to call `shutdown()` immediately after `deactivate()` to conserve memory on a 1GB-RAM Pi 3B. This is explicitly a residency **policy** SceneManager applies, not something the contract itself mandates — a future version could retain deactivated-but-not-shut-down scenes in memory for faster reactivation without touching this interface at all.

Everything else from the draft's lifecycle section (lazy construction, single-active-scene v1, graceful activation-failure handling modeled on `shader-effect-debugger`'s retry pattern, what must be measured on real hardware) is unchanged.

## 7. Status Contract — revised

```cpp
enum class SceneHealth {
    Ready,
    Loading,
    Degraded,
    Failed
};

struct SceneHudStatus {
    uint32_t schemaVersion = 1;

    std::string sceneId;
    std::string displayName;

    SceneHealth health = SceneHealth::Ready;
    std::optional<std::string> message;

    std::optional<std::string> mediaName;
    std::optional<std::string> modeName;

    std::vector<std::string> activeEffects;
    std::vector<std::string> statusLines;

    std::optional<float> progress;
    std::optional<float> motionEnergy;
    std::optional<int>   activeItemCount;

    bool paused = false;
};
```

**Changed from draft**: `schemaVersion`, `health`, and `message` added. Rationale, per review: without these, the HUD has to *infer* loading/failure/degraded states from missing fields, which is fragile — a scene reporting `mediaName = nullopt` could mean "no media loaded yet," "media failed to load," or "this scene has no media concept at all," and the HUD can't tell which without an explicit signal. `health` disambiguates that directly.

**`fps` removed from this struct entirely** (it was never formally a field, but the draft implied it was pollable alongside status) — see §13. All other field-by-field notes from the draft (the `mediaName` gaps, the `VideoSystem::fileChanged()` polling hazard, `statusLines` staying opaque, pull-based-once-per-frame cadence) are unchanged and confirmed by the review: **"Once-per-frame pull is acceptable. The HUD may internally throttle text layout and graph sampling without changing the status API. No event-based status mechanism is needed for v1."**

## 8. Capability Contract — revised

```cpp
struct SceneCommandDescriptor {
    SceneCommand command;
    std::string label;                      // e.g. "Reset Cycle", "Next Video"
    std::optional<std::string> shortLabel;
    bool prominent = false;
};

struct SceneCapabilities {
    std::vector<SceneCommandDescriptor> commands;
};
```

**Changed from draft**: a bare `std::set<SceneCommand>` only answers *whether* a command exists, not *how it should appear*. Descriptors let a scene supply accurate, scene-specific human-readable labels (`"Reset Cycle"`, `"Next Video"`, `"Regenerate Layout"`, `"Next Pattern"`) instead of forcing every action into generic wording that could mislead across scenes as different as `blueprint_emergence` and `quadrant-crosshair`. `HudCompositor` still owns icons and skin-specific visuals — the scene only supplies the label and prominence hint.

**Discovery timing, confirmed**: static, queried once on activation — unchanged from the draft, and the review confirms this is sufficient for v1.

**Unsupported controls**: hidden in the installation HUD, exactly as the draft proposed. For debug/design tooling, an optional mode may show unsupported slots as disabled placeholders so layout coverage can be tested — this is HUD-tooling behavior, not a contract change.

## 9. Command Contract — split into two enums

```cpp
enum class SceneCommand {
    NextMedia,
    PreviousMedia,
    Regenerate,
    Reset
};

enum class RuntimeCommand {
    NextScene,
    PreviousScene,
    ToggleHud,
    ToggleFullscreen,
    Exit
};
```

**Changed from draft**: the draft's single enum mixed scene-owned and runtime-owned commands despite them being dispatched through different owners — the review calls this out directly and splits them. `InputRouter` dispatches each `RuntimeCommand` to `ExperienceRuntime`/`SceneManager` and each `SceneCommand` to the active scene's `executeCommand()`. **This also answers the draft's own open question about a `HostCommand` channel** (§9/§12 item 5 in the draft) — `RuntimeCommand`'s `ToggleFullscreen`/`Exit` *is* that channel; no separate mechanism needed.

**`NextMode`/`PreviousMode`**: **resolved as option (c)** from the draft's own review-question list — wait for a per-scene design pass before shipping any mode-switching UI. The review explicitly rejects the draft's other proposed fallback (an opaque string command) for v1: *"Do not add opaque string commands in v1. They would weaken type safety and make layout behavior unpredictable."* A future, more expressive command model (`SceneAction{id, label, kind}`) is noted as a deliberate v2 extension, not a v1 escape hatch — do not build toward it prematurely.

**Command rejection during transitions**, refined: `RuntimeCommand` scene-switch commands are ignored while transitioning (not queued). `SceneCommand`s are rejected while the scene is unavailable or deactivating — the scene's own `executeCommand()` returning `false` is sufficient; no command queue in v1, matching the draft's original recommendation.

## 10. Transition Model

**The draft's proposed transition accepted as-is**: last outgoing frame → fade/hold → scene replacement → first incoming frame. No live dual-scene crossfade required for v1.

**New, per review**: transition/loading state must be exposed separately from `SceneHudStatus`, never folded into an individual scene's snapshot:

```cpp
enum class SceneTransitionPhase {
    Idle,
    FadingOut,
    Loading,
    FadingIn,
    Failed
};

struct SceneManagerStatus {
    std::string activeSceneId;
    std::optional<std::string> pendingSceneId;
    SceneTransitionPhase transitionPhase = SceneTransitionPhase::Idle;
    float transitionProgress = 0.0f;
    std::optional<std::string> message;
};
```

`HudCompositor` combines the active scene's `SceneHudStatus` with `SceneManager`'s own `SceneManagerStatus` — two distinct sources, never merged into one struct. This also cleanly answers "what happens to status during a failed transition": `SceneTransitionPhase::Failed` plus `message` covers it without inventing a new health state on the scene side.

## 11. Mesh Clip Ownership

**Resolved, per review**: build a new HUD-owned viewport mesh class first — `shared/src/hud-compositor/MediaViewportMesh.*` — rather than extracting `TFShapeFragmentRenderer` into a shared geometry utility during the first implementation. **Reuse the algorithmic precedent** (the rounded-corner + 45°-bevel-as-a-vertex-cut technique, arc-stepping approach) **not the class itself**. After both implementations exist, evaluate whether the shared portion is substantial enough to extract without making either API worse — explicitly deferred, not decided against permanently.

## 12. Asset Roots

**Resolved, per review**: the skin system uses the same host-resolved path philosophy as `SceneServices` but stays fully independent of it. `HudCompositor` receives a separate `HudServices`/`HudAssetPaths` object from `ExperienceRuntime` — **scenes never receive skin paths, confirmed**.

**Canonical asset tree**:

```
assets/
├── scenes/
│   ├── blueprint-emergence/
│   ├── temporal-fields/
│   └── ...
├── shared/
│   ├── media/
│   └── video-effects/
└── hud/
    ├── layouts/
    │   └── canonical-v1/
    └── skins/
        ├── dark-moss/
        ├── plywood-marble/
        └── ...
```

This directly answers the draft's own open question #7 (skin-asset-root convention) — it mirrors the scene-namespaced pattern from `SceneServices` without sharing a type or a resolution call with it.

## 13. Runtime Telemetry — new, replaces `fps` in status

```cpp
struct RuntimeTelemetry {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;

    std::optional<float> cpuTemperatureC;
    std::optional<uint64_t> residentMemoryBytes;
    std::optional<bool> throttled;
};
```

**Per review**: FPS, frame time, memory, temperature, and throttling are runtime metrics, not scene status, and must come from a separate source — `RuntimeServices`, not any individual scene and not `SceneHudStatus`. This prevents scenes from owning or duplicating system measurements that are properly global (matches the draft's own earlier observation that `ofGetFrameRate()` is a global, just formalizes it into a real typed contract instead of leaving it as an implicit "poll the global directly" convention). `cpuTemperatureC`/`residentMemoryBytes`/`throttled` are new fields with no current in-repo source — they're the first real Pi-hardware telemetry this contract calls for, and remain unpopulated (`nullopt`) until that instrumentation exists.

## 14. Migration Strategy — first integration sequence reframed

**Approved, two-stage sequencing** (the draft's original "two separate, sequenced validations" reasoning, now formalized with the review's stage names):

**Stage A — HUD mechanism validation.** Uses a dedicated HUD compositor harness, or `shader-effect-debugger` as a convenient existing host, to validate: destination-FBO-style rendering, the viewport mesh (§11), shell composition, skin package loading, GL state restoration (§5), and performance instrumentation. **This is explicitly not SceneManager validation** — `shader-effect-debugger` is not, and will not become, a scene (per §2's non-goals), so nothing proven in Stage A should be read as proving anything about scene lifecycle or switching. This work can start immediately; it isn't blocked on anything else in this contract.

**Stage B — SceneManager validation.** Uses `blob-region-prototype` + `temporal-fields` (per the draft's original scene-selection reasoning: lowest render-boundary difficulty paired with richest status/lifecycle surface) to validate: lifecycle (§6), status snapshots (§7), capability discovery (§8), command routing (§9), teardown/recreation, actual scene switching, and static-frame transitions (§10). This stage is gated on SceneManager's collision-resolution prerequisites clearing (per §2's non-goals — in-process switching remains Blocked until then).

The per-scene migration-risk table (`blob-region-prototype`/`temporal-fields`/`blueprint_emergence` Phase 1; `fragment-trail`/`quadrant-crosshair` Phase 2, both blocked on CrosshairSystem graduation) is otherwise unchanged from the draft.

## 15. Remaining Open Items (not blocking freeze)

These are implementation-level questions the HUD review correctly did not need to resolve — they belong to individual scenes or to SceneManager's own internal work, not the shared contract shape, and don't block treating this document as frozen:

1. Video-player wrapper unification — still Open (§3/§10 of the draft; unchanged).
2. `quadrant-crosshair`'s `ExpansionDirector` re-entrancy guard — still needs direct code investigation.
3. `blob-region-prototype`/`fragment-trail`'s `mediaName` accessor existence — still unconfirmed, Blocked by code investigation.
4. In-process vs. process-restart fallback timing, if collision resolution takes longer than expected — still undecided.
5. Whether `MediaViewportMesh` and `TFShapeFragmentRenderer` should ever share code — explicitly deferred per §11, not decided against.

## 16. Risks

Carried forward from the draft, all still live: building against an unratified render-ownership model risked rework (now resolved by §4's `SceneFrame` clarification — this specific risk is closed); conflating Stage A/Stage B validation risked wasted PoC effort (now closed by §14's explicit reframing); shipping live scene-switch controls before in-process switching exists remains a risk if not respected; polling hazards like `fileChanged()` remain a risk for any future status field not vetted the way `mediaName` was; nothing in this contract has been measured on real Pi 3B hardware — every GL-restoration baseline, telemetry field, and residency policy above is a design choice made to reduce risk, not a validated number.

---

## Approval Status

Per the HUD Architecture Review: **Approve after the requested revisions are incorporated.** All 11 requested changes (§14 of the review) are reflected above. The draft's original foundational direction — native-resolution scenes, runtime ownership, scene-draws-to-current-target, immutable status snapshots, capability-driven commands, adapter-first migration, independent HUD skins, no premature shared-service centralization — is preserved unchanged; the revisions remove ownership ambiguity and separate scene/manager/HUD/runtime concerns more cleanly, per the review's own framing.

This document is ready to become `shared/src/scene/SceneContract.h`'s implementation reference — struct/interface shapes only, no implementation — with any future change to it treated as a two-team conversation, not a unilateral edit.
