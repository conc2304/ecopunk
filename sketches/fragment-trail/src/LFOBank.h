#pragma once
#include "ofMain.h"
#include <array>

// Trimmed copy of quadrant-crosshair/src/LFOBank.h — CrosshairSystem::update()
// requires an LFOBank& for its line-width pulse (LFO_CROSSHAIR_PULSE), but
// none of quadrant-crosshair's other lanes (thresholds, dither scale, scan
// darkness, reaction-diffusion feed/kill, grid decay) apply here; only the
// one lane CrosshairSystem actually reads is kept.
enum LFOIndex {
	LFO_CROSSHAIR_PULSE = 0,
	LFO_COUNT = 1
};

class LFOBank {
public:
	void  setup();
	void  update(float dt);
	float get(int index) const;  // returns -1 to 1

private:
	struct Lane {
		float freq;
		float phase;
		float value;
	};
	std::array<Lane, LFO_COUNT> lanes;
	float timeAccum = 0.f;
};
