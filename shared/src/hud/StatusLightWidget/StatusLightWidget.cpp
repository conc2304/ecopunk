#include "StatusLightWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void StatusLightWidget::update(float dt) {
	HudWidget::update(dt);
	pulsePhase = std::fmod(pulsePhase + dt * options.blinkRate * motion.pulse, 1.0f);
}

void StatusLightWidget::draw() {
	ofPushStyle();
	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD);
	else ofEnableAlphaBlending();

	float r = su(bounds, 5.0f, 24.0f);
	float cx = bounds.x + r + su(bounds, 2.0f, 24.0f);
	float cy = bounds.y + bounds.height * 0.5f;

	float lightAlpha = motion.opacity;
	ofColor lightColor = theme.colors.muted;
	if (options.state == StatusState::Active) {
		lightColor = theme.colors.secondary;
		lightAlpha *= 0.6f + 0.4f * breathe(time, 0.6f);
	} else if (options.state == StatusState::Alert) {
		lightColor = theme.colors.accent;
		if (options.blinkOnAlert) lightAlpha *= (pulsePhase < 0.5f) ? 1.0f : 0.15f;
	}

	ofFill();
	ofSetColor(scaledAlpha(lightColor, lightAlpha));
	ofDrawCircle(cx, cy, r);

	ofNoFill();
	ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity * 0.5f));
	ofSetLineWidth(1.0f);
	ofDrawCircle(cx, cy, r * 1.7f);

	ofSetColor(scaledAlpha(theme.colors.primary, motion.opacity * 0.85f));
	drawTextFallback(options.label, cx + r * 2.2f, cy + su(bounds, 4.0f, 24.0f),
		theme.textScale * su(bounds, 0.6f, 24.0f));

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
