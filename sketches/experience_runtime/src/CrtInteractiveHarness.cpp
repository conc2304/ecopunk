#include "CrtInteractiveHarness.h"

#include "CrtPhosphorScene.h"

#include "ofMain.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace {
constexpr const char* kCaptureDir = "captures/crt-interactive";
const std::string kCrtId = CrtPhosphorScene::kSceneId;
}

bool CrtInteractiveHarness::isRequested() { return std::getenv("EXPERIENCE_RUNTIME_CRT_INTERACTIVE_HARNESS") != nullptr; }

CrtInteractiveHarness::CrtInteractiveHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	buildScript();
}

void CrtInteractiveHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	ofLogNotice("CrtInteractiveHarness") << (passed ? "PASS " : "FAIL ") << name << (detail.empty() ? "" : " - " + detail);
}

bool CrtInteractiveHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId && f.scene.sceneId == sceneId;
}
std::string CrtInteractiveHarness::metric(const char* id) const {
	const auto& sem = runtime_.currentHudFrameData().scene.semantic;
	if (!sem) return {};
	for (const auto& m : sem->metrics) if (m.metricId == id && m.valueId) return *m.valueId;
	return {};
}
int CrtInteractiveHarness::modeIndex() const { const auto& m = runtime_.currentHudFrameData().scene.mode; return m ? m->index : -1; }
std::string CrtInteractiveHarness::focused() const {
	const auto& f = runtime_.hudBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}
std::string CrtInteractiveHarness::phaseName() const {
	using P = hudpresent::HudInteractionPhase;
	switch (runtime_.hudBridgeForTesting().interactionPhase()) {
		case P::Passive: return "passive"; case P::Idle: return "idle"; case P::Focused: return "focused";
		case P::Active: return "active"; case P::Disabled: return "disabled";
	}
	return "?";
}

void CrtInteractiveHarness::capture(const std::string& name) {
	const char* tagEnv = std::getenv("EXPERIENCE_RUNTIME_CRT_INTERACTIVE_TAG");
	const std::string tag = tagEnv ? std::string(tagEnv) + "_" : std::string();
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	const std::string path = std::string(kCaptureDir) + "/" + tag + name + ".png";
	img.save(path);
	ofLogNotice("CrtInteractiveHarness") << "capture: bin/data/" << path << " phase=" << phaseName() << " row=" << runtime_.hudBridgeForTesting().interactiveRow()
		<< " structure=" << metric("scene.crt.metric.structure_pos") << " palette=" << metric("scene.crt.metric.palette_pos");
}

