#include "SparklineWidget.h"

#include "HudWidgetDrawUtils.h"
#include "../HudVitalsDraw.h"

#include <algorithm>

namespace hudpresent {

// System Telemetry v1 "vitals" variant: label + latest value on the left, the
// cached, timestamp-placed trace on the right. Strokes pre-built geometry only
// (HudVitalsDrawData, rebuilt in the renderer's update path by series
// revision) — no history reads, geometry building or per-frame allocation.
void SparklineWidget::drawVitals(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const HudVitalsDrawData& d = *input.vitals;
	const auto state = d.trace.state();
	const bool valid = state == sysvitals::State::Valid;
	const bool stale = state == sysvitals::State::Stale;

	ofPushStyle();
	ofEnableAlphaBlending();

	// Text column. Native 8x13 bitmap glyphs: a 24 px slot holds one line.
	const float baseline = bounds.y + bounds.height * 0.5f + kBitmapCapHeight * 0.5f;
	const float labelX = bounds.x + kVitalsPad;
	const float valueX = labelX + 10.0f * 8.0f; // label field = 10 glyph cells
	ofSetColor(colors.caption);
	if (!input.captionText.empty()) {
		const auto& caption = textCache_.truncateToWidth(input.captionText, valueX - labelX - 4.0f);
		drawText(caption.text, labelX, baseline);
	}
	ofSetColor(valid ? colors.primary : colors.muted); // stale/unavailable values never look live
	drawText(d.trace.valueText(), valueX, baseline);

	const bool layoutMatches = std::fabs(d.slotWidth - bounds.width) < 0.5f && std::fabs(d.slotHeight - bounds.height) < 0.5f;
	if (layoutMatches && d.plotW > 0.0f && d.plotH > 0.0f) {
		ofPushMatrix();
		ofTranslate(bounds.x + d.plotX, bounds.y + d.plotY);
		ofSetLineWidth(1.0f);
		ofNoFill();
		// Axis floor, recessed.
		ofSetColor(colors.muted);
		ofDrawLine(0.0f, d.plotH, d.plotW, d.plotH);
		// Reference line (e.g. 33.3 ms = the 30 FPS target; a reference only).
		if (d.trace.hasReference()) {
			ofSetColor(scaledAlpha(colors.warning, 0.35f));
			ofDrawLine(0.0f, d.trace.referenceY(), d.plotW, d.trace.referenceY());
		}
		ofSetColor(valid ? scaledAlpha(colors.secondary, 0.9f) : scaledAlpha(colors.muted, 0.9f));
		ofSetLineWidth(std::max(1.0f, wu(bounds, 1.2f)));
		for (const auto& line : d.lines) line.draw();
		ofFill();
		for (const auto& dot : d.dots) ofDrawCircle(dot.x, dot.y, 1.0f);
		if (d.trace.hasLatestPoint()) {
			ofSetColor(scaledAlpha(colors.accent, 0.9f));
			const auto p = d.trace.latestPoint();
			ofDrawCircle(p.x, p.y, 2.0f);
		}
		ofPopMatrix();

		// Honest placeholders: never an empty plot that could pass for a flat 0.
		const char* note = nullptr;
		if (state == sysvitals::State::Unavailable) note = "NO DATA";
		else if (stale) note = "STALE";
		if (note) {
			ofSetColor(stale ? colors.warning : colors.muted);
			const float noteX = bounds.x + d.plotX + (stale ? std::max(0.0f, d.plotW - 5.0f * 8.0f) : 6.0f);
			drawText(note, noteX, baseline);
		}
	}
	ofPopStyle();
}

void SparklineWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	if (input.vitals) { drawVitals(bounds, colors, input); return; }
	if (input.historySamples.empty()) return; // nothing tracked yet — not an error, just no data

	ofPushStyle();
	ofEnableAlphaBlending();
	ofNoFill();

	if (!input.captionText.empty()) {
		ofFill();
		ofSetColor(scaledAlpha(captionColor(colors, input.findRole("value")), 1.0f));
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
