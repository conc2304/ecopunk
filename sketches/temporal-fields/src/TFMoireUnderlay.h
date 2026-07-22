#pragma once

// Two very fine, low-opacity, counter-rotating line grids — a true
// underlay, meant to be drawn before the active composition each frame.
// Self-contained rather than a hud::HudWidget: it's two independently-
// animated full-canvas line fields with no positioning/triggering surface,
// and TFHudLayer already owns the underlay/overlay draw split this
// piggybacks on.
//
// Performance note: unlike the event-triggered effects in TFHudLayer, this
// runs every single frame regardless of composition activity — include it
// in any future performance validation pass, don't treat it as free just
// because it's visually subtle.
class TFMoireUnderlay {
	public:
		void setup(int canvasW, int canvasH);
		void update(float dt);
		void draw() const;

	private:
		void drawGrid(float angleDeg, float spacing, float opacity) const;

		int canvasW = 0;
		int canvasH = 0;
		float angleA = 0.0f;
		float angleB = 0.0f;
};
