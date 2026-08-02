#include "TickBurstWidget.h"
#include "../shared/HudUtils.h"
#include <algorithm>

namespace hud {

void TickBurstWidget::triggerAt(float nx, float ny) {
	bursts.push_back({ ofVec2f(ofClamp(nx, 0.0f, 1.0f), ofClamp(ny, 0.0f, 1.0f)), 0.0f });
}

void TickBurstWidget::update(float dt) {
	HudWidget::update(dt);
	for (auto& b : bursts) b.age += dt;
	bursts.erase(std::remove_if(bursts.begin(), bursts.end(),
	                 [this](const Burst& b) { return b.age >= options.duration; }),
	    bursts.end());
}

void TickBurstWidget::draw() {
	if (bursts.empty()) return;
	ofPushStyle();
	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
	ofSetLineWidth(std::max(1.0f, su(bounds, 1.2f)));

	for (const auto& b : bursts) {
		float u       = ofClamp(b.age / options.duration, 0.0f, 1.0f);
		float radius  = su(bounds, ofLerp(options.radiusStart, options.radiusEnd, u) * bounds.minDim());
		float len     = su(bounds, options.tickLength * bounds.minDim());
		float alpha   = motion.opacity * (1.0f - u);
		ofVec2f center = pointInBounds(bounds, b.p.x, b.p.y);

		ofSetColor(scaledAlpha(theme.colors.accent, alpha * 0.85f));
		for (int i = 0; i < options.tickCount; i++) {
			float angle = (TWO_PI / options.tickCount) * i;
			ofVec2f dir(std::cos(angle), std::sin(angle));
			ofVec2f inner = center + dir * radius;
			ofVec2f outer = center + dir * (radius + len);
			ofDrawLine(inner, outer);
		}
	}

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
