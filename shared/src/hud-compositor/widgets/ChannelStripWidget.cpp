#include "ChannelStripWidget.h"

#include "HudWidgetDrawUtils.h"

namespace hudpresent {

void ChannelStripWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	static const char* kChannelRoles[] = {"channel0", "channel1", "channel2", "channel3"};
	static const char* kChannelLabels[] = {"Q0", "Q1", "Q2", "Q3"};

	bool anyRendered = false;
	for (const char* role : kChannelRoles) {
		if (const auto* r = input.findRole(role)) {
			if (r->shouldRender) anyRendered = true;
		}
	}
	if (!anyRendered && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	// "grid" variant (Orbit Console v0.1 Option B): Quadrant status as a 2x2 block using the full
	// 43.2 px of universal.effect_summary, "Qn  STATE" per cell in two native lines, no truncation
	// beyond the half-width cell (state text such as "QUIET PHASE" fits whole).
	if (input.variantId == "grid") {
		const float cw = bounds.width / 2.0f;
		const float lineH = std::min(16.0f, bounds.height / 2.0f);
		const float top = bounds.y + (bounds.height - 2.0f * lineH) * 0.5f;
		for (int i = 0; i < 4; ++i) {
			const auto* r = input.findRole(kChannelRoles[i]);
			const bool has = r && r->shouldRender;
			const float x = bounds.x + cw * static_cast<float>(i % 2) + 4.0f;
			const float baseline = top + lineH * static_cast<float>(i / 2) + lineH * 0.5f + kBitmapCapHeight * 0.5f;
			ofSetColor(scaledAlpha(colors.muted, has ? 0.9f : 0.4f));
			drawText(kChannelLabels[i], x, baseline);
			if (has) {
				ofSetColor(scaledAlpha(r->dimmed ? colors.muted : colors.secondary, r->dimmed ? 0.6f : 1.0f));
				const auto& value = textCache_.truncateToWidth(r->formattedText, cw - 4.0f - 3.0f * 8.0f - 4.0f);
				drawText(value.text, x + 3.0f * 8.0f, baseline);
			}
		}
		ofPopStyle();
		return;
	}

	// Section title at native size (was 0.5x and illegible), padded 3 px;
	// the four cells sit below it and shrink to make room.
	const float pad = kContainerPad;
	const float captionBaseline = bounds.y + pad + kBitmapCapHeight;
	if (!input.captionText.empty()) {
		ofSetColor(captionColor(colors, input.findRole("channel0")));
		drawText(input.captionText, bounds.x + pad, captionBaseline);
	}

	float cellWidth = bounds.width / 4.0f;
	float cellTop = captionBaseline + kBitmapDescent + 4.0f;
	float cellHeight = std::max(12.0f, bounds.getBottom() - pad - cellTop);

	for (int i = 0; i < 4; ++i) {
		float x = bounds.x + cellWidth * static_cast<float>(i);
		ofRectangle cell(x + wu(bounds, 1.5f), cellTop, cellWidth - wu(bounds, 3.0f), cellHeight);

		const auto* r = input.findRole(kChannelRoles[i]);
		bool has = r && r->shouldRender;

		ofFill();
		ofSetColor(scaledAlpha(colors.background, has ? 1.0f : 0.4f));
		ofDrawRectangle(cell);
		drawOutline(cell, scaledAlpha(colors.muted, has ? 1.0f : 0.4f));

		// Fixed native pixel-space budget (see HudWidgetDrawUtils.h's v1
		// typography strategy comment) — each cell is only 1/4 of the
		// region's own already-narrow width, so this is the tightest
		// truncation budget of any binding in this widget layer.
		float cellPad = 4.0f; // Q-id / value inset from the cell's top-left
		float cellMaxWidth = std::max(0.0f, cell.width - cellPad);

		ofSetColor(scaledAlpha(colors.muted, 0.9f));
		const auto& label = textCache_.truncateToWidth(kChannelLabels[i], cellMaxWidth);
		drawText(label.text, cell.x + cellPad, cell.y + cellPad + kBitmapCapHeight);

		if (has) {
			ofSetColor(scaledAlpha(r->dimmed ? colors.muted : colors.secondary, r->dimmed ? 0.6f : 1.0f));
			const auto& value = textCache_.truncateToWidth(r->formattedText, cellMaxWidth);
			drawText(value.text, cell.x + cellPad, cell.getBottom() - cellPad - kBitmapDescent);
		}
	}

	ofPopStyle();
}

} // namespace hudpresent
