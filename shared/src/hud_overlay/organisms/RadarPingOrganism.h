#pragma once
#include "../HudOverlayOrganism.h"
#include "HudElements.h"

namespace hudoverlay {

// Design doc Section 4.3: a reticle + short status phrase near the radar's
// current sweep area, on its own independent timer (Section 08 explicitly
// defers exact wedge-angle coupling to the integration phase — this fires
// near the radar ring's edge at a random angle each time, not read off the
// live wedge angle).
class RadarPingOrganism : public HudOverlayOrganism {
public:
	void setup();
	void setTheme(const hud::HudTheme& theme);
	void setBounds(float x, float y, float w, float h);
	// nx, ny normalized bounds-relative — a point near the radar ring's edge.
	void trigger(float nx, float ny);
	void update(float dt) override;
	void draw() override;

private:
	hud::ReticleWidget reticle;
	hud::TextCalloutWidget callouts;
};

} // namespace hudoverlay
