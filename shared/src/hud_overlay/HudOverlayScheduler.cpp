#include "HudOverlayScheduler.h"
#include "ofMain.h"

namespace hudoverlay {

void HudOverlayScheduler::setup() {
	HudOverlayDialState defaults;
	rearm(lockTimer, defaults);
	rearm(faultTimer, defaults);
	rearm(radarTimer, defaults);
	rearm(handshakeTimer, defaults);
}

void HudOverlayScheduler::rearm(Timer& t, const HudOverlayDialState& dials) {
	float mid = (t.baseMinSeconds + t.baseMaxSeconds) * 0.5f;
	// EventCoupling narrows the random range toward the mean interval —
	// less jitter pulls timers apart, so higher coupling makes simultaneous
	// firing across organisms more likely.
	float lo = ofLerp(t.baseMinSeconds, mid, dials.eventCoupling);
	float hi = ofLerp(t.baseMaxSeconds, mid, dials.eventCoupling);
	float interval = ofRandom(lo, hi);

	// Intensity scales overall speed. Clamp the effective multiplier so a
	// near-zero intensity doesn't produce an absurdly long wait, and so a
	// maxed-out dial doesn't collapse organisms into a rapid-fire noise
	// floor (see design doc Section 05's ~0.7 "ambient ceiling" note).
	float speedScale = ofLerp(0.4f, 2.2f, ofClamp(dials.intensity, 0.0f, 1.0f));
	t.nextInterval = interval / std::max(0.05f, speedScale);
	t.elapsed = 0.0f;
}

void HudOverlayScheduler::tick(Timer& t, float dt, const HudOverlayDialState& dials, const std::function<void()>& cb) {
	t.elapsed += dt;
	if (t.elapsed >= t.nextInterval) {
		rearm(t, dials);
		if (cb) cb();
	}
}

void HudOverlayScheduler::update(float dt, const HudOverlayDialState& dials) {
	tick(lockTimer, dt, dials, onLockSequence);
	tick(faultTimer, dt, dials, onFaultCascade);
	tick(radarTimer, dt, dials, onRadarPing);
	tick(handshakeTimer, dt, dials, onHandshake);
}

} // namespace hudoverlay
