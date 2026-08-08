#pragma once

#include "ExperienceRuntime.h"
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
};
