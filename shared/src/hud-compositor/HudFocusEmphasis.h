#pragma once

// HudFocusEmphasis — OF-free focus hierarchy cues shared by every interactive HUD region.
//
//  * Nested focus: while an INTERACTIVE region (scene parameter group, Orbit observation, ...)
//    owns focus, the universal navigation buttons render at kNestedFocusNavEmphasis (50 %) of their
//    normal opacity/intensity. A hierarchy cue only: nothing is disabled, and a control that is
//    already disabled/dimmed stays proportionally dimmer (the factor MULTIPLIES, never replaces).
//  * Focus pulse: a slow, low-amplitude, time-based (frame-rate independent) modulation of the
//    focused region's existing highlight alpha. No geometry/layout/text change, no allocation.
//
//   pulse(t) = 1 - depth * (0.5 + 0.5 * sin(2*pi*hz*t))     in [1 - depth, 1]

#include <cmath>

namespace hudpresent {

constexpr float kNestedFocusNavEmphasis = 0.5f;
constexpr float kFocusPulseHz = 0.7f;     // ~1.4 s period: reads as "focus", not "warning"
constexpr float kFocusPulseDepth = 0.30f; // highlight alpha swings between 70 % and 100 %

inline float navEmphasis(bool nestedFocus) { return nestedFocus ? kNestedFocusNavEmphasis : 1.0f; }

inline float focusPulse(float seconds) {
	constexpr float kTwoPi = 6.28318530718f;
	return 1.0f - kFocusPulseDepth * (0.5f + 0.5f * std::sin(kTwoPi * kFocusPulseHz * seconds));
}

// Scales an 8-bit alpha by an emphasis factor (round to nearest; never above 255).
inline unsigned char scaleAlpha(unsigned char a, float k) {
	const float v = float(a) * k + 0.5f;
	return static_cast<unsigned char>(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v));
}

} // namespace hudpresent
