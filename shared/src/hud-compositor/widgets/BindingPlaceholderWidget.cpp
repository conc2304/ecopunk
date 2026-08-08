#include "BindingPlaceholderWidget.h"

#include "HudWidgetDrawUtils.h"

namespace hudpresent {

void BindingPlaceholderWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	ofPushStyle();
	ofEnableAlphaBlending();

	ofFill();
	ofSetColor(scaledAlpha(colors.warning, 0.18f));
	ofDrawRectangle(bounds);
	drawOutline(bounds, colors.warning);

	// A couple of diagonal hazard strokes — deliberately cheap (a
	// handful of ofDrawLine calls, no texture/pattern) and unmistakably
	// "not real content", matching this widget's tooling-only purpose.
	ofSetColor(scaledAlpha(colors.warning, 0.5f));
	ofSetLineWidth(1.0f);
	for (float x = bounds.x - bounds.height; x < bounds.x + bounds.width; x += 10.0f) {
		ofDrawLine(x, bounds.y + bounds.height, x + bounds.height, bounds.y);
	}

	ofSetColor(colors.warning);
	drawText("BINDING ERROR", bounds.x + wu(bounds, 3.0f), bounds.y + wu(bounds, 12.0f), wu(bounds, 0.5f, 120.0f));
	drawText(input.bindingId, bounds.x + wu(bounds, 3.0f), bounds.y + wu(bounds, 24.0f), wu(bounds, 0.45f, 120.0f));

	float y = bounds.y + wu(bounds, 36.0f);
	for (const auto& summary : input.compileIssueSummaries) {
		if (y > bounds.y + bounds.height - wu(bounds, 4.0f)) break;
		drawText(summary, bounds.x + wu(bounds, 3.0f), y, wu(bounds, 0.4f, 120.0f));
		y += wu(bounds, 10.0f);
	}

	ofPopStyle();
}

} // namespace hudpresent
