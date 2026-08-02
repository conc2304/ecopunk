#pragma once

#include <vector>

// Pure geometry/association helpers shared by Fragment (indirectly, via
// VideoRegionMathOf.h's ofRectangle wrappers), BlobTracker and
// VideoRegionController. This header and VideoRegionMath.cpp are plain
// float/std::vector math with ZERO openFrameworks dependency (confirmed: a
// probe compile of ofRectangle.h alone pulls in ofConstants.h -> GL/glew.h,
// so even ofRectangle is not includable in a bare standalone test binary
// the way sketches/temporal-fields/test/tf_timeline_tests.cpp's
// nlohmann::json-only dependency was) — so this file can be exercised by a
// standalone test the same way TFPresetTimeline is tested there. See
// test/videoregion_math_tests.cpp and test/Makefile.tests in this sketch.
//
// ofRectangle-typed convenience wrappers (toRect/toOf) live in the separate
// VideoRegionMathOf.h, included only by production .cpp files that already
// depend on ofRectangle (Fragment.cpp, BlobTracker.cpp,
// VideoRegionController.cpp) — never by the standalone test.
namespace VideoRegionMath {

	// Plain POD mirror of ofRectangle's (x, y, width, height) layout, with no
	// oF dependency, so the .cpp implementing the actual math below can
	// compile and link with nothing but a bare C++ compiler.
	struct Rect {
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
	};

	// normalizedBounds is [0,1] against a sourceWidth x sourceHeight frame;
	// returns the equivalent pixel-space rectangle. No clamping.
	Rect normalizedToSourcePixelsRect(const Rect & normalizedBounds, float sourceWidth, float sourceHeight);

	// Clamps rect so it lies fully inside bounds: size is capped to bounds'
	// size first, then position is shifted (not just clipped) so the full
	// clamped rect stays inside. Degenerate bounds (width/height <= 0)
	// collapse to a zero-size rect at bounds' origin.
	Rect clampRectToBounds(const Rect & rect, const Rect & bounds);

	// "background-size: cover" crop: the largest centered sub-rect of a
	// texW x texH source that exactly matches destRect's aspect ratio.
	// Self-contained equivalent of temporal-fields' tfComputeCropFillSrcRect
	// (sketches/temporal-fields/src/TFTextureCropFill.h) — not imported from
	// there because that header is sketch-local by design (see this
	// prototype's config.make comment); duplicating ~10 lines of well-proven
	// math here is cheaper than promoting a whole sketch-local header, and
	// this is the one piece of it a blob region actually needs.
	Rect computeCropFillSourceRect(float texWidth, float texHeight, const Rect & destRect);

	// Maps a normalized-source-space rectangle onto screen coordinates,
	// given the same cover-fit sourceRect the background draw used to fill
	// destRect. This is what keeps a blob fragment visually aligned with the
	// video content the background layer is already showing, including when
	// source and destination aspect ratios differ (see the crop-fill risk
	// called out in docs/blob-fragment-and-foreground-mask-architecture-analysis.md,
	// section 10).
	Rect mapNormalizedSourceRectToScreen(
		const Rect & normalizedRect,
		float sourceWidth,
		float sourceHeight,
		const Rect & cropFillSourceRect,
		const Rect & destRect);

	// Intersection-over-union, 0 when disjoint or either rect is degenerate.
	float rectIoU(const Rect & a, const Rect & b);

	// Rect center point, written into outX/outY.
	void rectCenter(const Rect & r, float & outX, float & outY);

	float distance(float ax, float ay, float bx, float by);

	// Linear interpolation of a Rect's position+size independently, using
	// separate factors so a caller (BlobTracker) can smooth position and
	// size at different rates. factor 0 = stay at `from`, 1 = jump to `to`.
	Rect lerpRect(const Rect & from, const Rect & to, float positionFactor, float sizeFactor);

	// Greedy nearest-first detection-to-track association (the core of
	// BlobTracker::update()'s matching step, extracted here so it's testable
	// without BlobTracker's stateful id/age/missing-time bookkeeping around
	// it). For each pair (trackRects[i], detectionRects[j]) that is a
	// candidate match (centroid distance <= maxDistance OR IoU >= minIoU),
	// pairs are considered in ascending-distance order and greedily
	// assigned, each track and each detection used at most once.
	//
	// Returns one entry per track in trackRects: the matched index into
	// detectionRects, or -1 if that track found no match this call.
	std::vector<int> greedyAssociateByDistance(
		const std::vector<Rect> & trackRects,
		const std::vector<Rect> & detectionRects,
		float maxDistance,
		float minIoU);

}
