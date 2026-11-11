#pragma once

#include "ExperienceRuntime.h"

#include <string>
#include <vector>

// PredominantHarness - DEVELOPMENT/DIAGNOSTIC ONLY (EXPERIENCE_RUNTIME_PREDOMINANT_HARNESS=<seconds per scene>).
// OC-003 desktop verification inside the REAL ExperienceRuntime. It injects NOTHING: the FX and RAW series are
// whatever the real samplers measured, and every row below is read from what the HUD actually received
// (HudFrameData snapshots) and from the Orbit model slots the widget draws. It walks the five production
// scenes with the runtime's own NextScene command, then selects every FX/RAW x AVG/DOM/VIB observation in
// Orbit A/B through the real configuration editor state and captures the composed window.
// Writes bin/data/captures/predominant/{results.txt,rows.csv,*.png}.
class PredominantHarness {
public:
	static bool isRequested();
	explicit PredominantHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	void row();
	void check(const std::string& name, bool ok, const std::string& detail = "");
	void capture(const std::string& name);
	void finish();

	ExperienceRuntime& runtime_;
	float sceneSeconds_ = 25.0f;
	int frame_ = 0, sceneIdx_ = 0, comboIdx_ = 0;
	double sceneStart_ = 0.0, comboStart_ = 0.0, lastRow_ = 0.0, t0_ = 0.0;
	bool inCombos_ = false, done_ = false;
	uint64_t lastFxGen_ = 0, lastRawGen_ = 0;
	std::string sceneId_;
	std::vector<std::string> lines_;
	std::vector<float> frameMs_;
	std::string csv_;
	struct Agg {
		int rows = 0, avg = 0, dom = 0, vib = 0, vibInvalidAvgValid = 0, domValidVibInvalid = 0, domInvalidAvgValid = 0;
		int rawRows = 0, rawAvg = 0, rawDom = 0, rawVib = 0, rawVibInvalidAvgValid = 0;
		int slotGenMismatch = 0, leak = 0, notProvided = 0;
	};
	std::vector<std::pair<std::string, Agg>> aggs_;
	int passes_ = 0, failures_ = 0;
	bool predExpected_ = false, rawExpected_ = false;
};
