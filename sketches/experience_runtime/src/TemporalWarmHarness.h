#pragma once

#include "ExperienceRuntime.h"

#include <string>
#include <vector>

// TemporalWarmHarness — Temporal first-activation stall verification in the
// real ExperienceRuntime (default roster, startup Blob, real ']' / '[' input).
//
//   Blob -> Temporal -> Blob -> Temporal -> Blueprint -> Temporal
//
// Per switch: wall time from request to the first live Idle frame and the
// worst single frame in between; per settled leg: process resident memory.
// Temporal legs also check: effects snapshot present, adapter on the
// canonical media, history refilled from a fresh start, activation-relative
// time. Writes bin/data/captures/temporal-warm/results.txt.
//
// Opt-in: EXPERIENCE_RUNTIME_TEMPORAL_WARM_HARNESS=1. Exits 0 only if every check passed.
class TemporalWarmHarness {
public:
	static bool isRequested();
	explicit TemporalWarmHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Leg {
		std::string sceneId;
		std::string label;
		int key; // 0 = startup leg
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	bool idleOn(const std::string& sceneId) const;
	void onLegLive();
	void onLegSettled();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Leg> legs_;
	size_t leg_ = 0;
	bool waiting_ = true;
	float switchStart_ = 0.0f;
	float maxFrame_ = 0.0f;
	float legSeconds_ = 0.0f;
	float clock_ = 0.0f;
	std::vector<double> residentMb_;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
