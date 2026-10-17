#pragma once

#include <string>
#include <vector>

// BlobPresetCoverage — predeploy check that every production media item has
// a Blob detection preset (assets/shared/blob/blob-detection-profiles.json).
//
// Media identity is the production code itself: discovered files go
// through MediaCatalog::build() (and its synthesizeMediaId()) exactly as
// VideoPlaybackService does; profiles are parsed by the production
// BlobDetectionProfileStore. Only VideoPlaybackService's directory scan is
// mirrored here (its ofDirectory needs openFrameworks): top level of the
// media root, ".mp4" case-insensitive, hidden files skipped.
//
// A missing preset is a WARNING, not a runtime problem: ExperienceRuntime
// applies the canonical detector defaults to unprofiled media. Structurally
// bad inputs are an ERROR. Pure C++17, no openFrameworks.
namespace blobpresetcoverage {

enum class Result { Pass, Warn, Error };
const char * toString(Result r);

struct MediaItem {
	std::string relativePath;
	std::string mediaId;
	std::string title; // catalog fallbackDisplayTitle
};

struct Report {
	Result result = Result::Error;
	std::vector<MediaItem> media; // deployable (enabled) production media
	std::vector<MediaItem> configured;
	std::vector<MediaItem> missing;
	std::vector<std::string> staleProfileIds; // presets whose media no longer exists
	std::vector<std::string> errors; // why the result is ERROR
	std::vector<std::string> notes; // non-blocking diagnostics
};

// Mirrors VideoPlaybackService's discovery. false (with error) if the root
// is missing or unreadable.
bool discoverMedia(const std::string & mediaRoot, std::vector<std::string> & relativePaths, std::string & error);

// catalogPath: optional overrides file, as VideoPlaybackService::Config::
// catalogPath ("" = none, which is what ExperienceRuntime uses today).
Report evaluate(const std::string & mediaRoot, const std::string & profilesPath, const std::string & catalogPath = "");

// Human-readable report (missing list, fallback explanation, how to fix).
std::string format(const Report & report, const std::string & profilesPath);

} // namespace blobpresetcoverage
