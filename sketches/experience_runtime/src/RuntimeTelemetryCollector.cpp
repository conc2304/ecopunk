#include "RuntimeTelemetryCollector.h"

#include "ofAppRunner.h"

#include <chrono>
#include <cstdlib>
#include <cstring>

// Real /proc and /sys sources by default. EXPERIENCE_RUNTIME_VITALS_SYNTHETIC
// (=1, or =dropout for periodic GPU-sensor loss) swaps in a deterministic fake
// for desktop HUD verification only — see syntheticSystemSource().
RuntimeTelemetryCollector::RuntimeTelemetryCollector()
	: vitals_([]() -> sysvitals::ISystemSource* {
		  const char* mode = std::getenv("EXPERIENCE_RUNTIME_VITALS_SYNTHETIC");
		  if (!mode || !*mode) return nullptr;
		  return sysvitals::syntheticSystemSource(std::strcmp(mode, "dropout") == 0);
	  }()) {}

double RuntimeTelemetryCollector::monotonicSeconds() {
	using namespace std::chrono;
	return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void RuntimeTelemetryCollector::update(float dt) {
	current_.fps = ofGetFrameRate();
	current_.frameTimeMs = dt * 1000.0f;
	// cpuTemperatureC / residentMemoryBytes / throttled: left at their
	// default std::nullopt (no in-repo source exists for any of them —
	// see the discovery report's Performance-Instrumentation Findings).
	// System Telemetry v1 does not populate these scalar fields; it publishes
	// the separate HudFrameData::systemVitals snapshot instead.
	const double now = monotonicSeconds();
	vitals_.update(now, current_.fps, ofGetLastFrameTime() * 1000.0f);
	vitals_.publish(now);
}
