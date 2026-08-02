#include "WindowChrome.h"
#include "../shared/HudUtils.h"

namespace hud {

void WindowChrome::draw() {
	ofPushStyle();
	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD);
	else ofEnableAlphaBlending();

	frame.draw(bounds, theme.colors, theme.frame, time);
	drawTitleBar();

	ofDisableBlendMode();
	ofPopStyle();
}

void WindowChrome::drawTitleBar() const {
	float barHeight = su(bounds, 16.0f, 160.0f);
	float pad = su(bounds, 4.0f, 160.0f);

	ofSetColor(scaledAlpha(theme.colors.background, motion.opacity));
	ofFill();
	ofDrawRectangle(bounds.x, bounds.y, bounds.width, barHeight);

	ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity));
	ofDrawLine(bounds.x, bounds.y + barHeight, bounds.x + bounds.width, bounds.y + barHeight);

	float closeSize = barHeight - pad * 2.0f;
	if (options.showCloseBox && closeSize > 2.0f) {
		drawCloseBox(bounds.x + bounds.width - closeSize - pad, bounds.y + pad, closeSize);
	}

	ofSetColor(scaledAlpha(theme.colors.primary, motion.opacity));
	float labelRightEdge = options.showCloseBox ? (bounds.width - closeSize - pad * 3.0f) : (bounds.width - pad * 2.0f);
	drawTextFallback(options.label, bounds.x + pad, bounds.y + barHeight - pad,
		theme.textScale * su(bounds, 0.42f, 160.0f));
	(void)labelRightEdge; // reserved for future label truncation/eliding
}

void WindowChrome::drawCloseBox(float x, float y, float size) const {
	ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity));
	ofNoFill();
	ofSetLineWidth(1.0f);
	ofDrawRectangle(x, y, size, size);
	float inset = size * 0.28f;
	ofDrawLine(x + inset, y + inset, x + size - inset, y + size - inset);
	ofDrawLine(x + size - inset, y + inset, x + inset, y + size - inset);
}

} // namespace hud
