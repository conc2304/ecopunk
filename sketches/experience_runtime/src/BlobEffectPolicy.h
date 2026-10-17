#pragma once

#include <cstdint>
#include <optional>
#include <string>

// BlobEffectPolicy.h — Blob production visual model, scene-level half.
//
// The full frame is drawn desaturated (backgroundEffectId()); tracked
// regions reveal Original color or a shader treatment chosen per fragment
// by the effect budget (BlobEffectBudget.h). This file owns only the
// media-change signal: each new canonical Shared Video media item starts a
// new palette generation, so new fragments draw fresh treatments while
// fragments already on screen keep theirs (no flicker on media change).
//
// Pure, openFrameworks-free (test/blob_effect_policy_tests.cpp).
// Scene-local, domain-owned — not a Shared Effects authority (Blob's
// HudFrameData.effects stays std::nullopt).
namespace blobeffects {

// The full-frame background effect for this visual model.
const std::string & backgroundEffectId();

class MediaGeneration {
public:
	// Call once per active update with the canonical
	// VideoPlaybackStatus::mediaId. Returns true only when a new
	// generation starts on this call:
	//   - nullopt (no active media): no change.
	//   - first media ever observed: generation stays 0, returns false.
	//   - same mediaId as last time: no change.
	//   - different mediaId: generation + 1.
	// Never called while Blob is inactive, so media that came and went
	// while another scene was showing counts as one change on return.
	bool observeMedia(const std::optional<std::string> & mediaId);

	uint64_t generation() const { return generation_; }

private:
	std::optional<std::string> lastMediaId_;
	uint64_t generation_ = 0;
};

} // namespace blobeffects
