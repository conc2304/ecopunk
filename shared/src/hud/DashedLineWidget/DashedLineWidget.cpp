#include "DashedLineWidget.h"
#include "../shared/HudUtils.h"
#include <algorithm>

namespace hud {

void DashedLineWidget::triggerBetween(float nxA, float nyA, float nxB, float nyB) {
	lines.push_back({ ofVec2f(nxA, nyA), ofVec2f(nxB, nyB), 0.0f });
}

void DashedLineWidget::update(float dt) {
	HudWidget::update(dt);
	float total = options.drawOnDuration + options.holdDuration + options.fadeOutDuration;
	for (auto& l : lines) l.age += dt;
	lines.erase(std::remove_if(lines.begin(), lines.end(),
	                [total](const Line& l) { return l.age >= total; }),
	    lines.end());
}

void DashedLineWidget::draw() {
	if (lines.empty()) return;
	ofPushStyle();
	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
	ofSetLineWidth(std::max(1.0f, su(bounds, 1.0f)));

	for (const auto& l : lines) {
		float drawU = ofClamp(l.age / options.drawOnDuration, 0.0f, 1.0f);
		float alpha = motion.opacity;
		if (l.age > options.drawOnDuration + options.holdDuration) {
			float u = ofClamp((l.age - options.drawOnDuration - options.holdDuration)
			        / std::max(0.001f, options.fadeOutDuration), 0.0f, 1.0f);
			alpha *= (1.0f - u);
		}

		ofVec2f a = pointInBounds(bounds, l.a.x, l.a.y);
		ofVec2f b = pointInBounds(bounds, ofLerp(l.a.x, l.b.x, drawU), ofLerp(l.a.y, l.b.y, drawU));

		float totalLen = a.distance(b);
		float dash = su(bounds, options.dashLenNorm * bounds.minDim());
		float gap  = su(bounds, options.gapLenNorm * bounds.minDim());
		float step = std::max(1.0f, dash + gap);
		ofVec2f dir = (totalLen > 0.0001f) ? (b - a) / totalLen : ofVec2f(0, 0);

		ofSetColor(scaledAlpha(theme.colors.secondary, alpha * 0.85f));
		for (float d = 0.0f; d < totalLen; d += step) {
			float segEnd = std::min(d + dash, totalLen);
			ofDrawLine(a + dir * d, a + dir * segEnd);
		}
	}

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
