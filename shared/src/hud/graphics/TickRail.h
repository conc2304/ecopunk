#pragma once

#include "ofGraphics.h"
#include "ofRectangle.h"

namespace hud {

struct TickRailStyle {
	bool vertical = false;
	ofColor color = ofColor(0);
	float alpha = 1.0f;
	float minorSpacing = 8.0f; // px between ticks
	int majorEvery = 5; // every Nth tick is major
	float minorLength = 6.0f;
	float majorLength = 14.0f;
	float thickness = 1.0f;
	bool baseline = true; // draw the rail line itself
	bool endCaps = true; // perpendicular caps at both ends
	bool ticksOnFarSide = false; // flip which side the ticks extend to
	float reveal = 1.0f; // ticks revealed progressively along the rail, [0,1]
};

// Measurement ticks laid along the long axis of `rect` (width for a
// horizontal rail, height for a vertical one). The rail sits on the rect's
// top (horizontal) or left (vertical) edge, ticks extend into the rect.
class TickRail {
public:
	static void draw(const ofRectangle & rect, const TickRailStyle & style);
};

}
