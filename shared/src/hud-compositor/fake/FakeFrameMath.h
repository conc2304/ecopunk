#pragma once

// ============================================================================
// FakeFrameMath.h — deterministic pseudo-random helpers shared by every
// fake scenario.
//
// Per this task's §11: "Fake data must depend only on: scenario ID + seed
// + fixed elapsed time. Do not use wall-clock time or global random
// state." Every function here is a pure function of its numeric inputs —
// no std::rand(), no <chrono>, no mutable static state — so
// scenario.sample(t) called twice with the same seed and t always
// produces bit-identical output (this task's "deterministic fake output"
// test requirement).
// ============================================================================

#include <cmath>
#include <cstdint>

namespace hudpresent {
namespace fakemath {

// splitmix64 — a small, well-known, dependency-free integer mixer. Used
// only to turn (seed, channel, quantized-time) into a well-distributed
// 64-bit value; not used for anything cryptographic.
inline uint64_t splitmix64(uint64_t x) {
	x += 0x9E3779B97F4A7C15ULL;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
	return x ^ (x >> 31);
}

// A stable pseudo-random value in [0,1) for one (seed, channel, integer
// time-step) combination — no interpolation, no memory.
inline float hashUnit(uint64_t seed, uint32_t channel, int64_t step) {
	uint64_t h = splitmix64(seed ^ (static_cast<uint64_t>(channel) << 32) ^ static_cast<uint64_t>(step));
	// Top 24 bits -> [0,1); plenty of precision for display purposes.
	return static_cast<float>(h >> 40) / static_cast<float>(1ULL << 24);
}

// Smoothly-interpolated deterministic "noise" in [0,1) — linear blend
// between two adjacent per-second hash values, so a Nominal-phase signal
// reads as a continuous, cinematic ramp rather than a per-second jump,
// while remaining a pure function of (seed, channel, t).
inline float smoothUnit(uint64_t seed, uint32_t channel, float t, float rateHz = 0.5f) {
	float scaled = t * rateHz;
	int64_t step = static_cast<int64_t>(std::floor(scaled));
	float frac = scaled - static_cast<float>(step);
	float a = hashUnit(seed, channel, step);
	float b = hashUnit(seed, channel, step + 1);
	return a + (b - a) * frac;
}

inline float clamp01(float v) {
	if (v < 0.0f) return 0.0f;
	if (v > 1.0f) return 1.0f;
	return v;
}

} // namespace fakemath
} // namespace hudpresent
