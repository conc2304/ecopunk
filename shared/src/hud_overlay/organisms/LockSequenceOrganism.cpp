#include "LockSequenceOrganism.h"

namespace hudoverlay {

void LockSequenceOrganism::setup() {
	reticle.setup();
	hud::ReticleOptions ro;
	ro.preset      = hud::ReticlePreset::Standard;
	ro.targetCount = 0; // ambient pool empty; only triggerAt() one-shots ever appear
	ro.showLabels  = false;
	reticle.setOptions(ro);
}

void LockSequenceOrganism::setTheme(const hud::HudTheme& theme) {
	reticle.setTheme(theme);
	burst.setTheme(theme);
	callouts.setTheme(theme);
}

void LockSequenceOrganism::setBounds(float x, float y, float w, float h) {
	reticle.setBounds(x, y, w, h);
	burst.setBounds(x, y, w, h);
	callouts.setBounds(x, y, w, h);
}

void LockSequenceOrganism::trigger(float nx, float ny, hud::CalloutAnchor statusCorner) {
	int xPix = static_cast<int>(nx * 1000.0f);
	int yPix = static_cast<int>(ny * 1000.0f);
	std::string coordText = "X:" + ofToString(xPix) + " Y:" + ofToString(yPix);

	armSequence({
	                { 0.00f, [this, nx, ny]() { reticle.triggerAt(nx, ny, "", 1.3f); } },
	                { 0.15f, [this, nx, ny]() { burst.triggerAt(nx, ny); } },
	                { 0.30f, [this, nx, ny, coordText]() { callouts.triggerAtPoint(nx, ny, coordText); } },
	                { 0.45f, [this, statusCorner]() { callouts.triggerAtCorner(statusCorner, "TARGET ACQUIRED"); } },
	            },
	    1.8f);
}

void LockSequenceOrganism::update(float dt) {
	tickSequence(dt);
	reticle.update(dt);
	burst.update(dt);
	callouts.update(dt);
}

void LockSequenceOrganism::draw() {
	reticle.draw();
	burst.draw();
	callouts.draw();
}

} // namespace hudoverlay
