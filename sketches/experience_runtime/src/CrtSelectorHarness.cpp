#include "CrtSelectorHarness.h"

#include "CrtPhosphorScene.h"

#include "ofMain.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace {
constexpr const char* kCaptureDir = "captures/crt-selectors";
const std::string kCrtId = CrtPhosphorScene::kSceneId;
}

bool CrtSelectorHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_CRT_SELECTOR_HARNESS") != nullptr;
}

CrtSelectorHarness::CrtSelectorHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	const auto ids = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
	check("production roster: crt-phosphor is the 6th of 7 scenes (gallery last)", ids.size() == 7 && ids[5] == kCrtId, "count=" + ofToString(ids.size()));
}

void CrtSelectorHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " - " + detail));
}

void CrtSelectorHarness::note(const std::string& line) { ofLogNotice("CrtSelectorHarness") << line; }

bool CrtSelectorHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId && f.scene.sceneId == sceneId;
}
int CrtSelectorHarness::modeIndex() const { const auto& m = runtime_.currentHudFrameData().scene.mode; return m ? m->index : -1; }
int CrtSelectorHarness::modeCount() const { const auto& m = runtime_.currentHudFrameData().scene.mode; return m ? m->count : 0; }
std::string CrtSelectorHarness::modeId() const { const auto& m = runtime_.currentHudFrameData().scene.mode; return m ? m->modeId : std::string(); }
std::string CrtSelectorHarness::metric(const char* id) const {
	const auto& sem = runtime_.currentHudFrameData().scene.semantic;
	if (!sem) return {};
	for (const auto& m : sem->metrics) if (m.metricId == id && m.valueId) return *m.valueId;
	return {};
}

void CrtSelectorHarness::capture(const std::string& name) {
	const char* tagEnv = std::getenv("EXPERIENCE_RUNTIME_CRT_SELECTOR_TAG");
	const std::string tag = tagEnv ? std::string(tagEnv) + "_" : std::string();
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	const std::string path = std::string(kCaptureDir) + "/" + tag + name + ".png";
	img.save(path);
	// Lit-text extent inside the effect_summary band (canvas px; 1280x720 windowed = 1:1).
	const ofPixels& px = img.getPixels();
	int x0 = 99999, x1 = -1, y0 = 99999, y1 = -1;
	for (int y = 212; y < 264 && y < (int)px.getHeight(); y++) {
		for (int x = 845; x < 1255 && x < (int)px.getWidth(); x++) {
			const ofColor c = px.getColor(x, y);
			if (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b > 150.f) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
		}
	}
	note("capture: bin/data/" + path + " (" + ofToString(img.getWidth()) + "x" + ofToString(img.getHeight()) + ") mode " + modeId() + " "
		+ ofToString(modeIndex() + 1) + "/" + ofToString(modeCount()) + " structure " + metric("scene.crt.metric.structure") + " (" + metric("scene.crt.metric.structure_pos")
		+ ") palette " + metric("scene.crt.metric.palette") + " (" + metric("scene.crt.metric.palette_pos") + ") | lit text bbox x[" + ofToString(x0) + "," + ofToString(x1)
		+ "] y[" + ofToString(y0) + "," + ofToString(y1) + "] in band x[845,1255) y[212,264)");
}

