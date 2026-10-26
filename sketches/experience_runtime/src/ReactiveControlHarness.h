#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// ReactiveControlHarness — HUD Runtime Reactive Control Artwork v0.1 real-
// runtime check (EXPERIENCE_RUNTIME_REACTIVE_HARNESS). Points the HUD skin
// loader at a scratch skins root (EXPERIENCE_RUNTIME_REACTIVE_SKINS_ROOT)
// holding three packages built from the Dark Moss shell:
//   dark-moss-experimental  shell + controls/scene_next/pressed.png
//   dark-moss-no-pressed    shell only (replacement without artwork)
//   dark-moss-bad-pressed   shell + an RGB (invalid) pressed.png
// and walks idle -> focused -> accepted press -> release on NEXT SCENE,
// refused (disabled) activation, replacement/unload, invalid-asset and
// upload-failure fallbacks and unskinned mode — capturing each state and
// measuring pixels in the control (artwork, label, focus brackets).
class ReactiveControlHarness {
public:
	static bool isRequested();
	explicit ReactiveControlHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string label;
		std::function<bool()> ready;
		std::function<void()> act;
	};
	struct Probe {
		float rimG = 0.0f;       // mean green in the rim band inside the control (artwork rim lives here)
		float rimR = 0.0f;
		int labelPixels = 0;     // bright label-text pixels in the control's centre band
		int bracketPixels = 0;   // accent pixels on the focus-bracket corners (outside the bounds)
	};

	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	Probe probe() const;
	std::string describe(const Probe& p) const;
	bool idleOn(const std::string& sceneId) const;
	bool sceneIdle() const;
	std::string focused() const;
	bool focusSceneNext();
	void buildScript();
	void finish();

	ExperienceRuntime& runtime_;
	std::string skinsRoot_;
	std::vector<Step> script_;
	size_t stepIndex_ = 0;
	int framesInStep_ = 0;
	float secondsInStep_ = 0.0f;
	uint64_t stageCallsAfterLoad_ = 0;
	uint64_t activationSerial_ = 0;
	Probe idle_, focused_, pressed_;
	bool done_ = false;
	int passes_ = 0;
	int failures_ = 0;
	std::vector<std::string> lines_;
};
