#pragma once
#include "ofMain.h"
#include "DebugMode.h"
#include "CrosshairSystem.h"
#include "TriggerBus.h"
#include "QuadrantManager.h"
#include "VideoSystem.h"
#include "ShaderLibrary.h"
#include "LFOBank.h"
#include "GridState.h"
#include "MotionExtraction.h"
#include "HudManager.h"
#include "ExpansionDirector.h"
#include "HudOverlayLayer.h"
#include "HudOverlayDialPanel.h"
#include "HudOverlayDialState.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);
    void windowResized(int w, int h);

private:
    ShaderLibrary   shaders;
    VideoSystem     video;
    LFOBank         lfo;
    GridState       grid;
    CrosshairSystem crosshair;
    TriggerBus      triggerBus;
    QuadrantManager quadrants;
    MotionExtraction motionEx;

    ExpansionDirector expansionDirector;

    float steeringBrightness  = 0.5f;
    int   motionOverlayAlpha  = 55;
    bool  showHUD             = true;
    DebugMode debug;

    // Motion trigger state
    float motionStillTimer    = 0.f;   // time spent below low-motion threshold
    float motionRampAlpha     = 55.f;  // overlay alpha target, ramps on onset

    HudManager hud;

    // HUD Glitch Overlay System — standalone, isolated module (see
    // shared/src/hud_overlay/README.md). Toggled with 'o'; while active it
    // fully replaces this sketch's draw with the overlay alone over a
    // solid near-black canvas, no composition wiring.
    hudoverlay::HudOverlayLayer hudOverlay;
    hudoverlay::HudOverlayDialPanel hudOverlayPanel;
    hudoverlay::HudOverlayDialState hudOverlayDials;
    bool hudOverlayActive = false;

    static float sampleBrightness(const ofPixels& px);
};
