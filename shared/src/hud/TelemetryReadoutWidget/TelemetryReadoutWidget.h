#pragma once
#include "../shared/HudWidget.h"
#include <string>

namespace hud {

enum class TelemetryMode {
	FrameCounter, // running count derived from elapsed time × fps, never resets
	CoordinateWalk // lat/long-style string that random-walks a small amount per tick
};

struct TelemetryReadoutOptions {
	TelemetryMode mode  = TelemetryMode::FrameCounter;
	float fps           = 24.0f;
	float walkStepMs    = 120.0f; // CoordinateWalk: interval between random-walk steps
	float walkStepDeg   = 0.01f;  // CoordinateWalk: max drift per step, in "degrees"
	double originLat    = 34.0522;
	double originLon    = -118.2437;
};

// Corner-anchored text readout, always present, continuously updating
// rather than snapping to a fresh value on an interval — see design doc
// Section 3.4. Two instances (different modes) cover both readouts;
// nothing existing in this library renders this kind of live telemetry text.
class TelemetryReadoutWidget : public HudWidget {
public:
	void setOptions(const TelemetryReadoutOptions& next) { options = next; }
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 90.0f, 20.0f }; }

private:
	double elapsedSeconds  = 0.0; // FrameCounter — accumulates for the widget's whole lifetime
	float  walkAccumMs     = 0.0f;
	double latDrift        = 0.0;
	double lonDrift        = 0.0;
	TelemetryReadoutOptions options;
};

} // namespace hud
