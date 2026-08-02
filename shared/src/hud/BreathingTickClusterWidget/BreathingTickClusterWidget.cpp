#include "BreathingTickClusterWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void BreathingTickClusterWidget::setup() {
	setAnchors({ { 0.08f, 0.18f }, { 0.08f, 0.5f }, { 0.08f, 0.82f } }, { "Z-01", "Z-02", "Z-03" });
}

void BreathingTickClusterWidget::setAnchors(const std::vector<ofVec2f>& positionsNorm,
    const std::vector<std::string>& labels) {
	anchors.clear();
	for (std::size_t i = 0; i < positionsNorm.size(); i++) {
		Anchor a;
		a.posNorm    = positionsNorm[i];
		a.label      = i < labels.size() ? labels[i] : "";
		a.driftPhase = ofRandom(TWO_PI);
		anchors.push_back(a);
	}
}

void BreathingTickClusterWidget::update(float dt) { HudWidget::update(dt); }

void BreathingTickClusterWidget::draw() {
	if (anchors.empty()) return;
	ofPushStyle();
	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
	ofSetLineWidth(std::max(1.0f, su(bounds, 1.0f)));

	float baseLen = su(bounds, 4.0f);
	float spacing = su(bounds, 6.0f);

	for (const auto& anchor : anchors) {
		float driftX = std::sin(time * TWO_PI / options.driftPeriod + anchor.driftPhase) * options.driftAmountNorm;
		float driftY = std::cos(time * TWO_PI / options.driftPeriod * 0.7f + anchor.driftPhase) * options.driftAmountNorm;
		ofVec2f origin = pointInBounds(bounds, anchor.posNorm.x + driftX, anchor.posNorm.y + driftY);

		for (int i = 0; i < options.ticksPerAnchor; i++) {
			float tickLen  = baseLen * (i + 1);
			float phase    = i * (TWO_PI / std::max(1, options.ticksPerAnchor)) * 0.5f;
			float pulse    = breathe(time + phase, 1.0f / options.breathPeriod, 0.4f, 1.0f);
			float x        = origin.x + spacing * i;
			float halfLen  = (tickLen * pulse) * 0.5f;

			ofSetColor(scaledAlpha(theme.colors.primary, motion.opacity * pulse * 0.8f));
			ofDrawLine(x, origin.y - halfLen, x, origin.y + halfLen);
		}

		ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity * 0.7f));
		drawTextFallback(anchor.label, origin.x, origin.y + baseLen * options.ticksPerAnchor * 0.5f + su(bounds, 10.0f),
		    theme.textScale * su(bounds, 0.55f));
	}

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
