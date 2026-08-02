#pragma once
#include "../HudOverlayOrganism.h"
#include "HudElements.h"

namespace hudoverlay {

// Design doc Section 4.2: a single sharp system hiccup, all parts fired
// together anchored to one point over a ~0.4s window — not a barrage of
// independent glitches. Composes GlitchTearWidget (existing, ambient-timer
// disabled here — see setup()) for the tear+accent-line-flash look via its
// trigger() API, plus new HalftonePatchWidget/TickBurstWidget atoms, plus a
// near-imperceptible full-canvas flash drawn directly (too trivial a
// primitive to warrant its own widget class).
class FaultCascadeOrganism : public HudOverlayOrganism {
public:
	void setup();
	void setTheme(const hud::HudTheme& theme);
	void setBounds(float x, float y, float w, float h);
	// Gates the halftone-patch component (default true, matching this
	// organism's original look). A consumer wanting a calmer tear+flash-only
	// eviction gesture (e.g. fragment-trail, per its design brief) sets this
	// false — the tear + scatter ticks + full-canvas flash still fire.
	void setHalftoneEnabled(bool enabled) { halftoneEnabled = enabled; }
	void trigger(float nx, float ny);
	void update(float dt) override;
	void draw() override;

private:
	hud::GlitchTearWidget tear;
	hud::HalftonePatchWidget halftone;
	hud::TickBurstWidget scatterTicks;
	float flashAlpha = 0.0f; // full-canvas flash envelope, driven directly by armSequence steps
	float boundsW = 0.0f, boundsH = 0.0f;
	bool halftoneEnabled = true;
};

} // namespace hudoverlay
