#include "FxGalleryHarness.h"

#include "ofMain.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <set>

namespace {
constexpr const char* kCaptureDir = "captures/fx-gallery";
const std::string kFxId = SharedEffectsGalleryScene::kSceneId;
}

bool FxGalleryHarness::isRequested() { return std::getenv("EXPERIENCE_RUNTIME_FX_GALLERY_HARNESS") != nullptr; }

FxGalleryHarness::FxGalleryHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	buildScript();
}

SharedEffectsGalleryScene& FxGalleryHarness::scene() { return runtime_.sharedEffectsGallerySceneForTesting(); }

void FxGalleryHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	ofLogNotice("FxGalleryHarness") << (passed ? "PASS " : "FAIL ") << name << (detail.empty() ? "" : " - " + detail);
}

bool FxGalleryHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId && f.scene.sceneId == sceneId;
}
std::string FxGalleryHarness::metric(const std::string& id) const {
	const auto& sem = runtime_.currentHudFrameData().scene.semantic;
	if (!sem) return {};
	for (const auto& m : sem->metrics) if (m.metricId == id && m.valueId) return *m.valueId;
	return {};
}
int FxGalleryHarness::controlsShown() const {
	int n = 0;
	for (int i = 0; i < 2; ++i) if (!metric("scene.fx.metric.c" + std::to_string(i) + "_label").empty()) ++n;
	return n;
}
std::string FxGalleryHarness::focused() const {
	const auto& f = runtime_.hudBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}
std::string FxGalleryHarness::phaseName() const {
	using P = hudpresent::HudInteractionPhase;
	switch (runtime_.hudBridgeForTesting().interactionPhase()) {
		case P::Passive: return "passive"; case P::Idle: return "idle"; case P::Focused: return "focused";
		case P::Active: return "active"; case P::Disabled: return "disabled";
	}
	return "?";
}

void FxGalleryHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	const std::string path = std::string(kCaptureDir) + "/" + name + ".png";
	img.save(path);
	const auto& m = runtime_.currentHudFrameData().scene.mode;
	ofLogNotice("FxGalleryHarness") << "capture: bin/data/" << path << " effect=" << (m ? m->modeId : "?") << " controls=" << controlsShown()
		<< " phase=" << phaseName() << " navEmphasis=" << runtime_.hudBridgeForTesting().rendererForTesting().navigationEmphasis();
}

void FxGalleryHarness::goToEffect(const std::string& id) {
	for (int i = 0; i < 60 && scene().controllerForTesting().mode().effectId != id; ++i) runtime_.keyPressed('m');
}

void FxGalleryHarness::focusRegion() {
	for (int i = 0; i < 16 && focused() != "interactive.region"; ++i) runtime_.keyPressed('\t');
}

