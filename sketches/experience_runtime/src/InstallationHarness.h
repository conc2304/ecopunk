#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <set>
#include <string>
#include <vector>

// InstallationHarness — Installation Controls & ENERGY v0.1 real-runtime check
// (EXPERIENCE_RUNTIME_INSTALLATION_HARNESS). Full five-scene roster, startup
// Blob, settings persistence off. Drives the real request interface and keys:
// scene auto-cycle across all five scenes (manual reset, disable), media
// auto-cycle (independence, manual reset, disable), ENERGY via the encoder-
// emulating keys with Quadrant's measured crosshair response and continuity,
// and the fullscreen toggle with observed window state. Same scripted-step
// pattern as BlueprintHudHarness.
class InstallationHarness {
public:
	static bool isRequested();
	explicit InstallationHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string label;
		std::function<bool()> ready;
		std::function<void()> act;
		float timeoutSeconds = 90.0f;
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	bool idleOn(const std::string& sceneId) const;
	bool idle() const;
	std::string activeScene() const;
	std::string mediaId() const;
	const InstallationStatus* status() const;
	void request(installation::Request::Kind kind, bool flag = false, float value = 0.0f, std::optional<float> duration = {});
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	float clock_ = 0.0f;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;

	// Scene automation observation.
	std::string lastScene_;
	std::vector<std::string> autoVisits_;
	float lastActivationClock_ = 0.0f;
	float manualSwitchClock_ = -1.0f;
	float firstAutoAfterManual_ = -1.0f;
	int switchesWhileDisabled_ = 0;
	// Media automation observation.
	std::string lastMedia_;
	int mediaChanges_ = 0;
	float lastMediaChangeClock_ = 0.0f;
	float manualMediaClock_ = -1.0f;
	float firstAutoMediaAfterManual_ = -1.0f;
	int mediaChangesWhileDisabled_ = 0;
	bool watchSceneDisabled_ = false;
	bool watchMediaDisabled_ = false;
	// ENERGY observation.
	double speedSum_ = 0.0;
	int speedSamples_ = 0;
	float lowSpeed_ = 0.0f;
	std::string sceneBefore_;
	std::string mediaBefore_;
	std::string focusBefore_;
	int windowWidthBefore_ = 0;
};
