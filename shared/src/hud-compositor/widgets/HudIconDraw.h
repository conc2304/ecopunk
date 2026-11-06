#pragma once

// Fills a HudIconPath contour set as an ofPath in a theme colour (24-unit viewBox).
#include "../HudIconPath.h"

#include "ofColor.h"
#include "ofPath.h"
#include "ofRectangle.h"
#include "ofGraphics.h"

#include <vector>

namespace hudpresent {

inline void buildIconPath(ofPath& path, const std::vector<hudicon::Contour>& contours) {
	path.clear();
	path.setPolyWindingMode(OF_POLY_WINDING_NONZERO);
	for (const auto& c : contours) {
		if (c.pts.empty()) continue;
		path.moveTo(c.pts.front().x, c.pts.front().y);
		for (size_t i = 1; i < c.pts.size(); ++i) path.lineTo(c.pts[i].x, c.pts[i].y);
		path.close();
	}
	path.setFilled(true);
	path.setStrokeWidth(0.0f);
}

// Draws `icon` scaled so the 24-unit box fits `box`'s smaller side, centred in `box`.
inline void drawIconPath(ofPath& icon, const ofRectangle& box, const ofColor& color) {
	const float size = std::min(box.width, box.height);
	icon.setFillColor(color);
	ofPushMatrix();
	ofTranslate(box.getCenter().x - size * 0.5f, box.getCenter().y - size * 0.5f);
	ofScale(size / 24.0f, size / 24.0f);
	icon.draw();
	ofPopMatrix();
}

} // namespace hudpresent
