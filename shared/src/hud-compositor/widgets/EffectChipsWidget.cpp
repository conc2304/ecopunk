#include "EffectChipsWidget.h"

#include "HudWidgetDrawUtils.h"

namespace hudpresent {

void EffectChipsWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	if ((!valueRole || !valueRole->shouldRender) && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;

	// Optional trailing status (effects health), right-aligned in accent on
	// the first row; the chips wrap before it.
	const auto* statusRole = input.findRole("status");
	const bool hasStatus = statusRole && statusRole->shouldRender && !statusRole->useAmbientFallback && !statusRole->formattedText.empty();
	float rowRight = bounds.getRight();
	if (hasStatus) {
		const auto& st = textCache_.truncateToWidth(statusRole->formattedText, bounds.width * 0.4f);
		const float w = hudTextWidth(st.text, st.widthLocal);
		ofSetColor(scaledAlpha(colors.accent, opacity));
		drawText(st.text, bounds.getRight() - w - 2.0f, bounds.y + std::min(bounds.height, kMinChipHeight + 2.0f) * 0.5f + 4.0f);
		rowRight = bounds.getRight() - w - wu(bounds, 8.0f);
	}

	if (valueRole && valueRole->shouldRender && valueRole->formattedItems.empty()) {
		// Present-but-empty list — a legitimate "no active effects" state,
		// distinct from missing (which would already have returned above
		// via shouldRender==false). Drawn as muted placeholder text, not
		// silently blank, so the empty-vs-missing distinction is visible
		// in the Validation Studio too.
		// Primary at reduced alpha: quieter than live chips, but legible
		// on a skin shell (muted's alpha-90 vanished on Dark Moss).
		// Vertically centred on the same line a chip row's text uses.
		ofSetColor(scaledAlpha(colors.primary, 0.75f * opacity));
		drawText("NONE ACTIVE", bounds.x + wu(bounds, 4.0f), bounds.y + std::min(bounds.height, kMinChipHeight + 2.0f) * 0.5f + 4.0f, wu(bounds, 0.5f, 120.0f));
		ofPopStyle();
		return;
	}

	// Never shorter than one native 13 px bitmap line (see
	// HudWidgetDrawUtils.h's fixed-native typography): the region-scaled
	// height alone collapses to ~2.5 px in a 21.6 px slot.
	float chipHeight = std::max(wu(bounds, 14.0f), kMinChipHeight);
	float chipPad = wu(bounds, 4.0f);
	float x = bounds.x;
	float y = bounds.y + wu(bounds, 2.0f);

	const std::vector<std::string>& items = (valueRole && !valueRole->useAmbientFallback)
		? valueRole->formattedItems
		: std::vector<std::string>{};

	// A single chip's own text may never exceed the region's own live
	// width — truncated to that budget as a backstop BEFORE row-wrapping
	// logic runs, so a pathologically long single effect ID can't produce
	// a chip wider than the region itself even on its own row.
	float maxChipTextWidth = std::max(0.0f, bounds.width - chipPad * 2.0f);

	for (const auto& item : items) {
		const auto& truncated = textCache_.truncateToWidth(item, maxChipTextWidth);
		// Fixed native pixel-space width (see HudWidgetDrawUtils.h's v1
		// typography strategy comment) — not the old region-size-scaled
		// estimate, which could under-size the chip box relative to the
		// text's actual (always-native-size) rendered width.
		float chipWidth = truncated.widthLocal + chipPad * 2.0f;
		if (x + chipWidth > rowRight && x > bounds.x) {
			x = bounds.x;
			y += chipHeight + wu(bounds, 3.0f);
		}
		if (y + chipHeight > bounds.y + bounds.height) break; // out of room this frame — no per-chip layout memory across frames needed

		ofFill();
		ofSetColor(scaledAlpha(colors.background, opacity));
		ofDrawRectangle(x, y, chipWidth, chipHeight);
		drawOutline(ofRectangle(x, y, chipWidth, chipHeight), scaledAlpha(colors.secondary, opacity));

		ofSetColor(scaledAlpha(colors.secondary, opacity));
		drawText(truncated.text, x + chipPad, y + chipHeight * 0.72f);

		x += chipWidth + chipPad;
	}

	if (valueRole && valueRole->useAmbientFallback) {
		ofSetColor(scaledAlpha(colors.muted, opacity));
		drawText("...", bounds.x + wu(bounds, 4.0f), bounds.y + wu(bounds, 14.0f), wu(bounds, 0.6f, 120.0f));
	}

	ofPopStyle();
}

} // namespace hudpresent
