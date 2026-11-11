// Orbit Console Scene Sampling v0.1 -- runtime-private, OpenFrameworks-free
// core: a fixed-capacity series of average-RGB/luminance samples taken from
// the rendered SceneFBO (before HUD composition), with scene-generation
// identity, monotonic timestamps and validity/staleness.
//
// PRIVATE to ExperienceRuntime. It is published to the HUD ONLY as the immutable
// scenecolor::SceneColorSnapshot (shared/src/hud-runtime/SceneColorSnapshot.h,
// HudFrameData::sceneColor) via SnapshotPublisher below. Ratified semantics:
// 160x90 probe (8x8 px cells for 1280x720), composite over opaque black, sRGB EOTF per texel BEFORE averaging
// (linear-light r,g,b), Rec.709 relative luminance on the averaged linear RGB,
// 2 Hz, 120 samples, stale after 3 periods.
//
// No allocation after construction: storage is a std::array ring.
#pragma once

#include "SceneColorSnapshot.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace scenecolorsampling {

constexpr std::size_t kCapacity = scenecolor::kCapacity;                  // 60 s @ 2 Hz
constexpr double kPeriodSeconds = scenecolor::kNominalPeriodSeconds;      // 2 Hz
constexpr int kStalePeriods = scenecolor::kStalePeriods;                  // stale after 3 missed periods
constexpr int kDefaultProbeMax = 160;        // halving chain stops once both sides <= this (160x90 for 1280x720: 8x8 px cells, 57 KiB readback)

struct Sample {
	double t = 0.0;        // monotonic seconds at which the frame was rendered
	float r = 0.0f, g = 0.0f, b = 0.0f; // 0..1, LINEAR light (sRGB EOTF applied before averaging)
	float luma = 0.0f;     // Rec.709 relative luminance of the averaged linear RGB, 0..1
	bool valid = false;    // false = gap (never a fake black sample)
	uint64_t generation = 0; // scene generation the pixels came from
	// OC-003: dominant / vibrant of the same frame (linear light), independent validity; see scenecolor::Sample.
	bool domValid = false, vibValid = false;
	float domR = 0.0f, domG = 0.0f, domB = 0.0f;
	float vibR = 0.0f, vibG = 0.0f, vibB = 0.0f;
};

using State = scenecolor::State;

inline float luma709(float r, float g, float b) { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }

