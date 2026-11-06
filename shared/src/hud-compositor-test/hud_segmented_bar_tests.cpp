// SegmentedBarLayout -- geometry of the reusable segmented bar (OF-free).
#include "../hud-compositor/widgets/SegmentedBarLayout.h"

#include <iostream>
#include <string>

namespace {
int g_total = 0, g_failures = 0;
void check(bool c, const std::string& what) {
	g_total++;
	if (!c) { g_failures++; std::cerr << "FAIL: " << what << "\n"; }
}
void bounds(const hudpresent::SegPolygon& p, float& x0, float& y0, float& x1, float& y1) {
	x0 = y0 = 1e9f; x1 = y1 = -1e9f;
	for (const auto& q : p) { x0 = std::min(x0, q.x); y0 = std::min(y0, q.y); x1 = std::max(x1, q.x); y1 = std::max(y1, q.y); }
}
} // namespace

int main() {
	using namespace hudpresent;

	// Level -> lit segments.
	check(litSegmentCount(0.0f, 12) == 0, "level 0 lights nothing");
	check(litSegmentCount(-1.0f, 12) == 0 && litSegmentCount(std::nanf(""), 12) == 0, "negative / NaN light nothing");
	check(litSegmentCount(0.001f, 12) == 1, "any real activity lights one segment");
	check(litSegmentCount(0.5f, 12) == 6, "0.5 of 12 = 6");
	check(litSegmentCount(0.51f, 12) == 7, "0.51 of 12 rounds up to 7");
	check(litSegmentCount(1.0f, 12) == 12 && litSegmentCount(7.0f, 12) == 12, "1.0 and above light all");
	check(litSegmentCount(0.5f, 0) == 0, "zero segments");

	// Colour-scale lookup ("under 30 green, under 60 yellow, under 100 red").
	const std::vector<float> below = {30.0f, 60.0f, 100.0f};
	check(thresholdIndex(0.0f, below) == 0 && thresholdIndex(29.9f, below) == 0, "under 30 -> first");
	check(thresholdIndex(30.0f, below) == 1 && thresholdIndex(59.9f, below) == 1, "30..60 -> second");
	check(thresholdIndex(60.0f, below) == 2 && thresholdIndex(99.9f, below) == 2, "60..100 -> third");
	check(thresholdIndex(100.0f, below) == 2 && thresholdIndex(500.0f, below) == 2, "past the last stop -> last");
	check(thresholdIndex(5.0f, {}) == 0, "empty scale -> 0");

	// Geometry: 12 segments, 200 x 12, default gap 4.
	SegmentedBarShape shape;
	shape.segments = 12;
	auto segs = buildSegmentedBar(200.0f, 12.0f, shape);
	check(segs.size() == 12, "12 segments built");
	for (const auto& s : segs) check(s.size() == 8, "each segment is a bevelled octagon (all 4 corners cut)");

	// Exact extents / gap are checked on the unbevelled shape (a bevel trims the corners).
	SegmentedBarShape sharp = shape;
	sharp.bevel = 0.0f;
	auto raw = buildSegmentedBar(200.0f, 12.0f, sharp);
	check(raw.size() == 12 && raw[0].size() == 4, "bevel 0 -> plain 4-point parallelograms");
	float x0, y0, x1, y1, lx0, ly0, lx1, ly1;
	bounds(raw.front(), x0, y0, x1, y1);
	bounds(raw.back(), lx0, ly0, lx1, ly1);
	check(std::fabs(x0) < 1e-3f, "bar starts at x=0");
	check(std::fabs(lx1 - 200.0f) < 1e-3f, "bar (slant included) ends exactly at the requested width");
	check(std::fabs(y0) < 1e-3f && std::fabs(y1 - 12.0f) < 1e-3f, "bar fills the requested height");
	// Gap = horizontal distance between a segment's right edge and the next one's left edge, on the bottom row.
	for (std::size_t i = 0; i + 1 < raw.size(); ++i) {
		const float rightBottom = raw[i][1].x, nextLeftBottom = raw[i + 1][0].x;
		check(std::fabs((nextLeftBottom - rightBottom) - 4.0f) < 1e-3f, "constant 4 px gap");
	}
	// Bevelled bar stays inside the nominal frame.
	for (const auto& s : segs) for (const auto& q : s) check(q.x >= -1e-3f && q.x <= 200.0f + 1e-3f && q.y >= -1e-3f && q.y <= 12.0f + 1e-3f, "bevelled points stay inside the bar rect");

	// Leans right: the top edge sits further right than the bottom edge.
	float topMin = 1e9f, botMin = 1e9f;
	for (const auto& q : segs[3]) {
		if (q.y < 0.5f) topMin = std::min(topMin, q.x);
		if (q.y > 11.5f) botMin = std::min(botMin, q.x);
	}
	check(topMin > botMin + 3.0f, "segments lean right (top edge shifted right of bottom edge)");

	// Gap is configurable and shrinks the segments.
	SegmentedBarShape wide = shape;
	wide.gap = 10.0f;
	wide.bevel = 0.0f;
	const auto wideSegs = buildSegmentedBar(200.0f, 12.0f, wide);
	check(wideSegs[0][1].x - wideSegs[0][0].x < raw[0][1].x - raw[0][0].x, "larger gap -> narrower segments");
	check(std::fabs(wideSegs.back()[2].x - 200.0f) < 1e-3f, "width is still met");

	// Degenerate input is safe.
	check(buildSegmentedBar(0.0f, 12.0f, shape).empty() && buildSegmentedBar(200.0f, 0.0f, shape).empty(), "zero size -> no segments");
	SegmentedBarShape none = shape;
	none.segments = 0;
	check(buildSegmentedBar(200.0f, 12.0f, none).empty(), "zero segments -> none");
	auto tiny = buildSegmentedBar(20.0f, 12.0f, shape);
	check(tiny.size() == 12, "tiny width still yields all segments");
	float tx0, ty0, tx1, ty1;
	bounds(tiny.back(), tx0, ty0, tx1, ty1);
	check(tx1 <= 20.0f + 1e-3f, "tiny width: nothing overflows the requested width");
	auto one = buildSegmentedBar(50.0f, 10.0f, SegmentedBarShape{1});
	check(one.size() == 1, "single segment");

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
