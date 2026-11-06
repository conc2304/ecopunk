#include "ProgressBarWidget.h"

#include "HudWidgetDrawUtils.h"
#include "SegmentedBar.h"

#include <algorithm>

namespace hudpresent {

void ProgressBarWidget::draw(const ofRectangle& outer, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	// Same left/right padding as the other containers so "100%" sits inside the panel.
	const float sidePad = containerPad(outer);
	const ofRectangle bounds(outer.x + sidePad, outer.y, std::max(0.0f, outer.width - sidePad * 2.0f), outer.height);
	const auto* valueRole = input.findRole("value");
	if ((!valueRole || !valueRole->shouldRender) && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;

	float localMaxWidth = std::max(0.0f, bounds.width);

	float barY = bounds.y + bounds.height - wu(bounds, 10.0f);
	float barH = std::max(3.0f, wu(bounds, 6.0f));
	const bool live = valueRole && valueRole->shouldRender && !valueRole->useAmbientFallback;
	// HudMissingPolicy::Placeholder substitutes a present-but-Ambient value (never a measurement):
	// show an explicit "--" and an empty bar, never a "0%" that could pass for real progress.
	const bool unavailable = live && valueRole->value.dataClass == HudDataClass::Ambient;
	const std::string valueText = unavailable ? std::string("--") : (valueRole ? valueRole->formattedText : std::string());

	// A slot too short for caption + value + bar on separate lines (e.g. the
	// 19.2 px progress slot of overlay.transition) draws "CAPTION value" on
	// one line above the bar, so neither line rises into the region above.
	const bool singleLine = !input.captionText.empty() && bounds.height < kBitmapTwoLineMinHeight;
	// Optional trailing status (e.g. the scene health beside "TRANSITION x%"),
	// right-aligned in accent on the caption line.
	const auto* statusRole = input.findRole("status");
	const bool hasStatus = statusRole && statusRole->shouldRender && !statusRole->useAmbientFallback && !statusRole->formattedText.empty();
	float statusW = 0.0f;
	if (hasStatus) {
		const auto& st = textCache_.truncateToWidth(statusRole->formattedText, localMaxWidth * 0.5f);
		statusW = hudTextWidth(st.text, st.widthLocal);
		ofSetColor(scaledAlpha(colors.primary, opacity));
		drawText(st.text, bounds.getRight() - statusW, captionBaselineY(bounds, wu(bounds, 10.0f)));
	}
	if (!input.captionText.empty()) {
		ofSetColor(scaledAlpha(captionColor(colors, valueRole), opacity));
		const auto& caption = textCache_.truncateToWidth(input.captionText, localMaxWidth);
		const float baseline = captionBaselineY(bounds, wu(bounds, 10.0f));
		// "centered" variant: the "CAPTION value" line is centred horizontally.
		float lineX = bounds.x;
		if (input.variantId == "centered") {
			const float captionW = hudTextWidth(caption.text, caption.widthLocal);
			float total = captionW;
			if (singleLine && live) total += 8.0f + hudTextWidth(valueText, textCache_.truncateToWidth(valueText, localMaxWidth).widthLocal);
			lineX = bounds.getCenter().x - total * 0.5f;
		}
		drawText(caption.text, lineX, baseline);
		if (singleLine && live) {
			const float valueX = lineX + hudTextWidth(caption.text, caption.widthLocal) + 8.0f;
			ofSetColor(scaledAlpha(colors.primary, opacity));
			const auto& value = textCache_.truncateToWidth(valueText, std::max(0.0f, bounds.getRight() - valueX - (hasStatus ? statusW + 8.0f : 0.0f)));
			drawText(value.text, valueX, baseline);
		}
	}

	float ratio = 0.0f;
	bool ambient = valueRole && valueRole->useAmbientFallback;
	if (valueRole && valueRole->shouldRender && !ambient && !unavailable) {
		ratio = std::clamp(valueRole->value.numberValue, 0.0f, 1.0f);
	} else if (ambient) {
		// Deterministic ambient motion — a slow sweep driven by the
		// orchestrator-supplied elapsedSeconds, per HudWidgetBase.h's "no
		// independent data-sampling clock" rule (this reads time as data,
		// it does not accumulate its own).
		ratio = 0.5f + 0.5f * std::sin(input.elapsedSeconds * 0.6f);
	}

	// variantId "segmented": the level is a SegmentedBar instead of the linear
	// track. Two-line slots put the value text and the bar on one row (text left,
	// bar fills the rest); a single-line slot keeps the bar under the caption line.
	if (input.variantId == "segmented") {
		SegmentedBarStyle style;
		style.segments = 12;
		style.opacity = opacity;
		if (ambient) style.color = colors.muted;

		ofRectangle barRect;
		if (singleLine) {
			const float h = std::clamp(bounds.height - 15.0f, 4.0f, 6.0f);
			barRect.set(bounds.x, bounds.getBottom() - h - 1.0f, bounds.width, h);
		} else {
			const float baseline = bottomBaselineY(bounds, wu(bounds, 10.0f));
			const float h = kBitmapCapHeight + 1.0f;
			// Reserve the width of "100%" so the bar does not resize as the text changes.
			const float textW = hudTextWidth("100%", textCache_.truncateToWidth("100%", localMaxWidth).widthLocal);
			const float barX = bounds.x + textW + 8.0f;
			barRect.set(barX, baseline - kBitmapCapHeight - 0.5f, std::max(0.0f, bounds.getRight() - barX), h);
			if (live) {
				ofSetColor(scaledAlpha(unavailable ? colors.caption : colors.primary, opacity));
				drawText(textCache_.truncateToWidth(valueText, textW).text, bounds.x, baseline);
			}
		}
		drawSegmentedBar(barRect, ratio, style, colors);
		ofPopStyle();
		return;
	}

	ofNoFill();
	ofSetColor(scaledAlpha(colors.muted, opacity * 0.6f));
	ofDrawRectangle(bounds.x, barY, bounds.width, barH);

	ofFill();
	ofSetColor(scaledAlpha(ambient ? colors.muted : colors.primary, opacity));
	ofDrawRectangle(bounds.x, barY, bounds.width * ratio, barH);

	if (live && !singleLine) {
		ofSetColor(scaledAlpha(unavailable ? colors.caption : colors.primary, opacity));
		const auto& value = textCache_.truncateToWidth(valueText, localMaxWidth);
		drawText(value.text, bounds.x, barY - wu(bounds, 4.0f));
	}

	ofPopStyle();
}

} // namespace hudpresent
