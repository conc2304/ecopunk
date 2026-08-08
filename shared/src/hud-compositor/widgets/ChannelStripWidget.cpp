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

	if (!input.captionText.empty()) {
		ofSetColor(colors.muted);
		drawText(input.captionText, bounds.x, bounds.y + wu(bounds, 10.0f), wu(bounds, 0.5f, 120.0f));
	}

	float cellWidth = bounds.width / 4.0f;
	float cellTop = bounds.y + wu(bounds, 14.0f);
	float cellHeight = bounds.height - wu(bounds, 16.0f);

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
		float cellPad = wu(bounds, 3.0f);
		float cellMaxWidth = std::max(0.0f, cell.width - cellPad);

		ofSetColor(scaledAlpha(colors.muted, 0.9f));
		const auto& label = textCache_.truncateToWidth(kChannelLabels[i], cellMaxWidth);
		drawText(label.text, cell.x + cellPad, cell.y + wu(bounds, 10.0f));

		if (has) {
			ofSetColor(scaledAlpha(r->dimmed ? colors.muted : colors.secondary, r->dimmed ? 0.6f : 1.0f));
			const auto& value = textCache_.truncateToWidth(r->formattedText, cellMaxWidth);
			drawText(value.text, cell.x + cellPad, cell.y + cell.height - wu(bounds, 4.0f));
		}
	}

	ofPopStyle();
}

} // namespace hudpresent
