#pragma once

#include <vector>
#include "ofRectangle.h"
#include "ofVec2f.h"

// The one new geometry concept this phase introduces (see
// docs/temporal-fields-implementation-brief.md's "New Grid Patterns" brief,
// Section 1). BSP and Blob Grid never needed this — both are purely
// axis-aligned rectangles via their own private Node/Fragment structs — so
// there is nothing here to be "consistent" with; this is genuinely new.
//
// Only two kinds exist at this level. A rectangular annulus ("frame", per
// Telescoping Frames) is deliberately NOT a third kind here: it's decomposed
// at construction time into up to four independent RECT shapes (top/bottom/
// left/right strips, see tfDecomposeFrameToRects() below), so every ring is
// just a handful of ordinary rectangle fragments — no new draw path needed
// for it at all.
enum class TFShapeKind { RECT, WEDGE };

struct TFFragmentShape {
	TFShapeKind kind = TFShapeKind::RECT;

	// Always valid regardless of kind: the shape's own axis-aligned
	// bounding box. RECT fragments render exactly this rectangle. WEDGE
	// fragments use this as the region TFFragmentTransition snapshots/
	// dissolves against — an accepted simplification (see
	// TFShapeFragmentRenderer.h) where an in-progress transition briefly
	// renders the wedge's full bounding square rather than being clipped to
	// the pie-slice/ring silhouette; only the steady-state (no transition
	// active) draw is clipped to the true wedge shape.
	ofRectangle bounds;

	// WEDGE-only. angles in degrees, 0 = +X axis, increasing clockwise
	// (matches ofRotateDeg / std::cos/sin usage elsewhere in this sketch).
	// innerRadius == 0 makes a sunburst pie slice; innerRadius > 0 makes a
	// concentric ring segment (or, with a full 0-360 sweep, a full ring).
	ofVec2f center;
	float innerRadius = 0.0f;
	float outerRadius = 0.0f;
	float angleBeginDeg = 0.0f;
	float angleEndDeg = 0.0f;
};

// Rectangle-strip decomposition of a rectangular annulus (Telescoping
// Frames' ring shape) — see Section 1 of the brief: cheaper and more
// Pi-friendly than ofPath winding/tessellation, since both bounds are
// already axis-aligned. Top/bottom strips span the full outer width;
// left/right strips span only the inner height, so the four strips meet at
// the corners with no overlap and no gaps. `inner` must be fully contained
// within `outer` (typical: same center, smaller size) — degenerate zero-
// width/height strips are simply omitted.
inline std::vector<ofRectangle> tfDecomposeFrameToRects(const ofRectangle& outer, const ofRectangle& inner) {
	std::vector<ofRectangle> strips;

	float topH = inner.y - outer.y;
	if (topH > 0.0f) {
		strips.emplace_back(outer.x, outer.y, outer.width, topH);
	}

	float bottomY = inner.y + inner.height;
	float bottomH = (outer.y + outer.height) - bottomY;
	if (bottomH > 0.0f) {
		strips.emplace_back(outer.x, bottomY, outer.width, bottomH);
	}

	float sideH = inner.height;
	float leftW = inner.x - outer.x;
	if (leftW > 0.0f && sideH > 0.0f) {
		strips.emplace_back(outer.x, inner.y, leftW, sideH);
	}

	float rightX = inner.x + inner.width;
	float rightW = (outer.x + outer.width) - rightX;
	if (rightW > 0.0f && sideH > 0.0f) {
		strips.emplace_back(rightX, inner.y, rightW, sideH);
	}

	return strips;
}
