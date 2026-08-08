#pragma once

#include "MediaMetadata.h"

#include <string>
#include <vector>

// ============================================================================
// MediaCatalog.h — turns a directory scan plus an optional catalog-file
// override list into a validated std::vector<MediaMetadata>.
//
// Split deliberately into two pieces:
//
//   1. MediaCatalog::build() — pure C++, no filesystem access, no JSON
//      parsing, no openFrameworks. Takes already-discovered relative paths
//      (scanned by the OF-dependent caller — see VideoPlaybackService.cpp)
//      and already-parsed override rows (parsed by
//      loadCatalogOverridesFromJsonFile() below), and produces the
//      validated catalog. This is the piece covered by
//      test/media_catalog_tests.cpp, following the same "dependency-free
//      standalone test" convention already established by
//      sketches/blob-region-prototype/test/videoregion_math_tests.cpp,
//      sketches/experience_runtime/test/lifecycle_state_tests.cpp, and
//      sketches/temporal-fields/test/tf_timeline_tests.cpp.
//
//   2. loadCatalogOverridesFromJsonFile() — file I/O + JSON parsing (via
//      libs/json/include/nlohmann/json.hpp, which every sketch in this
//      repo already has on its include path through core openFrameworks'
//      own ofJson.h dependency — no new addon/include-path wiring
//      required). Deliberately NOT exercised by the dependency-free test
//      target; validated instead by the real sketch build
//      (VideoPlaybackService's own setup() call site).
//
// No catalog file exists anywhere in this repo today (confirmed by
// docs/video-playback-ownership-probe-report.md §D, "Metadata / filename
// handling" row) — every runtime scene's media directory is scanned files
// only. build() is written so an entirely absent/empty overrides list is a
// fully supported, non-error path: every discovered file gets a
// synthesized MediaMetadata entry. A catalog file becomes purely additive
// — authoring curated titleId/fallbackDisplayTitle/enabled/piSafe values
// for specific files, never required for the system to function.
// ============================================================================

// One row parsed from an optional JSON catalog file. Any field left absent
// in the source JSON falls back to the same synthesis MediaCatalog::build()
// would have used for a file with no override at all.
struct MediaCatalogEntryOverride {
	std::string relativePath;  // required — the join key against a scanned file
	std::string mediaId;       // empty = synthesize
	std::string titleId;       // empty = synthesize
	std::string fallbackDisplayTitle;  // empty = synthesize
	bool enabled = true;
	bool piSafe = true;
};

struct MediaCatalogBuildResult {
	std::vector<MediaMetadata> entries;

	// Human-readable validation errors (duplicate mediaId, malformed/
	// unresolvable override entry, etc.) — logged by the caller, never
	// thrown. An entry that produced an error is excluded from `entries`,
	// not silently included half-valid — "missing or malformed catalog
	// entries must produce clear validation errors" plus "catalog loading
	// must not crash the runtime" (Implement-Shared-Video-Playback-
	// System-Agent-Prompt.md §5.1/§12) are both satisfied by this shape:
	// the runtime always gets a value back, callers can inspect why
	// anything is missing.
	std::vector<std::string> errors;
};

class MediaCatalog {
public:
	// discoveredRelativePaths: every playable-extension file found beneath
	// the canonical media root, relative to that root, forward-slash
	// separated (matching the scan convention already used by
	// VideoSampler/TimeOffsetVideoBuffer/VideoSystem — see
	// docs/video-playback-ownership-probe-report.md §B). Order is
	// preserved from the input (the caller decides scan order); build()
	// itself performs no scanning and no shuffling — playlist order is
	// VideoSelectionPolicy's concern, not the catalog's.
	//
	// overrides: parsed rows from an optional catalog file. An override
	// whose relativePath does not match any discovered file is reported as
	// an error and skipped (it names media that isn't actually present) —
	// this deliberately does NOT create a catalog entry for a file that
	// doesn't exist on disk.
	//
	// Duplicate mediaId values (whether both synthesized, both
	// overridden, or one of each) are rejected: the first occurrence
	// (input order) wins, every later duplicate is reported as an error
	// and excluded from `entries` entirely — "duplicate mediaId values
	// must be rejected" (§5.1) is read here as "never let two catalog
	// entries claim the same identity," not "reject the whole catalog."
	static MediaCatalogBuildResult build(
		const std::vector<std::string>& discoveredRelativePaths,
		const std::vector<MediaCatalogEntryOverride>& overrides);

	// Deterministic synthesis, exposed publicly so tests (and any future
	// caller that wants to predict an auto-assigned ID before build() runs)
	// don't have to reverse-engineer build()'s internals. Both are pure
	// functions of relativePath only — no randomness, no filesystem, no
	// dependency on catalog order — so the same relativePath always
	// produces the same mediaId/title across repeated scans, process
	// restarts, and machines (this is what "stable and independent of
	// absolute paths" in MediaMetadata.h means in practice).
	static std::string synthesizeMediaId(const std::string& relativePath);
	static std::string synthesizeTitleId(const std::string& mediaId);
	static std::string synthesizeFallbackDisplayTitle(const std::string& relativePath);
};

// File I/O + JSON parsing. `path` may not exist (a missing catalog file is
// not an error — see MediaCatalog.h's header comment: catalog files are
// purely additive) or may fail to parse (malformed JSON) — both cases
// return an empty vector and append a human-readable entry to outErrors
// rather than throwing. Defined in MediaCatalog.cpp; not part of the
// dependency-free test target (see that file's own top comment).
std::vector<MediaCatalogEntryOverride> loadCatalogOverridesFromJsonFile(
	const std::string& path, std::vector<std::string>& outErrors);
