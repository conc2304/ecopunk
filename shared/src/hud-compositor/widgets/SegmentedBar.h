#pragma once

// ============================================================================
// SegmentedBar.h -- reusable segmented level bar (drawing half; geometry lives
// in the OF-free SegmentedBarLayout.h).
//
//   SegmentedBarStyle style;
//   style.segments = 12;                       // gap defaults to 4 px
//   drawSegmentedBar(rect, level, style, colors);
//
// Segments lean right and every corner is bevelled. Lit segments are filled;
// empty ones are outline only. Colour is either one solid colour or a colour
// scale (see SegmentedBarStop). Anything left unset follows the theme:
// lit = HudWidgetColors::secondary, empty outline = HudWidgetColors::muted.
// ============================================================================

#include "HudWidgetBase.h"
#include "HudWidgetDrawUtils.h"
#include "SegmentedBarLayout.h"

#include "ofGraphics.h"

#include <optional>
#include <vector>

namespace hudpresent {

// "Under `below` use `color`". Stops are in the same 0..1 units as the bar's
// level and must ascend, e.g. {0.3 green}, {0.6 yellow}, {1.0 red}. A value at
// or past the last stop uses the last colour.
struct SegmentedBarStop {
	float below = 1.0f;
	ofColor color;
};

enum class SegmentedBarScaleBasis {
	PerSegment, // each segment takes the colour for its own position (classic VU meter)
	WholeBar    // every lit segment takes the colour for the current level
};

struct SegmentedBarStyle {
	int segments = 10;
	float gap = kSegmentedBarDefaultGap;
	float slant = -1.0f;                 // < 0: derived from height
	float bevel = -1.0f;                 // < 0: derived from height

	// Solid colour (default: theme secondary; pass colors.accent for the tertiary look).
	std::optional<ofColor> color;
	// Non-empty = colour scale; `color` is then ignored.
	std::vector<SegmentedBarStop> scale;
	SegmentedBarScaleBasis scaleBasis = SegmentedBarScaleBasis::PerSegment;

	std::optional<ofColor> emptyColor;   // outline of unlit segments (default: theme muted)
	float outlineWidth = 1.0f;
	float opacity = 1.0f;                // multiplies every alpha (dimmed / missing-data treatment)
};

namespace segbar_detail {
inline ofColor scaleColor(const std::vector<SegmentedBarStop>& scale, float value) {
	std::vector<float> below;
	below.reserve(scale.size());
	for (const auto& s : scale) below.push_back(s.below);
	return scale[thresholdIndex(value, below)].color;
}
} // namespace segbar_detail

// Draws the bar inside `bounds` (pixel space). `level` is 0..1; NaN / negative
// draws every segment empty.
inline void drawSegmentedBar(const ofRectangle& bounds, float level, const SegmentedBarStyle& style, const HudWidgetColors& theme) {
	SegmentedBarShape shape;
	shape.segments = style.segments;
	shape.gap = style.gap;
	shape.slant = style.slant;
	shape.bevel = style.bevel;
	const auto polys = buildSegmentedBar(bounds.width, bounds.height, shape);
	if (polys.empty()) return;

	const int n = static_cast<int>(polys.size());
	const int lit = litSegmentCount(level, n);
	const ofColor solid = style.color.value_or(theme.secondary);
	const ofColor empty = style.emptyColor.value_or(theme.muted);
	const float clamped = std::clamp(level, 0.0f, 1.0f);

	ofPushStyle();
	ofEnableAlphaBlending();
	ofSetLineWidth(style.outlineWidth);
	for (int i = 0; i < n; ++i) {
		ofBeginShape();
		for (const SegPoint& p : polys[static_cast<std::size_t>(i)]) ofVertex(bounds.x + p.x, bounds.y + p.y);
		const bool isLit = i < lit;
		if (isLit) {
			ofColor c = solid;
			if (!style.scale.empty()) {
				const float v = style.scaleBasis == SegmentedBarScaleBasis::PerSegment
					? (static_cast<float>(i) + 0.5f) / static_cast<float>(n)
					: clamped;
				c = segbar_detail::scaleColor(style.scale, v);
			}
			ofFill();
			ofSetColor(scaledAlpha(c, style.opacity));
		} else {
			ofNoFill();
			ofSetColor(scaledAlpha(empty, style.opacity));
		}
		ofEndShape(true);
	}
	ofPopStyle();
}

} // namespace hudpresent
