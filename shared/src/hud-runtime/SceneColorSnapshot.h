#pragma once

// ============================================================================
// SceneColorSnapshot.h - Orbit Console v0.1 runtime-to-HUD scene-colour snapshot.
//
// Architecture Tier C (ratified): an additive, optional, read-only, bounded,
// immutable snapshot carried by HudFrameData::sceneColor as
// std::shared_ptr<const SceneColorSnapshot>. A null pointer means "no snapshot
// yet" (never a snapshot of zeros). Built ONLY by ExperienceRuntime's
// SceneColorSampler (the HUD never samples or reads back); rebuilt only when the
// series revision or the evaluated state changes, so the per-frame HudFrameData
// copy is a refcount bump. OF-free: standard library only.
//
// Measurement definition (ExperienceRuntime-owned, documented here for consumers):
//   source      the runtime-owned SceneFBO, before HUD composition / shell / scaling
//   background  texels composited over opaque black (rgb * alpha), the same
//               convention the viewer sees
//   encoding    the SceneFBO is 8-bit RGBA with no sRGB internal format and no
//               framebuffer-sRGB, i.e. display-encoded sRGB; each probe texel is
//               converted with the sRGB EOTF BEFORE averaging
//   r,g,b       spatial average in LINEAR light, 0..1, Rec.709/sRGB primaries
//   luma        RELATIVE luminance Y = 0.2126 R + 0.7152 G + 0.0722 B on the
//               averaged linear RGB (not nits)
//   history     oldest -> newest, strictly increasing monotonic seconds
//   method      GPU reduction in half-float: every source texel is composited over black,
//               converted with the sRGB EOTF and only then box-averaged (160x90, then 16x9),
//               so the mean is exact linear light. Measured against a full-resolution CPU
//               reference over all five production scenes (399 samples): mean |dY| 0.00009, max 0.00043. Where half-float
//               render targets are unavailable (GLES2) a fallback averages an 8-bit encoded
//               160x90 probe after CPU linearisation: documented lower accuracy on
//               high-contrast sub-cell detail (mean |dY| 0.008, max 0.044 on dithered content)
//
// A Sample with valid == false is a GAP (a transition static frame, a readback
// failure or an unsupported format); its colour/luma are meaningless and never a
// synthetic zero. A valid black frame is {valid = true, r = g = b = luma = 0}.
// One snapshot never mixes generations: a scene change starts a new generation
// with an empty history.
// ============================================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace scenecolor {

// Unavailable: no valid sample has been observed in this generation.
// Valid:       the newest valid sample is younger than the stale limit.
// Stale:       a valid sample exists but newer ones stopped arriving.
enum class State : uint8_t { Unavailable, Valid, Stale };

// The space r,g,b are expressed in (a field so a future ruling can change it explicitly).
enum class ColorSpace : uint8_t { LinearSrgb };

constexpr size_t kCapacity = 120;          // 60 s at the 2 Hz target
constexpr double kNominalPeriodSeconds = 0.5;
constexpr int kStalePeriods = 3;           // stale after 3 missed periods

struct Sample {
	double t = 0.0;          // monotonic seconds at which the SOURCE frame was rendered
	float r = 0.0f, g = 0.0f, b = 0.0f; // linear light, 0..1
	float luma = 0.0f;       // relative luminance Y, 0..1
	bool valid = false;      // false = gap
};

struct SceneColorSnapshot {
	uint64_t revision = 0;            // monotonic; bumps on a new sample/gap, a state change or a new generation
	uint64_t generation = 0;          // scene generation of EVERY sample below
	std::string sceneId;              // scene identity of this generation ("" before the first activation)
	double capturedAtSeconds = 0.0;   // monotonic clock the state was evaluated at
	State state = State::Unavailable;
	ColorSpace colorSpace = ColorSpace::LinearSrgb;
	double nominalPeriodSeconds = kNominalPeriodSeconds;
	uint32_t count = 0;               // entries used in `history`
	std::array<Sample, kCapacity> history{}; // chronological, oldest first, [0, count)

	const Sample& at(size_t i) const { return history[i]; }
};

} // namespace scenecolor
