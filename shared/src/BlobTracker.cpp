#include "BlobTracker.h"
#include "VideoRegionMathOf.h"
#include <algorithm>
#include <limits>

using VideoRegionMath::Rect;

void BlobTracker::update(const std::vector<BlobDetection> & detections, float dt) {
	// Build the plain-Rect inputs greedyAssociateByDistance needs — the
	// actual matching algorithm lives in VideoRegionMath (see that header's
	// comment) so it's exercised standalone by
	// test/videoregion_math_tests.cpp against the exact same code path used
	// here, not a re-implementation.
	std::vector<Rect> trackRects;
	trackRects.reserve(tracks.size());
	for (const Track & t : tracks) {
		trackRects.push_back(t.smoothedBounds);
	}

	std::vector<Rect> detectionRects;
	detectionRects.reserve(detections.size());
	for (const BlobDetection & d : detections) {
		detectionRects.push_back(VideoRegionMath::toRect(d.normalizedBounds));
	}

	std::vector<int> matchedDetForTrack = VideoRegionMath::greedyAssociateByDistance(
		trackRects, detectionRects, config.maxAssociationDistanceNormalized, config.minIoU);

	std::vector<bool> detMatched(detections.size(), false);

	for (size_t ti = 0; ti < tracks.size(); ti++) {
		int di = matchedDetForTrack[ti];
		if (di < 0) {
			continue;
		}
		detMatched[di] = true;

		Track & track = tracks[ti];
		const BlobDetection & det = detections[di];
		Rect detRect = detectionRects[di];

		track.lastDetectionBounds = detRect;
		track.confidence = det.confidence;
		track.missingSeconds = 0.0f;
		track.ageSeconds += dt;
		track.smoothedBounds = VideoRegionMath::lerpRect(
			track.smoothedBounds, detRect, config.positionSmoothing, config.sizeSmoothing);
	}

	// Unmatched existing tracks: age their missing-time; the ones that
	// exceed maxMissingSeconds are dropped below. Bounds are deliberately
	// left untouched here — a track's position holds at its last smoothed
	// value during the grace period rather than snapping or vanishing, so
	// a one-frame missed detection doesn't read as churn downstream.
	for (size_t ti = 0; ti < tracks.size(); ti++) {
		if (matchedDetForTrack[ti] != -1) {
			continue;
		}
		tracks[ti].missingSeconds += dt;
		tracks[ti].ageSeconds += dt;
	}

	tracks.erase(
		std::remove_if(tracks.begin(), tracks.end(), [this](const Track & t) {
			return t.missingSeconds > config.maxMissingSeconds;
		}),
		tracks.end());

	// Unmatched detections become new tracks. smoothedBounds starts equal
	// to the raw detection (no artificial pop-in lag on first appearance).
	for (size_t di = 0; di < detections.size(); di++) {
		if (detMatched[di]) {
			continue;
		}
		Track t;
		t.id = nextTrackId++;
		Rect detRect = VideoRegionMath::toRect(detections[di].normalizedBounds);
		t.lastDetectionBounds = detRect;
		t.smoothedBounds = detRect;
		t.confidence = detections[di].confidence;
		t.ageSeconds = 0.0f;
		t.missingSeconds = 0.0f;
		tracks.push_back(t);
	}

	rebuildActiveRegions();
}

void BlobTracker::rebuildActiveRegions() {
	activeRegions.clear();
	activeRegions.reserve(tracks.size());
	for (const Track & t : tracks) {
		VideoRegion region;
		region.id = t.id;
		region.normalizedSourceBounds = VideoRegionMath::toOf(t.lastDetectionBounds);
		region.smoothedNormalizedBounds = VideoRegionMath::toOf(t.smoothedBounds);
		region.confidence = t.confidence;
		region.ageSeconds = t.ageSeconds;
		region.missingSeconds = t.missingSeconds;
		// effectName/effectAmount/temporalOffset intentionally left at
		// VideoRegion's defaults — assigned by VideoRegionController.
		activeRegions.push_back(region);
	}
}
