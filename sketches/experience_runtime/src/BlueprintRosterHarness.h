#pragma once

#include "ExperienceRuntime.h"

#include <cstdint>
#include <string>
#include <vector>

// BlueprintRosterHarness — BLUE-001 verification. Runs the real
// ExperienceRuntime with the full production roster [Blob, Temporal,
// Blueprint], starting in Blueprint, and walks
//
//     Blueprint -> Blob -> Temporal -> Blueprint (reactivation)
//
// through real RuntimeCommand::NextScene (']' via InputRouter). On every
// Idle live frame it checks: status sceneId == manager activeSceneId;
// HudFrameData.effects present iff Temporal owns the frame; capabilities
// belong to the active scene (Regenerate only on Blueprint); any semantic
// state ID belongs to the active scene; canonical video status present.
// Also: fresh cycle on Blueprint (re)activation, Regenerate ('2') reseeds,
// NextMedia ('n') goes through the canonical service, max frame time per leg.
// Captures the full window per leg under bin/data/captures/blueprint-roster/.
//
// Opt-in: EXPERIENCE_RUNTIME_BLUEPRINT_HARNESS=1. Exits 0 only if every check passed.
class BlueprintRosterHarness {
public:
	static bool isRequested();
	explicit BlueprintRosterHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Leg {
		std::string expectedSceneId;
		std::string label;
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	void checkIdleFrameInvariants();
	void onFirstLiveFrameOfLeg();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Leg> legs_;
	size_t leg_ = 0;
	int legFrame_ = -1;       // live Idle frames seen in the current leg; -1 = waiting for the switch to land
	int framesSinceSwitch_ = 0;
	float legSeconds_ = 0.0f;
	float switchStartTime_ = 0.0f;
	int seedAtLastBlueprintExit_ = 0;
	float maxFrameSecondsThisLeg_ = 0.0f;
	int invariantFailures_ = 0;
	int idleFramesChecked_ = 0;
	int cycleSeedBeforeRegenerate_ = 0;
	std::string mediaBeforeNext_;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
