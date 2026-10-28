#include "SettingsPageHarness.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace { constexpr const char* kDir = "captures/settings-page"; }

bool SettingsPageHarness::isRequested() { return std::getenv("EXPERIENCE_RUNTIME_SETTINGS_PAGE_HARNESS") != nullptr; }

SettingsPageHarness::SettingsPageHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	const char* v = std::getenv("EXPERIENCE_RUNTIME_SETTINGS_PAGE_HARNESS");
	verify_ = v != nullptr && std::string(v) == "verify";
	menu_ = v != nullptr && std::string(v) == "menu";
	// OK is debounced to 250 ms, so keep >= 24 frames between OKs.
	const int ok = 26, mv = 4;
	if (menu_) {
		const int R = OF_KEY_RIGHT;
		ops_ = {
			{R, mv, "focus 1"}, {R, mv, "focus 2"}, {R, mv, "focus 3"}, {R, mv, "focus 4"}, {R, mv, "focus 5"}, {R, mv, "focus 6"}, {R, 12, "focus 7 = MENU"},
			{'#', 6, "capture menu focused"},
			{OF_KEY_RETURN, 30, "open via MENU"},
			{OF_KEY_DOWN, mv, "-> Scene auto-cycle"}, {OF_KEY_RETURN, ok, "toggle Scene auto-cycle ON"},
			{OF_KEY_DOWN, mv, "-> Scene interval"}, {OF_KEY_RETURN, ok, "edit"}, {OF_KEY_RIGHT, mv, "+1"}, {OF_KEY_RIGHT, mv, "+2"},
			{OF_KEY_RETURN, ok, "confirm 300 s"},
			{OF_KEY_UP, mv, "up"}, {OF_KEY_UP, mv, "-> Return to Experience"},
			{'#', 6, "capture return focused"},
			{OF_KEY_RETURN, 40, "return via Return row"},
			{'#', 6, "capture closed"},
			{OF_KEY_F1, 30, "keyboard F1 still opens"},
			{OF_KEY_ESC, 40, "close again"},
		};
		std::filesystem::create_directories(ofToDataPath(kDir, true));
		return;
	}
	ops_ = {
		{OF_KEY_F1, 30, "open"},
		{OF_KEY_DOWN, mv, "-> Scene auto-cycle"}, {OF_KEY_RETURN, ok, "toggle Scene auto-cycle ON"},
		{OF_KEY_DOWN, mv, "-> Scene interval"}, {OF_KEY_RETURN, ok, "edit"}, {OF_KEY_RIGHT, mv, "+1"}, {OF_KEY_RIGHT, mv, "+2"},
		{OF_KEY_RETURN, ok, "confirm 300 s"},
		{OF_KEY_DOWN, mv, "-> Media auto-cycle"}, {OF_KEY_RETURN, ok, "toggle Media auto-cycle OFF"},
		{OF_KEY_DOWN, mv, "-> Media interval"}, {OF_KEY_RETURN, ok, "edit"}, {OF_KEY_LEFT, mv, "-1"}, {OF_KEY_LEFT, mv, "-2"},
		{OF_KEY_RETURN, ok, "confirm 15 s"},
		{OF_KEY_DOWN, mv, "-> Time display"}, {OF_KEY_RETURN, ok, "toggle 12-hour"},
		{OF_KEY_DOWN, mv, "-> Timezone"}, {OF_KEY_RETURN, ok, "edit"}, {OF_KEY_DOWN, mv, "ZONE slot"}, {OF_KEY_RIGHT, mv, "next zone"},
		{'#', 10, "capture editing"},
		{OF_KEY_ESC, 6, "Esc cancels the edit"},
		{OF_KEY_DOWN, mv, "-> Time sync"}, {OF_KEY_DOWN, mv, "-> Set date & time"}, {OF_KEY_RETURN, ok, "edit"}, {OF_KEY_RIGHT, mv, "year +1"},
		{OF_KEY_RETURN, ok, "stage date/time"},
		{OF_KEY_DOWN, mv, "-> Apply clock changes"}, {OF_KEY_RETURN, ok, "OK on Apply (disabled or confirm)"},
		{'#', 10, "capture apply"},
		{']', 10, "']' must not change scene while open"}, {'n', 10, "'n' must not change media while open"},
		{OF_KEY_ESC, 40, "Esc returns to the experience"},
		{'#', 20, "capture closed"},
	};
	std::filesystem::create_directories(ofToDataPath(kDir, true));
}

void SettingsPageHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	const std::string l = std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail);
	ofLogNotice("SettingsPageHarness") << l;
	lines_.push_back(l);
}

void SettingsPageHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kDir) + "/" + name + ".png");
	lines_.push_back("capture: bin/data/" + std::string(kDir) + "/" + name + ".png");
}

std::string SettingsPageHarness::fileText() const {
	const char* path = std::getenv("EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS");
	if (!path) return std::string();
	std::ifstream in(path);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

void SettingsPageHarness::step(float) {
	if (done_) return;
	runtime_.draw();
	++frame_;
	const auto& inst = runtime_.installationForTesting();
	const auto& page = runtime_.settingsPageForTesting();
	const std::string scene = runtime_.sceneManagerForTesting().status().activeSceneId;

	if (verify_) {
		if (frame_ < 40) return;
		const auto& s = inst.settings();
		check("restart: Scene auto-cycle ON persisted", s.sceneAutoCycle.enabled);
		check("restart: Scene interval 300 s persisted", s.sceneAutoCycle.durationSeconds == 300.0f, ofToString(s.sceneAutoCycle.durationSeconds));
		check("restart: Media auto-cycle OFF persisted", !s.mediaAutoCycle.enabled);
		check("restart: Media interval 15 s persisted", s.mediaAutoCycle.durationSeconds == 15.0f, ofToString(s.mediaAutoCycle.durationSeconds));
		check("restart: 12-hour display persisted", !s.clock24Hour);
		check("restart: page starts closed", !page.isOpen());
		finish();
		return;
	}

	switch (stage_) {
		case 0:
			if (frame_ < 90) return;
			sceneAtStart_ = scene;
			focusSerialAtStart_ = runtime_.lastFocusActivationForTesting().serial;
			check("startup: a production scene is active", !sceneAtStart_.empty(), sceneAtStart_);
			capture("00_experience");
			stage_ = 1; frame_ = 0;
			return;
		case 1: { // scripted keys
			if (op_ >= ops_.size()) { stage_ = 2; frame_ = 0; return; }
			const Op& op = ops_[op_];
			if (frame_ == 1) {
				if (op.key == '#') capture("page_" + ofToString(int(op_)));
				else runtime_.keyPressed(op.key);
				if (op.label == "open") {
					// opened on this call: update() has not run yet, so check after the wait
				}
			}
			if (frame_ < op.waitFrames) return;
			// Per-step assertions (state after the wait)
			const std::string focusedHud = runtime_.hudCompositorBridgeForTesting().focus().focused().value_or("");
			if (op.label == "focus 7 = MENU") {
				check("D-pad reaches MENU as the seventh focus stop", focusedHud == "controls.menu", focusedHud);
			} else if (op.label == "open via MENU") {
				check("OK on MENU opens the Settings page", page.isOpen());
				check("opening Settings changed no scene", scene == sceneAtStart_, scene);
				check("MENU activation recorded as Runtime::OpenSettings", runtime_.lastFocusActivationForTesting().command == "Runtime::OpenSettings" && runtime_.lastFocusActivationForTesting().accepted);
				check("page starts on Return to Experience", page.focusedRow() == settingspage::RowId::ReturnTop);
				capture("menu_page_open");
			} else if (op.label == "-> Return to Experience") {
				check("focus is back on the Return row", page.focusedRow() == settingspage::RowId::ReturnTop);
			} else if (op.label == "return via Return row") {
				check("Return to Experience closes the page", !page.isOpen());
				check("HUD focus restored to MENU", focusedHud == "controls.menu", focusedHud);
				check("scene unchanged after closing", scene == sceneAtStart_, scene);
			} else if (op.label == "keyboard F1 still opens") {
				check("keyboard F1 still opens Settings", page.isOpen());
			} else if (op.label == "close again") {
				check("Esc closes it again", !page.isOpen());
			} else if (op.label == "open") {
				check("F1 opens the Settings page", page.isOpen());
				check("scene auto-cycle is suspended while open", inst.automationSuspended());
				check("focus starts on Return to Experience", page.focusedRow() == settingspage::RowId::ReturnTop);
				capture("01_open");
			} else if (op.label == "toggle Scene auto-cycle ON") {
				check("scene auto-cycle ON confirmed", inst.settings().sceneAutoCycle.enabled);
				check("...and written to the settings file immediately", fileText().find("\"enabled\": true") != std::string::npos);
			} else if (op.label == "confirm 300 s") {
				check("scene interval confirmed = 300 s", inst.settings().sceneAutoCycle.durationSeconds == 300.0f, ofToString(inst.settings().sceneAutoCycle.durationSeconds));
			} else if (op.label == "toggle Media auto-cycle OFF") {
				check("media auto-cycle OFF confirmed", !inst.settings().mediaAutoCycle.enabled);
				check("Shared Video timer is off", !runtime_.videoServiceForTesting().automaticAdvanceEnabled());
			} else if (op.label == "confirm 15 s") {
				check("media interval confirmed = 15 s (30 s, two list steps down)", inst.settings().mediaAutoCycle.durationSeconds == 15.0f, ofToString(inst.settings().mediaAutoCycle.durationSeconds));
			} else if (op.label == "toggle 12-hour") {
				check("12-hour display preference confirmed", !inst.settings().clock24Hour);
			} else if (op.label == "next zone") {
				check("timezone is being edited, nothing staged yet", page.editing() && !page.hasStagedChanges());
			} else if (op.label == "Esc cancels the edit") {
				check("Esc cancelled the edit; page still open; nothing staged", page.isOpen() && !page.editing() && !page.hasStagedChanges());
			} else if (op.label == "stage date/time") {
				check("date/time staged (not applied)", page.hasStagedChanges() && page.stagedChange().dateTime.has_value());
			} else if (op.label == "OK on Apply (disabled or confirm)") {
				check("Apply is disabled without platform authorisation (no confirmation started)", !page.confirming() && !page.applyBlockedReason().empty(), page.applyBlockedReason());
			} else if (op.label == "']' must not change scene while open") {
				check("']' did not change the scene", scene == sceneAtStart_, scene);
				check("scene cycle (ON, enabled while the page was open) accumulated nothing", inst.sceneCycleElapsed() < 0.5f, ofToString(inst.sceneCycleElapsed(), 2));
			} else if (op.label == "Esc returns to the experience") {
				check("Esc closed the page without quitting the app", !page.isOpen());
				check("automation resumed", !inst.automationSuspended());
				check("same scene as before the page", scene == sceneAtStart_, scene);
				check("no HUD control was activated by page input", runtime_.lastFocusActivationForTesting().serial == focusSerialAtStart_);
			}
			++op_;
			frame_ = 0;
			return;
		}
		case 2: // paused-vs-running proof, then finish
			if (frame_ < 90) return;
			check("scene cycle counts again after return (fresh period, no catch-up)", inst.sceneCycleElapsed() > 0.5f && inst.sceneCycleElapsed() < 5.0f, ofToString(inst.sceneCycleElapsed(), 2));
			finish();
			return;
	}
}

void SettingsPageHarness::finish() {
	lines_.push_back("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	ofLogNotice("SettingsPageHarness") << lines_.back();
	std::ofstream out(ofToDataPath(std::string(kDir) + (verify_ ? "/results_verify.txt" : "/results.txt"), true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
