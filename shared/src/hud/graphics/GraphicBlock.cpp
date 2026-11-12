#include "GraphicBlock.h"
#include "ofMath.h"

namespace hud {

ofRectangle revealRect(const ofRectangle & r, RevealDir dir, float reveal) {
	float t = ofClamp(reveal, 0.0f, 1.0f);
	switch (dir) {
	case RevealDir::None: return r;
	case RevealDir::LeftToRight: return ofRectangle(r.x, r.y, r.width * t, r.height);
	case RevealDir::RightToLeft: return ofRectangle(r.x + r.width * (1.0f - t), r.y, r.width * t, r.height);
	case RevealDir::TopToBottom: return ofRectangle(r.x, r.y, r.width, r.height * t);
	case RevealDir::BottomToTop: return ofRectangle(r.x, r.y + r.height * (1.0f - t), r.width, r.height * t);
	}
	return r;
}

void GraphicBlock::draw(const ofRectangle & rect, const GraphicBlockStyle & s) {
	if (s.alpha <= 0.001f || s.reveal <= 0.0f) return;
	ofRectangle vis = revealRect(rect, s.revealDir, s.reveal);
	if (vis.width <= 0.0f || vis.height <= 0.0f) return;

	ofPushStyle();
	ofEnableBlendMode(s.blend);
	ofFill();
	ofSetColor(s.fill, static_cast<int>(s.fill.a * ofClamp(s.alpha, 0.0f, 1.0f)));
	ofDrawRectangle(vis);
	if (s.border && s.borderWidth > 0.0f) {
		const float w = s.borderWidth;
		ofSetColor(s.borderColor, static_cast<int>(s.borderColor.a * ofClamp(s.alpha, 0.0f, 1.0f)));
		// Four filled strips: crisp at any width, unlike GL line widths.
		ofDrawRectangle(vis.x, vis.y, vis.width, w);
		ofDrawRectangle(vis.x, vis.y + vis.height - w, vis.width, w);
		ofDrawRectangle(vis.x, vis.y + w, w, vis.height - 2 * w);
		ofDrawRectangle(vis.x + vis.width - w, vis.y + w, w, vis.height - 2 * w);
	}
	ofPopStyle();
}

}
