#pragma once

#include <cstdint>
#include <optional>
#include <string>

// ============================================================================
// VideoPlaybackStatus.h — the immutable, per-frame status snapshot published
// by VideoPlaybackService.
//
// Transcribed verbatim (types, field names, defaults) from the approved
// design in:
//   - Shared-Video-Playback-HUD-Semantic-Slot-Review.md ("Proposed
//     VideoPlaybackStatus")
//   - Implement-Shared-Video-Playback-System-Agent-Prompt.md §6
// Both sources agree on this exact shape; nothing here is invented.
//
// Deliberately has NO dependency on openFrameworks (no ofTexture, ofPixels,
// glm) and no dependency on the rest of shared/src/video-playback/ — this
// header is included by VideoPlaybackService.h (OF-dependent) AND by the
// dependency-free policy/test code under test/ (see that directory's
// Makefile.tests header comment for why: even ofRectangle.h pulls in
// GL/glew.h transitively, and this type must be constructible/comparable
// from pure policy tests with nothing but a bare compiler).
//
// Snapshot rules (enforced by VideoPlaybackService, not by this struct
// itself — a plain struct cannot enforce invariants on its own fields):
//   - No getter may clear a change flag (this struct has no getters at all;
//     every field is read directly, so there is nothing to make
//     destructive — see docs/video-playback-ownership-probe-report.md's
//     finding on VideoSystem::fileChanged() for the anti-pattern this
//     avoids).
//   - No field may expose a mutable internal reference, texture, pixel,
//     decoder handle, path, or playlist state.
//   - Media identity, health, timing, and selectionOrigin must all be
//     updated atomically, in one assignment of a freshly-built
//     VideoPlaybackStatus value — never field-by-field mutation of a
//     long-lived instance a caller might observe mid-update.
//   - `health == VideoPlaybackHealth::Ready` must imply `mediaId` names the
//     successfully active item.
//   - Loading/Failed states must not report a fake `0.0f` progress —
//     unavailable/unreliable values use `std::nullopt`, never `0`.
// ============================================================================

enum class VideoPlaybackHealth : uint8_t {
	Unavailable,  // no service, no catalog, or no playable media
	Loading,      // a selected item is being opened/prepared
	Ready,        // the active item is playable and current
	Degraded,     // playback continues, but fallback/recoverable trouble exists
	Failed        // no usable active media is available right now
};

enum class MediaSelectionOrigin : uint8_t {
	Startup,
	Automatic,
	ManualPrevious,
	ManualNext,
	Recovery
};

struct VideoPlaybackStatus {
	uint32_t schemaVersion = 1;

	// Active media identity — catalog-authored, never a raw filename.
	std::optional<std::string> mediaId;
	std::optional<std::string> titleId;
	std::optional<std::string> fallbackDisplayTitle;

	// Playback health.
	VideoPlaybackHealth health = VideoPlaybackHealth::Unavailable;
	std::optional<std::string> messageId;
	std::optional<std::string> errorCode;

	// Current loop iteration — see MediaCatalog/VideoPlaybackService.h's
	// header comments for the normative definition (normalized position
	// within the CURRENT loop of the active clip, resets on loop, distinct
	// from hold progress).
	std::optional<float> playbackProgress;  // [0,1]
	std::optional<float> durationSeconds;
	std::optional<float> positionSeconds;

	// Automatic-selection hold interval — how long the current item remains
	// the active automatic-selection candidate, independent of clip loops.
	std::optional<float> holdProgress;  // [0,1]
	std::optional<float> holdElapsedSeconds;
	std::optional<float> holdDurationSeconds;
	std::optional<float> holdRemainingSeconds;

	// Selection/navigation state.
	MediaSelectionOrigin selectionOrigin = MediaSelectionOrigin::Startup;

	bool canSelectPrevious = false;
	bool canSelectNext = false;
};
