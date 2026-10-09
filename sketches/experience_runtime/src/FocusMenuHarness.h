#pragma once

#include "ExperienceRuntime.h"

#include <string>
#include <vector>

// FocusMenuHarness — Playable HUD Menu v0.1 demonstration/verification.
// Opt-in: EXPERIENCE_RUNTIME_FOCUS_HARNESS=1. Runs the normal installed
// production scene and drives the REAL input path (runtime.keyPressed() with
// OF_KEY_LEFT/RIGHT/RETURN and the existing 'n' key), capturing each focus
// state to bin/data/captures/focus-menu/ with results.txt, then exits.
class FocusMenuHarness {
public:
	static bool isRequested();
	explicit FocusMenuHarness(ExperienceRuntime& runtime) : runtime_(runtime) {}
	void step(float dt);

private:
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void note(const std::string& line);
	void capture(const std::string& name);
	void press(int key, const std::string& label);
	std::string mediaId() const;
	std::string focused() const;
	bool bracketsAt(const std::string& regionId);

	ExperienceRuntime& runtime_;
	int frame_ = 0;
	int passes_ = 0, failures_ = 0;
	std::string media0_, media1_;
	std::vector<std::string> lines_;
};
