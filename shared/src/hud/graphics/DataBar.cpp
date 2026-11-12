#include "DataBar.h"
#include "ofMath.h"
#include <cmath>

namespace hud {

void DataBar::draw(const ofRectangle & rect, const DataBarStyle & s) {
	if (s.alpha <= 0.001f || rect.width <= 0 || rect.height <= 0) return;
	const float a = ofClamp(s.alpha, 0.0f, 1.0f);
	const float v = ofClamp(s.value, 0.0f, 1.0f);

	ofPushStyle();
	ofFill();
	if (!s.segmented) {
		ofSetColor(s.track, static_cast<int>(255 * a * s.trackAlpha));
		ofDrawRectangle(rect);
		ofSetColor(s.fill, static_cast<int>(s.fill.a * a));
		if (s.vertical)
			ofDrawRectangle(rect.x, rect.y + rect.height * (1 - v), rect.width, rect.height * v);
		else
			ofDrawRectangle(rect.x, rect.y, rect.width * v, rect.height);
	} else {
		const int n = std::max(1, s.segments);
		const float len = (s.vertical ? rect.height : rect.width);
		const float seg = (len - s.segmentGap * (n - 1)) / n;
		if (seg <= 0.5f) {
			ofPopStyle();
			return;
		}
		const float lit = v * n;
		for (int i = 0; i < n; i++) {
			float off = i * (seg + s.segmentGap);
			bool on = (i + 1) <= std::floor(lit + 0.001f);
			// Vertical bars fill from the bottom.
			ofRectangle r = s.vertical
				? ofRectangle(rect.x, rect.y + rect.height - off - seg, rect.width, seg)
				: ofRectangle(rect.x + off, rect.y, seg, rect.height);
			if (on)
				ofSetColor(s.fill, static_cast<int>(s.fill.a * a));
			else
				ofSetColor(s.track, static_cast<int>(255 * a * s.trackAlpha));
			ofDrawRectangle(r);
		}
	}
	ofPopStyle();
}

}
