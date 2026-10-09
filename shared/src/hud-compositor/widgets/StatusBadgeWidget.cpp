#include "StatusBadgeWidget.h"

#include "HudWidgetDrawUtils.h"

namespace hudpresent {

void StatusBadgeWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	const bool hasValue = valueRole && valueRole->shouldRender;
	// A captioned badge in a slot too short for two stacked native-size
	// lines (e.g. one half of universal.effect_summary) draws caption and
	// value on one vertically-centred line, so neither spills into the
	// neighbouring region — and draws nothing at all without a value, since
	// a caption or empty frame alone would read as data that isn't there.
	const bool singleLine = !input.captionText.empty() && bounds.height < kTwoLineMinHeight;
	if (singleLine && !hasValue) return;
	if (!hasValue && !input.regionAmbientFallback) return;

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

	if (singleLine) {
		const float baseline = bounds.y + bounds.height * 0.5f + 4.0f;
		ofSetColor(scaledAlpha(colors.muted, opacity));
		const auto& caption = textCache_.truncateToWidth(input.captionText, localMaxWidth * 0.5f);
		drawText(caption.text, bounds.x + pad, baseline);
		const float valueX = bounds.x + pad + caption.widthLocal + 8.0f;
		ofSetColor(scaledAlpha(valueRole->useAmbientFallback ? colors.muted : colors.accent, opacity));
		const auto& value = textCache_.truncateToWidth(valueRole->formattedText, std::max(0.0f, bounds.getRight() - pad - valueX));
		drawText(value.text, valueX, baseline);
		ofPopStyle();
		return;
	}

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
