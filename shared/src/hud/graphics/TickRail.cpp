#include "TickRail.h"
#include "ofMath.h"
#include <cmath>

namespace hud {

void TickRail::draw(const ofRectangle & rect, const TickRailStyle & s) {
	if (s.alpha <= 0.001f || s.reveal <= 0.0f || s.minorSpacing < 2.0f) return;
	const float length = s.vertical ? rect.height : rect.width;
	const float shown = length * ofClamp(s.reveal, 0.0f, 1.0f);
	const float t = std::max(1.0f, s.thickness);
	const float dir = s.ticksOnFarSide ? -1.0f : 1.0f;
	const int every = std::max(1, s.majorEvery);

	ofPushStyle();
	ofFill();
	ofSetColor(s.color, static_cast<int>(s.color.a * ofClamp(s.alpha, 0.0f, 1.0f)));

	// Rail baseline lies on the near edge; "far side" flips ticks across it.
	auto rect1 = [&](float along, float across, float lenAlong, float lenAcross) {
		if (s.vertical)
			ofDrawRectangle(rect.x + across, rect.y + along, lenAcross, lenAlong);
		else
			ofDrawRectangle(rect.x + along, rect.y + across, lenAlong, lenAcross);
	};
	auto tickAcross = [&](float len) { return dir > 0 ? 0.0f : -len + t; };

	if (s.baseline) rect1(0.0f, 0.0f, shown, t);

	const int n = static_cast<int>(std::floor(length / s.minorSpacing));
	for (int i = 0; i <= n; i++) {
		float along = i * s.minorSpacing;
		if (along > shown) break;
		bool major = (i % every) == 0;
		float len = major ? s.majorLength : s.minorLength;
		rect1(std::min(along, length - t), tickAcross(len), t, len);
	}
	if (s.endCaps) {
		float len = s.majorLength * 1.3f;
		rect1(0.0f, tickAcross(len), t, len);
		if (shown >= length - 0.5f) rect1(length - t, tickAcross(len), t, len);
	}
	ofPopStyle();
}

}
