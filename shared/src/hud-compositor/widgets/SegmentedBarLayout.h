#pragma once

// ============================================================================
// SegmentedBarLayout.h -- OpenFrameworks-free geometry for the reusable
// segmented bar (see SegmentedBar.h for the drawing half).
//
// Given a width, a height and a segment count the bar builds itself: N equal
// parallelograms leaning to the right ("/"), separated by a constant
// horizontal gap, every corner chamfered (bevelled). Polygons are returned in
// local space (origin = top-left of the bar) so the caller translates them.
//
//        ___________      ___________
//       /           \    /           \        top edge is shifted right by
//      /             \  /             \       `slant`; the whole bar,
//     /_______________\/_______________\      slant included, is `width` wide.
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace hudpresent {

struct SegPoint {
	float x = 0.0f;
	float y = 0.0f;
};

using SegPolygon = std::vector<SegPoint>;

constexpr float kSegmentedBarDefaultGap = 4.0f;

// Negative `slant` / `bevel` mean "derive from the bar height".
struct SegmentedBarShape {
	int segments = 10;
	float gap = kSegmentedBarDefaultGap; // horizontal gap between neighbouring segments, px
	float slant = -1.0f;                 // horizontal lean of each segment (top edge offset), px; auto = height * 0.5
	float bevel = -1.0f;                 // corner chamfer length along each edge, px; auto = clamp(height * 0.22, 1, 3)
};

// Segments lit for a 0..1 level: 0 only for level == 0 (or not a number),
// otherwise ceil, so any real value lights at least one segment and 1.0 lights all.
inline int litSegmentCount(float level, int segments) {
	if (segments <= 0 || !(level > 0.0f)) return 0;
	return std::clamp(static_cast<int>(std::ceil(std::min(level, 1.0f) * static_cast<float>(segments) - 1e-4f)), 1, segments);
}

// Index of the first threshold `value` is under (value < below[i]); values at or
// past the last threshold use the last entry. `below` must be ascending. Returns
// 0 for an empty list.
inline std::size_t thresholdIndex(float value, const std::vector<float>& below) {
	for (std::size_t i = 0; i < below.size(); ++i) {
		if (value < below[i]) return i;
	}
	return below.empty() ? 0 : below.size() - 1;
}

// Chamfer every corner of a convex polygon by cutting `bevel` px along both edges.
inline SegPolygon bevelPolygon(const SegPolygon& poly, float bevel) {
	const std::size_t n = poly.size();
	if (n < 3 || !(bevel > 0.0f)) return poly;
	auto edgeLen = [&](std::size_t a, std::size_t b) {
		return std::hypot(poly[b].x - poly[a].x, poly[b].y - poly[a].y);
	};
	// Two bevels share each edge, so neither may take more than half of it.
	float shortest = edgeLen(0, 1);
	for (std::size_t i = 1; i < n; ++i) shortest = std::min(shortest, edgeLen(i, (i + 1) % n));
	const float b = std::min(bevel, shortest * 0.5f);

	SegPolygon out;
	out.reserve(n * 2);
	for (std::size_t i = 0; i < n; ++i) {
		const SegPoint& p = poly[i];
		const SegPoint& prev = poly[(i + n - 1) % n];
		const SegPoint& next = poly[(i + 1) % n];
		auto toward = [&](const SegPoint& q) {
			const float len = std::hypot(q.x - p.x, q.y - p.y);
			return len > 0.0f ? SegPoint{p.x + (q.x - p.x) / len * b, p.y + (q.y - p.y) / len * b} : p;
		};
		out.push_back(toward(prev));
		out.push_back(toward(next));
	}
	return out;
}

// Builds the segment outlines for a bar `width` x `height`. Returns segments
// left to right (index 0 = lowest value). Empty for a degenerate request.
inline std::vector<SegPolygon> buildSegmentedBar(float width, float height, const SegmentedBarShape& shape) {
	std::vector<SegPolygon> out;
	const int n = shape.segments;
	if (n <= 0 || !(width > 0.0f) || !(height > 0.0f)) return out;

	const float gap = std::max(0.0f, shape.gap);
	float slant = shape.slant >= 0.0f ? shape.slant : height * 0.5f;
	// Each segment keeps at least 1 px of base width; give up slant first, then gap.
	const float gaps = gap * static_cast<float>(n - 1);
	slant = std::clamp(slant, 0.0f, std::max(0.0f, width - gaps - static_cast<float>(n)));
	const float effGap = (n > 1 && width - slant - static_cast<float>(n) < gaps)
		? std::max(0.0f, (width - slant - static_cast<float>(n)) / static_cast<float>(n - 1))
		: gap;
	const float segW = std::max(1.0f, (width - slant - effGap * static_cast<float>(n - 1)) / static_cast<float>(n));
	const float bevel = shape.bevel >= 0.0f ? shape.bevel : std::clamp(height * 0.22f, 1.0f, 3.0f);

	out.reserve(static_cast<std::size_t>(n));
	for (int i = 0; i < n; ++i) {
		const float x0 = static_cast<float>(i) * (segW + effGap);
		SegPolygon quad = {
			{x0, height},                       // bottom-left
			{x0 + segW, height},                // bottom-right
			{x0 + segW + slant, 0.0f},          // top-right (shifted right)
			{x0 + slant, 0.0f},                 // top-left
		};
		out.push_back(bevelPolygon(quad, bevel));
	}
	return out;
}

} // namespace hudpresent
