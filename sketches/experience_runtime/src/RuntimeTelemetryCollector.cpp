#include "RuntimeTelemetryCollector.h"

#include "ofAppRunner.h"

void RuntimeTelemetryCollector::update(float dt) {
	current_.fps = ofGetFrameRate();
	current_.frameTimeMs = dt * 1000.0f;
	// cpuTemperatureC / residentMemoryBytes / throttled: left at their
	// default std::nullopt (no in-repo source exists for any of them —
	// see the discovery report's Performance-Instrumentation Findings).
}
