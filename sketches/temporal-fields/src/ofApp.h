#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ShaderLibrary.h"
#include "TFBackgroundLayer.h"
#include "TFComposition.h"
#include "TFPatternBSP.h"
#include "TFPatternBlobGrid.h"
#include "TFParameterPanel.h"
#include "TFHudLayer.h"
#include "TimeOffsetVideoBuffer.h"
#include "MotionExtraction.h"

class ofApp : public ofBaseApp {

public:
	void setup() override;
	void update() override;
	void draw() override;

	void keyPressed(int key) override;

private:
	// Phase 2 verification only — tiles each playhead's current buffered
	// frame across the bottom of the screen so the ring buffer + playhead
	// pool can be visually confirmed before any real pattern (Phase 3/4)
	// consumes it. Remove once a pattern is driving playheads for real.
	void drawTimeOffsetDebugStrip();

	TFComposition composition;
	TFPatternBSP bspPattern;
	TFPatternBlobGrid blobGridPattern;
	TFParameterPanel paramPanel;
	ShaderLibrary shaderLib;
	TFBackgroundLayer backgroundLayer;

	TimeOffsetVideoBuffer timeOffsetBuffer;

	MotionExtraction motionEx;
	TFHudLayer       hudLayer;
};