void FxGalleryHarness::buildScript() {
	auto effectNow = [this]() { return scene().controllerForTesting().mode().effectId; };

	script_.push_back({"wait for scene", [] {}, 200, [this] {
		check("gallery scene is active and settled", idleOn(kFxId), runtime_.currentHudFrameData().scene.sceneId);
		const auto ids = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
		check("gallery is the 7th (last) production scene; no debugger scene added", ids.size() == 7 && ids.back() == kFxId, ofToString(ids.size()));
		roster_ = scene().controllerForTesting().modeCount();
		check("roster is non-empty and every mode maps to a canonical effect", roster_ >= 10 && scene().controllerForTesting().ok(), "modes=" + ofToString(roster_));
		check("canonical media status is authoritative (media name present)", runtime_.currentHudFrameData().scene.mediaName.has_value());
		capture("00_initial");
	}});

	// Full-roster cycle through the real mode key, asserting canonical activity + 0/1/2 control counts.
	auto seen = std::make_shared<std::set<std::string>>();
	auto counts = std::make_shared<std::array<int, 3>>(std::array<int, 3>{0, 0, 0});
	for (int i = 0; i < 30; ++i) {
		script_.push_back({"cycle mode", [this, i, seen] {
			if (i < roster_ || i == 0) runtime_.keyPressed('m');
		}, 4, [this, i, seen, counts, effectNow] {
			if (i >= roster_) return;
			const auto& f = runtime_.currentHudFrameData();
			const std::string cur = effectNow();
			const bool activityOk = f.effects && f.effects->slots.size() == 1 && f.effects->slots[0].effectId == cur;
			const bool modeOk = f.scene.mode && f.scene.mode->modeId == cur && f.scene.mode->index == scene().controllerForTesting().modeIndex();
			const int shown = controlsShown();
			check("mode " + ofToString(i + 1) + " '" + cur + "': HUD mode + canonical activity agree, no stale frame", activityOk && modeOk,
				std::string("activity=") + (f.effects && !f.effects->slots.empty() ? f.effects->slots[0].effectId : "none") + " mode=" + (f.scene.mode ? f.scene.mode->modeId : "none"));
			check("mode '" + cur + "': HUD controls == descriptor controls (" + ofToString(shown) + ")", shown == scene().controllerForTesting().controlCount() && shown <= 2);
			seen->insert(cur);
			if (shown >= 0 && shown <= 2) (*counts)[size_t(shown)]++;
			if (i == roster_ - 1) {
				check("full roster visited once; last mode wraps to the first", int(seen->size()) == roster_ && scene().controllerForTesting().modeIndex() == 0);
				check("roster has 0-, 1- and 2-control modes", (*counts)[0] >= 1 && (*counts)[1] >= 1 && (*counts)[2] >= 1,
					ofToString((*counts)[0]) + "/" + ofToString((*counts)[1]) + "/" + ofToString((*counts)[2]));
			}
		}});
	}

	// Zero-control mode: group hides, never traps.
	script_.push_back({"zero-control mode", [this] { goToEffect("invert"); }, 12, [this] {
		check("0-control effect: no rows shown", controlsShown() == 0);
		check("0-control effect: parameter region not available (hidden, passive)", phaseName() != "idle" && phaseName() != "focused" && phaseName() != "active", phaseName());
		for (int i = 0; i < 16; ++i) runtime_.keyPressed('\t');
		check("0-control effect: focus traversal cannot land on the region", focused() != "interactive.region", focused());
		capture("01_zero_controls");
	}});

	// One-control mode: exactly one row; Down exits.
	script_.push_back({"to one-control mode", [this] { goToEffect("threshold"); }, 12, nullptr});
	script_.push_back({"one-control mode", [this] { focusRegion(); }, 10, [this] {
		check("1-control effect shows exactly one control", controlsShown() == 1);
		check("region focus reached with Tab (single stop)", focused() == "interactive.region" && phaseName() == "focused", focused() + "/" + phaseName());
		runtime_.keyPressed(OF_KEY_RETURN);
	}});
	script_.push_back({"edit one control", [this] { tmp_ = metric("scene.fx.metric.c0_value"); runtime_.keyPressed(OF_KEY_RIGHT); }, 10, [this] {
		check("Right on a numeric control steps it deliberately", metric("scene.fx.metric.c0_value") != tmp_, tmp_ + " -> " + metric("scene.fx.metric.c0_value"));
		runtime_.keyPressed(OF_KEY_DOWN);
	}});
	script_.push_back({"Down exits (1 control)", [] {}, 8, [this] {
		check("1-control effect: Down on the only (last) control exits parameter focus", phaseName() == "focused" && !runtime_.hudBridgeForTesting().interactiveActive(), phaseName());
	}});
	script_.push_back({"debounce 1", [] {}, 150, nullptr});

	// Two-control mode: numeric then enum, Down walks, last Down exits.
	script_.push_back({"to two-control mode", [this] { goToEffect("heatmap_recolor"); }, 12, nullptr});
	script_.push_back({"two-control mode", [this] { focusRegion(); }, 10, [this] {
		check("2-control effect shows exactly two controls", controlsShown() == 2);
		capture("02_two_controls_focused_t0");
		runtime_.keyPressed(OF_KEY_RETURN);
	}});
	script_.push_back({"enum step", [this] { tmp_ = metric("scene.fx.metric.c0_value"); runtime_.keyPressed(OF_KEY_RIGHT); }, 10, [this] {
		check("enum control shows a human-readable value (THERMAL -> BIOLUM)", tmp_ == "THERMAL" && metric("scene.fx.metric.c0_value") == "BIOLUM", tmp_ + " -> " + metric("scene.fx.metric.c0_value"));
		check("enum position is shown (2/4)", metric("scene.fx.metric.c0_pos") == "2/4", metric("scene.fx.metric.c0_pos"));
		capture("03_two_controls_active_row0");
		runtime_.keyPressed(OF_KEY_DOWN);
	}});
	script_.push_back({"numeric step", [this] { tmp_ = metric("scene.fx.metric.c1_value"); runtime_.keyPressed(OF_KEY_LEFT); }, 10, [this] {
		check("Down moved to row 2; Left decrements the numeric control within bounds", metric("scene.fx.metric.c1_value") != tmp_ && runtime_.hudBridgeForTesting().interactiveRow() == 1, tmp_ + " -> " + metric("scene.fx.metric.c1_value"));
		capture("04_two_controls_active_row1");
	}});
	script_.push_back({"clamp numeric", [this] { for (int i = 0; i < 12; ++i) runtime_.keyPressed(OF_KEY_LEFT); }, 10, [this] {
		check("numeric control clamps at its minimum (INTENSE 0.5)", metric("scene.fx.metric.c1_value") == "0.5", metric("scene.fx.metric.c1_value"));
		runtime_.keyPressed(OF_KEY_DOWN);
	}});
	script_.push_back({"Down exits (2 controls)", [] {}, 8, [this] {
		check("2-control effect: Down on the LAST control exits parameter focus", phaseName() == "focused", phaseName());
	}});

	// Nested-focus presentation: nav dims to 50 %, pulse varies over time, and restores.
	script_.push_back({"nested focus emphasis", [] {}, 4, [this] {
		const auto& r = runtime_.hudBridgeForTesting().rendererForTesting();
		check("focus inside the parameter region: navigation emphasis is 0.5", r.nestedFocusActive() && r.navigationEmphasis() == 0.5f, ofToString(r.navigationEmphasis()));
		capture("05_focused_nav_dimmed_a");
	}});
	script_.push_back({"pulse phase", [] {}, 25, [this] {
		capture("06_focused_nav_dimmed_b"); // ~half a pulse later: highlight alpha differs
	}});
	script_.push_back({"focus nav again", [this] { for (int i = 0; i < 16 && focused().rfind("controls.", 0) != 0; ++i) runtime_.keyPressed('\t'); }, 8, [this] {
		const auto& r = runtime_.hudBridgeForTesting().rendererForTesting();
		check("focus back on universal navigation: emphasis restored to 1.0", !r.nestedFocusActive() && r.navigationEmphasis() == 1.0f, focused());
		capture("07_nav_normal_focus_on_navigation");
	}});
	script_.push_back({"orbit focus emphasis", [this] { for (int i = 0; i < 16 && focused() != "orbit.a"; ++i) runtime_.keyPressed('\t'); }, 8, [this] {
		const auto& r = runtime_.hudBridgeForTesting().rendererForTesting();
		check("focus on an Orbit observation (nested HUD region): navigation emphasis is 0.5", focused() == "orbit.a" && r.navigationEmphasis() == 0.5f, focused());
		capture("08_orbit_focus_nav_dimmed");
	}});
	script_.push_back({"debounce 2", [] {}, 60, nullptr});

	// Mode change while parameter focus is active: focus remains valid.
	script_.push_back({"to dither", [this] { goToEffect("dither"); }, 12, nullptr});
	script_.push_back({"setup mode-change-while-active", [this] { focusRegion(); }, 10, [this] { runtime_.keyPressed(OF_KEY_RETURN); }});
	// Re-entry resumes the previously selected row, so press Up first (clamps to row 0), then Down.
	script_.push_back({"go to row 2", [this] { runtime_.keyPressed(OF_KEY_UP); runtime_.keyPressed(OF_KEY_DOWN); }, 8, [this] {
		check("active on row 2 of a 2-control effect", phaseName() == "active" && runtime_.hudBridgeForTesting().interactiveRow() == 1);
		runtime_.keyPressed('m'); // dither (2) -> channelshift (1)
	}});
	script_.push_back({"after mode change", [] {}, 10, [this] {
		check("mode change did not steal or drop focus; effect advanced", scene().controllerForTesting().mode().effectId == "channelshift");
		check("stale row index repaired against a one-control list (row 0, still valid)", runtime_.hudBridgeForTesting().interactiveActive() && runtime_.hudBridgeForTesting().interactiveRow() == 0 && controlsShown() == 1,
			"row=" + ofToString(runtime_.hudBridgeForTesting().interactiveRow()) + " controls=" + ofToString(controlsShown()));
		for (int i = 0; i < 40 && scene().controllerForTesting().controlCount() != 0; ++i) runtime_.keyPressed('m'); // -> next 0-control mode (invert, after wrap)
	}});
	script_.push_back({"to zero-control mode", [] {}, 12, [this] {
		check("mode with zero controls: parameter focus exits (never trapped)", !runtime_.hudBridgeForTesting().interactiveActive() && controlsShown() == 0 && focused() != "interactive.region", phaseName() + "/" + focused());
	}});

	// Mode change with region NOT focused: no focus stolen.
	script_.push_back({"mode change unfocused", [this] { goToEffect("solarize"); for (int i = 0; i < 16 && focused() != "controls.menu"; ++i) runtime_.keyPressed('\t'); runtime_.keyPressed('m'); }, 10, [this] {
		check("mode change while the region is not focused does not steal focus", focused() == "controls.menu", focused());
	}});

	// Media / scene-switch hygiene.
	script_.push_back({"media next", [this] { runtime_.keyPressed('n'); }, 20, [this] {
		check("previous/next media still go through the canonical service (scene still the gallery, same effect)", idleOn(kFxId));
	}});
	script_.push_back({"to recolor", [this] { goToEffect("recolor"); }, 12, nullptr});
	script_.push_back({"away", [this] { focusRegion(); runtime_.keyPressed(OF_KEY_RETURN); }, 8, [this] {
		check("active in the gallery before switching away", phaseName() == "active", phaseName());
		runtime_.keyPressed(']'); // gallery is last: wraps to the first scene
	}});
	script_.push_back({"away settle", [] {}, 100, [this] {
		check("switched away: no active region, no stale focus, no gallery metrics in the other scene's HUD", !runtime_.hudBridgeForTesting().interactiveActive() && focused() != "interactive.region" && metric("scene.fx.metric.c0_label").empty(), phaseName() + "/" + focused());
		runtime_.keyPressed('[');
	}});
	script_.push_back({"back settle", [] {}, 120, [this] {
		check("back in the gallery: scene idle, region not active, effect preserved", idleOn(kFxId) && !runtime_.hudBridgeForTesting().interactiveActive() && scene().controllerForTesting().mode().effectId == "recolor");
		check("repeated activate/deactivate counted by the scene", scene().activationCountForTesting() >= 2, ofToString(scene().activationCountForTesting()));
		check("instance for the active effect is ready (shader loaded)", scene().instanceReadyForTesting());
		capture("09_after_return");
		ofLogNotice("FxGalleryHarness") << "RESULT: " << passes_ << " passed, " << failures_ << " failed";
		done_ = true;
		ofExit(failures_ == 0 ? 0 : 1);
	}});
}

void FxGalleryHarness::step(float dt) {
	(void)dt;
	if (done_ || at_ >= script_.size()) return;
	frame_++;
	Step& s = script_[at_];
	if (!acted_) {
		if (at_ == 0 && !(idleOn(kFxId) && frame_ > 150)) {
			if (frame_ > 2400) { check("gallery scene became active", false, "timeout"); done_ = true; ofExit(1); }
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
