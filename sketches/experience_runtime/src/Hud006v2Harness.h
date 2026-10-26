#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// See Hud006v2Harness.cpp.
class Hud006v2Harness {
public:
	static bool isRequested();
	explicit Hud006v2Harness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string label;
		std::function<bool()> ready;
		std::function<void()> act;
	};
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	std::string focused() const;
	bool idle() const;
	std::string resolved(const std::string& sourceId) const;
	bool focusOn(const std::string& region);
	int modeIndex() const;
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	uint64_t serial_ = 0;
	int modeBefore_ = -1;
	std::string mediaBefore_;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
