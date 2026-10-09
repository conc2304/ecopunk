#pragma once

// HudFocusNavigator — keyboard/encoder focus traversal over the HUD's existing
// actionable controls (Playable HUD Menu v0.1). openFrameworks-independent;
// covered by test/hud_focus_navigator_tests.cpp.
//
// Owns ONLY "which control has focus". It never decides what a control means
// or whether it may run: availability comes from the renderer's own resolved
// control state each frame (the same three-state rule LabelWidget draws:
// hidden / disabled / enabled), and activation is dispatched by
// ExperienceRuntime through its existing SceneCommand/RuntimeCommand paths.
//
// Input is device-neutral: a step of ±n detents and an activate click — what
// a rotary encoder produces — so hardware input can be added later by
// translating encoder events into HudFocusInput, with no change here.

#include <functional>
#include <optional>
#include <string>
#include <vector>

enum class HudControlAvailability { Hidden, Disabled, Enabled };

struct HudFocusInput {
	enum class Kind { Step, Activate };
	Kind kind = Kind::Step;
	int delta = 0; // Step only: +1 next, -1 previous; |delta| > 1 = several detents
};

class HudFocusNavigator {
public:
	using AvailabilityFn = std::function<HudControlAvailability(const std::string& regionId)>;

	// `slots` = focusable control region IDs in traversal order.
	explicit HudFocusNavigator(std::vector<std::string> slots) : slots_(std::move(slots)) {}

	// Call once per frame with the current availability. Focus is stable: an
	// unchanged, still-visible focused control keeps focus. If it becomes
	// hidden, focus moves to the next visible control (then previous), or
	// clears if none is visible. Never auto-focuses before the first input.
	void refresh(const AvailabilityFn& availability);

	// Moves focus by `delta` visible controls, wrapping. The FIRST step only
	// engages focus (on the first enabled visible control, else the first
	// visible one) rather than moving. Returns false if nothing is visible.
	bool step(int delta);

	// The focused control's region ID if it may be activated right now
	// (focused AND enabled); nullopt for no focus or a disabled control.
	std::optional<std::string> activationTarget() const;

	const std::optional<std::string>& focused() const { return focused_; }
	bool engaged() const { return engaged_; }
	HudControlAvailability focusedAvailability() const { return focusedAvailability_; }

private:
	std::vector<std::string> slots_;
	std::vector<HudControlAvailability> availability_;
	std::optional<std::string> focused_;
	HudControlAvailability focusedAvailability_ = HudControlAvailability::Hidden;
	bool engaged_ = false;

	int indexOf(const std::string& id) const;
	bool visible(size_t i) const { return i < availability_.size() && availability_[i] != HudControlAvailability::Hidden; }
	void setFocus(int index);
};
