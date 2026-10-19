#include "InstallationHarness.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;
const std::string kBlueprintId = BlueprintProductionScene::kSceneId;
const std::string kFragmentId = FragmentProductionScene::kSceneId;
const std::string kQuadrantId = QuadrantProductionScene::kSceneId;
constexpr const char* kCaptureDir = "captures/installation";
constexpr float kSceneCycleSeconds = 10.0f; // the validated minimum
constexpr float kMediaCycleSeconds = 5.0f;
bool g_manualSwitch = false;  // a switch the harness itself requested
bool g_samplingSpeed = false;

} // namespace

bool InstallationHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_INSTALLATION_HARNESS") != nullptr;
}

InstallationHarness::InstallationHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	const auto ids = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
	check("full roster: all five production scenes registered", ids.size() == 5, ofToString(ids.size()));
	buildScript();
}

void InstallationHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void InstallationHarness::note(const std::string& line) {
	ofLogNotice("InstallationHarness") << "[t=" << ofToString(clock_, 1) << "s] " << line;
	lines_.push_back("[t=" + ofToString(clock_, 1) + "s] " + line);
}

void InstallationHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png (" + ofToString(img.getWidth()) + "x"
		+ ofToString(img.getHeight()) + ")");
}

bool InstallationHarness::idle() const {
	return runtime_.currentHudFrameData().sceneManager.transitionPhase == SceneTransitionPhase::Idle;
}

bool InstallationHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return idle() && f.sceneManager.activeSceneId == sceneId && f.scene.sceneId == sceneId;
}

std::string InstallationHarness::activeScene() const { return runtime_.currentHudFrameData().sceneManager.activeSceneId; }

std::string InstallationHarness::mediaId() const {
	const auto& v = runtime_.currentHudFrameData().video;
	return (v && v->mediaId) ? *v->mediaId : std::string();
}

const InstallationStatus* InstallationHarness::status() const {
	const auto& s = runtime_.currentHudFrameData().installation;
	return s ? &(*s) : nullptr;
}

void InstallationHarness::request(installation::Request::Kind kind, bool flag, float value, std::optional<float> duration) {
	installation::Request r;
	r.kind = kind;
	r.flag = flag;
	r.value = value;
	r.durationSeconds = duration;
	const auto result = runtime_.submitInstallationRequest(r);
	if (!result.accepted) note("request rejected: " + result.code);
}

