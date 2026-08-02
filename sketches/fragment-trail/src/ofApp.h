#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "TimeOffsetVideoBuffer.h"
#include "CrosshairSystem.h"
#include "TriggerBus.h"
#include "LFOBank.h"
#include "ShaderLibrary.h"
#include "FTFragmentPool.h"
#include "FTModeController.h"
#include "FTParameterPanel.h"
#include "FTBackgroundLayer.h"
#include "FTOverlayDirector.h"
#include "HudOverlayLayer.h"
#include "HudOverlayDialPanel.h"
#include "HudOverlayDialState.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;
	void windowResized(int w, int h) override;

private:
	TimeOffsetVideoBuffer videoBuffer;
	CrosshairSystem crosshair;
	TriggerBus triggerBus;
	LFOBank lfoBank;
	ShaderLibrary shaderLib;

	FTFragmentPool pool;
	FTModeController modeController;
	FTParameterPanel paramPanel;
	FTBackgroundLayer backgroundLayer;

	// HUD Glitch Overlay System — here it composites live into the scene
	// (unlike quadrant-crosshair/blueprint_emergence/temporal-fields' own
	// exclusive full-screen 'o' toggle): ambient atoms always render, and
	// FTOverlayDirector drives the organisms off real fragment lifecycle
	// events instead of the internal random scheduler (which is disabled).
	// 'o' here only shows/hides the dial-tuning panel.
	hudoverlay::HudOverlayLayer hudOverlay;
	hudoverlay::HudOverlayDialPanel hudOverlayPanel;
	hudoverlay::HudOverlayDialState hudOverlayDials;
	FTOverlayDirector overlayDirector;

	// Section 9's resolution: kept small (Temporal Fields' own 5-8 default
	// range), not raised to match maxFragments — see FTFragmentPool's
	// playhead acquire/release fallback.
	static constexpr int kNumPlayheads = 6;
};
