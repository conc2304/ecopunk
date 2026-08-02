#include "FaultCascadeOrganism.h"

namespace hudoverlay {

static constexpr float kFullFlashDuration = 0.25f;
static constexpr float kFullFlashPeakAlpha = 0.08f; // "under 10% opacity"

void FaultCascadeOrganism::setup() {
	// This instance is triggered only by trigger() below — disable
	// GlitchTearWidget's own ambient random-interval loop so it doesn't
	// double-fire independently of this organism's schedule.
	hud::GlitchTearOptions to;
	to.triggerIntervalMin = 1.0e6f;
	to.triggerIntervalMax = 1.0e6f;
	to.sliceCountMin = 1;
	to.sliceCountMax = 2;
	tear.setup();
	tear.setOptions(to);
}

void FaultCascadeOrganism::setTheme(const hud::HudTheme& theme) {
	tear.setTheme(theme);
	halftone.setTheme(theme);
	scatterTicks.setTheme(theme);
}

void FaultCascadeOrganism::setBounds(float x, float y, float w, float h) {
	tear.setBounds(x, y, w, h);
	halftone.setBounds(x, y, w, h);
	scatterTicks.setBounds(x, y, w, h);
	boundsW = w;
	boundsH = h;
}

void FaultCascadeOrganism::trigger(float nx, float ny) {
	armSequence({
	                { 0.0f, [this, nx, ny]() {
	                     tear.trigger();
	                     if (halftoneEnabled) {
	                         halftone.triggerAt(ofClamp(nx - 0.08f, 0.0f, 1.0f), ofClamp(ny - 0.02f, 0.0f, 1.0f), 0.16f, 0.05f);
	                     }
	                     scatterTicks.triggerAt(nx, ny);
	                     flashAlpha = kFullFlashPeakAlpha;
	                 } },
	            },
	    0.4f);
}

void FaultCascadeOrganism::update(float dt) {
	tickSequence(dt);
	tear.update(dt);
	halftone.update(dt);
	scatterTicks.update(dt);
	if (flashAlpha > 0.0f) {
		flashAlpha -= (kFullFlashPeakAlpha / kFullFlashDuration) * dt;
		if (flashAlpha < 0.0f) flashAlpha = 0.0f;
	}
}

void FaultCascadeOrganism::draw() {
	tear.draw();
	halftone.draw();
	scatterTicks.draw();
	if (flashAlpha > 0.0f) {
		ofPushStyle();
		ofEnableAlphaBlending();
		ofFill();
		ofSetColor(255, 255, 255, static_cast<int>(flashAlpha * 255.0f));
		ofDrawRectangle(0, 0, boundsW, boundsH);
		ofPopStyle();
	}
}

} // namespace hudoverlay
