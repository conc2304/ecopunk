#pragma once

#include "ofMain.h"
#include "HudElements.h"
#include "RPVideoSampler.h"
#include "RPRevealMask.h"
#include "RPCompositor.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

private:
	void addEmitterAtRandomPosition();

	RPVideoSampler videoSampler;
	RPRevealMask revealMask;
	RPCompositor compositor;
	hud::PulseEmitterWidget pulseEmitter;

	bool showDebugOverlay = true;

	// Milestone 4/5/9 instrumentation — per-stage frame time in ms, so the
	// Pi 3B go/no-go measurements the engineering handoff calls for can
	// actually be read off on target hardware. This sketch can't make that
	// call itself; it can only make the numbers visible.
	float msVideoUpdate = 0.0f;
	float msMaskStampAndDecay = 0.0f;
	float msComposite = 0.0f;
};
