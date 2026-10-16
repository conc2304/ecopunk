#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "TimeOffsetVideoBuffer.h"
#include "ShaderLibrary.h"
#include "FragmentSceneCore.h"
#include "HudOverlayDialPanel.h"

// Standalone Fragment Trail: owns the window, its own media (the local
// TimeOffsetVideoBuffer fork scanning bin/data/media), the local
// ShaderLibrary, and the dev GUI. The artwork itself is
// fragmenttrail::FragmentSceneCore — the same core ExperienceRuntime's
// FragmentProductionScene runs.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;
	void windowResized(int w, int h) override;

private:
	TimeOffsetVideoBuffer videoBuffer;
	ShaderLibrary shaderLib;
	fragmenttrail::FragmentSceneCore core;

	// HUD Glitch Overlay System — here it composites live into the scene
	// (unlike quadrant-crosshair/blueprint_emergence/temporal-fields' own
	// exclusive full-screen 'o' toggle): ambient atoms always render, and
	// FTOverlayDirector drives the organisms off real fragment lifecycle
	// events instead of the internal random scheduler (which is disabled).
	// 'o' here only shows/hides the dial-tuning panel.
	hudoverlay::HudOverlayDialPanel hudOverlayPanel;

	// Section 9's resolution: kept small (Temporal Fields' own 5-8 default
	// range), not raised to match maxFragments — see FTFragmentPool's
	// playhead acquire/release fallback.
	static constexpr int kNumPlayheads = 6;
};
