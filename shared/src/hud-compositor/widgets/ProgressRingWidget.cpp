#include "ProgressRingWidget.h"

#include "HudWidgetDrawUtils.h"

#include <algorithm>
#include <cmath>

namespace hudpresent {

namespace {

// Hand-rolled arc polyline — this project's linked openFrameworks version
// has no ofDrawArc() (documented precedent:
// shared/src/hud/GaugeWidget's own header comment in
// shared/src/hud/README.md makes the same note); reused here rather than
// rediscovered.
void drawArc(float cx, float cy, float radius, float startDeg, float endDeg, float lineWidth) {
	ofPolyline line;
	int steps = std::max(2, static_cast<int>(std::ceil(std::abs(endDeg - startDeg) / 6.0f)));
	for (int i = 0; i <= steps; ++i) {
		float t = static_cast<float>(i) / static_cast<float>(steps);
		float deg = startDeg + (endDeg - startDeg) * t;
		float rad = ofDegToRad(deg);
		line.addVertex(cx + std::cos(rad) * radius, cy + std::sin(rad) * radius);
	}
	ofSetLineWidth(lineWidth);
	line.draw();
}

} // namespace

void ProgressRingWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	if ((!valueRole || !valueRole->shouldRender) && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();
	ofNoFill();

	float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;
	float cx = bounds.x + bounds.width * 0.5f;
	float cy = bounds.y + bounds.height * 0.5f;
	float radius = std::min(bounds.width, bounds.height) * 0.42f;
	float thickness = std::max(1.5f, wu(bounds, 4.0f));

	ofSetColor(scaledAlpha(colors.muted, opacity * 0.5f));
	drawArc(cx, cy, radius, -90.0f, 270.0f, thickness);

	bool ambient = valueRole && valueRole->useAmbientFallback;
	float ratio = 0.0f;
	if (valueRole && valueRole->shouldRender && !ambient) {
		ratio = std::clamp(valueRole->value.numberValue, 0.0f, 1.0f);
	} else if (ambient) {
		ratio = 0.5f + 0.5f * std::sin(input.elapsedSeconds * 0.5f);
	}

	ofSetColor(scaledAlpha(ambient ? colors.muted : colors.accent, opacity));
	drawArc(cx, cy, radius, -90.0f, -90.0f + 360.0f * ratio, thickness);

	// Fixed native pixel-space budget (see HudWidgetDrawUtils.h's v1
	// typography strategy comment) — not scale-adjusted.
	float localMaxWidth = std::max(0.0f, bounds.width - wu(bounds, 4.0f));

	if (!input.captionText.empty()) {
		ofFill();
		ofSetColor(scaledAlpha(colors.muted, opacity));
		const auto& caption = textCache_.truncateToWidth(input.captionText, localMaxWidth);
		drawText(caption.text, bounds.x + wu(bounds, 2.0f), bounds.y + bounds.height - wu(bounds, 4.0f));
	}
	if (valueRole && valueRole->shouldRender && !ambient) {
		ofFill();
		ofSetColor(scaledAlpha(colors.primary, opacity));
		const auto& value = textCache_.truncateToWidth(valueRole->formattedText, localMaxWidth);
		drawText(value.text, cx - wu(bounds, 12.0f), cy + wu(bounds, 4.0f));
	}

	ofPopStyle();
}

} // namespace hudpresent
