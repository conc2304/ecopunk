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
	ofSetCircleResolution(96); // default resolution reads as a polygon at this radius

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

	// Range rings + cross axes (opt-in).
	if (options.rangeRings) {
		ofNoFill();
		ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity * 0.28f));
		ofDrawCircle(center, radius * 0.33f);
		ofDrawCircle(center, radius * 0.66f);
		ofDrawLine(center.x - radius, center.y, center.x + radius, center.y);
		ofDrawLine(center.x, center.y - radius, center.x, center.y + radius);
	}

	// Wedge.
	ofFill();
	ofSetColor(scaledAlpha(theme.colors.accent, motion.opacity * 0.32f));
	drawWedge(center, radius, rotationDeg, rotationDeg + options.wedgeArcDeg);

	// Heading — a short own-ship vector from the centre.
	if (headingStrength > 0.01f) {
		float a = ofDegToRad(headingDeg);
		ofVec2f dir(std::cos(a), std::sin(a));
		ofSetColor(scaledAlpha(theme.colors.primary, motion.opacity * ofClamp(headingStrength, 0.0f, 1.0f)));
		ofDrawLine(center, center + dir * radius * 0.30f);
		ofDrawCircle(center, std::max(1.5f, su(bounds, 2.0f)));
	}

	// Contacts — same phase-locked flash as the ticks: brightest just after
	// the wedge's leading edge passes, then an afterglow decay.
	const float blipR = std::max(2.0f, su(bounds, 3.0f));
	for (const RadarContact& c : contacts) {
		float delta = std::fmod(rotationDeg - c.angleDeg + 720.0f, 360.0f);
		float flash = std::exp(-delta / 70.0f);
		float alpha = ofClamp(c.strength, 0.0f, 1.0f) * ofLerp(0.30f, 1.0f, flash) * motion.opacity;
		float a = ofDegToRad(c.angleDeg);
		ofVec2f p = center + ofVec2f(std::cos(a), std::sin(a)) * radius * ofClamp(c.range01, 0.0f, 1.0f);
		ofFill();
		ofSetColor(scaledAlpha(c.primary ? theme.colors.primary : theme.colors.accent, alpha));
		ofDrawCircle(p, c.primary ? blipR * 1.3f : blipR);
		if (c.primary) {
			// Lock brackets around the primary contact.
			ofNoFill();
			float b = blipR * 3.0f, l = blipR * 1.4f;
			ofSetColor(scaledAlpha(theme.colors.primary, ofClamp(c.strength, 0.0f, 1.0f) * motion.opacity));
			ofDrawLine(p.x - b, p.y - b, p.x - b + l, p.y - b); ofDrawLine(p.x - b, p.y - b, p.x - b, p.y - b + l);
			ofDrawLine(p.x + b, p.y - b, p.x + b - l, p.y - b); ofDrawLine(p.x + b, p.y - b, p.x + b, p.y - b + l);
			ofDrawLine(p.x - b, p.y + b, p.x - b + l, p.y + b); ofDrawLine(p.x - b, p.y + b, p.x - b, p.y + b - l);
			ofDrawLine(p.x + b, p.y + b, p.x + b - l, p.y + b); ofDrawLine(p.x + b, p.y + b, p.x + b, p.y + b - l);
		}
	}

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
