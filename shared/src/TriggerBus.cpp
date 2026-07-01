#include "TriggerBus.h"
#include <algorithm>

void TriggerBus::setup(int numTriggers) {
	triggers.assign(numTriggers, TriggerDef{});
}

void TriggerBus::update(float dt) {
	for (auto & t : triggers) {
		t.cooldownRemaining = std::max(0.0f, t.cooldownRemaining - dt);
	}
}

void TriggerBus::setCooldown(int triggerIndex, float seconds) {
	triggers[triggerIndex].cooldown = seconds;
}

void TriggerBus::addListener(int triggerIndex, Callback cb) {
	triggers[triggerIndex].listeners.push_back(std::move(cb));
}

void TriggerBus::setConditionActive(int triggerIndex, bool active) {
	TriggerDef & t = triggers[triggerIndex];
	if (active && !t.wasActive) {
		fire(triggerIndex);
	}
	t.wasActive = active;
}

void TriggerBus::fireImmediate(int triggerIndex) {
	fire(triggerIndex);
}

bool TriggerBus::wasActive(int triggerIndex) const {
	return triggers[triggerIndex].wasActive;
}

void TriggerBus::fire(int triggerIndex) {
	TriggerDef & t = triggers[triggerIndex];
	if (t.cooldownRemaining > 0.0f) {
		return;
	}
	t.cooldownRemaining = t.cooldown;
	for (auto & cb : t.listeners) {
		cb(triggerIndex);
	}
}
