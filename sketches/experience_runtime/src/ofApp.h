#pragma once

#include "ofMain.h"
#include "ExperienceRuntime.h"
#include "GlRestorationHarness.h"

#include <memory>

// Thin ofApp — forwards the openFrameworks callback surface into
// ExperienceRuntime and nothing else. Per the implementation prompt: "ofApp
// may own the openFrameworks callback surface, but it must not become the
// long-term location for scene management, FBO ownership, command routing,
// or telemetry logic."
//
// The one exception is wiring up the opt-in GlRestorationHarness (see that
// class) when EXPERIENCE_RUNTIME_GL_HARNESS is set — that's test-harness
// plumbing, not app/runtime logic, kept here because it needs to intercept
// the normal update/draw callback sequence.
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
};
