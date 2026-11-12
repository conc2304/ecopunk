#pragma once

#include "ofGraphics.h"
#include "ofRectangle.h"

namespace hud {

// Direction a primitive is progressively revealed in. None = whole shape
// (callers fade with `alpha`).
enum class RevealDir { None,
	LeftToRight,
	RightToLeft,
	TopToBottom,
	BottomToTop };

// The portion of `r` that is visible at `reveal` in [0,1].
ofRectangle revealRect(const ofRectangle & r, RevealDir dir, float reveal);

struct GraphicBlockStyle {
	ofColor fill = ofColor(0);
	float alpha = 1.0f; // multiplies fill alpha, [0,1]
	bool border = false;
	ofColor borderColor = ofColor(0);
	float borderWidth = 1.0f;
	ofBlendMode blend = OF_BLENDMODE_ALPHA;
	RevealDir revealDir = RevealDir::None;
	float reveal = 1.0f;
};

// Filled rectangle with optional border, alpha, simple blend mode and wipe
// reveal. Generic: no scene knowledge.
class GraphicBlock {
public:
	static void draw(const ofRectangle & rect, const GraphicBlockStyle & style);
};

}
