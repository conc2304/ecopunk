#pragma once

#include "DpadInput.h"
#include "HudFocusNavigator.h" // HudFocusInput (device-neutral step/activate)
#include "SceneContract.h"

#include <optional>

// InputRouter — translates a raw key into, at most, ONE of a SceneCommand
// or a RuntimeCommand, per Scene-HUD-Contract-v1.md §9's command split
// ("InputRouter dispatches each RuntimeCommand to ExperienceRuntime/
// SceneManager and each SceneCommand to the active scene's
// executeCommand()"). The two translate* methods are deliberately separate
// entry points — callers dispatch each result down its own path; nothing
// here merges the two into a single command type.
//
// Bindings here are minimal, development-only conveniences to exercise
// this scaffold — NOT a production/installation key map, and NOT a
// reinterpretation of any of the six production scenes' own key bindings
// (this is a separate, standalone application). 'f'/ESC intentionally
// reuse quadrant-crosshair's existing RuntimeCommand-shaped meanings
// (ToggleFullscreen/Exit) rather than picking new ones, since those two
// mappings were already unambiguous host-owned behavior per the discovery
// report §6/§8.
//
// Development-only, non-contract test hooks (direct reset() vs.
// SceneCommand::Reset, GL-contamination toggle, health-state cycling) are
// intentionally NOT routed through this class — see ExperienceRuntime's
// keyPressed for those, kept clearly separate from real command routing.
class InputRouter {
public:
	std::optional<SceneCommand> translateToSceneCommand(int key) const;
	std::optional<RuntimeCommand> translateToRuntimeCommand(int key) const;

	// Playable HUD Menu v0.1: HUD focus navigation. Not a command — it only
	// moves or activates focus; activation is then dispatched by
	// ExperienceRuntime through the two paths above. Arrow keys = step,
	// Enter = activate. A future rotary encoder produces the same
	// HudFocusInput (detent = Step ±1, click = Activate) without any change
	// to the focus or command logic.
	std::optional<HudFocusInput> translateToFocusInput(int key, bool shift = false) const;
	// Experience Settings page: the five-way D-pad (arrows = Up/Down/Left/Right,
	// Enter = OK, Esc = Back). Used only while the page is open; the page is
	// the sole consumer, so nothing leaks to scene/HUD controls.
	std::optional<DpadInput> translateToDpad(int key) const;
	// Opens the page (F1 or ','): a keyboard entry point until the frozen HUD
	// nav-row geometry gains a Settings button.
	bool isSettingsToggle(int key) const;
	// Installation ENERGY v0.1: enter/exit ENERGY adjustment ('e'); while it
	// is open, the encoder-style focus steps adjust ENERGY.
	bool isEnergyAdjustToggle(int key) const { return key == 'e' || key == 'E'; }
};
