#pragma once
#include "HudOverlayDialState.h"
#include <functional>

namespace hudoverlay {

// Owns one independent random-interval timer per organism type (design doc
// Section 06's "mirrors the reference prototype's loopSchedule pattern").
// Intensity scales every timer's speed; EventCoupling narrows each timer's
// own random interval range toward its mean, which is the "shared
// timing-jitter reduction" the design doc calls out as the reference
// prototype's actual implementation of coupling (Section 05) — a true
// shared-heartbeat scheduler is called out there as a follow-up, not
// required for this phase (Section 08).
class HudOverlayScheduler {
public:
	void setup();
	void update(float dt, const HudOverlayDialState& dials);

	std::function<void()> onLockSequence;
	std::function<void()> onFaultCascade;
	std::function<void()> onRadarPing;
	std::function<void()> onHandshake;

private:
	struct Timer {
		float elapsed = 0.0f;
		float nextInterval = 0.0f;
		float baseMinSeconds;
		float baseMaxSeconds;
	};

	Timer lockTimer { 0.0f, 0.0f, 3.0f, 8.0f };
	Timer faultTimer { 0.0f, 0.0f, 8.0f, 20.0f };
	Timer radarTimer { 0.0f, 0.0f, 4.0f, 10.0f };
	Timer handshakeTimer { 0.0f, 0.0f, 6.0f, 14.0f };

	void rearm(Timer& t, const HudOverlayDialState& dials);
	void tick(Timer& t, float dt, const HudOverlayDialState& dials, const std::function<void()>& cb);
};

} // namespace hudoverlay
