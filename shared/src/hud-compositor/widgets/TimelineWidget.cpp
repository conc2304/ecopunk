#include "TimelineWidget.h"

#include "HudWidgetDrawUtils.h"

#include <algorithm>

namespace hudpresent {

void TimelineWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* phaseRole = input.findRole("phase");
	if ((!phaseRole || !phaseRole->shouldRender) && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	float localMaxWidth = std::max(0.0f, bounds.width);

	if (!input.captionText.empty()) {
		ofSetColor(colors.muted);
		const auto& caption = textCache_.truncateToWidth(input.captionText, localMaxWidth);
		drawText(caption.text, bounds.x, bounds.y + wu(bounds, 10.0f));
	}

	float trackY = bounds.y + bounds.height * 0.6f;
	ofSetColor(scaledAlpha(colors.muted, 0.6f));
	ofSetLineWidth(1.5f);
	ofDrawLine(bounds.x, trackY, bounds.x + bounds.width, trackY);

	float progress = 0.5f;
	if (const auto* progressRole = input.findRole("progress")) {
		if (progressRole->shouldRender) progress = std::clamp(progressRole->value.numberValue, 0.0f, 1.0f);
	}
	float markerX = bounds.x + bounds.width * progress;

	ofFill();
	ofSetColor(colors.accent);
	ofDrawCircle(markerX, trackY, wu(bounds, 3.5f));

	if (phaseRole && phaseRole->shouldRender) {
		ofSetColor(colors.primary);
		const auto& phase = textCache_.truncateToWidth(phaseRole->formattedText, localMaxWidth);
		drawText(phase.text, bounds.x, bounds.y + bounds.height - wu(bounds, 2.0f));
	}

	ofPopStyle();
}

} // namespace hudpresent
