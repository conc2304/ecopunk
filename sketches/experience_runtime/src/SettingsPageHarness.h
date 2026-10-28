#pragma once

#include "ExperienceRuntime.h"

#include <string>
#include <vector>

// SettingsPageHarness — Experience Settings page v0.1 in the real runtime,
// driven only through ExperienceRuntime::keyPressed() (the same path the
// keyboard / D-pad uses). EXPERIENCE_RUNTIME_SETTINGS_PAGE_HARNESS=1 runs the
// scripted traversal and leaves its settings file behind;
// =verify (a second launch with the same EXPERIENCE_RUNTIME_INSTALLATION_
// SETTINGS file) proves the confirmed values survived a restart.
class SettingsPageHarness {
public:
	static bool isRequested();
	explicit SettingsPageHarness(ExperienceRuntime& runtime);
	void step(float dt);

private:
	struct Op { int key; int waitFrames; std::string label; };
	void check(const std::string& name, bool passed, const std::string& detail = {});
	void capture(const std::string& name);
	void finish();
	std::string fileText() const;
	ExperienceRuntime& runtime_;
	bool verify_ = false;
	bool menu_ = false; // =menu: open through the HUD MENU button (D-pad focus + OK), close via Return row
	int frame_ = 0;
	int stage_ = 0;
	size_t op_ = 0;
	std::vector<Op> ops_;
	std::string sceneAtStart_;
	int focusSerialAtStart_ = 0;
	bool done_ = false;
	int passes_ = 0, failures_ = 0;
	std::vector<std::string> lines_;
};
