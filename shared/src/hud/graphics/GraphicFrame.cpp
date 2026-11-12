#include "GraphicFrame.h"
#include "ofMath.h"
#include <algorithm>

namespace hud {

void GraphicFrame::draw(const ofRectangle & rect, const GraphicFrameStyle & s) {
	if (s.alpha <= 0.001f || s.strokeWidth <= 0.0f || s.reveal <= 0.0f) return;

	const float w = s.strokeWidth;
	// Frame outer edge; stroke grows inward from it so `outset` positions the
	// outer edge relative to rect.
	const float x = rect.x - s.outset;
	const float y = rect.y - s.outset;
	const float W = rect.width + 2 * s.outset;
	const float H = rect.height + 2 * s.outset;
	if (W <= 2 * w || H <= 2 * w) return;

	ofPushStyle();
	ofFill();
	ofSetColor(s.color, static_cast<int>(s.color.a * ofClamp(s.alpha, 0.0f, 1.0f)));

	if (s.mode == GraphicFrameStyle::Mode::CornersOnly) {
		const float L = std::min({ s.cornerLength, W * 0.5f, H * 0.5f });
		// top-left
		ofDrawRectangle(x, y, L, w);
		ofDrawRectangle(x, y + w, w, L - w);
		// top-right
		ofDrawRectangle(x + W - L, y, L, w);
		ofDrawRectangle(x + W - w, y + w, w, L - w);
		// bottom-left
		ofDrawRectangle(x, y + H - w, L, w);
		ofDrawRectangle(x, y + H - L, w, L - w);
		// bottom-right
		ofDrawRectangle(x + W - L, y + H - w, L, w);
		ofDrawRectangle(x + W - w, y + H - L, w, L - w);
	} else {
		// Walk the perimeter clockwise, drawing the first `reveal` fraction.
		float remaining = ofClamp(s.reveal, 0.0f, 1.0f) * 2.0f * (W + H - 2 * w);
		auto take = [&](float len) {
			float used = std::min(len, remaining);
			remaining -= used;
			return used;
		};
		float top = take(W);
		if (top > 0) ofDrawRectangle(x, y, top, w);
		float right = take(H - w);
		if (right > 0) ofDrawRectangle(x + W - w, y + w, w, right);
		float bottom = take(W - w);
		if (bottom > 0) ofDrawRectangle(x + W - w - bottom, y + H - w, bottom, w);
		float left = take(H - 2 * w);
		if (left > 0) ofDrawRectangle(x, y + H - w - left, w, left);
	}
	ofPopStyle();
}

}
