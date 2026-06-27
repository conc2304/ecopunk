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

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);

private:
    ShaderLibrary   shaders;
    VideoSystem     video;
    LFOBank         lfo;
    GridState       grid;
    CrosshairSystem crosshair;
    TriggerBus      triggerBus;
    QuadrantManager quadrants;
    MotionExtraction motionEx;

    float steeringBrightness  = 0.5f;
    int   motionOverlayAlpha  = 55;
    bool  showHUD             = true;
    DebugMode debug;

    // Motion trigger state
    float motionStillTimer    = 0.f;   // time spent below low-motion threshold
    float motionRampAlpha     = 55.f;  // overlay alpha target, ramps on onset

    HudManager hud;

    static float sampleBrightness(const ofPixels& px);
};
