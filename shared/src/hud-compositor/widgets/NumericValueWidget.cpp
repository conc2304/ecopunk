#include "NumericValueWidget.h"

#include "HudWidgetDrawUtils.h"

namespace hudpresent {

void NumericValueWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const auto* valueRole = input.findRole("value");
	if ((!valueRole || !valueRole->shouldRender) && !input.regionAmbientFallback) return;

	ofPushStyle();
	ofEnableAlphaBlending();

	float opacity = (valueRole && valueRole->dimmed) ? 0.5f : 1.0f;

	if (!input.captionText.empty()) {
		ofSetColor(scaledAlpha(colors.muted, opacity));
		drawText(input.captionText, bounds.x + wu(bounds, 4.0f), bounds.y + wu(bounds, 12.0f), wu(bounds, 0.6f, 120.0f));
	}

	if (valueRole && valueRole->shouldRender) {
		ofSetColor(scaledAlpha(valueRole->useAmbientFallback ? colors.muted : colors.primary, opacity));
		drawText(valueRole->formattedText, bounds.x + wu(bounds, 4.0f), bounds.y + bounds.height - wu(bounds, 4.0f), wu(bounds, 1.0f, 120.0f));
	}

	ofPopStyle();
}

} // namespace hudpresent
