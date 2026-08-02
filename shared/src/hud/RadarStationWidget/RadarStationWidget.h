#pragma once
#include "../shared/HudWidget.h"

namespace hud {

struct RadarStationOptions {
	float rotationPeriod = 6.0f;  // seconds per full 360° sweep
	float wedgeArcDeg     = 45.0f;
	int   tickCount       = 12;   // evenly spaced around the ring
	float ringRadiusNorm  = 0.42f; // fraction of bounds.minDim()
};

// Continuous ambient atom: a wedge rotating inside a fixed ring, with tick
// marks around the ring that flash bright as the wedge sweeps past them.
// The ticks read their brightness directly off the wedge's current angle
// (not an independent timer), so wedge and ticks are phase-locked by
// construction — this is the system's reference case for that, per the
// design doc's Section 3.2.
class RadarStationWidget : public HudWidget {
public:
	void setOptions(const RadarStationOptions& next) { options = next; }
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 80.0f, 80.0f }; }

private:
	RadarStationOptions options;
	float rotationDeg = 0.0f; // wedge leading-edge angle, wraps [0, 360)
};

} // namespace hud
