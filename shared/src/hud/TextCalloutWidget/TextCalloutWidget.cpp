#include "TextCalloutWidget.h"
#include "../shared/HudUtils.h"
#include <algorithm>

namespace hud {

void TextCalloutWidget::triggerAtPoint(float nx, float ny, const std::string& text) {
	callouts.push_back({ text, CalloutAnchor::Point, nx, ny, 0.0f });
}

void TextCalloutWidget::triggerAtCorner(CalloutAnchor anchor, const std::string& text) {
	callouts.push_back({ text, anchor, 0.0f, 0.0f, 0.0f });
}

void TextCalloutWidget::update(float dt) {
	HudWidget::update(dt);
	float total = options.holdDuration + options.fadeOutDuration;
	for (auto& c : callouts) c.age += dt;
	callouts.erase(std::remove_if(callouts.begin(), callouts.end(),
	                   [total](const Callout& c) { return c.age >= total; }),
	    callouts.end());
}

void TextCalloutWidget::draw() {
	if (callouts.empty()) return;
	ofPushStyle();
	if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();

	float textScale = theme.textScale * su(bounds, 0.6f);
	float pad = su(bounds, options.cornerPadding);

	for (const auto& c : callouts) {
		float alpha = motion.opacity;
		if (c.age > options.holdDuration) {
			float u = ofClamp((c.age - options.holdDuration) / std::max(0.001f, options.fadeOutDuration), 0.0f, 1.0f);
			alpha *= (1.0f - u);
		}
		ofSetColor(scaledAlpha(theme.colors.primary, alpha * 0.9f));

		ofVec2f p;
		switch (c.anchor) {
			case CalloutAnchor::Point:
				p = pointInBounds(bounds, c.nx, c.ny);
				p.x += su(bounds, 10.0f);
				break;
			case CalloutAnchor::TopLeft: p = { bounds.x + pad, bounds.y + pad + textScale * 10.0f }; break;
			case CalloutAnchor::TopRight: p = { bounds.x + bounds.width - pad - c.text.length() * 8.0f * textScale, bounds.y + pad + textScale * 10.0f }; break;
			case CalloutAnchor::BottomLeft: p = { bounds.x + pad, bounds.y + bounds.height - pad }; break;
			case CalloutAnchor::BottomRight: p = { bounds.x + bounds.width - pad - c.text.length() * 8.0f * textScale, bounds.y + bounds.height - pad }; break;
		}
		drawTextFallback(c.text, p.x, p.y, textScale);
	}

	ofDisableBlendMode();
	ofPopStyle();
}

} // namespace hud
