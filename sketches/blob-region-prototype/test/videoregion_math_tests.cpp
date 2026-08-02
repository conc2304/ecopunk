// Standalone, dependency-free tests for VideoRegionMath — the pure geometry
// and detection-association logic behind Fragment::setNormalizedSourceBounds(),
// BlobTracker's matching step, and VideoRegionController's screen-space
// placement. Deliberately does NOT link any part of openFrameworks: a probe
// compile showed even ofRectangle.h pulls in ofConstants.h -> GL/glew.h, so
// (mirroring sketches/temporal-fields/test/tf_timeline_tests.cpp's approach
// for TFPresetTimeline) VideoRegionMath.h/.cpp were deliberately kept to
// plain floats/std::vector with zero oF dependency specifically so this
// could stay a bare-compiler test. See VideoRegionMath.h's header comment.
//
// Build/run: make -C test -f Makefile.tests test

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include "../../../shared/src/VideoRegionMath.h"

namespace {

	int g_total = 0;
	int g_failures = 0;

	void reportFailure(const std::string & file, int line, const std::string & expr) {
		g_failures++;
		std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
	}

}

#define VR_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

#define VR_CHECK_NEAR(a, b, eps) \
	do { \
		g_total++; \
		double _a = static_cast<double>(a); \
		double _b = static_cast<double>(b); \
		if (std::abs(_a - _b) > (eps)) { \
			std::ostringstream _oss; \
			_oss << #a << " (" << _a << ") != " << #b << " (" << _b << ") within " << (eps); \
			reportFailure(__FILE__, __LINE__, _oss.str()); \
		} \
	} while (0)

using VideoRegionMath::Rect;

namespace {

	// ── normalizedToSourcePixelsRect ───────────────────────────────────────
	void test_normalizedToSourcePixels_basic() {
		Rect normalized { 0.25f, 0.5f, 0.1f, 0.2f };
		Rect px = VideoRegionMath::normalizedToSourcePixelsRect(normalized, 1000.0f, 500.0f);
		VR_CHECK_NEAR(px.x, 250.0f, 1e-4);
		VR_CHECK_NEAR(px.y, 250.0f, 1e-4);
		VR_CHECK_NEAR(px.width, 100.0f, 1e-4);
		VR_CHECK_NEAR(px.height, 100.0f, 1e-4);
	}

	void test_normalizedToSourcePixels_fullFrame() {
		Rect normalized { 0.0f, 0.0f, 1.0f, 1.0f };
		Rect px = VideoRegionMath::normalizedToSourcePixelsRect(normalized, 320.0f, 180.0f);
		VR_CHECK_NEAR(px.x, 0.0f, 1e-4);
		VR_CHECK_NEAR(px.y, 0.0f, 1e-4);
		VR_CHECK_NEAR(px.width, 320.0f, 1e-4);
		VR_CHECK_NEAR(px.height, 180.0f, 1e-4);
	}

	// ── clampRectToBounds ───────────────────────────────────────────────────
	void test_clamp_fullyInside_unchanged() {
		Rect r { 10, 10, 20, 20 };
		Rect bounds { 0, 0, 100, 100 };
		Rect c = VideoRegionMath::clampRectToBounds(r, bounds);
		VR_CHECK_NEAR(c.x, 10.0f, 1e-4);
		VR_CHECK_NEAR(c.y, 10.0f, 1e-4);
		VR_CHECK_NEAR(c.width, 20.0f, 1e-4);
		VR_CHECK_NEAR(c.height, 20.0f, 1e-4);
	}

