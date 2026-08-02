#pragma once
#include "../HudOverlayOrganism.h"
#include "HudElements.h"

namespace hudoverlay {

// Design doc Section 4.4: reticle at A -> ~200ms reticle at B -> dashed
// line grows between them (~0.6s draw-on) -> ~500ms after A, a pixel
// distance label appears at the line's midpoint. ~2.2s total lifespan.
class HandshakeOrganism : public HudOverlayOrganism {
public:
	void setup();
	void setTheme(const hud::HudTheme& theme);
	void setBounds(float x, float y, float w, float h);
	// All normalized bounds-relative; canvasW/H used only to compute the
	// pixel-distance label text.
	void trigger(float nxA, float nyA, float nxB, float nyB, float canvasW, float canvasH);
	void update(float dt) override;
	void draw() override;

private:
	hud::ReticleWidget reticles; // holds both A and B as independent one-shot targets
	hud::DashedLineWidget line;
	hud::TextCalloutWidget label;
};

} // namespace hudoverlay
