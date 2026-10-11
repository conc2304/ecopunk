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
};
