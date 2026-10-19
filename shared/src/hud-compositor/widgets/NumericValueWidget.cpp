#include "NumericValueWidget.h"

#include "HudWidgetDrawUtils.h"

namespace hudpresent {

void NumericValueWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	if ((!valueRole || !valueRole->shouldRender) && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;

	float pad = wu(bounds, 4.0f);
	// Fixed native pixel-space budget (see HudWidgetDrawUtils.h's v1
	// typography strategy comment) — not scale-adjusted.
	float localMaxWidth = std::max(0.0f, bounds.width - pad * 2.0f);

	// The contracted optional "label" role (HudWidgetContract: Identifier/
	// Text) is dynamic caption text — e.g. HUD Clock v0.1's local date/zone
	// line — and takes the caption line when it renders. Bindings with no
	// label source draw the static caption exactly as before.
	const auto* labelRole = input.findRole("label");
	const bool dynamicLabel = labelRole && labelRole->shouldRender && !labelRole->useAmbientFallback
		&& !labelRole->formattedText.empty();
	// With both a static caption and a dynamic label, the line reads
	// "CAPTION LABEL" (e.g. "MODE 3/9"); either alone is drawn as-is.
	const std::string captionLine = !dynamicLabel ? input.captionText
		: input.captionText.empty() ? labelRole->formattedText
		: input.captionText + " " + labelRole->formattedText;
	if (!captionLine.empty()) {
		ofSetColor(scaledAlpha(captionColor(colors, valueRole), opacity));
		const auto& caption = textCache_.truncateToWidth(captionLine, localMaxWidth);
		// Native bitmap glyphs rise ~9 px above their baseline regardless of
		// region size; in short regions (universal.timing 36 px,
		// flexible.secondary_b 43.2 px) the scaled baseline alone put the
		// caption's tops above the region edge. Taller regions are unchanged.
		const float captionBaseline = captionBaselineY(bounds, wu(bounds, 12.0f));
		drawText(caption.text, bounds.x + pad, captionBaseline);
	}

	if (valueRole && valueRole->shouldRender) {
		ofSetColor(scaledAlpha(valueRole->useAmbientFallback ? colors.muted : colors.primary, opacity));
		const auto& value = textCache_.truncateToWidth(valueRole->formattedText, localMaxWidth);
		// Same native-metric rule at the bottom: the scaled inset alone
		// (1.44 px at 43.2 px tall) sat the value on the region edge.
		const float valueBaseline = bottomBaselineY(bounds, wu(bounds, 4.0f));
		drawText(value.text, bounds.x + pad, valueBaseline);
	}

	ofPopStyle();
}

} // namespace hudpresent
