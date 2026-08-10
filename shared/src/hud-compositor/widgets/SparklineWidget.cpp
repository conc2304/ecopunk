#include "SparklineWidget.h"

#include "HudWidgetDrawUtils.h"

#include <algorithm>

namespace hudpresent {

void SparklineWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	if (input.historySamples.empty()) return; // nothing tracked yet — not an error, just no data

	ofPushStyle();
	ofEnableAlphaBlending();
	ofNoFill();

	if (!input.captionText.empty()) {
		ofFill();
		ofSetColor(scaledAlpha(colors.muted, 1.0f));
		// Fixed native pixel-space budget (see HudWidgetDrawUtils.h's v1
		// typography strategy comment) — not scale-adjusted.
		const auto& caption = textCache_.truncateToWidth(input.captionText, bounds.width);
		drawText(caption.text, bounds.x, bounds.y + wu(bounds, 10.0f));
		ofNoFill();
	}

	float minV = input.historyStats.hasData ? input.historyStats.minValue : 0.0f;
	float maxV = input.historyStats.hasData ? input.historyStats.maxValue : 1.0f;
	float range = std::max(1e-6f, maxV - minV);

	float plotTop = bounds.y + wu(bounds, 14.0f);
	float plotHeight = std::max(1.0f, bounds.height - wu(bounds, 18.0f));

	ofPolyline line;
	bool building = false;
	size_t n = input.historySamples.size();
	for (size_t i = 0; i < n; ++i) {
		const auto& sample = input.historySamples[i];
		float u = (n > 1) ? static_cast<float>(i) / static_cast<float>(n - 1) : 0.0f;
		float x = bounds.x + u * bounds.width;
		if (sample.gap) {
			// A gap breaks the polyline into a separate segment rather
			// than interpolating across it or drawing a fabricated 0 —
			// "never insert zero into a history for a missing source"
			// applies to what's DRAWN here too, not just what's stored.
			if (building && line.size() > 1) {
				ofSetColor(scaledAlpha(colors.secondary, 0.85f));
				ofSetLineWidth(std::max(1.0f, wu(bounds, 1.2f)));
				line.draw();
			}
			line.clear();
			building = false;
			continue;
		}
		float normalized = (sample.value - minV) / range;
		float y = plotTop + (1.0f - std::clamp(normalized, 0.0f, 1.0f)) * plotHeight;
		line.addVertex(x, y);
		building = true;
	}
	if (building && line.size() > 1) {
		ofSetColor(scaledAlpha(colors.secondary, 0.85f));
		ofSetLineWidth(std::max(1.0f, wu(bounds, 1.2f)));
		line.draw();
	}

	if (input.historyStats.hasData && input.historyStats.eventPulse) {
		ofFill();
		ofSetColor(scaledAlpha(colors.accent, 0.9f));
		float x = bounds.x + bounds.width;
		float y = plotTop + (1.0f - std::clamp((input.historySamples.back().value - minV) / range, 0.0f, 1.0f)) * plotHeight;
		ofDrawCircle(x, y, wu(bounds, 2.5f));
	}

	ofPopStyle();
}

} // namespace hudpresent
