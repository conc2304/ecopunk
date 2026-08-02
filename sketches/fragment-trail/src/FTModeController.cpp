#include "FTModeController.h"

namespace {
// Only effects with a jitterable per-fragment param beyond plain `alpha`
// are useful candidates for Mode C — see brief Section 4's own examples
// (CHANNEL_SHIFT offset, RECOLOR_MONO tint hue).
const std::vector<std::string> kModeCCandidates = { "channelshift", "recolor", "threshold", "dither" };
}

void FTModeController::setup() {
	switchTo(FTContentMode::EFFECT_VARIED);
}

void FTModeController::switchTo(FTContentMode mode) {
	currentMode = mode;
	dwellTimer = 0.f;
	if (mode == FTContentMode::EFFECT_PARAM_VARIANT) {
		modeCEffectName = kModeCCandidates[static_cast<size_t>(ofRandom(kModeCCandidates.size()))];
		ofLogNotice("FTModeController") << "EFFECT_PARAM_VARIANT locked to " << modeCEffectName;
	}
}

void FTModeController::update(float dt) {
	if (strategy != FTModeSwitchStrategy::TIMER) return;

	dwellTimer += dt;
	if (dwellTimer >= dwellDuration) {
		switch (currentMode) {
			case FTContentMode::TIME_SLICE: switchTo(FTContentMode::EFFECT_VARIED); break;
			case FTContentMode::EFFECT_VARIED: switchTo(FTContentMode::EFFECT_PARAM_VARIANT); break;
			case FTContentMode::EFFECT_PARAM_VARIANT: switchTo(FTContentMode::TIME_SLICE); break;
		}
	}
}

void FTModeController::onKeyPressed(int key) {
	// Direct mode keys always work regardless of strategy — handy for
	// checking a mode manually even while TIMER/TRIGGER_BUS is active.
	if (key == '1') switchTo(FTContentMode::TIME_SLICE);
	else if (key == '2') switchTo(FTContentMode::EFFECT_VARIED);
	else if (key == '3') switchTo(FTContentMode::EFFECT_PARAM_VARIANT);
	else if (key == 'm' || key == 'M') {
		switch (strategy) {
			case FTModeSwitchStrategy::MANUAL_KEY: setStrategy(FTModeSwitchStrategy::TIMER); break;
			case FTModeSwitchStrategy::TIMER: setStrategy(FTModeSwitchStrategy::TRIGGER_BUS); break;
			case FTModeSwitchStrategy::TRIGGER_BUS: setStrategy(FTModeSwitchStrategy::MANUAL_KEY); break;
		}
		ofLogNotice("FTModeController") << "switch strategy -> " << static_cast<int>(strategy);
	}
}

void FTModeController::onTriggerEvent(const TriggerEvent& e) {
	if (strategy != FTModeSwitchStrategy::TRIGGER_BUS) return;
	if (!e.active) return;

	// Fast, searching motion -> varied effects; sustained stillness -> slow
	// temporal drift. EFFECT_PARAM_VARIANT has no trigger mapping of its own
	// here — reachable via the manual keys regardless of active strategy.
	if (e.id == TriggerID::VELOCITY_HIGH) {
		switchTo(FTContentMode::EFFECT_VARIED);
	} else if (e.id == TriggerID::VELOCITY_LOW) {
		switchTo(FTContentMode::TIME_SLICE);
	}
}
