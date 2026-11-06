#include "NumericValueWidget.h"

#include "HudWidgetDrawUtils.h"
#include "../HudIconPath.h"

#include <utility>

namespace hudpresent {

namespace {

// "clock" variant: time at 2x (bold), date/zone in native size.
constexpr float kClockTimeScale = 2.0f;
// "inline" variant: caption and value share one line at 1.2x.
constexpr float kInlineScale = 1.2f;
// Gap between the inline caption and its value, in native glyph cells.
constexpr float kInlineGapGlyphs = 4.0f;
// "inline.col" variant: the value starts at a fixed glyph column so stacked status lines
// ("TRAIL STATUS" / "PRESET") share one value column. 16 cells = the longest caption (12) + gap.
constexpr float kInlineColumnGlyphs = 16.0f;
constexpr float kVariantPad = 4.0f; // >= kContainerPad (3 px)
constexpr float kBitmapLineHeight = 13.0f;

// "THU 15 OCT 2026 CEST" -> {"THU 15 OCT", "2026 CEST"}: the first three
// words (weekday day month) on top, the rest (year zone) below.
std::pair<std::string, std::string> splitClockLabel(const std::string& label) {
	size_t pos = 0;
	for (int words = 0; words < 3; ++words) {
		pos = label.find(' ', pos);
		if (pos == std::string::npos) return { label, std::string() };
		if (words < 2) ++pos;
	}
	return { label.substr(0, pos), label.substr(pos + 1) };
}

} // namespace

void NumericValueWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	if ((!valueRole || !valueRole->shouldRender) && !input.regionAmbientFallback) return;

	if (input.variantId == "clock") {
		drawClock(bounds, colors, input);
		return;
	}
	if (input.variantId == "inline" || input.variantId == "inline.col") {
		drawInline(bounds, colors, input);
		return;
	}

	ofPushStyle();
	ofEnableAlphaBlending();

	float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;

