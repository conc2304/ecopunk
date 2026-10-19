#pragma once
#include "../shared/HudWidget.h"

namespace hud {

struct RadarStationOptions {
	float rotationPeriod = 6.0f;  // seconds per full 360° sweep
	float wedgeArcDeg     = 45.0f;
	int   tickCount       = 12;   // evenly spaced around the ring
	float ringRadiusNorm  = 0.42f; // fraction of bounds.minDim()
	// v0.3 (additive): faint inner range rings + cross axes. Off by default
	// so existing consumers draw exactly as before.
	bool  rangeRings      = false;
};

// One tracked contact, supplied by the consumer from real state (never
// synthesised by the widget). Polar, relative to the radar's own centre.
struct RadarContact {
	float angleDeg = 0.0f;  // screen-space angle (0 = +x, clockwise, like the wedge)
	float range01  = 0.0f;  // 0 = centre, 1 = ring edge (clamped)
	float strength = 1.0f;  // 0..1, e.g. a fragment's live opacity
	bool  primary  = false; // the contact the hunter is currently nearest / locked to
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

	// v0.3 (additive). Contacts are drawn as blips that brighten as the
	// wedge sweeps over them (phase-locked like the ticks). An empty list is
	// the ambient fallback: ring, ticks and wedge only — nothing invented.
	void setContacts(std::vector<RadarContact> next) { contacts = std::move(next); }
	// Optional own-ship heading (e.g. the hunter's velocity direction);
	// strength 0 hides it.
	void setHeading(float angleDeg, float strength01) { headingDeg = angleDeg; headingStrength = strength01; }
	const std::vector<RadarContact>& getContacts() const { return contacts; }

private:
	RadarStationOptions options;
	float rotationDeg = 0.0f; // wedge leading-edge angle, wraps [0, 360)
	std::vector<RadarContact> contacts;
	float headingDeg = 0.0f;
	float headingStrength = 0.0f;
};

} // namespace hud
