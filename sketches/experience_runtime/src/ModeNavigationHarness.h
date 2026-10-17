#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// ModeNavigationHarness — Scene & Pattern Navigation real-runtime check
// (EXPERIENCE_RUNTIME_MODE_HARNESS). Full five-scene roster, startup Blob.
// Walks Blob -> Temporal -> Blueprint -> Fragment -> Quadrant -> Blob through
// the real ']' NextScene path, steps NEXT MODE through BOTH device-neutral
// inputs (the 'm' key and the focus ring's mode panel + Enter), and asserts
// scene/mode positions, wraparound, refusal and persistence from the real
// HudFrameData — capturing the composited window at each step. Same scripted
// step pattern as BlueprintHudHarness.
class ModeNavigationHarness {
public:
	static bool isRequested();
	explicit ModeNavigationHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string label;
		std::function<bool()> ready; // wait (one frame at a time) until true
		std::function<void()> act;   // runs once, on the first frame ready() holds
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	bool idleOn(const std::string& sceneId) const;
	int modeIndex() const;  // -1 when the active scene publishes no mode
	int modeCount() const;
	std::string modeId() const;
	std::string focused() const;
	std::string position() const; // "i/n" from SceneManagerStatus, "" if absent
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	int indexBefore_ = -1;
	bool transitioningAtPress_ = false;
	uint64_t serialBefore_ = 0;
	float crosshairX_ = 0.0f;
	float crosshairY_ = 0.0f;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