// sRGB electro-optical transfer function (encoded 0..1 -> linear 0..1).
inline float srgbToLinear(float c) {
	return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
// 256-entry table so the 1024-texel average costs no pow() per texel.
inline const std::array<float, 256>& srgbLut() {
	static const std::array<float, 256> lut = [] {
		std::array<float, 256> t{};
		for (int i = 0; i < 256; ++i) t[static_cast<std::size_t>(i)] = srgbToLinear(static_cast<float>(i) / 255.0f);
		return t;
	}();
	return lut;
}

// Linear-light average of `texels` RGBA8 texels of a display-encoded (sRGB) frame. Each texel is
// composited over opaque black in the encoded domain (rgb * alpha, what the viewer sees), converted
// with the sRGB EOTF, then averaged. Returns false for null/empty input.
inline bool averageLinearRgba8OverBlack(const uint8_t* px, std::size_t texels, float& r, float& g, float& b) {
	if (px == nullptr || texels == 0) return false;
	const auto& lut = srgbLut();
	double sr = 0.0, sg = 0.0, sb = 0.0;
	for (std::size_t i = 0; i < texels; ++i) {
		const unsigned a = px[i * 4 + 3];
		// rgb * alpha / 255, rounded to the nearest 8-bit code, then linearised.
		auto over = [a](unsigned c) { return (c * a + 127u) / 255u; };
		sr += lut[over(px[i * 4 + 0])];
		sg += lut[over(px[i * 4 + 1])];
		sb += lut[over(px[i * 4 + 2])];
	}
	const double k = 1.0 / static_cast<double>(texels);
	r = static_cast<float>(sr * k);
	g = static_cast<float>(sg * k);
	b = static_cast<float>(sb * k);
	return true;
}

class Series {
public:
	// Starts a new scene generation: history is cleared (samples from
	// different generations are never mixed) and the cadence restarts.
	void beginGeneration(uint64_t generation, double now) {
		generation_ = generation;
		count_ = 0;
		head_ = 0;
		lastValidAt_ = -1.0;
		nextDueAt_ = now; // first sample is due immediately
		hasGeneration_ = true;
		revision_++;
	}

	bool hasGeneration() const { return hasGeneration_; }
	uint64_t generation() const { return generation_; }
	uint64_t revision() const { return revision_; }
	std::size_t size() const { return count_; }

	// Cadence gate: true once per period. Self-corrects after a long frame
	// (no catch-up burst): the next due time is `now + period`.
	bool due(double now) const { return hasGeneration_ && now >= nextDueAt_; }
	void markIssued(double now) { nextDueAt_ = now + kPeriodSeconds; }

	// Pushes a result for `generation`. A result for any other generation
	// is rejected (returns false) -- e.g. an async readback that completed
	// after a scene switch.
	bool push(const Sample& s) {
		if (!hasGeneration_ || s.generation != generation_) return false;
		ring_[head_] = s;
		head_ = (head_ + 1) % kCapacity;
		if (count_ < kCapacity) count_++;
		if (s.valid) lastValidAt_ = s.t;
		revision_++;
		return true;
	}

	// Oldest = 0 .. newest = size()-1.
	const Sample& at(std::size_t i) const {
		const std::size_t start = (head_ + kCapacity - count_) % kCapacity;
		return ring_[(start + i) % kCapacity];
	}
	const Sample* newestValid() const {
		for (std::size_t i = count_; i > 0; --i) {
			const Sample& s = at(i - 1);
			if (s.valid) return &s;
		}
		return nullptr;
	}

	State state(double now) const {
		if (lastValidAt_ < 0.0) return State::Unavailable;
		return (now - lastValidAt_) > kStalePeriods * kPeriodSeconds ? State::Stale : State::Valid;
	}

private:
	std::array<Sample, kCapacity> ring_{};
	std::size_t head_ = 0;
	std::size_t count_ = 0;
	uint64_t generation_ = 0;
	uint64_t revision_ = 0;
	double lastValidAt_ = -1.0;
	double nextDueAt_ = 0.0;
	bool hasGeneration_ = false;
};

// Builds the immutable snapshot and republishes it ONLY when something observable changed:
// the series revision (new sample/gap/generation), the evaluated state (Valid -> Stale happens
// with no new sample), or the scene identity. Otherwise the same shared_ptr is returned, so the
// per-frame HudFrameData copy is a refcount bump and nothing is allocated.
class SnapshotPublisher {
public:
	std::shared_ptr<const scenecolor::SceneColorSnapshot> publish(const Series& series, double now, const std::string& sceneId, bool predominantProvided = false) {
		if (!series.hasGeneration()) return current_; // nothing begun yet: stay null
		const State st = series.state(now);
		if (current_ && series.revision() == lastSeriesRevision_ && st == lastState_
			&& series.generation() == lastGeneration_ && sceneId == lastSceneId_ && predominantProvided == lastProvided_) {
			return current_;
		}
		const auto t0 = std::chrono::steady_clock::now();
		auto snap = std::make_shared<scenecolor::SceneColorSnapshot>();
		snap->revision = ++revision_;
		snap->generation = series.generation();
		snap->sceneId = sceneId;
		snap->capturedAtSeconds = now;
		snap->state = st;
		snap->predominantProvided = predominantProvided;
		snap->count = static_cast<uint32_t>(series.size());
		for (std::size_t i = 0; i < series.size(); ++i) {
			const Sample& in = series.at(i);
			scenecolor::Sample& out = snap->history[i];
			out.t = in.t;
			out.valid = in.valid;
			if (in.valid) { out.r = in.r; out.g = in.g; out.b = in.b; out.luma = in.luma; }
			if (predominantProvided && in.valid) { // DOM/VIB only ever accompany a valid frame
				out.domValid = in.domValid; out.vibValid = in.vibValid;
				if (in.domValid) { out.domR = in.domR; out.domG = in.domG; out.domB = in.domB; }
				if (in.vibValid) { out.vibR = in.vibR; out.vibG = in.vibG; out.vibB = in.vibB; }
			}
		}
		lastSeriesRevision_ = series.revision();
		lastState_ = st;
		lastGeneration_ = series.generation();
		lastSceneId_ = sceneId;
		lastProvided_ = predominantProvided;
		++rebuilds_;
		current_ = std::move(snap);
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		rebuildMsTotal_ += ms;
		rebuildMsMax_ = std::max(rebuildMsMax_, ms);
		return current_;
	}
	// A "rebuild" is one new immutable snapshot (allocation + copy of the <=120-sample history). It is
	// triggered by a change of series revision (every pushed sample or gap), state (Valid<->Stale),
	// generation or scene id - so ONE rebuild per completed sample is the expected steady-state rate.
	std::uint64_t rebuilds() const { return rebuilds_; }
	double rebuildMsTotal() const { return rebuildMsTotal_; }
	double rebuildMsMax() const { return rebuildMsMax_; }

private:
	std::shared_ptr<const scenecolor::SceneColorSnapshot> current_;
	std::uint64_t revision_ = 0, lastSeriesRevision_ = 0, lastGeneration_ = 0, rebuilds_ = 0;
	double rebuildMsTotal_ = 0.0, rebuildMsMax_ = 0.0;
	State lastState_ = State::Unavailable;
	bool lastProvided_ = false;
	std::string lastSceneId_;
};

} // namespace scenecolorsampling
