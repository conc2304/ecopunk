#pragma once

// HudVitalsDial.h — presentation rules for the System Performance arc dials
// (flexible.primary, five columns). OF-free and header-only so the numbers are
// unit-testable (hud_vitals_tests).
//
// Two separate concepts, never conflated:
//   fill   = the normalised measurement (value mapped into [rangeMin, rangeMax]);
//   health = GOOD / OK / BAD, decided per metric by its own direction and limits.
//
// The ranges and limits below are PRESENTATION choices for the dial — telemetry
// semantics (metric ids, units, decimals, validity states) stay in
// HudVitalsTrace.cpp / SystemVitals. Pi 3B/4B oriented; the owner may tune them.

#include "SystemVitals.h"

#include <algorithm>

namespace hudpresent {

enum class VitalsHealth { Good, Ok, Bad };

struct VitalsDialSpec {
	float rangeMin, rangeMax;   // fill = 0 at rangeMin, 1 at rangeMax
	bool higherIsBetter;
	float goodLimit;            // higherIsBetter: value >= goodLimit is GOOD; else value <= goodLimit is GOOD
	float okLimit;              // higherIsBetter: value >= okLimit is OK;   else value <= okLimit is OK; beyond = BAD
};

inline const VitalsDialSpec& vitalsDialSpec(sysvitals::Metric m) {
	using sysvitals::Metric;
	static const VitalsDialSpec kFps    {0.0f, 60.0f, true, 28.0f, 20.0f};   // 30 FPS is the runtime target
	static const VitalsDialSpec kFrame  {0.0f, 66.7f, false, 33.3f, 50.0f};  // 33.3 ms = the existing 30 FPS reference
	static const VitalsDialSpec kCpu    {0.0f, 100.0f, false, 70.0f, 90.0f};
	static const VitalsDialSpec kGpu    {30.0f, 90.0f, false, 65.0f, 80.0f}; // degrees C
	static const VitalsDialSpec kRss    {0.0f, 1024.0f, false, 600.0f, 850.0f}; // MiB
	switch (m) {
		case Metric::Fps: return kFps;
		case Metric::FrameTimeMs: return kFrame;
		case Metric::CpuPercent: return kCpu;
		case Metric::GpuTempC: return kGpu;
		case Metric::RssMiB: return kRss;
		default: return kFps;
	}
}

inline float vitalsDialFraction(const VitalsDialSpec& s, float value) {
	const float span = s.rangeMax - s.rangeMin;
	return span <= 0.0f ? 0.0f : std::clamp((value - s.rangeMin) / span, 0.0f, 1.0f);
}

inline VitalsHealth vitalsDialHealth(const VitalsDialSpec& s, float value) {
	if (s.higherIsBetter) return value >= s.goodLimit ? VitalsHealth::Good : value >= s.okLimit ? VitalsHealth::Ok : VitalsHealth::Bad;
	return value <= s.goodLimit ? VitalsHealth::Good : value <= s.okLimit ? VitalsHealth::Ok : VitalsHealth::Bad;
}

} // namespace hudpresent
