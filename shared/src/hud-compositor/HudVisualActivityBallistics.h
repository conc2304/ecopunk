#pragma once

// OC-004 - HUD-owned attack/decay smoothing for the scene.visual.activity gauge (HUD region #6).
//
// The runtime publishes an UNSMOOTHED level + state (SceneVisualActivitySnapshot); smoothing belongs to the
// HUD renderer. Time-constant based (cadence independent). Defaults are the OC-004 report's suggested
// values (attack 0.08 s, decay 0.9 s). Plain C++17, no openFrameworks.
//
// Reset rules (the smoothed value must never carry an old observation across a discontinuity):
//   - any non-Valid state (null snapshot, Unavailable, Stale, Transition) -> reset, nothing shown;
//   - a generation change -> reset, the first new Valid observation seeds directly (no ramp from the
//     previous scene's value).
// A Valid level of exactly 0 is an observation: it decays the value toward 0 and displays 0.

#include "SceneVisualActivitySnapshot.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace hudpresent {

class HudVisualActivityBallistics {
public:
	static constexpr double kAttackSeconds = 0.08;
	static constexpr double kDecaySeconds = 0.9;

	// Called once per HUD update with the frame's (possibly null) snapshot and the update dt in seconds.
	void update(const scenevisual::SceneVisualActivitySnapshot* snap, double dt) {
		if (!snap || snap->state != scenevisual::State::Valid) { reset(); return; }
		if (haveGeneration_ && snap->generation != generation_) reset();
		generation_ = snap->generation;
		haveGeneration_ = true;
		const double x = std::clamp(static_cast<double>(snap->level), 0.0, 1.0);
		if (!have_) { value_ = x; have_ = true; return; }
		const double tau = x > value_ ? kAttackSeconds : kDecaySeconds;
		const double a = 1.0 - std::exp(-std::max(0.0, dt) / tau);
		value_ += a * (x - value_);
	}
	void reset() { have_ = false; value_ = 0.0; haveGeneration_ = false; }
	bool has() const { return have_; }
	float value() const { return static_cast<float>(value_); }

private:
	bool have_ = false, haveGeneration_ = false;
	uint64_t generation_ = 0;
	double value_ = 0.0;
};

} // namespace hudpresent
