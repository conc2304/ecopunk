#pragma once

#include "ExperienceRuntime.h"

#include <optional>
#include <string>

// BlobLifecycleHarness — Blob First Production Acceptance narrow patch.
//
// Deterministic, fixed-step, non-interactive self-test that drives the
// REAL production BlobProductionScene (not a fake/lifecycle-state object)
// through 20 deactivate/reactivate cycles inside the real
// ExperienceRuntime/SceneManager/HUD pipeline, then shutdown — proving the
// acceptance-review-flagged gap ("lifecycle/reactivation behavior was only
// exercised by one live, manual, un-scripted run") with explicit PASS/FAIL
// assertions instead.
//
// Same structural pattern as GlRestorationHarness (own isRequested() env
// var, step() driven from ofApp::update(), drives runtime.draw() itself,
// logResult() PASS/FAIL bookkeeping, screenshots under bin/data/captures/,
// exits the process via ofExit() when the scripted sequence completes) —
// a second instance of that established pattern, not a second test
// architecture. Kept as its own class (not folded into GlRestorationHarness)
// because that class is FakeScene-only by construction (reads
// SceneManager::devScene(), exercises FakeScene-only dev knobs) and is
// wired into ofApp mutually exclusively with Blob's installation; see
// ofApp.cpp's three-way branch (normal / GL harness / this harness).
//
// Opt-in only: activated by setting one of two mutually exclusive
// environment variables before launch:
//
//     EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS=1 \
//         ./bin/experience_runtime.app/Contents/MacOS/experience_runtime
//
//     EXPERIENCE_RUNTIME_BLOB_SOAK_HARNESS=1 \
//         ./bin/experience_runtime.app/Contents/MacOS/experience_runtime
//
// Blob Post-Acceptance Hardening: the second mode (Mode::Soak) was added to
// this same class rather than a new one — "prefer extending... do not
// create another parallel harness framework" — since it needs exactly the
// same runOneFrame()/captureScreenshot()/logResult() primitives the
// lifecycle mode already has; only the top-level scripted sequence differs
// (continuous real-time operation vs. scripted activate/deactivate
// cycling — a longevity proof, not a lifecycle-transition proof).
class BlobLifecycleHarness {
public:
	enum class Mode { Lifecycle, Soak };

	static bool isRequested();     // EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS
	static bool isSoakRequested(); // EXPERIENCE_RUNTIME_BLOB_SOAK_HARNESS

	explicit BlobLifecycleHarness(ExperienceRuntime & runtime, Mode mode = Mode::Lifecycle);

	// Call once per frame from ofApp::update(), AFTER runtime.update(dt) —
	// same calling convention as GlRestorationHarness::step(). This class
	// drives runtime.draw() itself; ofApp must not also call runtime.draw()
	// while this harness is active.
	void step(float dt);

private:
	void logResult(const std::string & checkName, bool passed, const std::string & detail);
	void runOneFrame(float dt);
	void captureScreenshot(const std::string & label);
	void trackPeaks();
	void stepSoak(float dt);
	void logSoakStatusLine();
	void finishSoak();

	ExperienceRuntime & runtime_;
	Mode mode_;

	enum class Phase {
		Warmup,          // proves the first activate() (from runtime.setup()) + first update/draw work
		CycleBegin,      // activateScene() + no-stale-carryover assertion for the current cycle
		CycleFrames,     // a few real update()+draw() frames for the current cycle
		CycleEnd,        // deactivateScene()
		FinalActivate,   // one more activate() after the 20 cycles, before shutdown
		FinalFrames,
		Shutdown,        // sceneManager.shutdown() + post-shutdown command-rejection checks
		Soak,            // Mode::Soak only — continuous real-time operation, no cycling
		Done
	};

	Phase phase_ = Phase::Warmup;
	int framesInPhase_ = 0;
	int cycleIndex_ = 0; // 0..kCycleCount-1

	static constexpr int kCycleCount = 20;
	static constexpr int kWarmupFrames = 10;
	// Deliberately generous, not just ">= 2": BlobSceneCore::activate()
	// resets havePrevFrame every cycle (see that method's own comment), and
	// BlobDetector only re-runs its diff pipeline every
	// config.processEveryNFrames (default 2) frames — 20 frames/cycle gives
	// ~10 real detection attempts per cycle, enough for real (non-zero)
	// region/fragment counts to actually appear in this harness's own
	// evidence when the active media has detectable motion, not just prove
	// "didn't crash" against an all-zero signal every cycle.
	static constexpr int kFramesPerCycle = 20;
	static constexpr int kFinalFrames = 10;

	int failureCount_ = 0;
	int totalChecks_ = 0;

	uint64_t lastFrameNumber_ = 0;
	int peakActiveItemCount_ = 0;
	int peakRegionCount_ = 0;

	// Blob Post-Acceptance Hardening: real scratch-FBO high-water tracking
	// (both the fragment-pool path and the separate background-effect
	// path — see BlobProductionScene's own comment on why there are two),
	// read live through ExperienceRuntime::blobSceneForTesting() rather
	// than estimated.
	int peakFragmentScratchWidth_ = 0;
	int peakFragmentScratchHeight_ = 0;
	int peakBackgroundScratchWidth_ = 0;
	int peakBackgroundScratchHeight_ = 0;

	// -- Mode::Soak state -----------------------------------------------
	static constexpr float kSoakDurationSeconds = 15.0f * 60.0f; // 15 real-time minutes
	static constexpr uint64_t kSoakMaxFrames = 60000; // safety valve if fps exceeds the 30fps target
	static constexpr float kSoakLogIntervalSeconds = 30.0f;

	float soakElapsedSeconds_ = 0.0f;
	float soakSinceLastLogSeconds_ = 0.0f;
	uint64_t soakFrameCount_ = 0;
	int soakMediaChangeCount_ = 0;
	std::optional<std::string> soakLastMediaId_;
	long long soakAllocNewBaseline_ = 0;
	long long soakAllocDeleteBaseline_ = 0;
	float soakMinFps_ = -1.0f;
	float soakMaxFrameTimeMs_ = -1.0f;
	bool soakGlHudIssueObserved_ = false;
};
