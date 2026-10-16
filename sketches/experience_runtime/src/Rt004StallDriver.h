#pragma once

#include <string>
#include <vector>

class ExperienceRuntime;

// RT-004 diagnostic driver — test-only (EXPERIENCE_RUNTIME_STALL_DRIVER).
// Runs on the NORMAL visible path (ofApp::update/draw drive the runtime as
// usual) and only injects the same key presses an operator would type,
// through ExperienceRuntime::keyPressed(): ']' = next scene, 'n' = next
// canonical media. No new runtime behavior.
//
// Independently of the RT-004 probe, it logs mean/max frame interval
// (ofGetLastFrameTime) per 5 s window to its own log, so probe-on and
// probe-off runs of the same script can be compared for probe overhead.
class Rt004StallDriver {
public:
	static bool isRequested();
	explicit Rt004StallDriver(ExperienceRuntime & runtime);

	// Call once per ofApp::update(), after runtime.update().
	void step(float dt);

private:
	struct Action {
		float atSeconds;
		int key; // 0 = exit; kMarkWindow = set the probe's "window" context to label
		std::string label;
	};
	static constexpr int kMarkWindow = -1;
	static constexpr int kSetLegacyDownscale = -2; // label "1" = legacy history downscale, "0" = cached
	static constexpr int kSetEnv = -3;             // label "NAME=VALUE" sets, "NAME=" unsets
	static constexpr int kMaskCycles = -4;         // label = cycle count (blob-short)
	static constexpr int kCapture = -5;            // label = scene-only capture path under bin/data (ambient-cycles)
	void runMaskCycles(int cycles);

	void log(const std::string & line);

	ExperienceRuntime & runtime_;
	std::vector<Action> script_;
	size_t next_ = 0;
	float elapsed_ = 0.0f;
	std::string logPath_;

	float windowElapsed_ = 0.0f;
	int windowFrames_ = 0;
	double windowSumMs_ = 0.0;
	double windowMaxMs_ = 0.0;

	// TEMP-008: per-labelled-window frame intervals (kMarkWindow boundaries),
	// summarised as a LABEL-WINDOW line when the next window starts.
	void closeLabelWindow();
	bool labelWindows_ = false; // ab-steady only; other scripts unchanged
	std::string labelName_;
	float labelStart_ = 0.0f;
	std::vector<double> labelMs_;
	int labelPatternChanges_ = 0;
	int labelLastPattern_ = -1;
	int labelLastAmbientDraws_ = 0; // ambient draws in the most recent frame
};
