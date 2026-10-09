#include "CadenceHarness.h"

#include "ofAppRunner.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;
const std::string kBlueprintId = BlueprintProductionScene::kSceneId;
constexpr const char* kCaptureDir = "captures/cadence";

const char* originName(MediaSelectionOrigin o) {
	switch (o) {
		case MediaSelectionOrigin::Startup: return "startup";
		case MediaSelectionOrigin::Automatic: return "automatic";
		case MediaSelectionOrigin::ManualNext: return "manual_next";
		case MediaSelectionOrigin::ManualPrevious: return "manual_previous";
		case MediaSelectionOrigin::Recovery: return "recovery";
	}
	return "?";
}

} // namespace

bool CadenceHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_CADENCE_HARNESS") != nullptr;
}

CadenceHarness::CadenceHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	buildScript();
}

void CadenceHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void CadenceHarness::note(const std::string& line) {
	ofLogNotice("CadenceHarness") << "[t=" << ofToString(clock_, 1) << "s] " << line;
	lines_.push_back("[t=" + ofToString(clock_, 1) + "s] " + line);
}

void CadenceHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png");
}

bool CadenceHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId
		&& f.scene.sceneId == sceneId;
}

std::string CadenceHarness::media() const {
	return runtime_.videoServiceForTesting().status().mediaId.value_or("(none)");
}

const BEComposition& CadenceHarness::blueprint() const {
	return const_cast<ExperienceRuntime&>(runtime_).blueprintSceneForTesting().coreForTesting().composition();
}

int CadenceHarness::blueprintSeed() const {
	return blueprint().getCycleSeed();
}

int CadenceHarness::blueprintCycleStarts() const {
	return const_cast<ExperienceRuntime&>(runtime_).blueprintSceneForTesting().coreForTesting().cycleStartCount();
}

// Every frame (after runtime.update): attribute each canonical media change.
void CadenceHarness::monitor() {
	const std::string cur = media();
	const int seed = blueprintSeed();
	// A cycle start = BEComposition's cycle-start callback: full startCycle()
	// (new seed) or a ghost-layers mini-cycle entry (same seed) — standalone
	// switches video at both.
	const int cycleStarts = blueprintCycleStarts();
	const bool cycleRestarted = cycleStarts != lastCycleStarts_;
	if (!lastMedia_.empty() && cur != lastMedia_) {
		const HudFrameData& f = runtime_.currentHudFrameData();
		MediaChange c;
		c.t = clock_;
		c.scene = f.sceneManager.transitionPhase == SceneTransitionPhase::Idle ? f.sceneManager.activeSceneId : "transition";
		c.origin = runtime_.videoServiceForTesting().status().selectionOrigin;
		c.cycleRestartedSameFrame = cycleRestarted;
		changes_.push_back(c);
		note("media change: " + lastMedia_ + " -> " + cur + " during " + c.scene + " (" + originName(c.origin) + ")" +
			(c.scene == kBlueprintId ? std::string(cycleRestarted ? " at a new composition cycle" : " inside the cycle") : ""));
		if (c.scene == kBlueprintId && c.origin == MediaSelectionOrigin::Automatic && !cycleRestarted) violations_++;
	}
	lastMedia_ = cur;
	lastSeed_ = seed;
	lastCycleStarts_ = cycleStarts;
}

int CadenceHarness::changesSince(size_t from, const std::string& scene, bool automaticOnly) const {
	int n = 0;
	for (size_t i = from; i < changes_.size(); ++i) {
		if (changes_[i].scene != scene) continue;
		if (automaticOnly && changes_[i].origin != MediaSelectionOrigin::Automatic) continue;
		n++;
	}
	return n;
}

