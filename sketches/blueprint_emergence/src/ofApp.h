#pragma once

#include "AnnotationRenderer.h"
#include "BEComposition.h"
#include "ErosionFBO.h"
#include "GridState.h"
#include "GridSystem.h"
#include "LFOBank.h"
#include "MotionExtraction.h"
#include "ShaderLibrary.h"
#include "TriggerBus.h"
#include "HudOverlayLayer.h"
#include "HudOverlayDialPanel.h"
#include "HudOverlayDialState.h"
#include "ofMain.h"

class ofApp : public ofBaseApp {

public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;

	void keyPressed(int key) override;
	void keyReleased(int key) override;
	void mouseMoved(int x, int y) override;
	void mouseDragged(int x, int y, int button) override;
	void mousePressed(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;
	void mouseScrolled(int x, int y, float scrollX, float scrollY) override;
	void mouseEntered(int x, int y) override;
	void mouseExited(int x, int y) override;
	void windowResized(int w, int h) override;
	void dragEvent(ofDragInfo dragInfo) override;
	void gotMessage(ofMessage msg) override;

private:
	void drawOccupancyDebug() const;
	void drawScene();
	void wireTriggerResponses();
	std::vector<std::string> loadCodeFragments() const;

	GridSystem grid;
	AnnotationRenderer annotations;
	VideoSampler videoSampler;
	BEComposition composition;

	LFOBank lfoBank;
	TriggerBus triggerBus;
	GridState gridState;
	ShaderLibrary shaderLib;
	ErosionFBO erosionFBO;
	MotionExtraction motionEx;

	float gridPulseBoost = 0.0f;
	float motionOverlayOpacity = 0.0f;

	bool showOccupancyDebug = false;
	bool bypassErosion = true; // TEMP DIAGNOSTIC, see ofApp::draw()
	bool hadHudWidget = false;

	// HUD Glitch Overlay System — standalone, isolated module (see
	// shared/src/hud_overlay/README.md). Toggled with 'o'; while active it
	// fully replaces this sketch's draw with the overlay alone over a
	// solid near-black canvas, no composition wiring.
	hudoverlay::HudOverlayLayer hudOverlay;
	hudoverlay::HudOverlayDialPanel hudOverlayPanel;
	hudoverlay::HudOverlayDialState hudOverlayDials;
	bool hudOverlayActive = false;
};
