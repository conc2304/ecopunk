#include "ResumeWindowHarness.h"

#include "ofAppRunner.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;
constexpr const char* kCaptureDir = "captures/resume-window";
constexpr float kStepTimeoutSeconds = 120.0f;
using Clip = VideoSelectionPolicy::ClipClass;

} // namespace

bool ResumeWindowHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_RESUME_HARNESS") != nullptr;
}

ResumeWindowHarness::ResumeWindowHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	buildScript();
}

void ResumeWindowHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void ResumeWindowHarness::note(const std::string& line) {
	ofLogNotice("ResumeWindowHarness") << line;
	lines_.push_back(line);
}

void ResumeWindowHarness::capture(const std::string& name) {
	ofPixels px;
	if (runtime_.readSceneFboPixelsForTesting(px) && px.getWidth() > 0) {
		double sum = 0.0;
		int n = 0;
		for (size_t y = 8; y < px.getHeight(); y += 16) {
			for (size_t x = 8; x < px.getWidth(); x += 16) {
				const ofColor c = px.getColor(x, y);
				sum += (c.r + c.g + c.b) / 3.0;
				n++;
			}
		}
		note(name + ": scene frame mean " + ofToString(n ? sum / n : 0.0, 1) + ", playhead " +
			ofToString(canonicalPosition(), 2) + " s");
	}
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png");
}

std::string ResumeWindowHarness::activeMedia() const {
	const auto s = runtime_.videoServiceForTesting().status();
	return s.mediaId.value_or("(none)");
}

float ResumeWindowHarness::canonicalPosition() const {
	return runtime_.videoServiceForTesting().status().positionSeconds.value_or(-1.0f);
}

bool ResumeWindowHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId
		&& f.scene.sceneId == sceneId;
}

