#pragma once

#include "ofGraphics.h"
#include "ofRectangle.h"

namespace hud {

struct DataBarStyle {
	bool vertical = false;
	bool segmented = false;
	int segments = 12; // used when segmented
	float segmentGap = 2.0f;
	float value = 0.5f; // normalized fill [0,1]; caller supplies it
	ofColor fill = ofColor(0);
	ofColor track = ofColor(0);
	float trackAlpha = 0.18f;
	float alpha = 1.0f;
};

// Generic solid/segmented bar. Takes a plain normalized value; knows nothing
// about where that value comes from.
class DataBar {
public:
	static void draw(const ofRectangle & rect, const DataBarStyle & style);
};

}
