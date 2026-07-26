#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ShaderLibrary.h"
#include "TFAmbientTextureLayer.h"
#include "TFBackgroundLayer.h"
#include "TFComposition.h"
#include "TFPatternBSP.h"
#include "TFPatternBlobGrid.h"
#include "TFPatternBands.h"
#include "TFPatternColumnGrid.h"
#include "TFPatternTelescopingFrames.h"
#include "TFPatternParticleField.h"
#include "TFPatternEcologicalSuccession.h"
#include "TFPatternNetworkGrowth.h"
#include "TFPatternTemporalTides.h"
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
	void windowResized(int w, int h) override;

private:
	// Phase 2 verification only — tiles each playhead's current buffered
	// frame across the bottom of the screen so the ring buffer + playhead
	// pool can be visually confirmed before any real pattern (Phase 3/4)
	// consumes it. Remove once a pattern is driving playheads for real.
	void drawTimeOffsetDebugStrip();

	TFComposition composition;
	TFPatternBSP bspPattern;
	TFPatternBlobGrid blobGridPattern;
	TFPatternBands bandsPattern;
	TFPatternColumnGrid columnGridPattern;
	TFPatternTelescopingFrames telescopingFramesPattern;
	TFPatternParticleField particleFieldPattern;
	TFPatternEcologicalSuccession ecologicalSuccessionPattern;
	TFPatternNetworkGrowth networkGrowthPattern;
	TFPatternTemporalTides temporalTidesPattern;
	TFParameterPanel paramPanel;
	ShaderLibrary shaderLib;
	TFBackgroundLayer backgroundLayer;
	TFAmbientTextureLayer ambientTextures;

	TimeOffsetVideoBuffer timeOffsetBuffer;

	MotionExtraction motionEx;
	TFHudLayer       hudLayer;

	bool showDebugGui = true;
};
