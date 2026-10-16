#include "ofApp.h"
#include "../../../shared/src/video-playback/SharedMediaRoot.h"

void ofApp::setup() {
    ofSetFrameRate(24);
    ofSetVerticalSync(true);
    ofBackground(13, 13, 13);
    ofHideCursor();
    ofDisableArbTex();  // force GL_TEXTURE_2D with normalized [0,1] coords

    video.setup(ecopunk::sharedMediaRoot());

    quadrantcrosshair::QuadrantSceneCore::Config config;
    config.canvasW = ofGetWidth();
    config.canvasH = ofGetHeight();
    config.trackWindow = true;
    core.setup(config);
    core.activate();

    debug.setup(&core.shaderLibrary(), &video, &core.motionExtraction());

    hudOverlay.setup(ofGetWidth(), ofGetHeight());
    hudOverlayPanel.setup();
}

quadrantcrosshair::QuadrantSceneCore::VideoFrame ofApp::currentFrame() {
    quadrantcrosshair::QuadrantSceneCore::VideoFrame frame;
    frame.texture = &video.getTexture();
    frame.pixels = &video.getPixels();
    frame.isFrameNew = video.isFrameNew();
    return frame;
}

void ofApp::update() {
    if (hudOverlayActive) {
        hudOverlayPanel.update(hudOverlayDials);
        hudOverlay.update(ofGetLastFrameTime(), hudOverlayDials);
        return;
    }
    if (debug.active) { debug.update(); return; }

    float dt = ofGetLastFrameTime();

    video.update();
    if (video.fileChanged()) {
        core.onMediaChanged(video.getCurrentFilename());
    }
    core.update(dt, currentFrame());
}

void ofApp::draw() {
    if (hudOverlayActive) {
        ofBackground(13, 13, 13);
        hudOverlay.draw();
        hudOverlayPanel.draw();
        return;
    }
    if (debug.active) { debug.draw(); return; }

    core.draw(currentFrame());
}

void ofApp::keyPressed(int key) {
    if (key == 'o' || key == 'O') { hudOverlayActive = !hudOverlayActive; return; }
    if (key == 'd' || key == 'D') { debug.active = !debug.active; return; }
    if (debug.active && debug.keyPressed(key)) return;

    auto& crosshair = core.crosshair();
    auto& expansionDirector = core.expansionDirector();
    int& motionOverlayAlpha = core.motionOverlayAlpha();
    if (key >= '1' && key <= '5') crosshair.setPreset(key - '1');
    if (key == OF_KEY_TAB)        crosshair.nextPreset();
    if (key == 'n' || key == 'N') video.nextFile();
    if (key == 'f' || key == 'F') ofToggleFullscreen();
    if (key == '[') motionOverlayAlpha = ofClamp(motionOverlayAlpha - 10, 0, 255);
    if (key == ']') motionOverlayAlpha = ofClamp(motionOverlayAlpha + 10, 0, 255);
    if (key == 'm' || key == 'M') core.motionExtraction().setOutputMode((core.motionExtraction().getOutputMode() + 1) % 3);
    if (key == 'h' || key == 'H') core.showHudLayer() = !core.showHudLayer();
    if (key == OF_KEY_ESC)        ofExit();

    // Expansion: E = random quadrant, F1–F4 = specific quadrant (for testing)
    if (key == 'e' || key == 'E')
        expansionDirector.trigger({ crosshair.getState().cx, crosshair.getState().cy });
    if (key == OF_KEY_F1)
        expansionDirector.triggerQuadrant(0, { crosshair.getState().cx, crosshair.getState().cy });
    if (key == OF_KEY_F2)
        expansionDirector.triggerQuadrant(1, { crosshair.getState().cx, crosshair.getState().cy });
    if (key == OF_KEY_F3)
        expansionDirector.triggerQuadrant(2, { crosshair.getState().cx, crosshair.getState().cy });
    if (key == OF_KEY_F4)
        expansionDirector.triggerQuadrant(3, { crosshair.getState().cx, crosshair.getState().cy });
}

void ofApp::windowResized(int w, int h) {
    core.windowResized(w, h);
    hudOverlay.windowResized(w, h);
}
