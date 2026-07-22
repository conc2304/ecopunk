#pragma once

#include "ofGraphics.h"
#include "ofRectangle.h"
#include "ofColor.h"

// Diagonal hatch overlay for a fragment about to be removed/reconfigured
// (BSP: a leaf about to re-partition; Blob Grid: a cell fading out of the
// mask). Self-contained rather than reusing AnnotationRenderer's line
// helpers — that class's drawing methods take Fragment*/GridSystem*
// (blueprint_emergence's own concrete types) baked into their signatures,
// so it isn't includable here without those dependencies. Shared between
// TFPatternBSP and TFPatternBlobGrid as a free inline function, the same
// way TFPlayheadAssignment.h's tfAssignPlayheadsByNoise() is shared, rather
// than introducing a base class for two callers.
inline void tfDrawQuarantineHatch(const ofRectangle& bounds, ofColor color, float opacity, float spacing = 10.0f) {
	if (opacity <= 0.001f) return;

	ofPushStyle();
	ofColor c = color;
	c.a = static_cast<unsigned char>(ofClamp(opacity, 0.0f, 1.0f) * color.a);
	ofSetColor(c);
	ofSetLineWidth(1.5f);

	// Single-direction diagonal stripes, clipped to bounds by starting each
	// line from whichever edge (left or top) the diagonal actually crosses.
	float span = bounds.width + bounds.height;
	for (float d = -bounds.height; d < span; d += spacing) {
		float x0 = bounds.x + d;
		float y0 = bounds.y;
		float x1 = bounds.x + d + bounds.height;
		float y1 = bounds.y + bounds.height;

		// Clip against the left edge.
		if (x0 < bounds.x) {
			y0 += (bounds.x - x0);
			x0 = bounds.x;
		}
		// Clip against the right edge.
		if (x1 > bounds.x + bounds.width) {
			y1 -= (x1 - (bounds.x + bounds.width));
			x1 = bounds.x + bounds.width;
		}
		if (x0 >= x1) continue;

		ofDrawLine(x0, y0, x1, y1);
	}

	ofPopStyle();
}
