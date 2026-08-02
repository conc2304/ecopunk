#include "HandshakeOrganism.h"
#include <cmath>

namespace hudoverlay {

void HandshakeOrganism::setup() {
	reticles.setup();
	hud::ReticleOptions ro;
	ro.preset      = hud::ReticlePreset::Standard;
	ro.targetCount = 0;
	ro.showLabels  = false;
	reticles.setOptions(ro);

	hud::DashedLineOptions lo;
	lo.drawOnDuration = 0.6f;
	line.setOptions(lo);
}

void HandshakeOrganism::setTheme(const hud::HudTheme& theme) {
	reticles.setTheme(theme);
	line.setTheme(theme);
	label.setTheme(theme);
}

void HandshakeOrganism::setBounds(float x, float y, float w, float h) {
	reticles.setBounds(x, y, w, h);
	line.setBounds(x, y, w, h);
	label.setBounds(x, y, w, h);
}

void HandshakeOrganism::trigger(float nxA, float nyA, float nxB, float nyB, float canvasW, float canvasH) {
	float dx = (nxB - nxA) * canvasW;
	float dy = (nyB - nyA) * canvasH;
	int pixelDist = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));
	std::string distText = ofToString(pixelDist) + "px";
	float midX = (nxA + nxB) * 0.5f, midY = (nyA + nyB) * 0.5f;

	armSequence({
	                { 0.00f, [this, nxA, nyA]() { reticles.triggerAt(nxA, nyA, "", 1.7f); } },
	                { 0.20f, [this, nxB, nyB]() { reticles.triggerAt(nxB, nyB, "", 1.5f); } },
	                { 0.25f, [this, nxA, nyA, nxB, nyB]() { line.triggerBetween(nxA, nyA, nxB, nyB); } },
	                { 0.50f, [this, midX, midY, distText]() { label.triggerAtPoint(midX, midY, distText); } },
	            },
	    2.2f);
}

void HandshakeOrganism::update(float dt) {
	tickSequence(dt);
	reticles.update(dt);
	line.update(dt);
	label.update(dt);
}

void HandshakeOrganism::draw() {
	line.draw();
	reticles.draw();
	label.draw();
}

} // namespace hudoverlay
