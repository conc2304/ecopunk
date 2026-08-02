#pragma once

#include "BlobDetector.h"
#include "VideoRegion.h"
#include "VideoRegionMath.h"
#include <vector>

// Associates each frame's BlobDetection list with persistent track
// identities, smooths their bounds, and emits VideoRegion objects. Owns
// identity + smoothing only (per docs/blob-region-architecture.md's
// ownership boundary) — it does not decide effects, does not cap the
// number of fragments actually rendered, and does not touch a texture, FBO,
// or shader. That's VideoRegionController's job, one layer up.
class BlobTracker {
public:
	struct Config {
		// A detection matches an existing track if EITHER condition holds
		// (see update()'s candidate-pair construction): centroid distance
		// below this, or IoU above minIoU. Both are in normalized [0,1]
		// source-frame units/fractions, so they're resolution-independent.
		float maxAssociationDistanceNormalized = 0.18f;
		float minIoU = 0.08f;

		// A track survives this many seconds with no matching detection
		// before being retired — this is what keeps a brief missed
		// detection from being treated as the blob disappearing.
		float maxMissingSeconds = 0.5f;

		// Exponential-style smoothing factor applied once per matched
		// update: smoothed = lerp(smoothed, target, factor). 0 = never
		// moves, 1 = snaps instantly (no smoothing). Position and size are
		// smoothed independently so a fast-moving-but-stable-sized blob
		// doesn't have its size lag as much as its position, or vice versa.
		float positionSmoothing = 0.35f;
		float sizeSmoothing = 0.25f;
	};

	void setup(const Config & initialConfig) { config = initialConfig; }

	Config & getConfig() { return config; }
	const Config & getConfig() const { return config; }

	void update(const std::vector<BlobDetection> & detections, float dt);

	const std::vector<VideoRegion> & getActiveRegions() const { return activeRegions; }

	int getTrackCount() const { return static_cast<int>(tracks.size()); }

private:
	struct Track {
		int id = -1;
		VideoRegionMath::Rect lastDetectionBounds; // most recent raw match, source-normalized
		VideoRegionMath::Rect smoothedBounds; // what VideoRegion reports
		float confidence = 0.0f;
		float ageSeconds = 0.0f;
		float missingSeconds = 0.0f;
	};

	Config config;
	std::vector<Track> tracks;
	std::vector<VideoRegion> activeRegions;
	int nextTrackId = 1;

	void rebuildActiveRegions();
};
