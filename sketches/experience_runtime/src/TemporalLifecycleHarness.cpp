#include "TemporalLifecycleHarness.h"

#include "ofAppRunner.h"
#include "ofFileUtils.h"
#include "ofGraphics.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

bool TemporalLifecycleHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_TEMPORAL_LIFECYCLE_HARNESS") != nullptr;
}

TemporalLifecycleHarness::TemporalLifecycleHarness(ExperienceRuntime& runtime)
	: runtime_(runtime) {
	ofLogNotice("TemporalLifecycleHarness") << "starting deterministic Temporal lifecycle/reactivation "
		"self-test (EXPERIENCE_RUNTIME_TEMPORAL_LIFECYCLE_HARNESS set) — " << kCycleCount << " cycles";
}

void TemporalLifecycleHarness::logResult(const std::string& checkName, bool passed, const std::string& detail) {
	totalChecks_++;
	if (!passed) {
		failureCount_++;
	}
	ofLogNotice("TemporalLifecycleHarness") << (passed ? "PASS" : "FAIL") << " — " << checkName
		<< (detail.empty() ? "" : (": " + detail));
}

void TemporalLifecycleHarness::runOneFrame(float dt) {
	runtime_.update(dt);
	runtime_.draw();

	const HudFrameData& frame = runtime_.currentHudFrameData();
	logResult("SceneFrame texture non-null after draw", frame.sceneFrame.texture != nullptr,
		"frameNumber=" + ofToString(runtime_.frameNumber()));
	logResult("frameNumber() strictly increasing", runtime_.frameNumber() > lastFrameNumber_,
		"was " + ofToString(lastFrameNumber_) + ", now " + ofToString(runtime_.frameNumber()));
	lastFrameNumber_ = runtime_.frameNumber();
}

void TemporalLifecycleHarness::captureScreenshot(const std::string& label) {
	std::string path = "captures/temporal_lifecycle_" + label + ".png";
	ofFilePath::createEnclosingDirectory(path);
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(path);
	ofLogNotice("TemporalLifecycleHarness") << "captured " << path;

	std::string scenePath = "captures/temporal_lifecycle_" + label + "_scene_only.png";
	bool saved = runtime_.saveSceneFrameCaptureForTesting(scenePath);
	logResult("saveSceneFrameCaptureForTesting() succeeds (" + label + ")", saved, scenePath);
}

