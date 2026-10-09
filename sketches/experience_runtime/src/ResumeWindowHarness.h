#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// ResumeWindowHarness — Shared Video long-clip resume-window verification in
// the real ExperienceRuntime (startup Blob, default roster, real input path).
//
// Finds a real long clip ('n'), switches Blob -> Temporal -> Blob mid-window
// (resume state and the canonical playhead must survive; Temporal's decoder
// must start at the canonical playhead), lets automatic advance end the
// window, then returns with Previous ('p') twice: visit 2 must start where
// visit 1 ended, visit 3 where visit 2 ended. Finally lets automatic advance
// run so the trace shows short/medium dwell.
//
// Run with EXPERIENCE_RUNTIME_RESUME_HARNESS=1, EXPERIENCE_RUNTIME_VIDEO_DWELL=10,20
// (the real catalog's longest clip is ~100 s) and
// EXPERIENCE_RUNTIME_VIDEO_RESUME_TRACE=1. Captures under
// bin/data/captures/resume-window/. Exits 0 only if every check passed.
class ResumeWindowHarness {
public:
	static bool isRequested();
	explicit ResumeWindowHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string label;
		std::function<bool()> ready;
		std::function<bool()> act; // false = stay on this step (loop)
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	std::string activeMedia() const;
	float canonicalPosition() const;
	bool idleOn(const std::string& sceneId) const;
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	int searchPresses_ = 0;
	std::string longMedia_;
	int longIndex_ = -1;
	float positionBeforeSwitch_ = 0.0f;
	float savedResume_ = 0.0f;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
