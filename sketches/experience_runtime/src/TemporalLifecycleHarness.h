#pragma once

#include "ExperienceRuntime.h"

#include <string>

// TemporalLifecycleHarness — Temporal Production Scene #2 Migration.
//
// Deterministic, fixed-step, non-interactive self-test that drives the REAL
// production TemporalProductionScene (not a fake/lifecycle-state object)
// through the reactivation sequence this migration's own prompt requires
// (§11.8): setup once -> activate -> >=60 real update/draw frames ->
// deactivate -> activate -> >=60 real update/draw frames -> deactivate ->
// shutdown — inside the real ExperienceRuntime/SceneManager/HUD pipeline,
// with explicit PASS/FAIL assertions. Two cycles only (not Blob's 20) — the
// full 20-cycle Blob<->Temporal soak/switching proof is the separate,
// later two-scene acceptance milestone (see this session's completion
// report, "Deviations from prompt" / "Recommended next step").
//
// Same structural pattern as BlobLifecycleHarness/GlRestorationHarness (own
// isRequested() env var, step() driven from ofApp::update(), drives
// runtime.draw() itself, logResult() PASS/FAIL bookkeeping, screenshots
// under bin/data/captures/) — a third instance of that established pattern,
// not a second test architecture. Kept as its own class for the same reason
// BlobLifecycleHarness is its own class rather than folded into
// GlRestorationHarness: mutually exclusive installation (Temporal, not
// Blob or FakeScene, must be the scene installed before runtime.setup()
// runs — see ofApp.cpp's harness-selection branch).
class TemporalLifecycleHarness {
public:
	static bool isRequested(); // EXPERIENCE_RUNTIME_TEMPORAL_LIFECYCLE_HARNESS

	explicit TemporalLifecycleHarness(ExperienceRuntime& runtime);

	// Call once per frame from ofApp::update(), AFTER runtime.update(dt) —
	// same calling convention as BlobLifecycleHarness::step(). This class
	// drives runtime.draw() itself; ofApp must not also call runtime.draw()
	// while this harness is active.
	void step(float dt);

private:
	void logResult(const std::string& checkName, bool passed, const std::string& detail);
	void runOneFrame(float dt);
	void captureScreenshot(const std::string& label);

	ExperienceRuntime& runtime_;

	enum class Phase {
		Warmup,            // proves the first activate() (from runtime.setup()) + first update/draw work
		IdleAfterDeactivate, // a few real runtime frames with Temporal deactivated — proves no scene work while inactive
		CycleBegin,        // activateScene() + reactivation-history assertions for the current cycle
		CycleFrames,       // >=60 real update()+draw() frames for the current cycle
		CycleEnd,          // deactivateScene()
		FinalActivate,     // one more activate() after both cycles, before shutdown
		FinalFrames,
		Shutdown,          // sceneManager.shutdown() + post-shutdown command-rejection checks
		Done
	};

	Phase phase_ = Phase::Warmup;
	int framesInPhase_ = 0;
	int cycleIndex_ = 0; // 0..kCycleCount-1

	static constexpr int kCycleCount = 2;
	static constexpr int kWarmupFrames = 10;
	static constexpr int kIdleFrames = 5;
	static constexpr int kFramesPerCycle = 60; // migration prompt §11.8's ">=60 real update/draw frames"
	static constexpr int kFinalFrames = 10;

	int failureCount_ = 0;
	int totalChecks_ = 0;

	uint64_t lastFrameNumber_ = 0;

	// Real, live-read history-frame-count samples — used to prove (a) the
	// dedicated decoder stops advancing while deactivated (IdleAfterDeactivate)
	// and (b) history genuinely recovers after reactivation (CycleFrames).
	int historyCountAtDeactivate_ = 0;
	int historyCountBeforeCycleFrames_ = 0;
};
