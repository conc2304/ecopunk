#include "MetadataCardWidget.h"

#include "HudWidgetDrawUtils.h"

namespace hudpresent {

void MetadataCardWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* titleRole = input.findRole("title");
	if ((!titleRole || !titleRole->shouldRender) && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	float opacity = (titleRole && titleRole->dimmed) ? 0.5f : 1.0f;

	ofFill();
	ofSetColor(scaledAlpha(colors.background, opacity));
	ofDrawRectangle(bounds);
	drawOutline(bounds, scaledAlpha(colors.muted, opacity));

	float pad = wu(bounds, 6.0f);
	float y = bounds.y + pad + wu(bounds, 8.0f);
	// Fixed native pixel-space budget (see HudWidgetDrawUtils.h's v1
	// typography strategy comment) — not scale-adjusted.
	float localMaxWidth = std::max(0.0f, bounds.width - pad * 2.0f);

	if (!input.captionText.empty()) {
		ofSetColor(scaledAlpha(colors.muted, opacity * 0.9f));
		const auto& t = textCache_.truncateToWidth(input.captionText, localMaxWidth);
		drawText(t.text, bounds.x + pad, y);
		y += wu(bounds, 14.0f);
	}

	if (titleRole && titleRole->shouldRender) {
		ofSetColor(scaledAlpha(titleRole->useAmbientFallback ? colors.muted : colors.primary, opacity));
		const auto& t = textCache_.truncateToWidth(titleRole->formattedText, localMaxWidth);
		drawText(t.text, bounds.x + pad, y);
		y += wu(bounds, 16.0f);
	}

	if (const auto* valueRole = input.findRole("value")) {
		if (valueRole->shouldRender) {
			ofSetColor(scaledAlpha(colors.secondary, opacity));
			const auto& t = textCache_.truncateToWidth(valueRole->formattedText, localMaxWidth);
			drawText(t.text, bounds.x + pad, y);
			y += wu(bounds, 14.0f);
		}
	}

	if (const auto* metaRole = input.findRole("meta")) {
		if (metaRole->shouldRender) {
			ofSetColor(scaledAlpha(colors.muted, opacity));
			const auto& t = textCache_.truncateToWidth(metaRole->formattedText, localMaxWidth);
			drawText(t.text, bounds.x + pad, y);
		}
	}

	ofPopStyle();
}

} // namespace hudpresent
