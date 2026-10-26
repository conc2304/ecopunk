#pragma once

#include "ExperienceRuntime.h"

#include <string>
#include <vector>

// SkinCycleHarness — day/night skin schedule + 'v' / 'V' switching AND the
// shared navigation-button faces (idle / focused / active) in the real
// runtime (EXPERIENCE_RUNTIME_SKIN_CYCLE_HARNESS; also set
// EXPERIENCE_RUNTIME_SKIN_SCHEDULE=1 so the run uses the schedule).
class SkinCycleHarness {
public:
	static bool isRequested();
	explicit SkinCycleHarness(ExperienceRuntime& runtime) : runtime_(runtime) {}
	void step(float dt);

private:
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void capture(const std::string& name);
	void finish();
	ExperienceRuntime& runtime_;
	int frame_ = 0;       // frames in the current stage
	int stage_ = 0;
	int skinIndex_ = 0;   // 0..2 manual skins captured
	std::string first_;
	std::string skin_;
	bool done_ = false;
	int passes_ = 0, failures_ = 0;
	std::vector<std::string> lines_;
};
