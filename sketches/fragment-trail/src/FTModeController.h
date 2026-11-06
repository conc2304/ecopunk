#pragma once

#include "ofMain.h"
#include "FTFragment.h"
#include "TriggerBus.h"
#include <string>

namespace fragmenttrail {

// All three mode-switch mechanisms from Section 9, behind a runtime-selectable
// strategy — defaults to MANUAL_KEY (Pi deployment is headless, per Blueprint
// Emergence's own handoff; keypress is meant for the desktop tuning pass
// mentioned throughout the brief, not final deployment behavior).
enum class FTModeSwitchStrategy {
	TIMER,        // auto-rotate on a fixed dwell duration
	TRIGGER_BUS,  // driven by TriggerBus's existing VELOCITY_HIGH/VELOCITY_LOW events
	MANUAL_KEY    // '1'/'2'/'3' select a mode directly
};

class FTModeController {
public:
	void setup();
	void update(float dt);
	void onKeyPressed(int key);
	// Registered with TriggerBus::addListener() by the owner (ofApp) — only
	// acts when strategy == TRIGGER_BUS.
	void onTriggerEvent(const TriggerEvent& e);

	void setStrategy(FTModeSwitchStrategy s) { strategy = s; }
	FTModeSwitchStrategy getStrategy() const { return strategy; }
	void setDwellDuration(float seconds) { dwellDuration = seconds; }

	FTContentMode getCurrentMode() const { return currentMode; }
	// Steps to the next mode in the same wrapping order TIMER rotation uses
	// (TIME_SLICE -> EFFECT_VARIED -> EFFECT_PARAM_VARIANT -> BACKDROP_EFFECT -> TIME_SLICE),
	// regardless of strategy — a host-driven step, like the '1'-'3' keys.
	void nextMode();
	static constexpr int kModeCount = 4; // FTContentMode values, all reachable
	int currentModeIndex() const { return static_cast<int>(currentMode); }
	const std::string& getModeCEffectName() const { return modeCEffectName; }
	// BACKDROP_EFFECT looks, re-rolled on EVERY switchTo() (mode change or scene
	// activation): the effect running over the whole backdrop and the (different)
	// single effect trail B runs. Trail A is always the raw clip.
	const FTFragmentSpawnParams& backdropLook() const { return backdropLook_; }
	const FTFragmentSpawnParams& trailBLook() const { return trailBLook_; }

private:
	void switchTo(FTContentMode mode);

	FTModeSwitchStrategy strategy = FTModeSwitchStrategy::MANUAL_KEY;
	FTContentMode currentMode = FTContentMode::EFFECT_VARIED;
	std::string modeCEffectName;
	FTFragmentSpawnParams backdropLook_;
	FTFragmentSpawnParams trailBLook_;

	float dwellDuration = 12.0f;
	float dwellTimer = 0.f;
};

} // namespace fragmenttrail
