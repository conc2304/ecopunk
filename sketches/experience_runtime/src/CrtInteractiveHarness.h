#pragma once

#include "ExperienceRuntime.h"

#include <functional>
#include <string>
#include <vector>

// CrtInteractiveHarness — real-runtime check of the generic interactive HUD region applied to the
// CRT PIXELS / PALETTE selector group (EXPERIENCE_RUNTIME_CRT_INTERACTIVE_HARNESS; also set
// EXPERIENCE_RUNTIME_CRT_SCENE=1). Drives the SAME key path an operator uses
// (ExperienceRuntime::keyPressed) and asserts from the real HUD bridge + HudFrameData:
//   unavailable (no option port) -> idle -> focused -> active PIXELS -> active PALETTE -> exit,
//   clamp on Up/Down, wrap on Left/Right, no key theft ('m' still steps composition), scene
//   change clears editing + focus. It captures the composited 1280x720 window at each state.
// It exercises the PRODUCTION path: ExperienceRuntime's port -> SceneManager::dispatchSceneOption ->
// CrtPhosphorScene::stepOption (Tier C). It starts with the port cleared to prove "unavailable".
class CrtInteractiveHarness {
public:
	static bool isRequested();
	explicit CrtInteractiveHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Step {
		std::string name;
		std::function<void()> act;
		int waitFrames = 8;
		std::function<void()> verify;
	};
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void capture(const std::string& name);
	void buildScript();
	bool idleOn(const std::string& sceneId) const;
	std::string metric(const char* id) const;
	std::string phaseName() const;
	std::string focused() const;
	int modeIndex() const;

	ExperienceRuntime& runtime_;
	std::vector<Step> script_;
	size_t at_ = 0;
	int frame_ = 0;
	bool started_ = false, acted_ = false, done_ = false;
	std::string baseStructure_, basePalette_;
	int modeBefore_ = -1;
	double frameMsSum_ = 0.0;
	int frameMsN_ = 0;
	int passes_ = 0, failures_ = 0;
};
