#pragma once
#include "../HudOverlayOrganism.h"
#include "HudElements.h"

namespace hudoverlay {

// Design doc Section 4.1: reticle snaps in (hard cut + overshoot) at t=0 ->
// ~150ms tick burst -> ~300ms coordinate label -> ~450ms status phrase at a
// fixed corner. ~1.8s total lifespan.
class LockSequenceOrganism : public HudOverlayOrganism {
public:
	void setup();
	void setTheme(const hud::HudTheme& theme);
	void setBounds(float x, float y, float w, float h);
	// nx, ny normalized bounds-relative.
	void trigger(float nx, float ny, hud::CalloutAnchor statusCorner);
	void update(float dt) override;
	void draw() override;

private:
	hud::ReticleWidget reticle;
	hud::TickBurstWidget burst;
	hud::TextCalloutWidget callouts;
};

} // namespace hudoverlay