void InstallationHarness::buildScript() {
	using K = installation::Request::Kind;
	auto secondsAtLeast = [this](float s) { return [this, s] { return secondsInStep_ >= s; }; };
	auto keys = [this](int key, int count) { for (int i = 0; i < count; ++i) runtime_.keyPressed(key); };
	auto crosshairScale = [this] { return runtime_.quadrantSceneForTesting().coreForTesting().crosshair().getEnergySpeedScale(); };

	script_ = {
		// -- Scene auto-cycle -------------------------------------------------
		{"blob live", [this] { return idleOn(kBlobId) && framesInStep_ >= 60 && status() != nullptr; }, [this] {
			const InstallationStatus* s = status();
			check("installation status published in HudFrameData (defaults: scene off, media on, ENERGY 0.5, windowed)",
				!s->sceneAutoCycleEnabled && s->mediaAutoCycleEnabled && s->energyTarget == 0.5f && !s->fullscreen && !s->launchFullscreen);
			request(K::SetMediaAutoCycle, false); // isolate the scene test
			request(K::SetSceneAutoCycle, true, 0.0f, kSceneCycleSeconds);
			lastActivationClock_ = clock_;
			autoVisits_.clear();
		}},
		{"auto -> temporal", [this] { return idleOn(kTemporalId); }, [this] {
			check("scene auto-cycle advanced Blob -> Temporal after the 10 s period (idle time only)",
				lastActivationClock_ > 0.0f && autoVisits_.size() == 1, ofToString(autoVisits_.size()) + " auto switch(es)");
		}},
		{"manual mid-period", secondsAtLeast(4.0f), [this] {
			g_manualSwitch = true;
			manualSwitchClock_ = clock_;
			runtime_.keyPressed(']'); // manual navigation 4 s into the period
		}},
		{"manual -> blueprint", [this] { return idleOn(kBlueprintId); }, [this] {
			check("manual NextScene reached Blueprint", activeScene() == kBlueprintId);
			manualSwitchClock_ = clock_; // Blueprint arrival: the period must restart here
		}},
		{"auto -> fragment", [this] { return idleOn(kFragmentId); }, [this] {
			const float sinceBlueprint = clock_ - manualSwitchClock_;
			check("manual navigation reset the cadence: next automatic switch a full period after Blueprint arrived",
				sinceBlueprint >= kSceneCycleSeconds - 0.1f, ofToString(sinceBlueprint, 1) + " s on Blueprint");
		}},
		{"auto -> quadrant", [this] { return idleOn(kQuadrantId); }, [] {}},
		{"auto -> blob (wrap)", [this] { return idleOn(kBlobId); }, [this] {
			std::string seq;
			for (const auto& s : autoVisits_) seq += (seq.empty() ? "" : ",") + s;
			std::set<std::string> visited(autoVisits_.begin(), autoVisits_.end());
			visited.insert(kBlueprintId); // reached manually
			check("automation cycled through all five production scenes and wrapped to Blob", visited.size() == 5
				&& autoVisits_.back() == kBlobId, seq);
			request(K::SetSceneAutoCycle, false);
			watchSceneDisabled_ = true;
			switchesWhileDisabled_ = 0;
		}},
		{"scene automation disabled", secondsAtLeast(15.0f), [this] {
			check("disabled scene automation: no switch in 15 s", switchesWhileDisabled_ == 0 && activeScene() == kBlobId);
			watchSceneDisabled_ = false;
			// -- Media auto-cycle (independent) -----------------------------
			request(K::SetMediaAutoCycle, true, 0.0f, kMediaCycleSeconds);
			mediaChanges_ = 0;
		}},
		{"automatic media change", [this] { return mediaChanges_ >= 1; }, [this] {
			check("media auto-cycle changed media on its own cadence while the scene stayed put (independent of scene automation)",
				activeScene() == kBlobId && status()->mediaAutoAdvanceActive && status()->mediaAutoCycleSeconds == kMediaCycleSeconds);
			mediaBefore_ = mediaId();
			runtime_.keyPressed('n'); // manual next
		}},
		{"manual media change", [this] { return mediaId() != mediaBefore_; }, [this] {
			manualMediaClock_ = clock_;
			mediaChanges_ = 0;
		}},
		{"next automatic media change", [this] { return mediaChanges_ >= 1; }, [this] {
			const float since = clock_ - manualMediaClock_;
			check("manual media navigation reset the media cadence (next automatic change >= the dwell later)",
				since >= kMediaCycleSeconds - 0.1f, ofToString(since, 1) + " s after the manual change");
			request(K::SetMediaAutoCycle, false);
			watchMediaDisabled_ = true;
			mediaChangesWhileDisabled_ = 0;
		}},
		{"media automation disabled", secondsAtLeast(12.0f), [this] {
			check("disabled media automation: no change in 12 s, Shared Video timer off",
				mediaChangesWhileDisabled_ == 0 && !status()->mediaAutoAdvanceActive);
			watchMediaDisabled_ = false;
			g_manualSwitch = true;
			runtime_.keyPressed('['); // Blob -> Quadrant (ring wraps backwards)
		}},

		// -- ENERGY ------------------------------------------------------------
		{"quadrant live", [this] { return idleOn(kQuadrantId) && framesInStep_ >= 60; }, [this, keys] {
			focusBefore_ = runtime_.hudCompositorBridgeForTesting().focus().focused().value_or("none");
			runtime_.keyPressed('e');
			check("'e' opens ENERGY adjustment", status() && runtime_.installationForTesting().energyAdjusting());
			keys(OF_KEY_LEFT, 8); // 8 encoder detents down: 0.5 -> 0.1
			check("encoder steps adjusted ENERGY, not focus",
				std::fabs(runtime_.installationForTesting().settings().energyTarget - 0.1f) < 1e-4f
					&& runtime_.hudCompositorBridgeForTesting().focus().focused().value_or("none") == focusBefore_);
			runtime_.keyPressed(OF_KEY_RETURN);
			check("Enter closes ENERGY adjustment", !runtime_.installationForTesting().energyAdjusting());
		}},
		{"energy low settle", secondsAtLeast(2.5f), [this] { speedSum_ = 0.0; speedSamples_ = 0; g_samplingSpeed = true; }},
		{"energy low sample", secondsAtLeast(3.0f), [this, crosshairScale] {
			g_samplingSpeed = false;
			lowSpeed_ = speedSamples_ > 0 ? static_cast<float>(speedSum_ / speedSamples_) : 0.0f;
			check("ENERGY smoothed to the low target, Quadrant crosshair slowed",
				std::fabs(status()->energy - 0.1f) < 0.03f && std::fabs(crosshairScale() - 0.48f) < 0.03f,
				"energy " + ofToString(status()->energy, 3) + ", speed scale " + ofToString(crosshairScale(), 2) + ", mean speed "
					+ ofToString(lowSpeed_, 1));
			capture("i01_quadrant_energy_low");
		}},
		{"energy high", [this] { return framesInStep_ >= 2; }, [this, keys] {
			runtime_.keyPressed('e');
			keys(OF_KEY_RIGHT, 16); // 0.1 -> 0.9
			runtime_.keyPressed('e'); // 'e' also closes
		}},
		{"energy high settle", secondsAtLeast(2.5f), [this] { speedSum_ = 0.0; speedSamples_ = 0; g_samplingSpeed = true; }},
		{"energy high sample", secondsAtLeast(3.0f), [this, crosshairScale] {
			g_samplingSpeed = false;
			const float highSpeed = speedSamples_ > 0 ? static_cast<float>(speedSum_ / speedSamples_) : 0.0f;
			check("ENERGY smoothed to the high target within bounds", std::fabs(status()->energy - 0.9f) < 0.03f
				&& status()->energy <= 1.0f, ofToString(status()->energy, 3));
			check("Quadrant responds observably: crosshair at high ENERGY moves much faster than at low",
				highSpeed > lowSpeed_ * 2.0f, ofToString(lowSpeed_, 1) + " -> " + ofToString(highSpeed, 1) + " (scale "
					+ ofToString(crosshairScale(), 2) + ")");
			capture("i02_quadrant_energy_high");
			g_manualSwitch = true;
			runtime_.keyPressed(']'); // Quadrant -> Blob
		}},
		{"away on blob", [this] { return idleOn(kBlobId) && framesInStep_ >= 30; }, [this] {
			g_manualSwitch = true;
			runtime_.keyPressed('['); // back to Quadrant
		}},
		{"quadrant again", [this] { return idleOn(kQuadrantId) && framesInStep_ >= 30; }, [this, crosshairScale] {
			check("ENERGY continuous across scene switches; reapplied after Quadrant's activation reset",
				std::fabs(status()->energyTarget - 0.9f) < 1e-4f && std::fabs(status()->energy - 0.9f) < 0.03f
					&& std::fabs(crosshairScale() - 1.96f) < 0.05f,
				"energy " + ofToString(status()->energy, 3) + ", scale " + ofToString(crosshairScale(), 2));
			const auto& m = runtime_.currentHudFrameData().scene.mode;
			sceneBefore_ = m ? m->modeId : std::string();
			runtime_.keyPressed('m');
		}},
		{"mode still works", [this] { return framesInStep_ >= 3; }, [this] {
			const auto& m = runtime_.currentHudFrameData().scene.mode;
			check("NEXT MODE unaffected ('m' on Quadrant)", m && m->modeId != sceneBefore_, m ? m->modeId : "none");
		}},

		// -- Fullscreen ---------------------------------------------------------
		{"fullscreen on", [this] { return framesInStep_ >= 30; }, [this] {
			sceneBefore_ = activeScene();
			mediaBefore_ = mediaId();
			windowWidthBefore_ = ofGetWidth();
			runtime_.keyPressed('f'); // RuntimeCommand::ToggleFullscreen -> installation authority
		}},
		{"fullscreen observed", [this] { return ofGetWindowMode() == OF_FULLSCREEN && ofGetWidth() != windowWidthBefore_ && secondsInStep_ >= 2.0f; },
			[this] {
				const InstallationStatus* s = status();
				check("fullscreen: observed window mode and size changed; status reports it", s->fullscreen,
					ofToString(windowWidthBefore_) + " -> " + ofToString(ofGetWidth()) + "x" + ofToString(ofGetHeight()));
				check("fullscreen: scene and media continuous; launch preference untouched",
					activeScene() == sceneBefore_ && mediaId() == mediaBefore_ && !s->launchFullscreen);
				capture("i03_fullscreen");
				runtime_.keyPressed('f');
			}, 15.0f},
		{"windowed again", [this] {
				if (framesInStep_ % 30 == 1) note("leaving fullscreen: mode " + std::string(ofGetWindowMode() == OF_WINDOW ? "window" : "fullscreen")
					+ ", " + ofToString(ofGetWidth()) + "x" + ofToString(ofGetHeight()) + ", frame " + ofToString(framesInStep_));
				return ofGetWindowMode() == OF_WINDOW && secondsInStep_ >= 2.0f;
			}, [this] {
			check("windowed again at 1280x720, status updated", !status()->fullscreen && ofGetWidth() == 1280 && ofGetHeight() == 720,
				ofToString(ofGetWidth()) + "x" + ofToString(ofGetHeight()));
			check("scene still continuous after leaving fullscreen", activeScene() == sceneBefore_);
			capture("i04_windowed_restored");
		}, 15.0f},
	};
}

void InstallationHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	clock_ += dt;
	framesInStep_++;
	secondsInStep_ += dt;

	// Scene changes (on settled frames): automatic unless the harness asked.
	if (idle()) {
		const std::string scene = activeScene();
		if (!lastScene_.empty() && scene != lastScene_) {
			if (watchSceneDisabled_) switchesWhileDisabled_++;
			if (!g_manualSwitch) autoVisits_.push_back(scene);
			lastActivationClock_ = clock_;
			g_manualSwitch = false;
		}
		lastScene_ = scene;
	}
	const std::string media = mediaId();
	if (!media.empty() && media != lastMedia_) {
		if (!lastMedia_.empty()) {
			mediaChanges_++;
			if (watchMediaDisabled_) mediaChangesWhileDisabled_++;
		}
		lastMedia_ = media;
	}
	if (g_samplingSpeed) {
		speedSum_ += runtime_.quadrantSceneForTesting().coreForTesting().crosshair().getState().speed;
		speedSamples_++;
	}

	Step& s = script_[stepIndex_];
	if (s.ready()) {
		s.act();
		if (done_) return;
		stepIndex_++;
		framesInStep_ = 0;
		secondsInStep_ = 0.0f;
		if (stepIndex_ >= script_.size()) finish();
	} else if (secondsInStep_ > s.timeoutSeconds) {
		check("step '" + s.label + "' reached", false, "timed out");
		finish();
	}
}

void InstallationHarness::finish() {
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	if (ofGetWindowMode() == OF_FULLSCREEN) ofSetFullscreen(false);
	ofExit(failures_ == 0 ? 0 : 1);
}
