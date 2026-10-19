#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <set>
#include <map>
#include <string>
#include <vector>

// ParitySoakHarness — production-parity closure soak for the three-scene
// roster. Default roster [Blob, Temporal, Blueprint], startup Blob, real input
// path (']' scene switch, 'n' media, '2' Regenerate, arrows focus).
//
//   Blob -> Temporal -> Blueprint -> Blob -> Temporal -> Blueprint
//
// Exercises: canonical NextMedia; Temporal's tuned/evolving parameters; all
// nine Temporal patterns via real TFFragmentTransition scene transitions
// (harness-only trigger: TFComposition::forceNextPattern(), the same call the
// standalone 't' key makes); Blueprint phase progress and RESEED; HUD focus.
// Every Idle frame: no stale status/effects/capabilities/semantics; first
// semantic frame of every leg: activation-relative time reset. Captures under
// bin/data/captures/parity-soak/.
//
// Opt-in: EXPERIENCE_RUNTIME_PARITY_HARNESS=1. Exits 0 only if every check passed.
class ParitySoakHarness {
public:
	static bool isRequested();
	explicit ParitySoakHarness(ExperienceRuntime& runtime);
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
	bool sceneFrameNotBlack(float* meanOut = nullptr);
	bool idleOn(const std::string& sceneId) const;
	void checkIdleInvariants();
	void addSwitchLeg(const std::string& sceneId, const std::string& label);
	void addTemporalPatternTour();
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	float maxFrameSeconds_ = 0.0f; // since the last switch request
	std::string mediaBefore_;
	int seedBefore_ = 0;
	// Fresh-time evidence (A): wall clock at the switch request, and each
	// scene's semantic active time / generation when it was last left.
	float switchRequestWall_ = 0.0f;
	std::map<std::string, float> exitActiveSeconds_;
	std::map<std::string, uint64_t> exitGeneration_;
	// Timer-bounded phase observation (B).
	int boundedAttempts_ = 0;
	float boundedFirstProgress_ = -1.0f;
	float boundedLastProgress_ = -1.0f;
	float boundedSampleSeconds_ = 0.0f;
	float unboundedCycleSeconds_ = 0.0f;
	bool boundedProgressMonotonic_ = true;
	int boundedMetricMismatches_ = 0;
	// Regenerate (C).
	float cycleElapsedBeforeReseed_ = 0.0f;
	int seedAfterReseed_ = 0;
	int blueprintExitSeed_ = 0;
	int patternBeforeForce_ = -1;
	std::set<int> visitedPatterns_;
	int invariantFailures_ = 0;
	int idleFramesChecked_ = 0;
	int blueprintBoundedFrames_ = 0;   // Blueprint frames with a timer-bounded phase
	int blueprintProgressMismatches_ = 0; // HUD metric presence/value != CompositionBase::getPhaseProgress()
	float blueprintMaxProgress_ = 0.0f;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
