#include "TechnicalGrid.h"
#include "ofMath.h"
#include <cmath>

namespace hud {

void TechnicalGrid::draw(const ofRectangle & rect, const TechnicalGridStyle & s) {
	if (s.alpha <= 0.001f || s.majorSpacing < 4.0f) return;
	const ofRectangle vis = revealRect(rect, s.revealDir, s.reveal);
	if (vis.width <= 0 || vis.height <= 0) return;

	const int div = std::max(1, s.minorDivisions);
	const float minor = s.majorSpacing / static_cast<float>(div);
	const bool drawMinor = div > 1 && minor >= s.minMinorSpacing;
	const float step = drawMinor ? minor : s.majorSpacing;
	const float a = ofClamp(s.alpha, 0.0f, 1.0f);

	ofPushStyle();
	ofFill();
	auto isMajor = [&](int i) { return !drawMinor || (i % div) == 0; };

	// Vertical lines: anchored to rect origin, so wipes don't slide the grid.
	for (int i = 0;; i++) {
		float x = rect.x + i * step;
		if (x > rect.x + rect.width + 0.5f) break;
		if (x > vis.x + vis.width + 0.5f) break;
		if (x < vis.x - 0.5f) continue;
		bool major = isMajor(i);
		float lw = major ? s.majorWidth : s.minorWidth;
		ofSetColor(s.color, static_cast<int>(255 * a * (major ? s.majorAlpha : s.minorAlpha)));
		ofDrawRectangle(x - lw * 0.5f, vis.y, lw, vis.height);
	}
	for (int i = 0;; i++) {
		float y = rect.y + i * step;
		if (y > rect.y + rect.height + 0.5f) break;
		if (y > vis.y + vis.height + 0.5f) break;
		if (y < vis.y - 0.5f) continue;
		bool major = isMajor(i);
		float lw = major ? s.majorWidth : s.minorWidth;
		ofSetColor(s.color, static_cast<int>(255 * a * (major ? s.majorAlpha : s.minorAlpha)));
		ofDrawRectangle(vis.x, y - lw * 0.5f, vis.width, lw);
	}
	ofPopStyle();
}

}
