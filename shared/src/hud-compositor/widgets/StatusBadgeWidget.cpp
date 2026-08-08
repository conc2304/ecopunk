#include "StatusBadgeWidget.h"

#include "HudWidgetDrawUtils.h"

namespace hudpresent {

void StatusBadgeWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	if ((!valueRole || !valueRole->shouldRender) && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;

	ofSetColor(scaledAlpha(colors.background, opacity));
	ofFill();
	ofDrawRectangle(bounds);
	drawOutline(bounds, scaledAlpha(colors.muted, opacity));

	float pad = wu(bounds, 4.0f);
	// Fixed native pixel-space budget (see HudWidgetDrawUtils.h's v1
	// typography strategy comment) — not scale-adjusted.
	float localMaxWidth = std::max(0.0f, bounds.width - pad * 2.0f);

	if (!input.captionText.empty()) {
		ofSetColor(scaledAlpha(colors.muted, opacity));
		const auto& truncatedCaption = textCache_.truncateToWidth(input.captionText, localMaxWidth);
		drawText(truncatedCaption.text, bounds.x + pad, bounds.y + wu(bounds, 12.0f));
	}

	if (valueRole && valueRole->shouldRender) {
		ofSetColor(scaledAlpha(valueRole->useAmbientFallback ? colors.muted : colors.accent, opacity));
		const auto& truncatedValue = textCache_.truncateToWidth(valueRole->formattedText, localMaxWidth);
		drawText(truncatedValue.text, bounds.x + pad, bounds.y + bounds.height - wu(bounds, 6.0f));
	}

	ofPopStyle();
}

} // namespace hudpresent
