#pragma once

#include <string>

// ============================================================================
// MediaMetadata.h — one canonical media catalog entry.
//
// Per Implement-Shared-Video-Playback-System-Agent-Prompt.md §5.1, verbatim
// shape. No openFrameworks dependency (pure C++) — consumed by both the
// dependency-free MediaCatalog/VideoSelectionPolicy tests and the real
// OF-dependent VideoPlaybackService.
// ============================================================================

struct MediaMetadata {
	// Stable, independent of absolute paths. Synthesized deterministically
	// from relativePath when no catalog-file override supplies one — see
	// MediaCatalog::build()'s header comment for the exact algorithm — so
	// the same checkout produces the same mediaId across runs without
	// depending on where the repo happens to be cloned.
	std::string mediaId;

	// Resolves beneath the canonical media root the owning
	// VideoPlaybackService was configured with — never an absolute path,
	// never a path outside that root.
	std::string relativePath;

	// Vocabulary-addressable ID for the curated cinematic title. Resolved
	// by the HUD's vocabulary layer, never rendered directly by this
	// subsystem — see VideoPlaybackStatus.h's header comment and
	// Shared-Video-Playback-HUD-Semantic-Slot-Review.md §5's title
	// resolution order.
	std::string titleId;

	// Fallback-only display text, used solely when titleId fails to
	// resolve in vocabulary (missing pack entry, vocabulary not yet
	// loaded, etc.) — see VideoPlaybackStatus.h. A lightly humanized
	// version of the filename (extension stripped, separators replaced
	// with spaces), never the literal raw filename — "raw filenames must
	// not be the normal cinematic title" per the approved product
	// direction, and this field is explicitly the exceptional fallback
	// path, not the normal one.
	std::string fallbackDisplayTitle;

	bool enabled = true;
	bool piSafe = true;
};
