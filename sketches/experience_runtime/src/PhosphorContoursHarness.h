#pragma once

#include "ExperienceRuntime.h"

#include <string>

// PhosphorContoursHarness — real-runtime check of the OPT-IN Phosphor Contours scene
// (EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_HARNESS=1 together with EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_SCENE=1).
// Injects the same key presses an operator would type through ExperienceRuntime::keyPressed()
// ('m' NEXT MODE, '2' reseed, 'n' NEXT MEDIA, '1' Reset, ']' / '[' scene switch) and asserts from the real
// HudFrameData + scene seams: opt-in eighth ring entry, preset wrap, deterministic reseed, history cleared on
// media change / re-entry / resize, GPU targets released while inactive, GPU cost at 1280x720.
// Captures the composited window to bin/data/captures/phosphor-contours/.
class PhosphorContoursHarness {
public:
	static bool isRequested();
	explicit PhosphorContoursHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	bool idleOn(const std::string& sceneId) const;
	int modeIndex() const;
	std::string metricValue(const std::string& metricId) const;
	std::string mediaId() const;

	ExperienceRuntime& runtime_;
	int phase_ = 0;
	int frame_ = 0;
	int k_ = 0;
	bool sent_ = false;
	uint64_t clearsBefore_ = 0;
	std::string mediaBefore_;
	std::string seekMedia_;
	int seekTries_ = 0;
	double gpuMsSum_ = 0.0;
	int gpuMsN_ = 0;
	int passes_ = 0, failures_ = 0;
};
