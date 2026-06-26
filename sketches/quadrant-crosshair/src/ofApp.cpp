#include "ofApp.h"

void ofApp::setup() {
    ofSetFrameRate(24);
    ofSetVerticalSync(true);
    ofBackground(13, 13, 13);
    ofHideCursor();
    ofDisableArbTex();  // force GL_TEXTURE_2D with normalized [0,1] coords

    shaders.setup();
#ifdef PLATFORM_PI
    video.setup("/home/pi/blueprint/media");
#else
    video.setup(ofToDataPath("media", true));
#endif
    lfo.setup();
    rd.setup();
    grid.setup();
    crosshair.setup();
    triggerBus.setup();
    quadrants.setup(&shaders);
    motionEx.setup();
    debug.setup(&shaders, &video, &motionEx);

    // TriggerBus → QuadrantManager
    triggerBus.addListener([this](const TriggerEvent& e) {
        quadrants.onTrigger(e);
    });

    // TriggerBus → CrosshairSystem bloom
    triggerBus.addListener([this](const TriggerEvent& e) {
        if (e.id == TriggerID::VELOCITY_HIGH && e.active)
            crosshair.triggerBloom(24.f, 0.4f);
        if (e.id == TriggerID::DWELL)
            crosshair.setBloomFill(e.active ? 1.f : 0.f, 2.0f);
    });

    // TriggerBus → RD feed bump on CORNER_NEAR
    triggerBus.addListener([this](const TriggerEvent& e) {
        if (e.id == TriggerID::CORNER_NEAR && e.active)  rdFeedBump = 0.012f;
        if (e.id == TriggerID::CORNER_NEAR && !e.active) rdFeedBump = 0.f;
    });
}

void ofApp::update() {
    if (debug.active) { debug.update(); return; }

    float dt = ofGetLastFrameTime();

    // 1. Source systems
    video.update();
    lfo.update(dt);

    // 2. Video CPU sampling + motion extraction
    if (video.isFrameNew()) {
        const ofPixels& px = video.getPixels();
        CrosshairState  cs = crosshair.getState();
        crosshair.sampleColor(px, cs.cx, cs.cy);
        steeringBrightness = sampleBrightness(px);

        // Decay weight: long memory default, slowly swept by LFO
        float decayWeight = ofMap(lfo.get(LFO_RD_FEED), -1, 1, 0.92f, 0.995f);
        float energy      = motionEx.getMotionEnergy();
        // Sensitivity: auto-adjusts inversely with energy to keep output readable
        float sensitivity = ofMap(energy, 0.f, 0.3f, 6.0f, 2.0f, true);

        motionEx.update(video.getTexture(), decayWeight, sensitivity);

        // Feed centroid to crosshair as soft attractor
        crosshair.setMotionAttractor(
            motionEx.getMotionCentroidX() * ofGetWidth(),
            motionEx.getMotionCentroidY() * ofGetHeight(),
            energy
        );

        // Output mode switching based on energy thresholds
        if (energy > 0.25f) {
            motionStillTimer = 0.f;
            if (motionEx.getOutputMode() != 1) motionEx.setOutputMode(1); // CHROMA_PRESERVE
        } else if (energy < 0.02f) {
            motionStillTimer += dt;
            if (motionStillTimer > 5.f && motionEx.getOutputMode() != 2)
                motionEx.setOutputMode(2); // SIGNED_FIELD
        } else {
            motionStillTimer = 0.f;
            if (motionEx.getOutputMode() != 0) motionEx.setOutputMode(0); // LUMA_GLOW
        }

        // Ramp overlay alpha up on motion onset (energy crossing 0.1 rising)
        float targetAlpha = (energy > 0.1f) ? 80.f : 55.f;
        motionRampAlpha += (targetAlpha - motionRampAlpha) * 0.05f;
        motionOverlayAlpha = (int)motionRampAlpha;
    }

    // 3. Crosshair motion
    crosshair.update(dt, lfo);

    // 4. Triggers
    triggerBus.update(crosshair.getState(), dt);

    // 5. Reaction-diffusion — feed/kill driven by LFOs + trigger bump
    float feed = 0.055f + ofMap(lfo.get(LFO_RD_FEED), -1, 1, -0.008f, 0.008f) + rdFeedBump;
    float kill = 0.062f + ofMap(lfo.get(LFO_RD_KILL), -1, 1, -0.006f, 0.006f);
    rd.update(feed, kill);

    // 6. Grid
    float decayRate = ofMap(lfo.get(LFO_GRID_DECAY), -1, 1, 0.96f, 0.995f);
    CrosshairState cs = crosshair.getState();
    grid.update(cs.cx, cs.cy, dt, decayRate);
    grid.uploadTexture();

    // 7. Quadrants
    quadrants.setVideoBrightness(steeringBrightness);
    quadrants.update(dt, cs, lfo, rd.getTexture(), grid.getTexture(),
                     motionEx.getMotionTexture());
}

void ofApp::draw() {
    if (debug.active) { debug.draw(); return; }

    ofBackground(13, 13, 13);

    // Layer 1: quadrants (erosion + per-quadrant effects including motion_effect)
    quadrants.draw(video.getTexture(), video.getVideoSize());

    // Layer 2: motion extraction atmospheric overlay (~22% opacity, adjustable)
    ofEnableAlphaBlending();
    ofSetColor(255, 255, 255, motionOverlayAlpha);
    motionEx.getMotionTexture().draw(0, 0, ofGetWidth(), ofGetHeight());
    ofDisableAlphaBlending();

    // Layer 3: crosshair — always on top
    crosshair.draw();

    // Layer 4: debug HUD (toggle with H)
    if (showHUD) quadrants.drawHUD();
}

void ofApp::keyPressed(int key) {
    if (key == 'd' || key == 'D') { debug.active = !debug.active; return; }
    if (debug.active && debug.keyPressed(key)) return;

    if (key >= '1' && key <= '5') crosshair.setPreset(key - '1');
    if (key == OF_KEY_TAB)        crosshair.nextPreset();
    if (key == 'r' || key == 'R') rd.setup();
    if (key == 'n' || key == 'N') video.nextFile();
    if (key == 'f' || key == 'F') ofToggleFullscreen();
    if (key == '[') motionOverlayAlpha = ofClamp(motionOverlayAlpha - 10, 0, 255);
    if (key == ']') motionOverlayAlpha = ofClamp(motionOverlayAlpha + 10, 0, 255);
    if (key == 'm' || key == 'M') motionEx.setOutputMode((motionEx.getOutputMode() + 1) % 3);
    if (key == 'h' || key == 'H') showHUD = !showHUD;
    if (key == OF_KEY_ESC)        ofExit();
}

float ofApp::sampleBrightness(const ofPixels& px) {
    int   W   = px.getWidth(), H = px.getHeight();
    float sum = 0.f;
    int   n   = 0;
    const int STEP = 16;
    for (int y = 0; y < H; y += STEP) {
        for (int x = 0; x < W; x += STEP) {
            auto c = px.getColor(x, y);
            sum += 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
            n++;
        }
    }
    return n > 0 ? sum / (n * 255.f) : 0.5f;
}
