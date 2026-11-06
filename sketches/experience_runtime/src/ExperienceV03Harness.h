#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// ExperienceV03Harness — HUD & Scene Experience v0.3 real-runtime check
// (EXPERIENCE_RUNTIME_V03_HARNESS). Full five-scene roster, startup Blob.
// Covers, through real device-neutral inputs and the real HudFrameData:
//   - navigation row: PREV/NEXT SCENE through focus + Enter, "[EXP x/y]"
//   - IMMERSIVE: focus + Enter enters, any key / pointer exits with the
//     first input consumed, focus restored, scene + media continuous
//   - media progress slot ("current / total" from Shared Video)
//   - Blueprint visible occupancy (method in the .cpp, measured on the
//     scene FBO cropped to the part the HUD viewport shows)
//   - Fragment radar inside the visible area, contacts == live fragments
//   - Quadrant reticle motion at 60 and 30 fps + state badge detail
// Same scripted step pattern as ModeNavigationHarness.
class ExperienceV03Harness {
public:
	static bool isRequested();
	explicit ExperienceV03Harness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string label;
		std::function<bool()> ready; // wait (one frame at a time) until true
		std::function<void()> act;   // runs once, on the first frame ready() holds
	};

	struct MotionWindow {
		int frames = 0;
		double sumSpeed = 0.0, sumRawSpeed = 0.0, sumRawPer60 = 0.0;
		double sumSpeedDelta = 0.0, sumRawSpeedDelta = 0.0;
		double sumWidthDelta = 0.0;
		int dashToggles = 0, rawDashToggles = 0;
		float lastSpeed = -1.0f, lastRaw = -1.0f, lastWidth = -1.0f;
		bool dash = false, rawDash = false;
		float seconds = 0.0f;
		float prevX = 0.0f, prevY = 0.0f;
		bool havePrev = false;
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	void captureScene(const std::string& name);
	bool idleOn(const std::string& sceneId) const;
	std::string focused() const;
	std::string mediaId() const;
	std::string resolvedText(const std::string& sourceId) const;
	bool focusRegion(const std::string& regionId); // one RIGHT press every other frame until focused
	float visibleOccupancy();
	void sampleMotion(MotionWindow& w, float dt);
	std::string describe(const MotionWindow& w) const;
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	float lastDt_ = 0.0f;
	std::string sceneBefore_;
	std::string mediaBefore_;
	uint64_t liveFramesBefore_ = 0;
	std::vector<float> occupancy_;
	std::vector<int> occupancyPhase_;   // CompositionBase::CyclePhase per sample
	std::vector<int> occupancyFragments_;
	float occupancyNext_ = 0.0f;
	float occupancySeconds_ = 120.0f;
	MotionWindow motion60_, motion30_;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
