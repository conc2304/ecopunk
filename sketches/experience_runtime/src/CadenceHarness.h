#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// CadenceHarness — Blueprint cycle-driven Shared Video cadence, in the real
// ExperienceRuntime (default roster, startup Blob, real input path).
//
//   Blob -> Blueprint -> Temporal -> Blueprint -> Blob
//
// Every frame it records each canonical media change with the live scene,
// the selection origin and whether Blueprint's composition cycle restarted
// that same frame. Blueprint must show no timer-driven change inside a cycle
// and exactly one change at a natural cycle boundary; Blob/Temporal must get
// automatic cadence back with a fresh (not overdue) dwell window. Also:
// RESEED (new cycle -> one media change, as standalone 'r'), manual Next /
// Previous (immediate, cycle continues).
//
// Opt-in: EXPERIENCE_RUNTIME_CADENCE_HARNESS=1 (add
// EXPERIENCE_RUNTIME_VIDEO_RESUME_TRACE=1 for the Shared Video trace).
// Captures under bin/data/captures/cadence/. Exits 0 only if every check passed.
class CadenceHarness {
public:
	static bool isRequested();
	explicit CadenceHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string label;
		std::function<bool()> ready;
		std::function<void()> act;
		float timeoutSeconds = 60.0f;
	};
	struct MediaChange {
		float t = 0.0f;
		std::string scene;
		MediaSelectionOrigin origin = MediaSelectionOrigin::Startup;
		bool cycleRestartedSameFrame = false; // a composition-cycle start fired this frame
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	bool idleOn(const std::string& sceneId) const;
	std::string media() const;
	int blueprintSeed() const;
	int blueprintCycleStarts() const;
	const BEComposition& blueprint() const;
	void monitor();
	int changesSince(size_t from, const std::string& scene, bool automaticOnly) const;
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	float clock_ = 0.0f;

	std::string lastMedia_;
	int lastSeed_ = 0;
	int lastCycleStarts_ = 0;
	int cycleStartsAtMark_ = 0;
	std::vector<MediaChange> changes_;
	size_t mark_ = 0;               // changes_ index at a remembered point
	int cycleStartSeed_ = 0;
	float cycleStartClock_ = 0.0f;
	std::string mediaAtMark_;
	int seedAtMark_ = 0;
	int violations_ = 0;            // automatic Blueprint change without a cycle restart

	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
