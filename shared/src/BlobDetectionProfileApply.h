#pragma once

#include "BlobDetectionProfileStore.h"
#include "BlobDetector.h"
#include "ofUtils.h"

#include <cmath>
#include <cstdlib>
#include <string>

// Bridges BlobDetectionProfile (OF-free) and BlobDetector::Config, shared
// by the production scene and the standalone developer app.
namespace blobdetectionprofile {

// The authored profile file. EXPERIENCE_RUNTIME_BLOB_DETECTION_PROFILES
// points elsewhere (fixtures, scratch copies); otherwise the canonical
// Blob-owned asset, resolved like the shared media root.
inline std::string profilesPath() {
	if (const char * p = std::getenv("EXPERIENCE_RUNTIME_BLOB_DETECTION_PROFILES")) {
		if (*p != '\0') {
			return p;
		}
	}
	return ofToDataPath("../../../../assets/shared/blob/blob-detection-profiles.json", true);
}

// float -> double with 6 decimals, so 0.0008f is stored as 0.0008 rather
// than 0.000800000038.
inline double tidy(float v) {
	return std::round(static_cast<double>(v) * 1e6) / 1e6;
}

inline BlobDetectionProfile fromConfig(const BlobDetector::Config & c) {
	BlobDetectionProfile p;
	p.threshold = c.threshold;
	p.minBlobAreaNormalized = tidy(c.minBlobAreaNormalized);
	p.maxBlobAreaNormalized = tidy(c.maxBlobAreaNormalized);
	p.erodeIterations = c.erodeIterations;
	p.dilateIterations = c.dilateIterations;
	return p;
}

// BlobDetector::Config's own defaults: the single source of truth for
// "no profile" (absent fields fall back to these, too).
inline BlobDetectionProfile canonicalDefaults() {
	return fromConfig(BlobDetector::Config {});
}

// Writes the five profile fields; every other detector field is untouched.
inline void applyTo(BlobDetector::Config & c, const BlobDetectionProfile & effective) {
	BlobDetectionProfile p = resolveProfile(canonicalDefaults(), &effective);
	c.threshold = *p.threshold;
	c.minBlobAreaNormalized = static_cast<float>(*p.minBlobAreaNormalized);
	c.maxBlobAreaNormalized = static_cast<float>(*p.maxBlobAreaNormalized);
	c.erodeIterations = *p.erodeIterations;
	c.dilateIterations = *p.dilateIterations;
}

} // namespace blobdetectionprofile
