#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ContourSource.h"
#include "ContourDisplacementEffect.h"
#include "ContourMaskSource.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;
	void windowResized(int w, int h) override;

private:
	ContourSource source;
	ContourDisplacementEffect effect;

	// v2: polygon mask pipeline. maskImage doubles as both the original
	// v1 raster-mask demo (effect.setMask()) and, when maskSource ==
	// MASK_SOURCE_EXTERNAL_TEXTURE, the texture ContourMaskSource contour-
	// traces for Capability 1.
	ofImage maskImage;
	bool haveMaskImage = false;
	ContourMaskSource maskPolySource;
	int lastAppliedMaskSource = -1;

	ofxPanel panel;
	ofxButton presetCleanBtn;
	ofxButton presetTopographicBtn;
	ofxButton presetSideDissolveBtn;
	ofxButton presetAnalogScanBtn;
	ofxButton presetGhostBtn;
	ofxButton presetSilhouetteBtn;
	ofxButton presetConvergingBtn;
	ofxButton saveCustomBtn;
	ofxButton loadCustomBtn;

	bool showOverlay = true;
	int lastAppliedInputMode = -1;
	void applyInputMode(int mode);
	void updateMaskPolygon();

	void onPresetClean();
	void onPresetTopographic();
	void onPresetSideDissolve();
	void onPresetAnalogScan();
	void onPresetGhost();
	void onPresetSilhouette();
	void onPresetConverging();
	void onSaveCustom();
	void onLoadCustom();

	void drawDebugViews();
	std::string presetsDir() const;
};
