#pragma once

#include "ofGraphics.h"
#include "ofRectangle.h"

namespace hud {

struct GraphicFrameStyle {
	enum class Mode { Full,
		CornersOnly };
	Mode mode = Mode::Full;
	ofColor color = ofColor(0);
	float alpha = 1.0f;
	float strokeWidth = 2.0f;
	// Positive = frame sits outside `rect` (outset); negative = inside (inset).
	float outset = 0.0f;
	// CornersOnly: arm length in px.
	float cornerLength = 16.0f;
	// Full: fraction of the perimeter drawn clockwise from the top-left, [0,1].
	float reveal = 1.0f;
};

// Rectangular outline built from filled strips (crisp at any stroke width).
class GraphicFrame {
public:
	static void draw(const ofRectangle & rect, const GraphicFrameStyle & style);
};

}