void CrtInteractiveHarness::buildScript() {
	auto key = [this](int k) { return [this, k] { runtime_.keyPressed(k); }; };
	auto pos = [this](const char* id) { return metric(id); };
	auto S = [pos] { return pos("scene.crt.metric.structure_pos"); };
	auto P = [pos] { return pos("scene.crt.metric.palette_pos"); };
	runtime_.setRegionOptionSink(nullptr); // start with NO port to prove the region is then unavailable

	script_.push_back({"wait for CRT", [] {}, 160, [this] {
		check("CRT scene settled with rows published", idleOn(kCrtId) && !metric("scene.crt.metric.structure").empty(), "scene=" + runtime_.currentHudFrameData().scene.sceneId);
		baseStructure_ = metric("scene.crt.metric.structure"); basePalette_ = metric("scene.crt.metric.palette");
		check("no option port => region is DISABLED (hidden from focus, no affordance)", phaseName() == "disabled", phaseName());
		for (int i = 0; i < 12; i++) runtime_.keyPressed('\t');
		check("no option port => focus traversal never lands on the region", focused() != "interactive.region", focused());
		capture("00_unavailable");
	}});
	script_.push_back({"restore the PRODUCTION port", [this] {
		// Exactly what the ExperienceRuntime constructor installs: the typed SceneManager dispatch (Tier C).
		runtime_.setRegionOptionSink([this](int row, int delta) { return runtime_.sceneManagerForTesting().dispatchSceneOption(row, delta); });
	}, 8, [this] {
		check("with the production port + advertised option axes the region is IDLE-interactive", phaseName() == "idle", phaseName());
		capture("01_idle");
	}});
	script_.push_back({"focus the region (Tab to the single stop)", [this] {
		for (int i = 0; i < 14 && focused() != "interactive.region"; i++) runtime_.keyPressed('\t');
	}, 8, [this, S, P] {
		check("exactly one focus stop reaches the region", focused() == "interactive.region", focused());
		check("FOCUSED (not active)", phaseName() == "focused", phaseName());
		check("focus movement alone changed no value", S() == "1/6" && metric("scene.crt.metric.structure") == baseStructure_ && metric("scene.crt.metric.palette") == basePalette_, S() + " " + P());
		capture("02_focused");
	}});
	script_.push_back({"Enter -> ACTIVE", key(OF_KEY_RETURN), 8, [this] {
		check("Enter activates the region on PIXELS (row 0)", phaseName() == "active" && runtime_.hudBridgeForTesting().interactiveRow() == 0, phaseName());
		capture("03_active_pixels");
	}});
	script_.push_back({"Right (PIXELS next)", key(OF_KEY_RIGHT), 10, [this, S] {
		check("Right -> structure 2/6", S() == "2/6", S());
		check("palette untouched by a PIXELS step", metric("scene.crt.metric.palette") == basePalette_);
		capture("04_pixels_2of6");
	}});
	script_.push_back({"Left x2 (wrap)", [this] { runtime_.keyPressed(OF_KEY_LEFT); runtime_.keyPressed(OF_KEY_LEFT); }, 10, [this, S] {
		check("Left Left wraps 2/6 -> 1/6 -> 6/6", S() == "6/6", S());
		capture("05_pixels_6of6_longest");
	}});
	script_.push_back({"Up at first row (clamp)", key(OF_KEY_UP), 4, [this] {
		check("Up on PIXELS stays on PIXELS (clamp)", runtime_.hudBridgeForTesting().interactiveRow() == 0 && phaseName() == "active");
	}});
	script_.push_back({"Down (PALETTE)", key(OF_KEY_DOWN), 6, [this] {
		check("Down selects PALETTE", runtime_.hudBridgeForTesting().interactiveRow() == 1);
	}});
	script_.push_back({"Right (PALETTE next)", key(OF_KEY_RIGHT), 10, [this, S, P] {
		check("Right -> palette 2/8", P() == "2/8", P());
		check("structure untouched by a PALETTE step", S() == "6/6", S());
		capture("06_active_palette_2of8");
	}});
	script_.push_back({"Left x2 (wrap)", [this] { runtime_.keyPressed(OF_KEY_LEFT); runtime_.keyPressed(OF_KEY_LEFT); }, 10, [this, P] {
		check("Left Left wraps 2/8 -> 1/8 -> 8/8", P() == "8/8", P());
		capture("07_palette_8of8_longest");
	}});
	script_.push_back({"m while active (no key theft)", [this] { modeBefore_ = modeIndex(); runtime_.keyPressed('m'); }, 8, [this] {
		check("'m' still steps composition while the region is active", modeIndex() == (modeBefore_ + 1) % 4, "before=" + ofToString(modeBefore_) + " after=" + ofToString(modeIndex()));
		check("region remains active after a global key", phaseName() == "active");
	}});
	script_.push_back({"Down on the LAST row -> exit", key(OF_KEY_DOWN), 8, [this, S, P] {
		check("Down on the last row (PALETTE) leaves editing; region stays FOCUSED", phaseName() == "focused" && focused() == "interactive.region", phaseName());
		check("applied values are kept on exit (no revert)", S() == "6/6" && P() == "8/8", S() + " " + P());
		capture("08_exit_focused");
	}});
	script_.push_back({"wait out the closing-Enter debounce", [] {}, 150, nullptr});
	script_.push_back({"Enter -> re-enter, then Esc", key(OF_KEY_RETURN), 8, [this] {
		check("re-entry resumes the previously selected row (PALETTE)", phaseName() == "active" && runtime_.hudBridgeForTesting().interactiveRow() == 1, phaseName());
		runtime_.keyPressed(OF_KEY_ESC);
	}});
	script_.push_back({"after Esc", [] {}, 8, [this, S, P] {
		check("Esc leaves editing without quitting; values kept", phaseName() == "focused" && S() == "6/6" && P() == "8/8", phaseName());
	}});
	script_.push_back({"wait out the Esc debounce", [] {}, 150, nullptr});
	script_.push_back({"re-enter, then switch scene", [this] { runtime_.keyPressed(OF_KEY_RETURN); }, 8, [this] {
		check("active again before the scene switch", phaseName() == "active");
		runtime_.keyPressed('[');
	}});
	script_.push_back({"away", [] {}, 90, [this] {
		const std::string other = runtime_.currentHudFrameData().sceneManager.activeSceneId;
		check("scene change leaves editing and clears the region", other != kCrtId && !runtime_.hudBridgeForTesting().interactiveActive() && phaseName() == "passive", other + " " + phaseName());
		check("no stale focus on the region in the other scene", focused() != "interactive.region", focused());
		runtime_.keyPressed(']');
	}});
	script_.push_back({"back", [] {}, 120, [this] {
		check("back in CRT: not active, no stale focus", idleOn(kCrtId) && !runtime_.hudBridgeForTesting().interactiveActive() && focused() != "interactive.region", phaseName() + " " + focused());
		check("applied values persisted across the scene switch", metric("scene.crt.metric.structure_pos") == "6/6" && metric("scene.crt.metric.palette_pos") == "8/8");
		runtime_.setRegionOptionSink(nullptr);
	}});
	script_.push_back({"port removed", [] {}, 8, [this] {
		check("removing the port makes the region unavailable again", phaseName() == "disabled", phaseName());
		ofLogNotice("CrtInteractiveHarness") << "desktop frame time: mean " << ofToString(frameMsN_ ? frameMsSum_ / frameMsN_ : 0.0, 2) << " ms over " << frameMsN_ << " frames";
		ofLogNotice("CrtInteractiveHarness") << "RESULT: " << passes_ << " passed, " << failures_ << " failed";
		done_ = true;
		ofExit(failures_ == 0 ? 0 : 1);
	}});
}

void CrtInteractiveHarness::step(float dt) {
	if (done_ || at_ >= script_.size()) return;
	frame_++;
	if (at_ > 0) { frameMsSum_ += dt * 1000.0; frameMsN_++; }
	Step& s = script_[at_];
	if (!acted_) {
		if (at_ == 0 && !(idleOn(kCrtId) && !metric("scene.crt.metric.structure").empty() && frame_ > 150)) {
			if (frame_ > 1800) { check("CRT scene became active", false, "timeout"); done_ = true; ofExit(1); }
			return;
		}
		if (s.act) s.act();
		acted_ = true; frame_ = 0;
		return;
	}
	if (frame_ >= s.waitFrames) {
		if (s.verify) s.verify();
		at_++; acted_ = false; frame_ = 0;
	}
}