void CrtSelectorHarness::step(float dt) {
	if (phase_ >= 9) return;
	frame_++;
	if (phase_ == 3 || phase_ == 4) { frameMsSum_ += dt * 1000.0; frameMsN_++; }
	switch (phase_) {
	case 0: // wait until CRT is the settled active scene with a published mode + semantic payload
		if (idleOn(kCrtId) && modeCount() > 0 && !metric("scene.crt.metric.structure").empty() && frame_ > 150) {
			structureAtStart_ = metric("scene.crt.metric.structure");
			paletteAtStart_ = metric("scene.crt.metric.palette");
			check("starts in crt-phosphor with composition 1/4 (4 implemented modes, planned hidden)", modeIndex() >= 0 && modeCount() == 4, "idx=" + ofToString(modeIndex()) + " count=" + ofToString(modeCount()));
			check("structure row published with a position", !metric("scene.crt.metric.structure_pos").empty(), metric("scene.crt.metric.structure_pos"));
			check("palette row published with a position", !metric("scene.crt.metric.palette_pos").empty(), metric("scene.crt.metric.palette_pos"));
			capture("01_initial");
			phase_ = 1; frame_ = 0;
		}
		if (frame_ > 1800) { check("CRT scene became active", false, "timeout"); phase_ = 9; ofExit(1); }
		break;
	case 1: // NEXT MODE x8 through the real 'm' key path
		if (!sent_) { runtime_.keyPressed('m'); sent_ = true; frame_ = 0; k_++; break; }
		if (frame_ >= 8) {
			const int want = k_ % 4;
			check("NEXT MODE #" + ofToString(k_) + " -> composition " + ofToString(want + 1) + "/4 (wraps)", modeIndex() == want && modeCount() == 4, "idx=" + ofToString(modeIndex()));
			check("composition step leaves structure/palette untouched (#" + ofToString(k_) + ")", metric("scene.crt.metric.structure") == structureAtStart_ && metric("scene.crt.metric.palette") == paletteAtStart_);
			check("planned modes unreachable (#" + ofToString(k_) + ")", modeId() != "contour_emission" && modeId() != "signal_collapse", modeId());
			if (k_ == 3) capture("02_composition_4of4");
			if (k_ == 4) capture("03_composition_wrapped_1of4");
			if (k_ == 2) capture("04_composition_3of4");
			sent_ = false;
			if (k_ >= 8) { phase_ = 2; frame_ = 0; k_ = 0; runtime_.keyPressed('m'); runtime_.keyPressed('m'); } // park on 3/4 before the switch test
		}
		break;
	case 2: // let the two parking presses settle, then leave the scene
		if (frame_ >= 8) { indexBeforeSwitch_ = modeIndex(); check("parked on composition 3/4", indexBeforeSwitch_ == 2, "idx=" + ofToString(indexBeforeSwitch_)); runtime_.keyPressed('['); phase_ = 3; frame_ = 0; }
		break;
	case 3: // away: CRT is inactive, HUD must show the other scene's data (no stale CRT labels)
		if (!runtime_.currentHudFrameData().sceneManager.activeSceneId.empty() && idleOn(runtime_.currentHudFrameData().sceneManager.activeSceneId)
			&& runtime_.currentHudFrameData().sceneManager.activeSceneId != kCrtId) {
			const std::string other = runtime_.currentHudFrameData().sceneManager.activeSceneId;
			check("switched away to '" + other + "'; no CRT metrics leak into its HUD frame", metric("scene.crt.metric.structure").empty(), "structure metric='" + metric("scene.crt.metric.structure") + "'");
			runtime_.keyPressed(']'); phase_ = 4; frame_ = 0;
		}
		if (frame_ > 1800) { check("scene switch away", false, "timeout"); phase_ = 9; ofExit(1); }
		break;
	case 4: // back: selection persisted (session state), rows republished
		if (idleOn(kCrtId) && frame_ > 90) {
			check("re-entry keeps the session selection (composition 3/4)", modeIndex() == indexBeforeSwitch_, "idx=" + ofToString(modeIndex()));
			check("re-entry republishes structure/palette rows unchanged", metric("scene.crt.metric.structure") == structureAtStart_ && metric("scene.crt.metric.palette") == paletteAtStart_);
			capture("05_after_reentry");
			runtime_.keyPressed('1'); // SceneCommand::Reset through the real path
			phase_ = 5; frame_ = 0;
		}
		if (frame_ > 1800) { check("scene re-entry", false, "timeout"); phase_ = 9; ofExit(1); }
		break;
	case 5:
		if (frame_ >= 8) {
			check("Reset returns composition to 1/4", modeIndex() == 0, "idx=" + ofToString(modeIndex()));
			capture("06_after_reset");
			note(std::string("desktop frame time over the switch window: mean ") + ofToString(frameMsN_ ? frameMsSum_ / frameMsN_ : 0.0, 2) + " ms over " + ofToString(frameMsN_) + " frames");
			note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
			phase_ = 9;
			ofExit(failures_ == 0 ? 0 : 1);
		}
		break;
	}
}
