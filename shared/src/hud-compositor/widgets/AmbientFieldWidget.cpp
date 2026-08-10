#include "AmbientFieldWidget.h"

#include "HudWidgetDrawUtils.h"

#include <cmath>

namespace hudpresent {

void AmbientFieldWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	ofPushStyle();
	ofEnableAlphaBlending();
	ofNoFill();

	float influence = 0.5f;
	if (const auto* infl = input.findRole("influence")) {
		if (infl->shouldRender) influence = infl->value.numberValue;
	}

	ofSetColor(scaledAlpha(colors.muted, 0.55f));
	int lineCount = 5;
	for (int i = 0; i < lineCount; ++i) {
		float u = static_cast<float>(i) / static_cast<float>(lineCount - 1);
		ofPolyline line;
		int steps = 24;
		for (int s = 0; s <= steps; ++s) {
			float sx = bounds.x + bounds.width * (static_cast<float>(s) / static_cast<float>(steps));
			float wobble = std::sin(input.elapsedSeconds * (0.4f + influence * 0.6f) + u * 6.0f + static_cast<float>(s) * 0.3f);
			float sy = bounds.y + bounds.height * (0.15f + 0.7f * u) + wobble * wu(bounds, 4.0f);
			line.addVertex(sx, sy);
		}
		ofSetLineWidth(1.0f);
		line.draw();
	}

	if (!input.captionText.empty()) {
		ofFill();
		ofSetColor(scaledAlpha(colors.muted, 0.7f));
		float pad = wu(bounds, 3.0f);
		// Fixed native pixel-space budget (see HudWidgetDrawUtils.h's v1
		// typography strategy comment) — not scale-adjusted.
		const auto& caption = textCache_.truncateToWidth(input.captionText, std::max(0.0f, bounds.width - pad));
		drawText(caption.text, bounds.x + pad, bounds.y + wu(bounds, 10.0f));
	}

	ofPopStyle();
}

} // namespace hudpresent
