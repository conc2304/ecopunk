#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// PresentationHarness — RT-007 Fullscreen Presentation & HUD Scaling real-
// runtime check (EXPERIENCE_RUNTIME_RT007_HARNESS). Full five-scene roster.
// Windowed start: direct 1280x720 path -> live resize to 1600x900 (the Rock
// Pi target) through the canonical composition, all five scenes captured ->
// real fullscreen (state read back from the window, never assumed) -> back
// to a 1280x720 window (direct path again). Started with
// EXPERIENCE_RUNTIME_FULLSCREEN=1 it instead verifies native fullscreen
// startup first. Alignment is measured, not eyeballed: the canonical
// composition FBO and the scaled framebuffer of the SAME frame are compared
// through the transform, against the old top-left (unscaled) mapping.
class PresentationHarness {
public:
	static bool isRequested();
	explicit PresentationHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string label;
		std::function<bool()> ready;
		std::function<void()> act;
	};
	struct Alignment {
		float transformed = -1.0f; // mean |luma| difference through the transform
		float topLeft = -1.0f;     // same, with the pre-RT-007 unscaled top-left mapping
		int samples = 0;
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	bool idleOn(const std::string& sceneId) const;
	std::string focused() const;
	std::string mediaId() const;
	std::string describeTransform() const;
	Alignment measureAlignment();
	void checkComposed(const std::string& where);
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	bool startedFullscreen_ = false;
	uint64_t sceneFboAllocsAtStart_ = 0;
	uint64_t switchReallocsAtStart_ = 0;
	uint64_t directFramesMark_ = 0;
	uint64_t composedFramesMark_ = 0;
	std::string mediaAtStart_;
	int stableFrames_ = 0;
	int lastW_ = 0, lastH_ = 0;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
