# Double-HUD Prevention Invariant + Later-Migration Matrix

Status: **documentation only** — no production scene code was touched to
produce this. Written as part of HUD Runtime & Validation Studio Engineering
Session 2, whose own instructions were explicit: *"Document the invariant...
do not migrate production scenes this session."* Everything below is a
survey of the current repository state plus a plan; the "later suppression
requirement" column is a recommendation for a future session, not a change
made now.

## 1. The invariant

> **Once a scene is runtime-hosted by `ExperienceRuntime` for cinematic
> installation presentation, `HudCompositor` (the `shared/src/hud-compositor/`
> presentation-model renderer this and the prior engineering session built)
> is the only HUD drawn for that frame. No scene may also draw its own
> local HUD/overlay layer while hosted.**

Why this has to be a stated invariant rather than something that falls out
automatically: every one of this repo's six installation scenes was built
and matured as a **standalone `ofApp`** long before `ExperienceRuntime`,
`SceneManager`, or the shared `HudCompositor` existed. Each standalone
`ofApp::draw()` currently does two things in the same call: render the
scene's own visual content, and then draw that scene's own local HUD/overlay
layer on top of it (a per-scene widget library instance, a debug-toggle
overlay, or both). `IEcopunkScene::draw()` — the contract these scenes must
eventually implement to be hosted — is scene-content-only; it says nothing
about a scene HUD, and none of the six scenes has had its local HUD call
site removed. So the risk is concrete, not hypothetical: **the mechanical
act of wrapping an existing `ofApp`'s content-drawing code into an
`IEcopunkScene::draw()` override, without also deleting or gating that
`ofApp`'s local HUD call, produces two HUD layers stacked in the same
frame** — the scene's own (old) HUD drawn first, `HudCompositor`'s (new,
shared) HUD drawn second by `ExperienceRuntime`, both visible at once.

This session did not migrate any production scene (see §4, "why none of
the six are migrated yet" — none currently implement `IEcopunkScene` at
all; only `sketches/experience_runtime/src/FakeScene.h`'s test double does),
so the invariant above is currently satisfied vacuously: there is exactly
one real `HudCompositor`-family renderer running today
(`hud_validation_studio`, feeding it fake data) and zero scenes double-HUD
because zero scenes are hosted. This doc exists so the *next* session that
migrates a real scene has a checklist instead of rediscovering the risk by
looking at a screenshot with two HUDs on it.

## 2. Where the invariant must be enforced

Three places, all belonging to the future migration work, not this session:

1. **Per-scene, at migration time**: each scene's local HUD draw call (see
   the matrix in §5) must be removed or permanently disabled for the
   `IEcopunkScene::draw()` code path specifically. Scenes may keep their
   local HUD for **standalone/dev-tool use** (running the sketch directly
   via its own `ofApp`, outside `ExperienceRuntime`) — that path never
   collides with `HudCompositor` because `HudCompositor` only exists inside
   `ExperienceRuntime`. The distinction is: local HUD code may stay in the
   scene's source tree; it must not execute on the runtime-hosted path.
2. **`ExperienceRuntime`/`SceneManager`, structurally**: `SceneManager`
   composites scene content into `SceneFrame` (an FBO/texture handoff, see
   `sketches/experience_runtime/src/SceneManager.h`), and `HudCompositor`
   draws on top of that composited frame, once, after all scenes have run
   their `draw()`. As long as `IEcopunkScene::draw()` only writes into the
   scene's own FBO region and never draws directly to the window/screen
   surface, structurally there is nowhere for a second HUD to appear even
   if a scene tried — this is a property worth preserving, not something to
   relax "for convenience" during migration.
3. **Validation, at migration time**: a smoke test (extending this repo's
   existing "one-HUD-pass" test precedent already present in
   `sketches/experience_runtime/test/`, per this session's required-tests
   list) that hosts each newly-migrated scene for a few frames and asserts
   only one HUD draw pass occurred — not just "the build compiles."
   Visual screenshot inspection (the same method that caught this session's
   UV-cropping and em-dash bugs) is a strong secondary check: a doubled HUD
   is visually obvious (duplicate labels, doubled progress bars, mismatched
   typography from two different widget libraries stacked).

## 3. Why an overlay/profile binding layer is the intended long-term seam,
not a per-widget special case

