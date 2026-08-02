#pragma once

#include "ofRectangle.h"
#include <string>

// Canonical shared data model for a tracked video region (a blob today; a
// motion mask, manual region, or HUD-selected region later — see
// VideoRegionController). Normalized source coordinates are the canonical
// representation: analysis-resolution and render-resolution rectangles are
// derived at the edges (BlobDetector produces normalized output already;
// Fragment::setNormalizedSourceBounds() converts to source pixels at the
// render boundary; VideoRegionMath::mapNormalizedSourceRectToScreen()
// converts to screen pixels at the render boundary), never carried as
// canonical state here.
//
// This struct owns nothing: no texture, no FBO, no video player, no shader,
// no CV image. See docs/blob-region-architecture.md for the full ownership
// boundary this type sits inside:
//   Blob detector owns detections.
//   Blob tracker owns identity and smoothing.
//   Region controller owns active region lifecycle.
//   Fragment renderer owns drawing.
//   Video source owns decode and current-frame texture.
//   Shader library owns shader instances.
struct VideoRegion {
	int id = -1;

	// Raw (unsmoothed) latest normalized bounds from the tracker's
	// detection-to-track association, [0,1] against the full source frame.
	ofRectangle normalizedSourceBounds;

	// Smoothed normalized bounds — what renderers should actually draw.
	// BlobTracker updates this via VideoRegionMath::lerpRect each time the
	// track is matched; it holds its last value (does not snap) while the
	// track is in its missing-detection grace period.
	ofRectangle smoothedNormalizedBounds;

	float confidence = 0.0f; // currently mirrors BlobDetection::confidence at match time
	float ageSeconds = 0.0f; // time since this track's id was first created
	float missingSeconds = 0.0f; // time since the last matched detection; 0 while actively matched

	// Assigned and held stable by VideoRegionController, keyed by id — see
	// VideoRegionController::assignEffectForId(). Empty until the
	// controller has processed this region at least once.
	std::string effectName;
	float effectAmount = 1.0f;

	float temporalOffset = 0.0f; // reserved for later (per-region delayed-frame sampling); unused in V1
};
