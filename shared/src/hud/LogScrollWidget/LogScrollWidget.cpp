#include "LogScrollWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void LogScrollWidget::setup() {
	lines.clear();
	scrollOffset = 0.0f;
}

void LogScrollWidget::pushLine(const std::string & line) {
	lines.push_back(line);
	while (static_cast<int>(lines.size()) > options.maxLines) lines.pop_front();
}

void LogScrollWidget::update(float dt) {
	HudWidget::update(dt);
	lineHeightPx = std::max(8.0f, su(bounds, 12.0f, 90.0f));
	scrollOffset += options.scrollSpeed * motion.speed * dt;
	if (scrollOffset >= lineHeightPx) scrollOffset = std::fmod(scrollOffset, lineHeightPx);
}

void LogScrollWidget::draw() {
	if (lines.empty()) return;
	ofPushStyle();
	ofFill();
	ofSetColor(scaledAlpha(theme.colors.background, motion.opacity));
	ofDrawRectangle(bounds.rect());

	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD);
	else ofEnableAlphaBlending();

	if (options.showFrame) frame.draw(bounds, theme.colors, theme.frame, time);

	float p = theme.frame.padding + su(bounds, 4.0f, 160.0f);
	float textScale = theme.textScale * su(bounds, 0.55f, 160.0f);

	// Newest line anchored near the bottom, drifting upward continuously;
	// lines above it fade out approaching the top edge of bounds.
	float baseY = bounds.y + bounds.height - p - scrollOffset;
	int n = static_cast<int>(lines.size());
	for (int i = 0; i < n; i++) {
		const std::string & text = lines[n - 1 - i];
		float y = baseY - i * lineHeightPx;
		if (y < bounds.y + p * 0.5f || y > bounds.y + bounds.height) continue;
		float topFade = ofClamp((y - bounds.y) / (bounds.height * 0.3f), 0.0f, 1.0f);
		ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity * topFade));
		drawTextFallback(text, bounds.x + p, y, textScale);
	}

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