void CadenceHarness::buildScript() {
	auto& video = runtime_.videoServiceForTesting();
	auto holdElapsed = [&video]() { return video.status().holdElapsedSeconds.value_or(-1.0f); };

	script_ = {
		// -- Blob: automatic cadence ----------------------------------------
		{"blob live", [this] { return idleOn(kBlobId) && framesInStep_ >= 30; }, [this, &video] {
			check("Blob: timer-driven automatic advance on", video.automaticAdvanceEnabled());
			mark_ = changes_.size();
		}},
		{"blob automatic advance", [this] { return changesSince(mark_, kBlobId, true) >= 1; }, [this] {
			check("Blob: an automatic (dwell-timer) media change happened", true);
			mediaAtMark_ = media();
			runtime_.keyPressed('['); // Blob -> Blueprint (ring wraps backward)
		}, 180.0f},

		// -- Blueprint visit 1: cycle-driven cadence -------------------------------
		{"blueprint live", [this] { return idleOn(kBlueprintId) && framesInStep_ >= 2; }, [this, &video] {
			check("Blueprint: timer-driven auto-advance suspended", !video.automaticAdvanceEnabled());
			check("Blueprint activation kept the current media (no change for becoming active)", media() == mediaAtMark_,
				mediaAtMark_ + " / " + media());
			mark_ = changes_.size();
			cycleStartSeed_ = blueprintSeed();
			cycleStartsAtMark_ = blueprintCycleStarts();
			cycleStartClock_ = clock_;
			capture("c01_blueprint_cycle_start");
		}},
		{"blueprint natural cycle boundary", [this] { return blueprintCycleStarts() != cycleStartsAtMark_; }, [this] {
			note("cycle boundary type: " + std::string(blueprintSeed() != cycleStartSeed_ ? "full startCycle()" : "ghost-layers mini-cycle"));
			const float cycleSeconds = clock_ - cycleStartClock_;
			const int automatic = changesSince(mark_, kBlueprintId, true);
			const int all = changesSince(mark_, kBlueprintId, false);
			const bool atBoundary = !changes_.empty() && changes_.back().cycleRestartedSameFrame
				&& changes_.back().origin == MediaSelectionOrigin::Automatic;
			check("Blueprint: no media change inside the cycle, exactly one at the new cycle",
				automatic == 1 && all == 1 && atBoundary,
				"cycle " + ofToString(cycleSeconds, 1) + " s, " + ofToString(all) + " change(s)");
			const auto s = runtime_.videoServiceForTesting().status();
			check("HUD media status follows the boundary change", s.mediaId.value_or("") == media() && s.fallbackDisplayTitle.has_value(),
				s.fallbackDisplayTitle.value_or("(no title)"));
			capture("c02_blueprint_after_cycle_boundary");
		}, 900.0f},
		{"blueprint reseed", [this] { return framesInStep_ >= 30; }, [this] {
			mark_ = changes_.size();
			seedAtMark_ = blueprintSeed();
			mediaAtMark_ = media();
			runtime_.keyPressed('2'); // RESEED -> SceneCommand::Regenerate -> restartCycle()
		}},
		{"after reseed", [this] { return framesInStep_ >= 5; }, [this] {
			check("RESEED starts a new cycle and, as standalone 'r', one boundary media change",
				blueprintSeed() != seedAtMark_ && changesSince(mark_, kBlueprintId, true) == 1 && media() != mediaAtMark_);
		}},
		{"manual next", [this] { return framesInStep_ >= 30; }, [this] {
			seedAtMark_ = blueprintSeed();
			mediaAtMark_ = media();
			runtime_.keyPressed('n');
		}},
		{"after manual next", [this] { return framesInStep_ >= 10; }, [this] {
			check("manual Next during Blueprint: immediate, cycle continues",
				media() != mediaAtMark_ && changes_.back().origin == MediaSelectionOrigin::ManualNext && blueprintSeed() == seedAtMark_);
			runtime_.keyPressed('p');
		}},
		{"after manual previous", [this] { return framesInStep_ >= 10; }, [this] {
			check("manual Previous during Blueprint: back to the prior media, cycle continues",
				media() == mediaAtMark_ && changes_.back().origin == MediaSelectionOrigin::ManualPrevious && blueprintSeed() == seedAtMark_);
			capture("c03_blueprint_after_manual_navigation");
			runtime_.keyPressed('['); // Blueprint -> Temporal
		}},

		// -- Temporal: automatic cadence restored, fresh window ----------------------
		{"temporal live", [this] { return idleOn(kTemporalId) && framesInStep_ >= 2; }, [this, &video, holdElapsed] {
			check("Temporal: automatic advance restored", video.automaticAdvanceEnabled());
			check("Temporal: fresh dwell window (no overdue timer from Blueprint)", holdElapsed() >= 0.0f && holdElapsed() < 5.0f,
				"hold elapsed " + ofToString(holdElapsed(), 2) + " s");
			mark_ = changes_.size();
			mediaAtMark_ = media();
		}},
		{"temporal settled", [this] { return secondsInStep_ >= 5.0f; }, [this] {
			check("Temporal: no immediate automatic advance after Blueprint", changesSince(mark_, kTemporalId, true) == 0);
			check("Temporal follows canonical media",
				runtime_.temporalSceneForTesting().videoAdapterForTesting().loadedMediaId() == media());
			capture("c04_temporal");
			mediaAtMark_ = media();
			runtime_.keyPressed(']'); // Temporal -> Blueprint
		}},

		// -- Blueprint visit 2: suppressed again ----------------------------------------
		{"blueprint again live", [this] { return idleOn(kBlueprintId) && framesInStep_ >= 2; }, [this, &video] {
			check("Blueprint again: auto-advance suspended again", !video.automaticAdvanceEnabled());
			check("Blueprint again: activation kept the current media", media() == mediaAtMark_);
			mark_ = changes_.size();
		}},
		{"blueprint again hold", [this] { return secondsInStep_ >= 45.0f; }, [this] {
			const int inCycle = changesSince(mark_, kBlueprintId, true);
			check("Blueprint again: 45 s (> the 30 s timer) without a timer-driven change",
				violations_ == 0, ofToString(inCycle) + " cycle-boundary change(s) in that time");
			mediaAtMark_ = media();
			runtime_.keyPressed(']'); // Blueprint -> Blob
		}, 120.0f},

		// -- Blob again: restored -----------------------------------------------------------
		{"blob again live", [this] { return idleOn(kBlobId) && framesInStep_ >= 2; }, [this, &video, holdElapsed] {
			check("Blob again: automatic advance restored", video.automaticAdvanceEnabled());
			check("Blob again: fresh dwell window", holdElapsed() >= 0.0f && holdElapsed() < 5.0f,
				"hold elapsed " + ofToString(holdElapsed(), 2) + " s");
			mark_ = changes_.size();
		}},
		{"blob again no overdue", [this] { return secondsInStep_ >= 5.0f; }, [this] {
			check("Blob again: no immediate overdue advance", changesSince(mark_, kBlobId, true) == 0);
		}},
		{"blob again automatic", [this] { return changesSince(mark_, kBlobId, true) >= 1; }, [this] {
			check("Blob again: automatic cadence works", true);
			capture("c05_blob_restored");
		}, 180.0f},
	};
}

void CadenceHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	clock_ += dt;
	framesInStep_++;
	secondsInStep_ += dt;
	monitor();

	Step& s = script_[stepIndex_];
	if (s.ready()) {
		s.act();
		if (done_) return;
		stepIndex_++;
		framesInStep_ = 0;
		secondsInStep_ = 0.0f;
		if (stepIndex_ >= script_.size()) finish();
	} else if (secondsInStep_ > s.timeoutSeconds) {
		check("step '" + s.label + "' reached", false, "timed out after " + ofToString(s.timeoutSeconds, 0) + " s");
		finish();
	}
}

void CadenceHarness::finish() {
	check("no timer-driven Blueprint media change anywhere in the run", violations_ == 0, ofToString(violations_) + " violation(s)");
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