void TemporalLifecycleHarness::step(float dt) {
	if (phase_ == Phase::Done) {
		return;
	}

	SceneManager& sceneManager = runtime_.sceneManagerForTesting();
	TemporalProductionScene& temporal = runtime_.temporalSceneForTesting();

	switch (phase_) {
		case Phase::Warmup: {
			runOneFrame(dt);
			framesInPhase_++;
			if (framesInPhase_ >= kWarmupFrames) {
				logResult("Setup once + first activate + first update/draw sequence completed", true,
					ofToString(kWarmupFrames) + " frames, sceneId=" + sceneManager.currentSceneStatus().sceneId);

				bool effectsPresentAfterWarmup = runtime_.currentHudFrameData().effects.has_value();
				logResult("HudFrameData.effects present after first completed Temporal frame (DEC-015 forwarding path)",
					effectsPresentAfterWarmup, "");

				captureScreenshot("00_warmup");
				historyCountAtDeactivate_ = temporal.coreForTesting().historyFrameCount();
				sceneManager.deactivateScene();
				framesInPhase_ = 0;
				phase_ = Phase::IdleAfterDeactivate;
			}
			break;
		}

		case Phase::IdleAfterDeactivate: {
			// Real runtime frames with Temporal deactivated — SceneManager
			// still calls updateActiveScene()/drawActiveScene() every
			// frame (it has no separate "inactive scene" concept — see
			// SceneManager.h's own comment), so this genuinely proves
			// TemporalProductionScene::update()'s own core_.isActive()
			// guard, not just an absence of calls.
			runtime_.update(dt);
			runtime_.draw();
			framesInPhase_++;
			if (framesInPhase_ >= kIdleFrames) {
				int historyNow = temporal.coreForTesting().historyFrameCount();
				logResult("No dedicated-decoder/history advance while deactivated",
					historyNow == historyCountAtDeactivate_,
					"history frame count stayed " + ofToString(historyCountAtDeactivate_) + " across "
						+ ofToString(kIdleFrames) + " idle frames (now " + ofToString(historyNow) + ")");
				framesInPhase_ = 0;
				phase_ = Phase::CycleBegin;
			}
			break;
		}

		case Phase::CycleBegin: {
			sceneManager.activateScene();

			// Diagnostic-only status pull, positioned deliberately BEFORE
			// any update() runs this cycle — same harness-only exception
			// precedent as BlobLifecycleHarness's own identical comment
			// (a pull between ExperienceRuntime frames, not a second
			// production per-frame pull).
			sceneManager.captureSceneStatus();
			historyCountBeforeCycleFrames_ = temporal.coreForTesting().historyFrameCount();
			logResult("Reactivation does not surface stale inactive-period history as current (cycle "
					+ ofToString(cycleIndex_) + ")",
				historyCountBeforeCycleFrames_ <= historyCountAtDeactivate_,
				"history frame count immediately after activate()=" + ofToString(historyCountBeforeCycleFrames_)
					+ " (was " + ofToString(historyCountAtDeactivate_) + " at deactivate — must not already be"
					+ " higher from stale carryover before any new frame has been captured)");

			framesInPhase_ = 0;
			phase_ = Phase::CycleFrames;
			break;
		}

		case Phase::CycleFrames: {
			runOneFrame(dt);
			framesInPhase_++;
			if (framesInPhase_ >= kFramesPerCycle) {
				int playheads = temporal.coreForTesting().numPlayheads();
				logResult("Playhead pool valid again after reactivation (cycle " + ofToString(cycleIndex_) + ")",
					playheads > 0, "numPlayheads=" + ofToString(playheads));

				bool effectsCoherent = runtime_.currentHudFrameData().effects.has_value();
				logResult("Canonical effect snapshot remains coherent after reactivation (cycle "
						+ ofToString(cycleIndex_) + ")",
					effectsCoherent, "");

				captureScreenshot("cycle" + ofToString(cycleIndex_) + "_end");
				sceneManager.deactivateScene();
				historyCountAtDeactivate_ = temporal.coreForTesting().historyFrameCount();
				framesInPhase_ = 0;
				phase_ = Phase::CycleEnd;
			}
			break;
		}

		case Phase::CycleEnd: {
			cycleIndex_++;
			if (cycleIndex_ >= kCycleCount) {
				phase_ = Phase::FinalActivate;
			} else {
				phase_ = Phase::CycleBegin;
			}
			break;
		}

		case Phase::FinalActivate: {
			sceneManager.activateScene();
			framesInPhase_ = 0;
			phase_ = Phase::FinalFrames;
			break;
		}

		case Phase::FinalFrames: {
			runOneFrame(dt);
			framesInPhase_++;
			if (framesInPhase_ >= kFinalFrames) {
				captureScreenshot("99_final");
				phase_ = Phase::Shutdown;
			}
			break;
		}

		case Phase::Shutdown: {
			sceneManager.deactivateScene();
			sceneManager.shutdown();
			logResult("shutdown() completed without error", true, "");

			bool rejectedAfterShutdown = !sceneManager.dispatchSceneCommand(SceneCommand::NextMedia);
			logResult("Commands rejected after shutdown()", rejectedAfterShutdown, "");

			ofLogNotice("TemporalLifecycleHarness") << "==================================================";
			ofLogNotice("TemporalLifecycleHarness") << "TEMPORAL LIFECYCLE HARNESS SUMMARY";
			ofLogNotice("TemporalLifecycleHarness") << "total checks: " << totalChecks_;
			ofLogNotice("TemporalLifecycleHarness") << "failures: " << failureCount_;
			ofLogNotice("TemporalLifecycleHarness") << (failureCount_ == 0 ? "RESULT: PASS" : "RESULT: FAIL");
			ofLogNotice("TemporalLifecycleHarness") << "==================================================";

			phase_ = Phase::Done;
			// Same non-interactive-self-test convention as
			// BlobLifecycleHarness — exits the process once the scripted
			// sequence completes, rather than sitting idle in Phase::Done.
			ofExit();
			break;
		}

		case Phase::Done:
			break;
	}
}
