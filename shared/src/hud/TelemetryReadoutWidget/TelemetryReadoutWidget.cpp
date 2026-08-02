#include "TelemetryReadoutWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void TelemetryReadoutWidget::update(float dt) {
	HudWidget::update(dt);
	elapsedSeconds += dt;

	if (options.mode == TelemetryMode::CoordinateWalk) {
		walkAccumMs += dt * 1000.0f;
		while (walkAccumMs >= options.walkStepMs) {
			walkAccumMs -= options.walkStepMs;
			latDrift += ofRandom(-options.walkStepDeg, options.walkStepDeg);
			lonDrift += ofRandom(-options.walkStepDeg, options.walkStepDeg);
		}
	}
}

void TelemetryReadoutWidget::draw() {
	ofPushStyle();
	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
	ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity * 0.8f));

	float textScale = theme.textScale * su(bounds, 0.55f);

	if (options.mode == TelemetryMode::FrameCounter) {
		long frame = static_cast<long>(elapsedSeconds * options.fps);
		std::string text = "FRAME " + ofToString(frame);
		float w = text.length() * 8.0f * textScale;
		drawTextFallback(text, bounds.x + bounds.width - w - su(bounds, 6.0f),
		    bounds.y + bounds.height - su(bounds, 8.0f), textScale);
	} else {
		double lat = options.originLat + latDrift;
		double lon = options.originLon + lonDrift;
		std::string text = ofToString(std::abs(lat), 4) + (lat >= 0.0 ? "N " : "S ")
		                  + ofToString(std::abs(lon), 4) + (lon >= 0.0 ? "E" : "W");
		drawTextFallback(text, bounds.x + su(bounds, 6.0f), bounds.y + su(bounds, 14.0f), textScale);
	}

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
