#include "PhosphorContoursHarness.h"

#include "PhosphorContoursScene.h"

#include "ofMain.h"

#include <cstdlib>
#include <filesystem>

namespace {
constexpr const char* kCaptureDir = "captures/phosphor-contours";
const std::string kPhcId = PhosphorContoursScene::kSceneId;
}

bool PhosphorContoursHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_HARNESS") != nullptr;
}

PhosphorContoursHarness::PhosphorContoursHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	if (const char* m = std::getenv("EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_HARNESS_MEDIA")) seekMedia_ = m;
	const auto ids = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
	check("production roster: phosphor-contours is the 8th scene after the gallery", ids.size() >= 8 && ids[7] == kPhcId, "count=" + ofToString(ids.size()));
	// Raw Video is ALWAYS the last scene of the Production roster (convention in ExperienceRuntime::installProductionScenes).
	check("production roster: raw-video is the last scene", !ids.empty() && ids.back() == RawVideoScene::kSceneId, ids.empty() ? "empty" : "last=" + ids.back());
}

void PhosphorContoursHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " - " + detail));
}
void PhosphorContoursHarness::note(const std::string& line) { ofLogNotice("PhosphorContoursHarness") << line; }

bool PhosphorContoursHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId && f.scene.sceneId == sceneId;
}
std::string PhosphorContoursHarness::metricValue(const std::string& metricId) const {
	const auto& sem = runtime_.currentHudFrameData().scene.semantic;
	if (!sem) return std::string();
	for (const auto& m : sem->metrics) if (m.metricId == metricId && m.valueId) return *m.valueId;
	return std::string();
}
int PhosphorContoursHarness::modeIndex() const { const auto& m = runtime_.currentHudFrameData().scene.mode; return m ? m->index : -1; }
std::string PhosphorContoursHarness::mediaId() const { return runtime_.videoServiceForTesting().status().mediaId.value_or(std::string()); }

void PhosphorContoursHarness::capture(const std::string& name) {
	const char* tagEnv = std::getenv("EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_TAG");
	const std::string tag = tagEnv ? std::string(tagEnv) + "_" : std::string();
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	const std::string path = std::string(kCaptureDir) + "/" + tag + name + ".png";
	img.save(path);
	note("capture: bin/data/" + path + " " + ofToString(img.getWidth()) + "x" + ofToString(img.getHeight()) + " mode " + ofToString(modeIndex() + 1) + "/4 media '" + mediaId() + "'");
}

