#pragma once

#include "ExperienceRuntime.h"

#include <string>
#include <vector>

// CrtSelectorHarness — real-runtime check of the CRT Phosphor three-selector rows
// (EXPERIENCE_RUNTIME_CRT_SELECTOR_HARNESS; also set EXPERIENCE_RUNTIME_CRT_SCENE=1 so the
// run starts in the CRT scene, and optionally EXPERIENCE_RUNTIME_CRT_STRUCTURE / _PALETTE /
// _MODE for the startup selection, EXPERIENCE_RUNTIME_CRT_SELECTOR_TAG for the capture prefix).
// Test-only: it injects the same key presses an operator would type through
// ExperienceRuntime::keyPressed() ('m' = NEXT MODE, ']' / '[' = scene switch, '1' = Reset) and
// asserts from the real HudFrameData: composition wrap 1..4, independence of structure/palette,
// planned modes unreachable, persistence across a scene switch, and Reset. It also captures
// the composited 1280x720 window and measures the lit text extent inside universal.effect_summary.
class CrtSelectorHarness {
public:
	static bool isRequested();
	explicit CrtSelectorHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	bool idleOn(const std::string& sceneId) const;
	int modeIndex() const;
	int modeCount() const;
	std::string modeId() const;
	std::string metric(const char* id) const; // Identifier valueId of a scene metric, "" if absent

	ExperienceRuntime& runtime_;
	int phase_ = 0;
	int frame_ = 0;       // frames since the current wait/step began
	int k_ = 0;           // NEXT MODE presses so far
	bool sent_ = false;
	int indexBeforeSwitch_ = -1;
	std::string structureAtStart_, paletteAtStart_;
	double frameMsSum_ = 0.0;
	int frameMsN_ = 0;
	int passes_ = 0, failures_ = 0;
};