This session bound `SceneManager`-owned transition phase/progress/message
into `HudCompositor` via the existing overlay-region/profile-binding
mechanism (`overlay.transition`, three bindings sliced into one region —
see `HudRegionCatalog.cpp`/`HudPresentationProfile.cpp`), specifically so
that manager-level concerns (transition phase, scene switching) never leak
into per-scene widget logic. The same pattern is the recommended seam for
double-HUD suppression *if* a future session decides a scene needs to
contribute HUD-relevant data without owning HUD rendering: expose it as a
new resolvable value on `HudFrameData`/`SceneHudStatus` (subject to the
existing contract-change escalation process — this doc does not propose any
new field) and bind it through a profile, rather than letting the scene
draw its own competing HUD element. This keeps "who renders the HUD" a
structural, one-answer question (`HudCompositor`, always) rather than a
per-scene judgment call.

## 4. Current state: why zero scenes are migrated yet

`grep`-confirmed at the time of writing: the only type in this repository
implementing `IEcopunkScene` is
`sketches/experience_runtime/src/FakeScene.h` (a deterministic test double
built for `ExperienceRuntime`'s own lifecycle tests) plus the contract
declaration itself in `shared/src/scene/SceneContract.h`. None of the six
production scenes below implement `IEcopunkScene`. Each still runs as its
own standalone `ofApp`-based sketch. This is consistent with — not a
deviation from — this session's explicit non-goal ("do not migrate
production scenes this session") and the master roadmap's own staged plan
(`docs/Ecopunk-HUD-System-Master-Roadmap.md`), which sequences scene
migration as a distinct, later wave.

## 5. Later-migration matrix

Scope: the six scenes the master roadmap
(`docs/Ecopunk-HUD-System-Master-Roadmap.md:21-26`) names as the
installation's actual rotation. `radar-effects-gallery`, `radar-pulse`, and
`shader-effect-debugger` are dev/gallery tools, not installation scenes —
included in a second table below for completeness since two of them also
use the `shared/src/hud/` widget library and could theoretically be misread
as in-scope.

| Scene | Current scene-local HUD owner | Current draw call site | Later suppression/removal requirement |
|---|---|---|---|
| `blob-region-prototype` | None (debug-only overlay, not a cinematic HUD) — `ofApp::drawDebugOverlay()` draws raw/tracked blob-detection boxes and two small analysis-resolution debug thumbnails directly with `ofDrawRectangle`/`ofDrawBitmapStringHighlight` | `ofApp.cpp` — `drawDebugOverlay()`, called unconditionally from `ofApp::draw()` (`sketches/blob-region-prototype/src/ofApp.cpp:242`) | Gate `drawDebugOverlay()` off on the `IEcopunkScene::draw()` path (e.g. behind the same kind of "is this running standalone vs. hosted" flag the migration introduces). Lowest risk of the six — it's development instrumentation, not a competing widget system, so no `hud::`/HUD-widget-library collision to resolve, just a call-site gate. |
| `contour-portrait` | None (debug-only overlay) — `ofApp::draw()` prints an `fps`/`vertices`/`source available`/keybinding-hints text block via `ofDrawBitmapStringHighlight` when `showOverlay` is true, plus a `Show Gui`-gated `ofxGui` panel | `ofApp.cpp:183-191` (`if (showOverlay) {...}`), `ofApp.cpp:194-196` (`panel.draw()`) | Same shape as `blob-region-prototype`: gate both the text overlay and the raw `ofxGui` panel off the hosted path — `.claude/CLAUDE.md`'s "no raw `ofxGui` exposure in the runtime HUD" prohibition applies directly here. Lowest risk. |
| `temporal-fields` | `TFHudLayer` (`sketches/temporal-fields/src/TFHudLayer.h`) — this sketch's own dedicated HUD layer class, with underlay/overlay separation and pattern-switch/fragment-reassignment event-driven visibility cadence | `ofApp.cpp:220` (`hudLayer.drawUnderlay()`), `ofApp.cpp:227` (`hudLayer.drawOverlay()`) | Full local-HUD-class suppression required, not just a text toggle: `hudLayer.drawUnderlay()`/`drawOverlay()` must not execute on the hosted path. `TFHudLayer`'s `setup()`/`update()`/`resize()` lifecycle calls (`ofApp.cpp:73,177,299`) likely still need to run if any non-draw side effects live there (needs inspection at migration time) — do not assume update-and-draw can be cut as one block without checking for that. |
| `fragment-trail` | `hudoverlay::HudOverlayLayer` + `HudOverlayDialPanel` + `FTOverlayDirector` — this sketch's **deliberately separate local fork** (already documented in `sketches/fragment-trail/config.make` and this repo's root `CLAUDE.md` as excluded from the shared `shared/src/hud-compositor`/`shared/src/video-effects` systems specifically because of class-name collisions) | `ofApp.cpp:115` (`hudOverlay.drawAmbient()`), `:117` (`hudOverlay.drawOrganisms()`), `:119` (`hudOverlayPanel.draw()`) | Highest-friction case of the six: this fork's whole reason for existing is that it collides by name with the shared widget library, so this is not a simple call-site gate — it needs its own design pass at migration time (consistent with `fragment-trail`'s existing "deliberately not migrated" status for the video-effects service; expect the same category of decision here, not a mechanical port). Flagging now so a future session doesn't assume this one is cheap. |
| `quadrant-crosshair` | `HudManager` (`sketches/quadrant-crosshair/src/HudManager.h`) — a dedicated per-sketch HUD manager wired to this scene's `TriggerBus`/`CrosshairSystem`/`LFOBank`/`QuadrantManager`/`MotionExtraction` | `ofApp.cpp:183` (`if (showHUD) hud.draw(uiAlpha);`) | Already has a `showHUD` boolean gate at the call site — the mechanism to reuse is already there, just needs its condition extended (or replaced) to also be `false` whenever hosted. Also note `quadrant-crosshair`'s separate `DebugMode` (already documented in root `CLAUDE.md` as "deliberately not migrated" for the video-effects service, for unrelated live-GUI-tuning reasons) — confirm at migration time whether `DebugMode` draws through this same `hud.draw()` call or has its own separate path that also needs gating. |
| `blueprint_emergence` | `hud::HudWidget`-family instances owned by `BEComposition` (a single active `hudWidget` selected per-pattern from `DataCardWidget`/`GaugeWidget`/`ContourWidget`/`HexGridWidget`/`FlowFieldWidget`/`NodeNetworkWidget`/`ReticleWidget`/`StatusLightWidget`/`LogScrollWidget`/`GlitchTearWidget`, plus a separate always-available `circleScanner` (`ScannerWidget`) | `ofApp.cpp:318` (`if (hud::HudWidget* widget = composition.getActiveHudWidget()) widget->draw(...)`), `ofApp.cpp:322` (`if (hud::HudWidget* scanner = composition.getCircleScannerWidget()) scanner->draw(...)`) | Two independent call sites to gate (active pattern widget + circle scanner), both currently `nullptr`-guarded already — extend that guard to also check hosted-state. `BEComposition`'s widget *construction*/`update()` logic (state machine driving which widget is active, `hudStatusLight` state transitions, etc.) should be checked for non-draw side effects the scene's own runtime behavior depends on, same caveat as `temporal-fields`. |

Non-installation dev/gallery tools (included for completeness, not in
scope for the "six scenes" migration wave):

| Sketch | Current HUD-adjacent usage | Note |
|---|---|---|
| `radar-pulse` | `hud::PulseEmitterWidget`, `hud::HudTheme`, `hud::PulseEmitterConfig` | Standalone effect-preview tool, not one of the roadmap's six installation scenes; no `IEcopunkScene` migration is planned for it, so no suppression requirement applies. |
| `radar-effects-gallery` | `hud::PulseEmitterWidget` (`ofApp.h`), plus its own `GalleryCompositor` (explicitly decoupled from `hud::PulseInfo` per that file's own header comment) | Same as above — a gallery/preview tool, out of scope for scene migration. |
| `shader-effect-debugger` | None found (`shared/src/hud` appears on its include sweep per `config.make`, but no `hud::` symbol usage found in its own `src/`) | Reference/debug tool for the shared video-effect service (per root `CLAUDE.md`), not an installation scene. |

## 6. Summary for the next migration session

- Two of the six (`blob-region-prototype`, `contour-portrait`) are low-risk:
  debug-only text/thumbnail overlays behind a togglable bool, easy to gate.
- Three of the six (`temporal-fields`, `quadrant-crosshair`,
  `blueprint_emergence`) each own a real per-sketch HUD widget layer with
  its own lifecycle (`setup`/`update`/`draw`) — suppression must be
  draw-call-only, not a wholesale deletion, since update-side effects need
  individual verification.
- One of the six (`fragment-trail`) is a known-hard case, already flagged
  once in this repo (video-effects) for the same underlying reason
  (a deliberately separate local fork) — expect its HUD suppression to need
  its own design pass, not a mechanical gate.
- No scene currently implements `IEcopunkScene`; this doc's matrix describes
  the state to migrate *from*, not a change made in this session.
