#pragma once

#include <vector>

// A bank of independent sinusoidal oscillators, each running at its own
// frequency/phase. Pure CPU sine math — no GL, no allocation per frame.
// Generalized from quadrant-crosshair/src/LFOBank.h: lanes there were a
// fixed enum sized for that sketch's needs; here the lane count and the
// meaning of each lane index are owned by the caller (cast its own enum
// to int), so this class stays reusable across sketches.
class LFOBank {
	public:
		void setup(int numLanes);
		void update(float deltaTime);

		// Returns [-1.0, 1.0]
		float get(int lane) const;

		// Returns [0.0, 1.0] — convenience for opacity/weight uses
		float getUnipolar(int lane) const;

		void setFrequency(int lane, float hz);
		void setPhaseOffset(int lane, float radians);

	private:
		struct Lane {
			float frequency = 0.05f; // hz
			float phase = 0.0f;      // radians, advances each update()
			float phaseOffset = 0.0f;
			float value = 0.0f;
		};
		std::vector<Lane> lanes;
		float elapsed = 0.0f;
};