	void test_clamp_negativePosition_shiftedIntoBounds() {
		// A detection near the frame edge (e.g. a blob whose bounding box
		// extends past x=0) must be shifted, not just have its size cut —
		// this is exactly what Fragment::setNormalizedSourceBounds() relies
		// on to guarantee the crop never samples outside the source texture.
		Rect r { -5, -5, 20, 20 };
		Rect bounds { 0, 0, 100, 100 };
		Rect c = VideoRegionMath::clampRectToBounds(r, bounds);
		VR_CHECK(c.x >= 0.0f);
		VR_CHECK(c.y >= 0.0f);
		VR_CHECK(c.x + c.width <= 100.0f + 1e-4);
		VR_CHECK(c.y + c.height <= 100.0f + 1e-4);
		VR_CHECK_NEAR(c.width, 20.0f, 1e-4);
	}

	void test_clamp_largerThanBounds_sizeCapped() {
		Rect r { -50, -50, 500, 500 };
		Rect bounds { 0, 0, 100, 60 };
		Rect c = VideoRegionMath::clampRectToBounds(r, bounds);
		VR_CHECK_NEAR(c.x, 0.0f, 1e-4);
		VR_CHECK_NEAR(c.y, 0.0f, 1e-4);
		VR_CHECK_NEAR(c.width, 100.0f, 1e-4);
		VR_CHECK_NEAR(c.height, 60.0f, 1e-4);
	}

	void test_clamp_degenerateBounds_collapsesToZero() {
		Rect r { 10, 10, 20, 20 };
		Rect bounds { 5, 5, 0, 0 };
		Rect c = VideoRegionMath::clampRectToBounds(r, bounds);
		VR_CHECK_NEAR(c.width, 0.0f, 1e-4);
		VR_CHECK_NEAR(c.height, 0.0f, 1e-4);
	}

	// ── computeCropFillSourceRect / mapNormalizedSourceRectToScreen ─────────
	void test_cropFill_matchingAspect_wholeFrame() {
		Rect dest { 0, 0, 1920, 1080 };
		Rect src = VideoRegionMath::computeCropFillSourceRect(1920.0f, 1080.0f, dest);
		VR_CHECK_NEAR(src.x, 0.0f, 1e-3);
		VR_CHECK_NEAR(src.y, 0.0f, 1e-3);
		VR_CHECK_NEAR(src.width, 1920.0f, 1e-3);
		VR_CHECK_NEAR(src.height, 1080.0f, 1e-3);
	}

	void test_cropFill_widerSource_cropsSides() {
		// A 2:1 source into a 1:1 destination crops the left/right, keeps
		// full height — same "cover" behavior as CSS background-size:cover.
		Rect dest { 0, 0, 500, 500 };
		Rect src = VideoRegionMath::computeCropFillSourceRect(2000.0f, 1000.0f, dest);
		VR_CHECK_NEAR(src.height, 1000.0f, 1e-3);
		VR_CHECK_NEAR(src.width, 1000.0f, 1e-3); // 1000 tall * (500/500 destAspect=1) = 1000 wide crop
		VR_CHECK_NEAR(src.x, 500.0f, 1e-3); // centered: (2000-1000)/2
		VR_CHECK_NEAR(src.y, 0.0f, 1e-3);
	}

	void test_mapNormalizedSourceRectToScreen_identity() {
		// cropFillSourceRect == the whole source frame, dest == 1:1 scale of
		// the source in pixels: a normalized rect should map straight
		// through to the same pixel rect in screen space.
		Rect cropSrc { 0, 0, 320, 180 };
		Rect dest { 0, 0, 320, 180 };
		Rect normalized { 0.5f, 0.5f, 0.1f, 0.1f };
		Rect screen = VideoRegionMath::mapNormalizedSourceRectToScreen(normalized, 320.0f, 180.0f, cropSrc, dest);
		VR_CHECK_NEAR(screen.x, 160.0f, 1e-3);
		VR_CHECK_NEAR(screen.y, 90.0f, 1e-3);
		VR_CHECK_NEAR(screen.width, 32.0f, 1e-3);
		VR_CHECK_NEAR(screen.height, 18.0f, 1e-3);
	}

