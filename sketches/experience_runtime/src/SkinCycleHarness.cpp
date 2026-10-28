#include "SkinCycleHarness.h"

#include "HudReactiveControlArt.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace { constexpr const char* kDir = "captures/skin-cycle"; }

bool SkinCycleHarness::isRequested() { return std::getenv("EXPERIENCE_RUNTIME_SKIN_CYCLE_HARNESS") != nullptr; }

void SkinCycleHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	const std::string l = std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail);
	ofLogNotice("SkinCycleHarness") << l;
	lines_.push_back(l);
}

void SkinCycleHarness::capture(const std::string& name) {
	std::filesystem::create_directories(ofToDataPath(kDir, true));
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kDir) + "/" + name + ".png");
	lines_.push_back("capture: bin/data/" + std::string(kDir) + "/" + name + ".png (skin " + runtime_.currentSkinForTesting() + ")");
}

// Stages (frames counted per stage). Per skin: settle -> faces/palette checks +
// focused capture -> Enter on MODE -> active capture 2 frames later -> release
// check -> next skin with 'v'.
void SkinCycleHarness::step(float) {
	if (done_) return;
	runtime_.draw();
	++frame_;
	const auto& bridge = runtime_.hudCompositorBridgeForTesting();
	const auto& r = bridge.renderer();
	const bool dayInk = r.colors().primary.r < 100;
	auto next = [&](int stage) { stage_ = stage; frame_ = 0; };

	switch (stage_) {
		case 0: // startup on the scheduled skin
			if (frame_ < 90) return;
			first_ = skin_ = runtime_.currentSkinForTesting();
			check("schedule active at startup, a v2 skin loaded", runtime_.skinAutoForTesting() && !skin_.empty()
				&& bridge.skinLoader().active().has_value(), skin_);
			check("palette matches the skin's period", dayInk == (skin_ == "plywood-marble"), dayInk ? "day ink" : "night text");
			next(10);
			return;
		case 10: // engage focus on MODE (RIGHT x3: PREV SCENE, NEXT SCENE, MODE)
			runtime_.keyPressed(OF_KEY_RIGHT);
			runtime_.keyPressed(OF_KEY_RIGHT);
			runtime_.keyPressed(OF_KEY_RIGHT);
			next(11);
			return;
		case 11: // checks + focused capture
			if (frame_ < 20) return;
			{
				const auto& active = bridge.skinLoader().active();
				check("[" + skin_ + "] all three button faces loaded and handed to the renderer",
					active && active->buttonIdle && active->buttonFocused && active->buttonActive && r.hasButtonFaces(),
					active ? "idle/focused/active" : "no skin");
				if (auto* pg = runtime_.typographyPlaygroundForTesting()) {
					lines_.push_back("playground: label draws " + ofToString(pg->labelDraws()) + ", font loads " + ofToString(pg->fontLoads())
						+ ", active font index " + ofToString(pg->fontIndex()) + ", palette day=" + (pg->dayPaletteForTesting() ? "1" : "0") + " | " + pg->styleSummaryForTesting());
				}
				check("[" + skin_ + "] focus is on MODE", bridge.focus().focused() && *bridge.focus().focused() == "controls.mode");
				const ofRectangle b = r.pixelBoundsFor("controls.mode");
				const auto rect = hudpresent::reactive::buttonFaceRect(b.x, b.y, b.width, b.height);
				check("[" + skin_ + "] MODE face rectangle = 80x80 centred on the 72x72 control", rect.width == 80 && rect.height == 80
					&& rect.x == 191 && rect.y == 630, ofToString(rect.x) + "," + ofToString(rect.y));
				capture("faces_" + ofToString(skinIndex_) + "_" + skin_ + "_focused");
				runtime_.keyPressed(OF_KEY_RETURN); // MODE: accepted -> active face
			}
			next(12);
			return;
		case 12: // active capture 2 frames after Enter
			if (frame_ < 2) return;
			check("[" + skin_ + "] active face fading in over the focused control", r.buttonActiveOpacity("controls.mode") > 0.5f,
				ofToString(r.buttonActiveOpacity("controls.mode"), 2));
			capture("faces_" + ofToString(skinIndex_) + "_" + skin_ + "_active");
			next(13);
			return;
		case 13: // release
			if (frame_ < 40) return;
			check("[" + skin_ + "] active face released after the feedback interval", r.buttonActiveOpacity("controls.mode") == 0.0f);
			capture("faces_" + ofToString(skinIndex_) + "_" + skin_ + "_idle_others");
			if (skinIndex_ == 0) {
				// manual cycling: remaining skins
			}
			if (++skinIndex_ < 3) {
				runtime_.keyPressed('v');
				next(14);
			} else {
				runtime_.keyPressed('V');
				next(20);
			}
			return;
		case 14: // skin switched manually
			if (frame_ < 60) return;
			skin_ = runtime_.currentSkinForTesting();
			check("'v' switched skin live (manual; schedule paused)", !runtime_.skinAutoForTesting() && !skin_.empty()
				&& bridge.skinLoader().active() && bridge.skinLoader().active()->skinId == skin_, skin_);
			check("[" + skin_ + "] palette follows the skin", dayInk == (skin_ == "plywood-marble"), dayInk ? "day ink" : "night text");
			next(11);
			return;
		case 20:
			if (frame_ < 60) return;
			check("'V' returns to the schedule (same skin as at startup)", runtime_.skinAutoForTesting() && runtime_.currentSkinForTesting() == first_,
				runtime_.currentSkinForTesting());
			finish();
			return;
	}
}

void SkinCycleHarness::finish() {
	lines_.push_back("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	ofLogNotice("SkinCycleHarness") << lines_.back();
	std::ofstream out(ofToDataPath(std::string(kDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
