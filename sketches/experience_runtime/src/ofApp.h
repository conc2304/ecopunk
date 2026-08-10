#pragma once

#include "ofMain.h"
#include "BlobLifecycleHarness.h"
#include "ExperienceRuntime.h"
#include "GlRestorationHarness.h"
#include "TemporalLifecycleHarness.h"

#include <memory>

// Thin ofApp — forwards the openFrameworks callback surface into
// ExperienceRuntime and nothing else. Per the implementation prompt: "ofApp
// may own the openFrameworks callback surface, but it must not become the
// long-term location for scene management, FBO ownership, command routing,
// or telemetry logic."
//
// The exception is wiring up the opt-in harnesses (see those classes) when
// their respective environment variables are set — that's test-harness
// plumbing, not app/runtime logic, kept here because each needs to
// intercept the normal update/draw callback sequence. All are mutually
// exclusive with each other and with the normal run (see setup()): the GL
// harness always exercises FakeScene, the Blob lifecycle/soak harnesses
// always exercise the installed BlobProductionScene, the Temporal lifecycle
// harness always exercises the installed TemporalProductionScene, and a
// normal run installs BlobProductionScene (the default) OR
// TemporalProductionScene (EXPERIENCE_RUNTIME_TEMPORAL_SCENE=1 — Temporal
// Production Scene #2 Migration) as the resident production scene.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;

	void keyPressed(int key) override;

private:
	ExperienceRuntime runtime;
	std::unique_ptr<GlRestorationHarness> harness;
	std::unique_ptr<BlobLifecycleHarness> blobHarness;
	std::unique_ptr<TemporalLifecycleHarness> temporalHarness;
};
