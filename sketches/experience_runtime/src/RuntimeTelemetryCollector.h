#pragma once

#include "SceneContract.h"
#include "SystemVitals.h"

// RuntimeTelemetryCollector — the concrete producer of RuntimeTelemetry
// (Scene-HUD-Contract-v1.md §13). Only fps/frameTimeMs are populated in
// this increment; cpuTemperatureC/residentMemoryBytes/throttled stay
// std::nullopt — per the contract, "they're the first real Pi-hardware
// telemetry this contract calls for, and remain unpopulated (nullopt)
// until that instrumentation exists." No Pi-specific shell commands or
// filesystem reads are implemented here.
//
// System Telemetry v1: the collector additionally owns a PRIVATE SystemVitals
// (FPS / max frame time / host CPU % / GPU temp / process RSS histories).
// It is deliberately not part of RuntimeTelemetry or HudFrameData — see
// SystemVitals.h. Sampling happens in update() only.
class RuntimeTelemetryCollector {
public:
	RuntimeTelemetryCollector();
	void update(float dt);
	const RuntimeTelemetry& snapshot() const { return current_; }

	const sysvitals::SystemVitals& systemVitals() const { return vitals_; }
	// Immutable snapshot republished (cheaply reused when unchanged) from
	// update(); null until the first update(). This is what ExperienceRuntime
	// copies into HudFrameData::systemVitals.
	const std::shared_ptr<const sysvitals::SystemVitalsSnapshot>& systemVitalsSnapshot() const { return vitals_.published(); }
	// Monotonic seconds on the same clock update() samples with.
	static double monotonicSeconds();

private:
	RuntimeTelemetry current_;
	sysvitals::SystemVitals vitals_;
};