	void test_mapNormalizedSourceRectToScreen_croppedAndScaled() {
		// Source 2000x1000 cover-fit into a 500x500 destination (from the
		// crop-fill test above: cropSrc = {500,0,1000,1000}, scale = 0.5).
		// A blob at source-pixel (1000,500) size (100,100) — i.e. dead
		// center of the crop — should land at destination center.
		Rect cropSrc { 500, 0, 1000, 1000 };
		Rect dest { 0, 0, 500, 500 };
		Rect normalized { 1000.0f / 2000.0f, 500.0f / 1000.0f, 100.0f / 2000.0f, 100.0f / 1000.0f };
		Rect screen = VideoRegionMath::mapNormalizedSourceRectToScreen(normalized, 2000.0f, 1000.0f, cropSrc, dest);
		VR_CHECK_NEAR(screen.x, 250.0f, 1e-2); // (1000 - 500)*0.5 = 250
		VR_CHECK_NEAR(screen.y, 250.0f, 1e-2); // (500 - 0)*0.5 = 250
		VR_CHECK_NEAR(screen.width, 50.0f, 1e-2); // 100px blob * scale 0.5
		VR_CHECK_NEAR(screen.height, 50.0f, 1e-2);
	}

	// ── rectIoU ──────────────────────────────────────────────────────────
	void test_iou_identicalRects_isOne() {
		Rect r { 10, 10, 20, 20 };
		VR_CHECK_NEAR(VideoRegionMath::rectIoU(r, r), 1.0f, 1e-4);
	}

	void test_iou_disjointRects_isZero() {
		Rect a { 0, 0, 10, 10 };
		Rect b { 100, 100, 10, 10 };
		VR_CHECK_NEAR(VideoRegionMath::rectIoU(a, b), 0.0f, 1e-4);
	}

	void test_iou_halfOverlap() {
		Rect a { 0, 0, 10, 10 };
		Rect b { 5, 0, 10, 10 };
		// intersection = 5x10=50, union = 100+100-50=150
		VR_CHECK_NEAR(VideoRegionMath::rectIoU(a, b), 50.0f / 150.0f, 1e-4);
	}

	// ── lerpRect ─────────────────────────────────────────────────────────
	void test_lerpRect_zeroFactor_staysAtFrom() {
		Rect from { 0, 0, 10, 10 };
		Rect to { 100, 100, 50, 50 };
		Rect r = VideoRegionMath::lerpRect(from, to, 0.0f, 0.0f);
		VR_CHECK_NEAR(r.x, from.x, 1e-4);
		VR_CHECK_NEAR(r.y, from.y, 1e-4);
		VR_CHECK_NEAR(r.width, from.width, 1e-4);
		VR_CHECK_NEAR(r.height, from.height, 1e-4);
	}

	void test_lerpRect_oneFactor_jumpsToTarget() {
		Rect from { 0, 0, 10, 10 };
		Rect to { 100, 100, 50, 50 };
		Rect r = VideoRegionMath::lerpRect(from, to, 1.0f, 1.0f);
		float toCx, toCy, rCx, rCy;
		VideoRegionMath::rectCenter(to, toCx, toCy);
		VideoRegionMath::rectCenter(r, rCx, rCy);
		VR_CHECK_NEAR(rCx, toCx, 1e-3);
		VR_CHECK_NEAR(rCy, toCy, 1e-3);
		VR_CHECK_NEAR(r.width, to.width, 1e-3);
		VR_CHECK_NEAR(r.height, to.height, 1e-3);
	}

	void test_lerpRect_independentPositionAndSizeFactors() {
		// Regression guard for the actual reason position/size use separate
		// factors in BlobTracker::Config: position should be able to move
		// while size stays put, and vice versa.
		Rect from { 0, 0, 10, 10 };
		Rect to { 100, 0, 10, 10 }; // only position differs
		Rect r = VideoRegionMath::lerpRect(from, to, 1.0f, 0.0f);
		VR_CHECK_NEAR(r.width, 10.0f, 1e-3);
		VR_CHECK_NEAR(r.height, 10.0f, 1e-3);
		float rCx, rCy;
		VideoRegionMath::rectCenter(r, rCx, rCy);
		VR_CHECK_NEAR(rCx, 105.0f, 1e-2); // to's center.x
	}

