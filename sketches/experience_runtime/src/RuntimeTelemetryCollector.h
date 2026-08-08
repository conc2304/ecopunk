#pragma once

#include "SceneContract.h"

// RuntimeTelemetryCollector — the concrete producer of RuntimeTelemetry
// (Scene-HUD-Contract-v1.md §13). Only fps/frameTimeMs are populated in
// this increment; cpuTemperatureC/residentMemoryBytes/throttled stay
// std::nullopt — per the contract, "they're the first real Pi-hardware
// telemetry this contract calls for, and remain unpopulated (nullopt)
// until that instrumentation exists." No Pi-specific shell commands or
// filesystem reads are implemented here.
class RuntimeTelemetryCollector {
public:
	void update(float dt);
	const RuntimeTelemetry& snapshot() const { return current_; }

private:
	RuntimeTelemetry current_;
};
