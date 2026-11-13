#pragma once

#include <array>
#include <cstdio>
#include <string>

// RawVideoAdjust — OF-free dial model for RawVideoScene (covered by test/raw_video_adjust_tests.cpp).
//
// Three independent dials (BRIGHTNESS / CONTRAST / SATURATION), each an 11-step detent scale whose
// CENTRE detent (index 5) is the identity (raw video, untouched). The mapping mirrors the semantics of
// shared/assets/video-effects/utility/video_adjust.glsl:
//   brightness  additive offset            -0.25 .. +0.25   (0.05 / step)   identity 0.0
//   contrast    multiplier about mid-grey    0.50 ..  1.50  (0.10 / step)   identity 1.0
//   saturation  mix(luma, rgb, s)            0.00 ..  2.00  (0.20 / step)   identity 1.0
// Dials CLAMP at the ends (a dial that wraps from max to min is a surprise); step() reports whether the
// value actually moved.
namespace rawvideo {

enum Dial { kBrightness = 0, kContrast = 1, kSaturation = 2 };
constexpr int kDialCount = 3;
constexpr int kSteps = 11;
constexpr int kCentre = 5;

inline const char* dialLabel(int dial) {
	switch (dial) {
		case kBrightness: return "BRIGHTNESS";
		case kContrast: return "CONTRAST";
		case kSaturation: return "SATURATION";
		default: return "";
	}
}
inline const char* dialId(int dial) {
	switch (dial) {
		case kBrightness: return "brightness";
		case kContrast: return "contrast";
		case kSaturation: return "saturation";
		default: return "";
	}
}

// Shader-space value of a dial at detent `index` (clamped into range).
inline float dialValue(int dial, int index) {
	if (index < 0) index = 0;
	if (index >= kSteps) index = kSteps - 1;
	const float d = float(index - kCentre);
	switch (dial) {
		case kBrightness: return d * 0.05f;
		case kContrast: return 1.0f + d * 0.10f;
		case kSaturation: return 1.0f + d * 0.20f;
		default: return 0.0f;
	}
}

// Human value for the HUD row: brightness is a signed offset in percent, contrast/saturation are a percentage
// of the original (100% = identity).
inline std::string dialText(int dial, int index) {
	const float v = dialValue(dial, index);
	char buf[24];
	if (dial == kBrightness) {
		const int pct = static_cast<int>(v * 100.0f + (v >= 0.0f ? 0.5f : -0.5f));
		if (pct == 0) return "0%";
		std::snprintf(buf, sizeof buf, "%+d%%", pct);
	} else {
		std::snprintf(buf, sizeof buf, "%d%%", static_cast<int>(v * 100.0f + 0.5f));
	}
	return buf;
}

inline std::string dialPosition(int index) {
	if (index < 0) index = 0;
	if (index >= kSteps) index = kSteps - 1;
	return std::to_string(index + 1) + "/" + std::to_string(kSteps);
}

struct Dials {
	std::array<int, kDialCount> index { kCentre, kCentre, kCentre };

	// Steps `dial` by +/-1, clamped. Returns true only if the detent changed.
	bool step(int dial, int delta) {
		if (dial < 0 || dial >= kDialCount || (delta != 1 && delta != -1)) return false;
		const int next = index[dial] + delta;
		if (next < 0 || next >= kSteps) return false;
		index[dial] = next;
		return true;
	}
	void reset() { index = { kCentre, kCentre, kCentre }; }
	bool isIdentity() const { return index[0] == kCentre && index[1] == kCentre && index[2] == kCentre; }
	float value(int dial) const { return dialValue(dial, index[dial]); }
};

} // namespace rawvideo
