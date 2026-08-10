#pragma once

#include "ExperienceRuntime.h"
#include "TFEffectPicker.h"
#include "ofImage.h"

#include <string>
#include <vector>

// GlRestorationHarness — deterministic, fixed-dt, non-interactive
// self-test that drives an already-set-up ExperienceRuntime through a
// scripted sequence and asserts Development Stream 1's required behaviors
// directly, in the style of sketches/hud_validation_harness (fixed steps,
// no reliance on real time, screenshots saved under bin/data/captures/,
// direct state checks logged as pass/fail rather than eyeballed from an
// interactive session).
//
// Covers: GL-state restoration (scissor/stencil/blend/shader/texture/
// viewport/matrix/style/framebuffer), FBO/SceneFrame ownership, status/
// capability polling discipline, semantic-payload variant coverage, FBO
// reallocation safety, and measured steady-state heap-allocation behavior.
//
// Opt-in only: activated by setting the EXPERIENCE_RUNTIME_GL_HARNESS
// environment variable before launch, e.g.:
//
//     EXPERIENCE_RUNTIME_GL_HARNESS=1 \
//         ./bin/experience_runtime.app/Contents/MacOS/experience_runtime
//
// Exits the process automatically (ofExit()) once the scripted sequence
// completes and results are logged.
class GlRestorationHarness {
public:
	static bool isRequested();

	explicit GlRestorationHarness(ExperienceRuntime& runtime);

	// Call once per frame from ofApp::update(), AFTER runtime.update(dt)
	// and BEFORE runtime.draw() — this class drives runtime.draw() itself
	// at the points it needs to (see step()'s own comments), so ofApp
	// must NOT also call runtime.draw() while a harness is active.
	void step(float dt);

private:
	void logResult(const std::string& checkName, bool passed, const std::string& detail);
	void runOneFrame(float dt, bool contaminate);
	void checkSemanticVariant(FakeScene::SemanticVariant variant, const std::string& label);

	// Final Shared Effects Source-of-Truth Seam Proof session: a short,
	// self-contained phase appended AFTER the allocation investigation
	// window (see step()'s tail) that proves TFEffectPicker (the approved
	// production owner of EffectActivityStatus for Temporal Fields) can
	// stand in for FakeScene at the SceneManager::captureEffectActivityStatus()
	// seam without touching any other forwarding code — see
	// SceneManager::setEffectActivitySourceOverrideForTesting(). Kept as a
	// separate appended phase (its own local frame counter,
	// seamProofFrameIndex_) rather than interleaved with the cases above so
	// none of the existing frame-index cases needed renumbering — see this
	// class's revision history for why renumbering has repeatedly been a
	// source of off-by-one bugs here.
	void stepSeamProof(float dt);

	ExperienceRuntime& runtime_;
	int frameIndex_ = 0;
	bool finished_ = false;
	int failureCount_ = 0;
	int framesRendered_ = 0;

	// Architecture-Closure Session: cumulative HUD-ONLY allocation totals
	// (see ExperienceRuntime::lastHudOnlyNewCount()/lastHudOnlyDeleteCount())
	// accumulated frame-by-frame across the same 50/500/5000-frame windows
	// the whole-process AllocationCounter measurement already uses.
	long long cumulativeHudOnlyNew_ = 0;
	long long cumulativeHudOnlyDelete_ = 0;

	ofImage baselineCapture_;
	ofImage postContaminationCapture_;

	// -- Final Shared Effects Source-of-Truth Seam Proof state -----------
	// A real production TFEffectPicker (Temporal Fields' approved owner of
	// EffectActivityStatus), exercised with shaderLib == nullptr — see
	// TFEffectPicker::activityStatus()'s own header comment: every one of
	// setup()/update()/activityStatus() is safe (and, for the Degraded-health
	// branch, specifically meaningful) with a null ShaderLibrary. This is
	// the ONLY thing that makes it possible to prove this seam without
	// instantiating real GL/shader machinery, keeping this a narrow seam
	// proof rather than a Temporal migration.
	TFEffectPicker seamProofPicker_;
	int seamProofFrameIndex_ = 0;
	bool seamProofFinished_ = false;

	// Baselines of the two PRE-EXISTING cumulative counters (FakeScene's
	// statusPollCount, and this class's own framesRendered_), captured the
	// instant the seam-proof phase begins (case 0, before its own
	// runOneFrame() call). Needed because case 13, much earlier in this
	// harness's scripted sequence, legitimately calls runOneFrame() TWICE
	// within one real tick (to grab two distinct screenshots without a
	// second update()) — from that point on, framesRendered_ (a draw-count)
	// permanently runs 1 ahead of statusPollCount (a real-tick count), so
	// comparing their raw cumulative totals (as case 6, earlier and
	// correctly, does BEFORE case 13 ever runs) is no longer valid. Seam
	// Proof's own "still exactly one status poll per frame" check instead
	// compares DELTAS since these baselines — both counters increase 1:1
	// with each other for the entire seam-proof phase itself (nothing in
	// stepSeamProof() double-draws), so this is a correct, non-vacuous
	// re-proof of the invariant going forward, not a workaround.
	int seamProofBaselineStatusPollCount_ = 0;
	int seamProofBaselineFramesRendered_ = 0;

	// Counted INSIDE the override lambda installed on SceneManager (see
	// stepSeamProof()'s s==0 case) — proves "activityStatus() called
	// exactly once per frame" and "update() happens before capture" by
	// construction: both counters increment together, once per lambda
	// invocation, and the lambda's body calls update() textually before
	// activityStatus().
	int seamProofUpdateCalls_ = 0;
	int seamProofCaptureCalls_ = 0;
	float seamProofLastDt_ = 0.0f;

	// The exact value the override lambda most recently returned to
	// SceneManager — compared field-by-field against
	// currentHudFrameData().effects to prove the runtime forwarding
	// boundary carries it through unmodified ("forwarded without
	// reconstruction").
	std::optional<videoeffects::EffectActivityStatus> seamProofLastForwarded_;
};
