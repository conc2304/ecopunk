#include "ProgressBarWidget.h"

#include "HudWidgetDrawUtils.h"

#include <algorithm>

namespace hudpresent {

void ProgressBarWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	if ((!valueRole || !valueRole->shouldRender) && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;

	float localMaxWidth = std::max(0.0f, bounds.width);

	if (!input.captionText.empty()) {
		ofSetColor(scaledAlpha(colors.muted, opacity));
		const auto& caption = textCache_.truncateToWidth(input.captionText, localMaxWidth);
		drawText(caption.text, bounds.x, bounds.y + wu(bounds, 10.0f));
	}

	float barY = bounds.y + bounds.height - wu(bounds, 10.0f);
	float barH = std::max(3.0f, wu(bounds, 6.0f));

	ofNoFill();
	ofSetColor(scaledAlpha(colors.muted, opacity * 0.6f));
	ofDrawRectangle(bounds.x, barY, bounds.width, barH);

	float ratio = 0.0f;
	bool ambient = valueRole && valueRole->useAmbientFallback;
	if (valueRole && valueRole->shouldRender && !ambient) {
		ratio = std::clamp(valueRole->value.numberValue, 0.0f, 1.0f);
	} else if (ambient) {
		// Deterministic ambient motion — a slow sweep driven by the
		// orchestrator-supplied elapsedSeconds, per HudWidgetBase.h's "no
		// independent data-sampling clock" rule (this reads time as data,
		// it does not accumulate its own).
		ratio = 0.5f + 0.5f * std::sin(input.elapsedSeconds * 0.6f);
	}

	ofFill();
	ofSetColor(scaledAlpha(ambient ? colors.muted : colors.accent, opacity));
	ofDrawRectangle(bounds.x, barY, bounds.width * ratio, barH);

	if (valueRole && valueRole->shouldRender && !ambient) {
		ofSetColor(scaledAlpha(colors.primary, opacity));
		const auto& value = textCache_.truncateToWidth(valueRole->formattedText, localMaxWidth);
		drawText(value.text, bounds.x, barY - wu(bounds, 4.0f));
	}

	ofPopStyle();
}

} // namespace hudpresent