	float pad = containerPad(bounds);
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

// Time on the left at 2x, bold; date on the right as two native-size lines
// ("THU 15 OCT" / "2026 CEST"). The time's cap height is centred on the
// two-line date block, and both are centred in the region.
void NumericValueWidget::drawClock(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	ofPushStyle();
	ofEnableAlphaBlending();

	const float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;
	const float left = bounds.x + kVariantPad;
	const float right = bounds.getRight() - kVariantPad;
	const float centerY = bounds.getCenter().y;

	float timeRight = left;
	if (valueRole && valueRole->shouldRender) {
		const auto& time = textCache_.truncateToWidth(valueRole->formattedText, (right - left) / kClockTimeScale);
		ofSetColor(scaledAlpha(valueRole->useAmbientFallback ? colors.muted : colors.primary, opacity));
		drawTextScaled(time.text, left, centerY + kBitmapCapHeight * kClockTimeScale * 0.5f, kClockTimeScale, true);
		timeRight = left + hudTextWidth(time.text, time.widthLocal, kClockTimeScale) + 1.0f;
	}

	const auto* labelRole = input.findRole("label");
	if (labelRole && labelRole->shouldRender && !labelRole->useAmbientFallback && !labelRole->formattedText.empty()) {
		const auto lines = splitClockLabel(labelRole->formattedText);
		const float budget = std::max(0.0f, right - timeRight - 12.0f);
		const auto& top = textCache_.truncateToWidth(lines.first, budget);
		const auto& bottom = textCache_.truncateToWidth(lines.second, budget);
		const float blockX = right - std::max(hudTextWidth(top.text, top.widthLocal), hudTextWidth(bottom.text, bottom.widthLocal));
		// System-state icon (sun by day, moon at night; not tied to the selected theme) centred in the gap between the time and
		// the date, in the time's colour. Drawn only when it fits with a few pixels either side.
		{
			const float gap = blockX - timeRight;
			const float iconSize = std::min(18.0f, gap - 6.0f);
			if (iconSize >= 10.0f) {
				if (!iconsBuilt_) {
					auto build = [](ofPath& path, const char* d) {
						std::vector<hudicon::Contour> contours;
						path.clear();
						if (!hudicon::parse(d, contours)) return;
						path.setPolyWindingMode(OF_POLY_WINDING_NONZERO);
						for (const auto& c : contours) {
							path.moveTo(c.pts.front().x, c.pts.front().y);
							for (size_t i = 1; i < c.pts.size(); ++i) path.lineTo(c.pts[i].x, c.pts[i].y);
							path.close();
						}
						path.setFilled(true);
						path.setStrokeWidth(0.0f);
					};
					build(sunPath_, hudicon::lightModePath());
					build(moonPath_, hudicon::darkModePath());
					iconsBuilt_ = true;
				}
				ofPath& icon = colors.daytime ? sunPath_ : moonPath_;
				// Take the current ofSetColor (exactly what the time text uses) rather than the path's own cached fill colour.
				icon.setUseShapeColor(false);
				ofColor iconColor = scaledAlpha(valueRole->useAmbientFallback ? colors.muted : colors.primary, opacity);
				// Skin modes: the typography preset recolours the time text; follow it.
				if (const auto* hook = hudpresent::devTextHook()) iconColor = hook->textColor(hudpresent::devTextRegion(), iconColor);
				iconColor.a = static_cast<unsigned char>(iconColor.a * 0.7f + 0.5f); // the icon is a quieter 70% of the time's colour
				ofSetColor(iconColor);
				ofPushMatrix();
				ofTranslate(timeRight + (gap - iconSize) * 0.5f, centerY - iconSize * 0.5f);
				ofScale(iconSize / 24.0f, iconSize / 24.0f);
				icon.draw();
				ofPopMatrix();
			}
		}
		// Block spans (top caps) .. (bottom baseline): centre it on centerY.
		const float topBaseline = centerY - (kBitmapLineHeight - kBitmapCapHeight) * 0.5f;
		ofSetColor(scaledAlpha(captionColor(colors, valueRole), opacity));
		drawText(top.text, blockX, topBaseline);
		if (!bottom.text.empty()) drawText(bottom.text, blockX, topBaseline + kBitmapLineHeight);
	}

	ofPopStyle();
}

// "CAPTION    VALUE" on a single line at 1.2x, vertically centred.
void NumericValueWidget::drawInline(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	// A caption with no value reads as a dead label: an inline status row with nothing
	// to show (e.g. Temporal's never-published field_activity) draws nothing.
	if (!valueRole || !valueRole->shouldRender || valueRole->useAmbientFallback) return;
	ofPushStyle();
	ofEnableAlphaBlending();

	const float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;
	const float left = bounds.x + std::max(kVariantPad, kContainerPad);
	const float availWidth = std::max(0.0f, bounds.getRight() - std::max(kVariantPad, kContainerPad) - left);
	// Scale down from 1.2x when caption + gap + value would not fit the
	// region (flexible.secondary_a is 192 px wide, not the 410 px it was).
	const bool column = input.variantId == "inline.col";
	float naturalLocal = HudTextMetricsCache::kGlyphWidthLocal * kInlineGapGlyphs;
	if (column) naturalLocal = HudTextMetricsCache::kGlyphWidthLocal * kInlineColumnGlyphs; // caption + gap collapse into the fixed column
	else if (!input.captionText.empty()) naturalLocal += textCache_.truncateToWidth(input.captionText, 1e4f).widthLocal;
	if (valueRole && valueRole->shouldRender) naturalLocal += textCache_.truncateToWidth(valueRole->formattedText, 1e4f).widthLocal;
	const float inlineScale = naturalLocal > 0.0f ? std::min(kInlineScale, std::max(0.6f, availWidth / naturalLocal)) : kInlineScale;
	const float maxLocal = availWidth / inlineScale;
	const float baseline = bounds.getCenter().y + kBitmapCapHeight * inlineScale * 0.5f;

	float x = left;
	if (!input.captionText.empty()) {
		const auto& caption = textCache_.truncateToWidth(input.captionText, maxLocal * 0.6f);
		ofSetColor(scaledAlpha(captionColor(colors, valueRole), opacity));
		drawTextScaled(caption.text, x, baseline, inlineScale);
		x += (column ? HudTextMetricsCache::kGlyphWidthLocal * kInlineColumnGlyphs
			: caption.widthLocal + HudTextMetricsCache::kGlyphWidthLocal * kInlineGapGlyphs) * inlineScale;
	}

	if (valueRole && valueRole->shouldRender) {
		const float remaining = std::max(0.0f, (bounds.getRight() - std::max(kVariantPad, kContainerPad) - x) / inlineScale);
		const auto& value = textCache_.truncateToWidth(valueRole->formattedText, remaining);
		ofSetColor(scaledAlpha(valueRole->useAmbientFallback ? colors.muted : colors.primary, opacity));
		drawTextScaled(value.text, x, baseline, inlineScale);
	}

	ofPopStyle();
}

} // namespace hudpresent
