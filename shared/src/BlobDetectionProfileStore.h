#pragma once

#include "nlohmann/json.hpp"

#include <optional>
#include <string>
#include <vector>

// BlobDetectionProfileStore — per-video Blob detection presets.
//
//   canonical Shared Video mediaId (VideoPlaybackStatus::mediaId)
//     -> BlobDetectionProfile -> BlobDetector::Config (BlobDetectionProfileApply.h)
//
// Authored by developer tooling (the standalone blob-region-prototype's
// save key); read-only for the production runtime, which never writes it.
// File: assets/shared/blob/blob-detection-profiles.json
//
//   {
//     "schemaVersion": 1,
//     "profiles": {
//       "media.auto.1a2b3c4d": { "threshold": 30, "minBlobAreaNormalized": 0.0005,
//                               "maxBlobAreaNormalized": 0.35, "erodeIterations": 0,
//                               "dilateIterations": 2 }
//     }
//   }
//
// Every field is optional: an absent field means "the detector's canonical
// default". The whole document is kept as loaded, so saving one media's
// profile preserves every other entry (including ones this version could
// not parse) and any unknown fields; keys serialize sorted for stable diffs.
//
// Blob-local and openFrameworks-free (test: sketches/experience_runtime/
// test/blob_detection_profile_tests.cpp). Not a Shared Video or Shared
// Effects schema.

struct BlobDetectionProfile {
	std::optional<int> threshold; // 0..255
	std::optional<double> minBlobAreaNormalized; // [0,1]
	std::optional<double> maxBlobAreaNormalized; // [0,1], > min
	std::optional<int> erodeIterations; // 0..8
	std::optional<int> dilateIterations; // 0..8

	bool operator==(const BlobDetectionProfile & o) const {
		return threshold == o.threshold && minBlobAreaNormalized == o.minBlobAreaNormalized
			&& maxBlobAreaNormalized == o.maxBlobAreaNormalized && erodeIterations == o.erodeIterations
			&& dilateIterations == o.dilateIterations;
	}
};

// Fields of `over` that are set win; everything else comes from `defaults`.
BlobDetectionProfile resolveProfile(const BlobDetectionProfile & defaults, const BlobDetectionProfile * over);

// Multi-line "[BLOB DETECTION PROFILE]" block for logs/overlays.
std::string describeProfile(const std::string & mediaId, const std::string & source, const BlobDetectionProfile & effective);

class BlobDetectionProfileStore {
public:
	static constexpr int kSchemaVersion = 1;
	enum class LoadStatus { Loaded, Missing, Invalid };

	// Missing file: empty store, writable (first save creates it).
	// Invalid document (unparseable, wrong schemaVersion, no "profiles"
	// object): empty store, NOT writable, so a save can never clobber it.
	// Individually invalid entries are skipped (that media gets defaults)
	// but kept in the document. messages() explains every rejection.
	LoadStatus load(const std::string & path);
	LoadStatus loadFromString(const std::string & text);

	const BlobDetectionProfile * find(const std::string & mediaId) const;
	size_t size() const { return profiles_.size(); }
	std::vector<std::string> mediaIds() const; // valid profiles, sorted
	const std::vector<std::string> & messages() const { return messages_; }
	bool writable() const { return writable_; }

	// Updates one media's entry in memory (other entries untouched).
	// Refuses an empty mediaId: no hidden filename-based fallback key.
	bool set(const std::string & mediaId, const BlobDetectionProfile & profile, std::string & error);

	std::string serialize() const; // deterministic, 2-space indented
	// Writes via a temp file + rename. Refuses when !writable().
	bool save(const std::string & path, std::string & error) const;

private:
	static bool parseEntry(const nlohmann::json & entry, BlobDetectionProfile & out, std::string & why);
	void reset();

	nlohmann::json document_;
	std::vector<std::pair<std::string, BlobDetectionProfile>> profiles_;
	std::vector<std::string> messages_;
	bool writable_ = true;
};

// Applies a profile once per media change, never per frame.
class BlobDetectionProfileTracker {
public:
	struct Applied {
		std::string mediaId;
		std::string source; // "per_media" | "defaults"
		BlobDetectionProfile effective;
	};

	// Returns a result only when mediaId differs from the last applied one
	// (including the first media seen). nullopt mediaId: nothing applies.
	std::optional<Applied> observe(const std::optional<std::string> & mediaId, const BlobDetectionProfileStore & store,
		const BlobDetectionProfile & defaults);
	// Forces the next observe() to reapply (after a reload/save).
	void invalidate() { lastMediaId_.reset(); }
	const std::optional<Applied> & last() const { return last_; }

private:
	std::optional<std::string> lastMediaId_;
	std::optional<Applied> last_;
};