	// ── greedyAssociateByDistance (the actual BlobTracker matching code) ───
	void test_associate_exactMatches() {
		std::vector<Rect> tracks = { { 0, 0, 10, 10 }, { 100, 100, 10, 10 } };
		std::vector<Rect> detections = { { 100, 100, 10, 10 }, { 0, 0, 10, 10 } }; // deliberately reordered
		auto result = VideoRegionMath::greedyAssociateByDistance(tracks, detections, 0.5f, 0.1f);
		VR_CHECK(result.size() == 2);
		VR_CHECK(result[0] == 1); // track 0 (at 0,0) matches detection 1 (at 0,0)
		VR_CHECK(result[1] == 0); // track 1 (at 100,100) matches detection 0 (at 100,100)
	}

	void test_associate_noCandidateWithinRange_unmatched() {
		std::vector<Rect> tracks = { { 0, 0, 10, 10 } };
		std::vector<Rect> detections = { { 1000, 1000, 10, 10 } };
		auto result = VideoRegionMath::greedyAssociateByDistance(tracks, detections, 5.0f, 0.9f);
		VR_CHECK(result.size() == 1);
		VR_CHECK(result[0] == -1);
	}

	void test_associate_closerTrackWinsContestedDetection() {
		// Two tracks both plausibly match one detection; the nearer one
		// should win, and the loser should end up unmatched rather than
		// being forced onto a worse pairing.
		std::vector<Rect> tracks = { { 0, 0, 10, 10 }, { 3, 0, 10, 10 } };
		std::vector<Rect> detections = { { 1, 0, 10, 10 } };
		auto result = VideoRegionMath::greedyAssociateByDistance(tracks, detections, 50.0f, 0.0f);
		VR_CHECK(result[0] == 0); // track 0 (center distance 1) beats track 1 (center distance 2)
		VR_CHECK(result[1] == -1);
	}

	void test_associate_ambiguousDetectionCountMismatch() {
		// More detections than tracks: exactly one becomes a new-track
		// candidate (unmatched) from the caller's perspective (BlobTracker
		// creates a new Track for every detection index not present in the
		// returned matches).
		std::vector<Rect> tracks = { { 0, 0, 10, 10 } };
		std::vector<Rect> detections = { { 0, 0, 10, 10 }, { 500, 500, 10, 10 } };
		auto result = VideoRegionMath::greedyAssociateByDistance(tracks, detections, 5.0f, 0.1f);
		VR_CHECK(result.size() == 1);
		VR_CHECK(result[0] == 0);
	}

}

int main() {
	test_normalizedToSourcePixels_basic();
	test_normalizedToSourcePixels_fullFrame();

	test_clamp_fullyInside_unchanged();
	test_clamp_negativePosition_shiftedIntoBounds();
	test_clamp_largerThanBounds_sizeCapped();
	test_clamp_degenerateBounds_collapsesToZero();

	test_cropFill_matchingAspect_wholeFrame();
	test_cropFill_widerSource_cropsSides();
	test_mapNormalizedSourceRectToScreen_identity();
	test_mapNormalizedSourceRectToScreen_croppedAndScaled();

	test_iou_identicalRects_isOne();
	test_iou_disjointRects_isZero();
	test_iou_halfOverlap();

	test_lerpRect_zeroFactor_staysAtFrom();
	test_lerpRect_oneFactor_jumpsToTarget();
	test_lerpRect_independentPositionAndSizeFactors();

	test_associate_exactMatches();
	test_associate_noCandidateWithinRange_unmatched();
	test_associate_closerTrackWinsContestedDetection();
	test_associate_ambiguousDetectionCountMismatch();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
