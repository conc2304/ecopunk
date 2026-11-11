# CRT selectors — Tier C proposal: interactive option axes for a scene

Status: IMPLEMENTED 2026-11-10 on the project owner's explicit approval (option 1 below); Architecture ratification of the
additive contract change is still pending. SceneContract.h gained SceneOptionAxis, SceneCapabilities::optionAxes and a
non-pure IEcopunkScene::stepOption; SceneManager gained dispatchSceneOption. SceneCommand is unchanged.
Raised because the CRT PIXELS/PALETTE selectors could not finish their value-stepping inside the frozen
contract. Revised after the interactive-region work (see "What is already built" below).

## What is blocked

The CRT scene has three independent selectable axes: composition mode (4), pixel structure (6),
colour palette (8). Composition already works (`controls.mode` / `m` -> `SceneCommand::NextMode`, displayed once in
`universal.media_title`). PIXELS and PALETTE are shown as ONE interactive selector group (two rows) in
`universal.effect_summary`. Changing their VALUES from the HUD needs:

| Need | Today | Gap |
|---|---|---|
| Composition forward | `SceneCommand::NextMode` -> `SceneManager::dispatchSceneCommand` -> scene `executeCommand` | works (wired; forward-only, wraps) |
| Composition backward | none | no `PreviousMode` |
| Structure / palette step (either direction) | none | `SceneCommand` is a frozen enum of five values (`SceneContract.h` header comment forbids adding values); `IEcopunkScene` has only `executeCommand(SceneCommand)` |
| Row focus + adjust | focus slots are a fixed control list (`HudCompositorBridge.h:159`); ENERGY adjust is installation-owned | no per-row focus stops; no "adjust this scene option" mode |

## Options considered

1. **(Recommended) Additive, scene-advertised option axes.**
   - `SceneCapabilities` gains `std::vector<SceneOptionAxis> optionAxes` (`{id, label, count}`; empty by default).
   - `IEcopunkScene` gains a NON-pure `virtual bool stepOption(int axisIndex, int delta) { return false; }`.
   - `SceneManager` gains `dispatchSceneOption(axis, delta)` mirroring `dispatchSceneCommand` (same Idle/FadingIn gating).
   - Existing scenes compile unchanged (default impl, empty axes). The CRT scene implements it by calling the same
     `crt::Selection::step()` that `NextMode` already uses (seam already in `CrtPhosphorScene`).
   - HUD side is ALREADY BUILT (see below): the runtime calls its region-option port with `(row, delta)`; with this
     proposal approved the port body becomes `sceneManager_.dispatchSceneOption(row, delta)` and the region becomes
     available whenever the active scene advertises `optionAxes`.
   - Affected consumers: `SceneContract.h`, `SceneManager`, `ExperienceRuntime` (dispatch + adjust state),
     `HudCompositorBridge` (focus slots), `HudRealFrameResolver` (availability sources), `FakeScene` (optional),
     tests (`hud_focus_navigator_tests`, new scene-option tests). No other scene changes behaviour.
2. **Add enum values** (`PreviousMode`, `NextOption`, `PreviousOption`, ...). Smaller diff but breaks the "do not add
   enum values" rule, forces every scene's `switch` to handle them, and cannot express which axis.
3. **Runtime-local typed setter on the concrete CRT scene** (cast from `ExperienceRuntime`). Rejected: it bypasses
   `IEcopunkScene::executeCommand`, creates a second command authority and breaks the scene-agnostic runtime.

## Not requested

No change to HUD geometry, region IDs, skin schema, `HudFrameData`, `SceneServices`, or Shared Video/Effects.
Look presets (Reference/Clean/Damaged) and the experimental behaviours stay out of the HUD.

## What is already built (HUD side, no contract change)

- Generic opt-in: `ScenePresentationProfile::interactiveRegions` (declarative; CRT declares `universal.effect_summary`,
  `SelectorGroup`, 2 rows). Renderer derives idle/focused/active/disabled looks from the active skin palette
  (`HudInteractionStyle.h`); ExperienceRuntime owns focus + input (`HudRegionInteraction`, one focus stop
  `interactive.region`; Up/Down row, Left/Right value, Enter/Esc leave).
- The ONE exit of a value step is `ExperienceRuntime::setRegionOptionSink(std::function<bool(int row,int delta)>)`.
  Production installs NO sink, so the region is "unavailable" (hidden from focus traversal, no affordance) and CRT
  behaves exactly as before. The test harness installs a sink that calls the scene's existing test hook.

## Interim behaviour (shipped)

`NextMode` steps composition forward; PIXELS/PALETTE are chosen at startup via `EXPERIENCE_RUNTIME_CRT_STRUCTURE` /
`_PALETTE` / `_MODE` (stable ids). `crt::Selection` already owns wrap/count/id logic, so option 1 reduces to wiring
the port body and advertising the axes.
