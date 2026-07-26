#pragma once

#include <cmath>
#include <string>

// Self-contained, stateless easing shapes for TFPresetTimeline's state
// transitions -- deliberately free of ofJson/ofParameter so this (and
// TFPresetTimeline itself) can be unit-tested without linking any part of
// openFrameworks. All four curves satisfy f(0)=0 and f(1)=1 exactly, so a
// transition's very last frame always lands precisely on its target value
// with no separate "snap to exact target" step needed.
namespace tftimeline {

	enum class EasingType { Linear, Smoothstep, Smootherstep, EaseInOutSine };

	inline float easeLinear(float t) { return t; }

	inline float easeSmoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

	inline float easeSmootherstep(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

	inline float easeInOutSine(float t) {
		// Not named PI -- openFrameworks' ofMathConstants.h #defines PI as a
		// function-like macro (glm::pi<float>()), which would otherwise
		// collide with a same-named local constant here.
		constexpr float kPi = 3.14159265358979323846f;
		return -(std::cos(kPi * t) - 1.0f) * 0.5f;
	}

	// Unrecognized names fall back to Linear and set outUnknown so the
	// caller can warn once per state (at load time) rather than every
	// frame.
	inline EasingType resolveEasingType(const std::string& name, bool& outUnknown) {
		outUnknown = false;
		if (name.empty() || name == "linear") return EasingType::Linear;
		if (name == "smoothstep") return EasingType::Smoothstep;
		if (name == "smootherstep") return EasingType::Smootherstep;
		if (name == "easeInOutSine") return EasingType::EaseInOutSine;
		outUnknown = true;
		return EasingType::Linear;
	}

	inline float applyEasing(EasingType type, float t) {
		switch (type) {
			case EasingType::Linear: return easeLinear(t);
			case EasingType::Smoothstep: return easeSmoothstep(t);
			case EasingType::Smootherstep: return easeSmootherstep(t);
			case EasingType::EaseInOutSine: return easeInOutSine(t);
		}
		return t;
	}

}
