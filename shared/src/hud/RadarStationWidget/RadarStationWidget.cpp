#include "RadarStationWidget.h"
#include "../shared/HudUtils.h"
#include <cmath>

namespace hud {

// ofDrawArc/ofPath::arc are not used in this library (see GaugeWidget.cpp) —
// build the wedge as a manual triangle fan instead.
static void drawWedge(const ofVec2f& c, float r, float aDeg0, float aDeg1, int steps = 12) {
	ofPath path;
	path.setFilled(true);
	path.setCurveResolution(2);
	path.moveTo(c.x, c.y);
	for (int i = 0; i <= steps; ++i) {
		float a = ofDegToRad(ofLerp(aDeg0, aDeg1, i / static_cast<float>(steps)));
		path.lineTo(c.x + std::cos(a) * r, c.y + std::sin(a) * r);
	}
	path.close();
	path.draw();
}

void RadarStationWidget::update(float dt) {
	HudWidget::update(dt);
	float period = std::max(0.1f, options.rotationPeriod / std::max(0.01f, motion.speed));
	rotationDeg += (360.0f / period) * dt;
	rotationDeg = std::fmod(rotationDeg, 360.0f);
	if (rotationDeg < 0.0f) rotationDeg += 360.0f;
}

void RadarStationWidget::draw() {
	ofPushStyle();
	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();

	ofVec2f center = bounds.center();
	float   radius = bounds.minDim() * options.ringRadiusNorm;

	// Ring.
	ofNoFill();
	ofSetLineWidth(std::max(1.0f, su(bounds, 1.0f)));
	ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity * 0.6f));
	ofDrawCircle(center, radius);

	// Tick marks — brightness is a direct function of how recently the
	// wedge's leading edge swept past this tick's angle, so the flash is
	// phase-locked to the wedge by construction (same rotationDeg drives
	// both), not via independently-timed per-tick animation.
	float tickLen = su(bounds, 6.0f);
	for (int i = 0; i < options.tickCount; i++) {
		float tickAngleDeg = (360.0f / options.tickCount) * i;
		float delta = std::fmod(rotationDeg - tickAngleDeg + 360.0f, 360.0f);
		float decayWindowDeg = std::max(1.0f, 360.0f / options.tickCount) * 3.0f;
		float flash = (delta < decayWindowDeg) ? std::exp(-delta / (decayWindowDeg * 0.35f)) : 0.0f;
		float alpha = ofLerp(0.18f, 1.0f, flash) * motion.opacity;

		float a = ofDegToRad(tickAngleDeg);
		ofVec2f dir(std::cos(a), std::sin(a));
		ofVec2f inner = center + dir * (radius - tickLen * 0.5f);
		ofVec2f outer = center + dir * (radius + tickLen * 0.5f);
		ofSetColor(scaledAlpha(theme.colors.accent, alpha));
		ofDrawLine(inner, outer);
	}

	// Wedge.
	ofFill();
	ofSetColor(scaledAlpha(theme.colors.accent, motion.opacity * 0.32f));
	drawWedge(center, radius, rotationDeg, rotationDeg + options.wedgeArcDeg);

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