void PhosphorContoursHarness::step(float dt) {
	(void)dt;
	if (phase_ == 99) return;
	frame_++;
	PhosphorContoursScene& scene = runtime_.phosphorContoursSceneForTesting();
	switch (phase_) {
	case 0: // settled in the scene
		if (idleOn(kPhcId) && frame_ > 150) {
			const HudFrameData& f = runtime_.currentHudFrameData();
			check("starts in phosphor-contours, health Ready", f.scene.health == SceneHealth::Ready, "health=" + ofToString((int)f.scene.health));
			check("shaders compiled; targets allocated at native 1280x720", scene.pipelineUsableForTesting() && scene.pipelineAllocatedForTesting() && scene.pipelineWidthForTesting() == 1280);
			check("mode 1/4 (TOPOLOGY) published as the scene mode", modeIndex() == 0 && f.scene.mode && f.scene.mode->count == 4 && f.scene.mode->displayName == "TOPOLOGY");
			scene.enableGlStateProbeForTesting();
			check("TOPOLOGY selects the Hybrid structure algorithm", scene.algorithmForTesting() == phc::Algorithm::Hybrid);
			{
				const float lo = scene.pipelineForTesting().rangeLo(), hi = scene.pipelineForTesting().rangeHi();
				check("auto tonal range measured (span >= 0.2, inside [0,1])", hi - lo >= 0.199f && lo >= 0.f && hi <= 1.001f, ofToString(lo, 2) + ".." + ofToString(hi, 2));
			}
			check("selector rows published: DENSITY MEDIUM 2/3, DECAY TRAIL 2/3", metricValue("scene.contours.metric.c0_label") == "DENSITY" && metricValue("scene.contours.metric.c0_value") == "MEDIUM" && metricValue("scene.contours.metric.c0_pos") == "2/3"
				&& metricValue("scene.contours.metric.c1_label") == "DECAY" && metricValue("scene.contours.metric.c1_value") == "TRAIL" && metricValue("scene.contours.metric.c1_pos") == "2/3");
			check("scene advertises exactly two option axes (DENSITY, DECAY) of 3 steps", runtime_.sceneManagerForTesting().activeOptionAxisCount() == 2);
			check("media title comes from the canonical video snapshot", f.scene.mediaName.has_value());
			capture("01_reference_dense_a");
			mediaBefore_ = mediaId(); clearsBefore_ = scene.historyClearsForTesting();
			runtime_.keyPressed('n'); // canonical NEXT MEDIA
			phase_ = 1; frame_ = 0;
		}
		if (frame_ > 1800) { check("phosphor-contours became active", false, "timeout"); phase_ = 99; ofExit(1); }
		break;
	case 1: // media change -> different content, history cleared
		if (frame_ >= 120) {
			check("NEXT MEDIA changed the canonical media id", mediaId() != mediaBefore_, mediaBefore_ + " -> " + mediaId());
			check("history cleared on media change", scene.historyClearsForTesting() > clearsBefore_, "clears " + ofToString(clearsBefore_) + " -> " + ofToString(scene.historyClearsForTesting()));
			capture("02_reference_dense_other_media");
			runtime_.keyPressed('m'); phase_ = 2; frame_ = 0; k_ = 1;
		}
		break;
	case 2: // NEXT MODE x4 -> 2/4, 3/4, 4/4, wraps 1/4
		if (frame_ >= 40) {
			const int want = k_ % 4;
			check("NEXT MODE #" + ofToString(k_) + " -> mode " + ofToString(want + 1) + "/4 (wraps)", modeIndex() == want, "idx=" + ofToString(modeIndex()));
			check("mode " + ofToString(want + 1) + " selects algorithm " + std::string(phc::algorithmName(scene.algorithmForTesting())),
				scene.algorithmForTesting() == ((want == 1 || want == 3) ? phc::Algorithm::Isophote : (want == 2 ? phc::Algorithm::RasterRelief : phc::Algorithm::Hybrid)));
			if (k_ == 1) capture("03_isolines");
			if (k_ == 2) capture("04_scanlines");
			if (k_ == 3) capture("04b_strata");
			if (k_ >= 4) { phase_ = 3; frame_ = 0; runtime_.keyPressed('2'); break; }
			k_++; runtime_.keyPressed('m'); frame_ = 0;
		}
		break;
	case 3: // selector axes through the production port, then reseed + reset
		if (frame_ == 1) {
			SceneManager& sm = runtime_.sceneManagerForTesting();
			bool ok = sm.dispatchSceneOption(0, +1);          // DENSITY MEDIUM -> DENSE
			const int d1 = scene.densityForTesting(); const float lv1 = scene.lookForTesting().levels;
			ok = sm.dispatchSceneOption(0, +1) && ok;         // DENSE -> SPARSE (wraps)
			const int d2 = scene.densityForTesting(); const float lv2 = scene.lookForTesting().levels;
			ok = sm.dispatchSceneOption(1, -1) && ok;         // DECAY TRAIL -> CRISP
			const int c1 = scene.decayForTesting(); const float tau1 = scene.lookForTesting().persistenceTau;
			ok = sm.dispatchSceneOption(1, -1) && ok;         // CRISP -> SMEAR (wraps)
			const int c2 = scene.decayForTesting(); const float tau2 = scene.lookForTesting().persistenceTau;
			check("option axes step through SceneManager::dispatchSceneOption", ok);
			check("DENSITY wraps MEDIUM->DENSE->SPARSE and scales line count (dense > sparse)", d1 == 2 && d2 == 0 && lv1 > lv2, "levels " + ofToString(lv1) + " vs " + ofToString(lv2));
			check("DECAY wraps TRAIL->CRISP->SMEAR and scales persistence (smear > crisp)", c1 == 0 && c2 == 2 && tau2 > tau1, "tau " + ofToString(tau1) + " vs " + ofToString(tau2));
			check("invalid axis rejected", !sm.dispatchSceneOption(2, +1));
		}
		if (frame_ == 12) {
			check("HUD rows show SPARSE 1/3 and SMEAR 3/3", metricValue("scene.contours.metric.c0_value") == "SPARSE" && metricValue("scene.contours.metric.c0_pos") == "1/3"
				&& metricValue("scene.contours.metric.c1_value") == "SMEAR" && metricValue("scene.contours.metric.c1_pos") == "3/3");
			capture("05b_sparse_smear");
		}
		if (frame_ >= 20) {
			check("Regenerate reseeds (seed 1 -> 2)", scene.seedForTesting() == 2, "seed=" + ofToString(scene.seedForTesting()));
			runtime_.keyPressed('1');
			phase_ = 4; frame_ = 0;
		}
		break;
	case 4:
		if (frame_ >= 10) {
			check("Reset returns to mode 1/4, DENSITY MEDIUM, DECAY TRAIL, seed 1", modeIndex() == 0 && scene.seedForTesting() == 1 && scene.densityForTesting() == 1 && scene.decayForTesting() == 1, "idx=" + ofToString(modeIndex()) + " seed=" + ofToString(scene.seedForTesting()));
			runtime_.keyPressed(']'); phase_ = 5; frame_ = 0;
		}
		break;
	case 5: // away: GPU targets released while inactive
		if (idleOn(runtime_.currentHudFrameData().sceneManager.activeSceneId) && runtime_.currentHudFrameData().sceneManager.activeSceneId != kPhcId && frame_ > 30) {
			check("targets released while another scene is active", !scene.pipelineAllocatedForTesting());
			clearsBefore_ = scene.historyClearsForTesting();
			runtime_.keyPressed('['); phase_ = 6; frame_ = 0;
		}
		if (frame_ > 1800) { check("scene switch away", false, "timeout"); phase_ = 99; ofExit(1); }
		break;
	case 6: // back: reallocated + history cleared
		if (idleOn(kPhcId) && frame_ > 60) {
			check("re-entry re-allocates targets and clears history", scene.pipelineAllocatedForTesting() && scene.historyClearsForTesting() > clearsBefore_);
			check("health Ready after re-entry", runtime_.currentHudFrameData().scene.health == SceneHealth::Ready);
			capture("05_after_reentry");
			scene.resizeForTesting(960, 540);
			phase_ = 7; frame_ = 0;
		}
		if (frame_ > 1800) { check("scene re-entry", false, "timeout"); phase_ = 99; ofExit(1); }
		break;
	case 7: // resize at runtime, then restore; GPU timing
		if (frame_ == 10) {
			check("resize to 960x540 reallocated the pipeline", scene.pipelineWidthForTesting() == 960 && scene.nativeRenderSize().x == 960);
			scene.resizeForTesting(1280, 720);
			scene.pipelineForTesting().setPassTiming(true);
		}
		if (frame_ > 30) { gpuMsSum_ += scene.pipelineForTesting().totalMs(); gpuMsN_++; }
		if (frame_ >= 200) {
			scene.pipelineForTesting().setPassTiming(false);
			check("restored 1280x720", scene.pipelineWidthForTesting() == 1280);
			note("GPU pipeline cost at 1280x720 (glFinish-bracketed, EMA): " + ofToString(gpuMsSum_ / std::max(1, gpuMsN_), 2) + " ms/frame on this desktop GPU (NOT Pi)");
			phase_ = 40;
			frame_ = 0;
		}
		break;
	case 40: // shader failure: scene degrades honestly (untreated video), then recovers when shaders reload
		if (frame_ == 1) scene.pipelineForTesting().failShadersForTesting();
		if (frame_ == 20) {
			const HudFrameData& f = runtime_.currentHudFrameData();
			check("simulated shader failure -> health Degraded with a message", f.scene.health == SceneHealth::Degraded && f.scene.message.has_value(), "health=" + ofToString((int)f.scene.health));
			check("simulated shader failure -> pipeline unusable, scene still drawing (no crash)", !scene.pipelineUsableForTesting());
			capture("07_shader_failure_fallback");
			scene.pipelineForTesting().loadShaders();
		}
		if (frame_ == 40) {
			check("shaders reloaded -> health Ready again", runtime_.currentHudFrameData().scene.health == SceneHealth::Ready && scene.pipelineUsableForTesting());
			check("no GL state leak: blend/scissor/depth/viewport/FBO/program identical before and after every scene draw (" + ofToString(scene.glProbeSamplesForTesting()) + " frames probed)",
				scene.glProbeSamplesForTesting() > 100 && scene.glStateMismatchesForTesting() == 0, "mismatches=" + ofToString(scene.glStateMismatchesForTesting()));
			phase_ = seekMedia_.empty() ? 8 : 30;
			frame_ = 0;
		}
		break;
	case 30: { // optional: step NEXT MEDIA (canonical) until the title contains EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_HARNESS_MEDIA
		if (frame_ >= 150) {
			const auto& nm = runtime_.currentHudFrameData().scene.mediaName;
			if (nm && nm->find(seekMedia_) != std::string::npos) {
				capture("06_media_" + seekMedia_);
				check("sought media is shown through the HUD viewport: " + *nm, true);
				phase_ = 8;
			} else if (++seekTries_ > 30) {
				check("sought media '" + seekMedia_ + "' found within 30 NEXT MEDIA steps", false);
				phase_ = 8;
			} else {
				runtime_.keyPressed('n');
				frame_ = 0;
			}
		}
		break;
	}
	case 8:
		note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
		phase_ = 99;
		ofExit(failures_ == 0 ? 0 : 1);
		break;
	}
}
