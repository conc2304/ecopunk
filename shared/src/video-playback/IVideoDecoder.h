#pragma once

#include "glm/vec2.hpp"
#include "ofPixels.h"
#include "ofTexture.h"

#include <string>

// ============================================================================
// IVideoDecoder.h — the seam between VideoPlaybackService's policy
// (selection/history/hold — see VideoSelectionPolicy.h, which has NO
// dependency on this file or on openFrameworks at all) and one actual
// decoded video source.
//
// OF-dependent (ofTexture/ofPixels/glm) — deliberately NOT part of the
// dependency-free test target; see VideoSelectionPolicy.h's header comment
// and test/README (if present) for which pieces of this subsystem are
// pure-tested vs. build-validated. Per Implement-Shared-Video-Playback-
// System-Agent-Prompt.md §14.5: "If real decoding cannot run in CI, split
// pure policy tests from an integration harness using a decoder test
// double" — this interface IS that seam; a future integration harness can
// implement it as a fake without touching VideoPlaybackService's own
// logic. This increment validates the real OfVideoDecoder implementation
// via the blob-region-prototype build/migration instead of a separate
// fake-decoder harness — see this increment's implementation report,
// "Deviations from this prompt".
// ============================================================================

class IVideoDecoder {
public:
	virtual ~IVideoDecoder() = default;

	// Synchronous, matching every existing wrapper in this repo
	// (VideoSampler, TimeOffsetVideoBuffer, VideoSystem — see
	// docs/video-playback-ownership-probe-report.md §B) — oF's
	// ofVideoPlayer::load() itself resolves synchronously enough for those
	// wrappers to treat load-then-play as one atomic step, and this
	// interface preserves that rather than inventing an async contract
	// nothing in this codebase currently needs.
	virtual bool load(const std::string& absolutePath) = 0;

	virtual void update(float dt) = 0;
	virtual void close() = 0;

	virtual bool isLoaded() const = 0;
	virtual bool isFrameNew() const = 0;

	// Both return values in a well-defined "unavailable" state rather than
	// asserting/throwing when nothing is loaded: getPosition() -> -1.0f,
	// getDuration() -> 0.0f. VideoPlaybackService.cpp is responsible for
	// translating those sentinels into VideoPlaybackStatus's std::nullopt
	// fields — this interface itself stays a thin decoder wrapper, not a
	// status-shaping layer.
	virtual float getPosition() const = 0;
	virtual float getDuration() const = 0;

	virtual glm::ivec2 getSize() const = 0;
	virtual const ofTexture* getTexture() const = 0;  // nullptr if unavailable
	virtual const ofPixels* getPixels() const = 0;    // nullptr if unavailable
};
