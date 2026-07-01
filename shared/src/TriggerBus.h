#pragma once

#include <functional>
#include <vector>

// Generic event bus: triggers fire on the rising edge of a condition
// (false -> true), respect a per-trigger cooldown, and notify listeners.
// The bus does not know what a "trigger" means — it only tracks edges and
// cooldowns. The caller (a sketch-level composition class) evaluates its
// own state each frame and reports booleans via setConditionActive(), or
// calls fireImmediate() for one-off events (phase changes, etc.) that have
// no persistent "is this still true" state.
//
// Adapted from quadrant-crosshair/src/TriggerBus.h, which hardcoded its
// condition checks against CrosshairState. That coupling is exactly what
// keeps this version out of being reusable, so here the bus is mechanics
// only: cooldown bookkeeping + edge detection + listener dispatch.
class TriggerBus {
	public:
		using Callback = std::function<void(int triggerIndex)>;

		void setup(int numTriggers);
		void update(float dt); // ticks cooldown timers down

		void setCooldown(int triggerIndex, float seconds);
		void addListener(int triggerIndex, Callback cb);

		// Reports the current state of a level-triggered condition. Fires
		// listeners once when it transitions false -> true, subject to cooldown.
		void setConditionActive(int triggerIndex, bool active);

		// Fires immediately for event-driven triggers (no persistent state),
		// still subject to cooldown.
		void fireImmediate(int triggerIndex);

		bool wasActive(int triggerIndex) const;

	private:
		struct TriggerDef {
			float cooldown = 0.0f;
			float cooldownRemaining = 0.0f;
			bool wasActive = false;
			std::vector<Callback> listeners;
		};

		void fire(int triggerIndex);

		std::vector<TriggerDef> triggers;
};
