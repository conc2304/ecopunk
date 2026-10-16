#pragma once
#include "ofMain.h"
#include "DebugMode.h"
#include "VideoSystem.h"
#include "QuadrantSceneCore.h"
#include "HudOverlayLayer.h"
#include "HudOverlayDialPanel.h"
#include "HudOverlayDialState.h"

// Standalone Quadrant Crosshair: owns the window, its own media (VideoSystem
// scanning bin/data/media), the DebugMode tuning panel and the dev keys. The
// artwork itself is quadrantcrosshair::QuadrantSceneCore — the same core
// ExperienceRuntime's QuadrantProductionScene runs.
class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);
    void windowResized(int w, int h);

private:
    quadrantcrosshair::VideoSystem       video;
    quadrantcrosshair::QuadrantSceneCore core;
    quadrantcrosshair::DebugMode         debug;

    // HUD Glitch Overlay System — standalone, isolated module (see
    // shared/src/hud_overlay/README.md). Toggled with 'o'; while active it
    // fully replaces this sketch's draw with the overlay alone over a
    // solid near-black canvas, no composition wiring.
    hudoverlay::HudOverlayLayer hudOverlay;
    hudoverlay::HudOverlayDialPanel hudOverlayPanel;
    hudoverlay::HudOverlayDialState hudOverlayDials;
    bool hudOverlayActive = false;

    quadrantcrosshair::QuadrantSceneCore::VideoFrame currentFrame();
};
