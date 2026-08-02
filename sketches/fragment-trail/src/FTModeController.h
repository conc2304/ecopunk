#pragma once

#include "ofMain.h"
#include "FTFragment.h"
#include "TriggerBus.h"
#include <string>

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
	const std::string& getModeCEffectName() const { return modeCEffectName; }

private:
	void switchTo(FTContentMode mode);

	FTModeSwitchStrategy strategy = FTModeSwitchStrategy::MANUAL_KEY;
	FTContentMode currentMode = FTContentMode::EFFECT_VARIED;
	std::string modeCEffectName;

	float dwellDuration = 12.0f;
	float dwellTimer = 0.f;
};
