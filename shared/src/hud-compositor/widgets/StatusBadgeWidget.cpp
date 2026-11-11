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

	// "frameless" (profile-declared, e.g. the scene primary-state badge in universal.activity): no
	// widget-drawn backing or outline - the skin's own panel is the only frame, so a passive
	// status readout never looks like a focus/interactive border.
	if (input.variantId != "frameless") {
		ofSetColor(scaledAlpha(colors.background, opacity));
		ofFill();
		ofDrawRectangle(bounds);
		drawOutline(bounds, scaledAlpha(colors.muted, opacity));
	}

	float pad = containerPad(bounds);
	// Fixed native pixel-space budget (see HudWidgetDrawUtils.h's v1
	// typography strategy comment) — not scale-adjusted.
	float localMaxWidth = std::max(0.0f, bounds.width - pad * 2.0f);

	if (singleLine) {
		const float baseline = bounds.y + bounds.height * 0.5f + 4.0f;
		ofSetColor(scaledAlpha(captionColor(colors, valueRole), opacity));
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
		ofSetColor(scaledAlpha(captionColor(colors, valueRole), opacity));
		const auto& truncatedCaption = textCache_.truncateToWidth(input.captionText, localMaxWidth);
		drawText(truncatedCaption.text, bounds.x + pad, captionBaselineY(bounds, wu(bounds, 12.0f)));
	}

	if (valueRole && valueRole->shouldRender) {
		// Native-metric bottom clearance: the scaled inset alone (2.16 px in a
		// 43.2 px region) put the value against the region edge, touching the
		// next region's caption (universal.primary_state over activity).
		const float baseline = bottomBaselineY(bounds, wu(bounds, 6.0f));
		float valueX = bounds.x + pad;
		float valueRight = bounds.getRight() - pad;

		// v0.3 reserved phase-icon slot: a cap-height+ square left of the
		// value, keyed by the icon role's raw identifier.
		const auto* iconRole = input.findRole("icon");
		if (iconRole && iconRole->shouldRender && iconRole->value.present && iconRole->value.dataClass != HudDataClass::Ambient
			&& !iconRole->useAmbientFallback && !valueRole->useAmbientFallback) {
			const float side = kBitmapCapHeight + 4.0f;
			ofRectangle square(valueX, baseline - kBitmapCapHeight - 2.0f, side, side);
			drawReservedIcon(iconRole->value.textValue, square, scaledAlpha(colors.accent, opacity));
			valueX += side + 6.0f;
		}

		// v0.3 detail (e.g. strategy/effect name), right-aligned, caption
		// colour, given at most ~45% of the line.
		const auto* detailRole = input.findRole("detail");
		// Optional roles draw only real values — never the binding's missing-data
		// placeholder ("-", tagged HudDataClass::Ambient by HudMissingDataController).
		if (detailRole && detailRole->shouldRender && detailRole->value.present && detailRole->value.dataClass != HudDataClass::Ambient
			&& !detailRole->useAmbientFallback
			&& !detailRole->formattedText.empty()) {
			const auto& detail = textCache_.truncateToWidth(detailRole->formattedText, localMaxWidth * 0.45f);
			ofSetColor(scaledAlpha(captionColor(colors, detailRole), opacity));
			drawText(detail.text, valueRight - detail.widthLocal, baseline);
			valueRight -= detail.widthLocal + 10.0f;
		}

		ofSetColor(scaledAlpha(valueRole->useAmbientFallback ? colors.muted : colors.accent, opacity));
		const auto& truncatedValue = textCache_.truncateToWidth(valueRole->formattedText, std::max(0.0f, valueRight - valueX));
		drawText(truncatedValue.text, valueX, baseline);
	}

	ofPopStyle();
}

} // namespace hudpresent
