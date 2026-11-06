// OC-003B -- RAW VIDEO sampling gate (OF-free, header-only; test/raw_video_gate_tests.cpp).
//
// Decides, once per frame at the main-thread sampling point, whether the borrowed
// VideoPlaybackService::currentTexture() may be measured, and owns the RAW
// generation counter. Pure logic: the caller supplies an Observation built from
// the service (status().health, mediaId, currentTexture(), sourceSize(),
// isFrameNew(), activationCount()) and the runtime's scene generation.
//
// Eligibility is never inferred from a non-null texture pointer alone:
//   Ready AND mediaId present AND texture allocated AND texture size == source size
//   AND at least one NEW decoded frame observed since the current activation.
// The activation-count change frame itself never latches: isFrameNew() on that
// frame can still describe the outgoing media's last update.
//
// RAW generation advances on a media activation (activationCount change), on a
// scene activation (scene generation change), and when RAW becomes ineligible
// (Failed/Unavailable/Degraded/no mediaId) or eligible again, so history is
// cleared and any in-flight readback from the old generation is rejected
// (SceneColorSampler::beginGeneration does both). FX is never touched.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace rawvideo {

// GL internal-format constants (values from the GL headers) so this stays OF/GL-free.
constexpr int kGlRgb = 0x1907, kGlRgba = 0x1908, kGlRgb8 = 0x8051, kGlRgba8 = 0x8058;

enum class SourceKind { SceneFbo, DecodedVideo };

// FX (SceneFbo) validation is unchanged: 8-bit RGBA only. A decoded-video source may
// also be opaque 8-bit RGB (the GStreamer/ofVideoPlayer-owned texture). Never both
// of them when framebuffer sRGB is on.
inline bool sourceFormatSupported(SourceKind kind, int internalFormat, bool srgbFramebuffer) {
	if (srgbFramebuffer) return false;
	if (internalFormat == kGlRgba || internalFormat == kGlRgba8) return true;
	return kind == SourceKind::DecodedVideo && (internalFormat == kGlRgb || internalFormat == kGlRgb8);
}

// EXPERIENCE_RUNTIME_VIDEO_COLOR: disabled unless it is exactly "1".
inline bool videoColorEnabledFromEnv(const char* value) { return value != nullptr && std::strcmp(value, "1") == 0; }

// No new decoded frame for this long while Ready: the live head is not being refreshed, so RAW records gaps
// (and goes Stale) rather than re-measuring the same frame as if it were live.
constexpr double kStallSeconds = 1.5; // 3 sampling periods

struct Observation {
	bool healthReady = false;       // VideoPlaybackStatus::health == Ready (Degraded/Loading/Failed/Unavailable: false)
	bool hasMediaId = false;
	bool textureAllocated = false;  // currentTexture() != nullptr && isAllocated()
	int texW = 0, texH = 0;
	int srcW = 0, srcH = 0;         // VideoPlaybackService::sourceSize()
	bool frameNew = false;          // VideoPlaybackService::isFrameNew()
	uint64_t activationCount = 0;   // VideoPlaybackService::activationCount()
	uint64_t sceneGeneration = 0;   // the runtime's scene generation
	double nowSeconds = 0.0;        // monotonic clock, for decoder-stall detection
};

enum class Verdict {
	Unavailable,        // not Ready / no media id: RAW has nothing to measure
	AwaitingFirstFrame, // Ready, but no new decoded frame yet in this activation (old/placeholder texture)
	Gap,                // a frame was seen, but the texture is missing/mismatched right now, or the decoder stopped producing
	                    // new frames for kStallSeconds: record a gap (then Stale), never a guess
	Eligible            // measure it
};

struct Decision {
	Verdict verdict = Verdict::Unavailable;
	bool newGeneration = false; // the caller must call beginGeneration(rawGeneration, ...)
	uint64_t rawGeneration = 0;
};

class Gate {
public:
	Decision observe(const Observation& o) {
		Decision d;
		const bool ready = o.healthReady && o.hasMediaId;
		bool bump = false;

		const bool activationChanged = !seen_ || o.activationCount != lastActivation_;
		const bool sceneChanged = !seen_ || o.sceneGeneration != lastScene_;
		if (activationChanged) { latched_ = false; bump = true; }
		if (sceneChanged) bump = true;
		if (ready != lastReady_ && seen_) bump = true; // eligibility lost, or regained without a new activation
		seen_ = true;
		lastActivation_ = o.activationCount;
		lastScene_ = o.sceneGeneration;
		lastReady_ = ready;

		if (bump) { ++rawGeneration_; d.newGeneration = true; }
		d.rawGeneration = rawGeneration_;

		if (!ready) { latched_ = false; d.verdict = Verdict::Unavailable; return d; }
		// The frame an activation lands on never counts: isFrameNew() may still describe the outgoing media.
		if (!activationChanged && o.frameNew && o.textureAllocated) { latched_ = true; lastNewFrameAt_ = o.nowSeconds; }
		if (!latched_) { d.verdict = Verdict::AwaitingFirstFrame; return d; }
		const bool texOk = o.textureAllocated && o.texW > 0 && o.texH > 0 && o.texW == o.srcW && o.texH == o.srcH;
		const bool stalled = o.nowSeconds - lastNewFrameAt_ > kStallSeconds;
		d.verdict = (texOk && !stalled) ? Verdict::Eligible : Verdict::Gap;
		return d;
	}

	uint64_t rawGeneration() const { return rawGeneration_; }
	bool frameLatched() const { return latched_; }

private:
	bool seen_ = false, lastReady_ = false, latched_ = false;
	uint64_t lastActivation_ = 0, lastScene_ = 0, rawGeneration_ = 0;
	double lastNewFrameAt_ = 0.0;
};

} // namespace rawvideo
