#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// BlueprintHudHarness — production HUD check with Blueprint Emergence as the
// third scene. Default roster [Blob, Temporal, Blueprint], startup Blob, real
// input path only (arrows / Enter / ']' through ExperienceRuntime::keyPressed).
//
// Script: focus engage + disabled-control refusal on Blob; focus stability
// across Blob -> Temporal -> Blueprint; RESEED visible, focusable and
// activated by Enter (SceneCommand::Regenerate, new composition cycle);
// Blueprint phase/timing/activity fresh (no stale Blob/Temporal values);
// focus leaves RESEED when Blob hides it. Captures under
// bin/data/captures/blueprint-hud/.
//
// Opt-in: EXPERIENCE_RUNTIME_BLUEPRINT_HUD_HARNESS=1. Exits 0 only if every check passed.
class BlueprintHudHarness {
public:
	static bool isRequested();
	explicit BlueprintHudHarness(ExperienceRuntime& runtime);
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
	bool bracketsAt(const std::string& regionId);
	bool idleOn(const std::string& sceneId) const;
	std::string focused() const;
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	int seedBeforeReseed_ = 0;
	uint64_t activationSerialBefore_ = 0;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
