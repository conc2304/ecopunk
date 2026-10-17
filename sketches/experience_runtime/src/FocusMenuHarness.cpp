#include "FocusMenuHarness.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {
constexpr const char* kPrev = "controls.media_previous";
constexpr const char* kNext = "controls.media_next";
}

bool FocusMenuHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_FOCUS_HARNESS") != nullptr;
}

void FocusMenuHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void FocusMenuHarness::note(const std::string& line) {
	ofLogNotice("FocusMenuHarness") << line;
	lines_.push_back(line);
}

void FocusMenuHarness::capture(const std::string& name) {
	std::filesystem::create_directories(ofToDataPath("captures/focus-menu", true));
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save("captures/focus-menu/" + name + ".png");
	note("capture: bin/data/captures/focus-menu/" + name + ".png (focus=" + focused() + ")");
}

void FocusMenuHarness::press(int key, const std::string& label) {
	note("key " + label + " (frame " + ofToString(frame_) + ")");
	runtime_.keyPressed(key); // the real input path: InputRouter -> focus -> existing command paths
}

std::string FocusMenuHarness::mediaId() const {
	const auto& v = runtime_.currentHudFrameData().video;
	return (v && v->mediaId) ? *v->mediaId : std::string("(none)");
}

std::string FocusMenuHarness::focused() const {
	const auto& f = runtime_.hudCompositorBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}

// Focus brackets: accent-coloured (244,255,106) 2 px lines 3 px outside the
// region's top-left corner. Read from the frame just drawn.
bool FocusMenuHarness::bracketsAt(const std::string& regionId) {
	const ofRectangle b = runtime_.hudCompositorBridgeForTesting().renderer().pixelBoundsFor(regionId);
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	const ofPixels& px = img.getPixels();
	const int x = int(b.x - 3.0f) + 5;
	for (int y = int(b.y - 3.0f) - 2; y <= int(b.y - 3.0f) + 2; ++y) {
		const ofColor c = px.getColor(x, y);
		if (c.r > 200 && c.g > 200 && c.b < 170) return true;
	}
	return false;
}

void FocusMenuHarness::step(float /*dt*/) {
	runtime_.draw();
	const auto& bridge = runtime_.hudCompositorBridgeForTesting();
	const auto& renderer = bridge.renderer();

	switch (frame_) {
		case 60:
			capture("focus_00_baseline_no_focus");
			media0_ = mediaId();
			check("baseline: no focus before any input", focused() == "none" && !bridge.focus().engaged());
			check("baseline: no focus brackets drawn", !bracketsAt(kNext) && !bracketsAt(kPrev));
			check("baseline: PREV MEDIA disabled at start of media history, NEXT enabled",
				renderer.controlEnabled(kPrev) == false && renderer.controlEnabled(kNext) == true,
				"media=" + media0_);
			check("baseline: scene/reseed controls hidden (unsupported in HudFrameData today)",
				!renderer.controlEnabled("controls.scene_previous") && !renderer.controlEnabled("controls.scene_next") &&
				!renderer.controlEnabled("controls.reseed"));
			press(OF_KEY_RIGHT, "RIGHT");
			break;
		case 75:
			capture("focus_01_next_media_focused");
			check("first step engages on the first ENABLED control (NEXT MEDIA)", focused() == kNext);
			check("focus brackets drawn around NEXT MEDIA", bracketsAt(kNext) && !bracketsAt(kPrev));
			press(OF_KEY_LEFT, "LEFT");
			break;
		case 90:
			capture("focus_02_prev_media_focused_disabled");
			check("LEFT moves focus to PREV MEDIA (disabled controls are focusable)", focused() == kPrev && bracketsAt(kPrev));
			press(OF_KEY_RETURN, "ENTER");
			break;
		case 92:
			capture("focus_03_refused_activation_flash");
			check("ENTER on disabled PREV MEDIA is refused, nothing dispatched",
				runtime_.lastFocusActivationForTesting().refused && runtime_.lastFocusActivationForTesting().command.empty());
			break;
		case 150:
			check("refused activation left the media unchanged", mediaId() == media0_, mediaId());
			check("focus stable across ~60 frames of HUD updates", focused() == kPrev);
			press(OF_KEY_RIGHT, "RIGHT");
			press(OF_KEY_RETURN, "ENTER");
			break;
		case 152:
			capture("focus_04_activate_next_media_flash");
			check("ENTER on NEXT MEDIA dispatched SceneCommand::NextMedia and was accepted",
				runtime_.lastFocusActivationForTesting().command == "SceneCommand::NextMedia" &&
				runtime_.lastFocusActivationForTesting().accepted);
			break;
		case 240:
			capture("focus_05_after_next_media");
			media1_ = mediaId();
			check("activation took effect: media changed", media1_ != media0_, media0_ + " -> " + media1_);
			check("PREV MEDIA is now enabled (history grew)", renderer.controlEnabled(kPrev) == true);
			check("focus stayed on NEXT MEDIA through the media change", focused() == kNext && bracketsAt(kNext));
			press(OF_KEY_LEFT, "LEFT");
			press(OF_KEY_RETURN, "ENTER");
			break;
		case 242:
			capture("focus_06_activate_prev_media_flash");
			check("ENTER on PREV MEDIA dispatched SceneCommand::PreviousMedia and was accepted",
				runtime_.lastFocusActivationForTesting().command == "SceneCommand::PreviousMedia" &&
				runtime_.lastFocusActivationForTesting().accepted);
			break;
		case 330:
			capture("focus_07_after_prev_media");
			check("PREV MEDIA returned to the first item", mediaId() == media0_, media1_ + " -> " + mediaId());
			press('n', "n (existing media key)");
			break;
		case 420:
			check("existing 'n' key still works alongside focus", mediaId() != media0_, mediaId());
			check("existing key did not move focus", focused() == kPrev);
			press(OF_KEY_RIGHT, "RIGHT");
			press(OF_KEY_RIGHT, "RIGHT");
			press(OF_KEY_RIGHT, "RIGHT");
			// Three visible stops: PREV MEDIA, NEXT MEDIA, and the Scene &
			// Pattern Navigation mode panel (flexible.secondary_a).
			check("RIGHT x3 wraps back to PREV MEDIA (PREV, NEXT, mode panel)", focused() == kPrev);
			press(OF_KEY_DOWN, "DOWN");
			check("DOWN steps forward like RIGHT", focused() == kNext);
			press(OF_KEY_UP, "UP");
			check("UP steps back like LEFT", focused() == kPrev);
			break;
		case 425: {
			note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
			std::ofstream out(ofToDataPath("captures/focus-menu/results.txt", true));
			for (const auto& l : lines_) out << l << "\n";
			out.close();
			ofExit(failures_ == 0 ? 0 : 1);
			break;
		}
		default:
			break;
	}
	frame_++;
}
