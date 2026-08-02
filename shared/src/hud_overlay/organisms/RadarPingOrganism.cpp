#include "RadarPingOrganism.h"

namespace hudoverlay {

static const std::vector<std::string> kPingLabels = { "CONTACT", "TRACE", "PING", "ECHO" };

void RadarPingOrganism::setup() {
	reticle.setup();
	hud::ReticleOptions ro;
	ro.preset      = hud::ReticlePreset::Standard;
	ro.targetCount = 0;
	ro.showLabels  = false;
	reticle.setOptions(ro);
}

void RadarPingOrganism::setTheme(const hud::HudTheme& theme) {
	reticle.setTheme(theme);
	callouts.setTheme(theme);
}

void RadarPingOrganism::setBounds(float x, float y, float w, float h) {
	reticle.setBounds(x, y, w, h);
	callouts.setBounds(x, y, w, h);
}

void RadarPingOrganism::trigger(float nx, float ny) {
	std::string label = kPingLabels[static_cast<int>(ofRandom(kPingLabels.size()))];
	armSequence({
	                { 0.0f, [this, nx, ny]() { reticle.triggerAt(nx, ny, "", 0.9f); } },
	                { 0.1f, [this, nx, ny, label]() { callouts.triggerAtPoint(nx, ny, label); } },
	            },
	    1.4f);
}

void RadarPingOrganism::update(float dt) {
	tickSequence(dt);
	reticle.update(dt);
	callouts.update(dt);
}

void RadarPingOrganism::draw() {
	reticle.draw();
	callouts.draw();
}

} // namespace hudoverlay