void ResumeWindowHarness::buildScript() {
	auto policy = [this]() -> const VideoSelectionPolicy& { return runtime_.videoServiceForTesting().selectionPolicyForTesting(); };
	auto visitCheck = [this, policy](const std::string& label) {
		const float start = policy().activeWindowStartSeconds().value_or(-1.0f);
		const float pos = canonicalPosition();
		// The window starts exactly at the saved position; the decoder's seek
		// lands on the nearest earlier keyframe, so the live playhead may sit
		// up to one GOP (a few seconds) before it — never back at 0.
		check(label + ": starts at the saved resume position, not 0",
			std::fabs(start - savedResume_) < 0.6f && savedResume_ > 1.0f && pos >= savedResume_ - 5.0f,
			"saved " + ofToString(savedResume_, 2) + " s, window start " + ofToString(start, 2) + " s, playhead " +
			ofToString(pos, 2) + " s");
	};

	script_ = {
		{"blob live", [this] { return idleOn(kBlobId) && framesInStep_ >= 30; }, [] { return true; }},
		// Step through the catalog with real NextMedia until a long clip is active.
		{"find long clip", [this] { return framesInStep_ >= 20; }, [this, policy] {
			const Clip cls = policy().activeClipClass();
			const auto s = runtime_.videoServiceForTesting().status();
			note("visited " + activeMedia() + " (" + ofToString(s.durationSeconds.value_or(-1), 1) + " s, class " +
				(cls == Clip::Short ? "short" : cls == Clip::Medium ? "medium" : cls == Clip::Long ? "long" : "unknown") +
				", start " + ofToString(policy().activeWindowStartSeconds().value_or(-1), 1) + " s)");
			// The longest real clips (>= 60 s) show several consecutive windows.
			if (cls == Clip::Long && s.durationSeconds.value_or(0.0f) >= 60.0f
				&& policy().activeWindowStartSeconds().value_or(-1) == 0.0f) {
				longMedia_ = activeMedia();
				longIndex_ = policy().activeCatalogIndex();
				check("found a real long clip; visit 1 starts at 0", true, longMedia_);
				capture("r01_long_visit1_blob");
				return true;
			}
			if (++searchPresses_ > 40) {
				check("found a real long clip", false, "none in 40 selections");
				finish();
				return true;
			}
			runtime_.keyPressed('n');
			return false;
		}},
		// Scene switches mid-window must not touch resume state or the playhead.
		{"visit 1 running", [this] { return secondsInStep_ >= 4.0f; }, [this] {
			capture("r01b_long_visit1_blob_4s");
			positionBeforeSwitch_ = canonicalPosition();
			runtime_.keyPressed(']'); // Blob -> Temporal
			return true;
		}},
		{"temporal live", [this] { return idleOn(kTemporalId) && framesInStep_ >= 15; }, [this, policy] {
			const float canonical = canonicalPosition();
			const auto& adapter = runtime_.temporalSceneForTesting().videoAdapterForTesting();
			const float temporal = adapter.buffer().getLivePositionSeconds();
			check("scene switch kept the same canonical media", activeMedia() == longMedia_, activeMedia());
			check("scene switch did not reset the canonical playhead", canonical > positionBeforeSwitch_,
				ofToString(positionBeforeSwitch_, 2) + " s -> " + ofToString(canonical, 2) + " s");
			check("scene switch did not touch saved resume state", policy().resumeSecondsFor(longIndex_) == 0.0f);
			// Temporal is asked to start at the canonical playhead; where its
			// decoder lands depends on the file's keyframe spacing (OF seeks with
			// infinite tolerance), so the landing is reported, not asserted.
			check("Temporal follows canonical media and is started at the canonical playhead",
				adapter.loadedMediaId() == longMedia_ && adapter.lastRequestedStartSeconds() > positionBeforeSwitch_ - 0.5f,
				"requested " + ofToString(adapter.lastRequestedStartSeconds(), 2) + " s; decoder now at " +
				ofToString(temporal, 2) + " s (keyframe-snapped) vs canonical " + ofToString(canonical, 2) + " s");
			capture("r02_long_visit1_temporal");
			positionBeforeSwitch_ = canonical;
			runtime_.keyPressed('['); // Temporal -> Blob
			return true;
		}},
		{"blob again", [this] { return idleOn(kBlobId) && framesInStep_ >= 10; }, [this, policy] {
			check("Blob -> Temporal -> Blob: same media, playhead still advancing, resume untouched",
				activeMedia() == longMedia_ && canonicalPosition() > positionBeforeSwitch_
				&& policy().resumeSecondsFor(longIndex_) == 0.0f,
				ofToString(canonicalPosition(), 2) + " s");
			return true;
		}},
		// Window 1 ends by automatic advance (max dwell).
		{"window 1 ends", [this] { return activeMedia() != longMedia_; }, [this, policy] {
			savedResume_ = policy().resumeSecondsFor(longIndex_);
			check("visit 1 saved a resume position at the window end", savedResume_ > 15.0f,
				ofToString(savedResume_, 2) + " s, now on " + activeMedia());
			runtime_.keyPressed('p'); // PreviousMedia -> back to the long clip
			return true;
		}},
		{"visit 2", [this] { return activeMedia() == longMedia_ && framesInStep_ >= 10; }, [this, visitCheck] {
			visitCheck("visit 2 (PreviousMedia)");
			capture("r03_long_visit2");
			return true;
		}},
		{"window 2 ends", [this] { return activeMedia() != longMedia_; }, [this, policy] {
			const float previous = savedResume_;
			savedResume_ = policy().resumeSecondsFor(longIndex_);
			check("visit 2 advanced the resume position by one window", savedResume_ > previous + 10.0f || savedResume_ == 0.0f,
				ofToString(previous, 2) + " s -> " + ofToString(savedResume_, 2) + " s");
			runtime_.keyPressed('p');
			return true;
		}},
		{"visit 3", [this] { return activeMedia() == longMedia_ && framesInStep_ >= 10; }, [this, visitCheck] {
			if (savedResume_ > 0.0f) visitCheck("visit 3 (PreviousMedia)");
			else note("visit 3: resume wrapped to 0 (end of clip reached)");
			capture("r04_long_visit3");
			return true;
		}},
		// Let automatic advance run: the trace records short/medium dwell.
		{"natural advance", [this] { return secondsInStep_ >= 85.0f; }, [] { return true; }},
	};
}

void ResumeWindowHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	framesInStep_++;
	secondsInStep_ += dt;

	Step& s = script_[stepIndex_];
	if (s.ready()) {
		const bool advance = s.act();
		if (done_) return;
		if (advance) {
			stepIndex_++;
			if (stepIndex_ >= script_.size()) {
				finish();
				return;
			}
		}
		framesInStep_ = 0;
		secondsInStep_ = 0.0f;
	} else if (secondsInStep_ > kStepTimeoutSeconds) {
		check("step '" + s.label + "' reached", false, "timed out");
		finish();
	}
}

void ResumeWindowHarness::finish() {
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
