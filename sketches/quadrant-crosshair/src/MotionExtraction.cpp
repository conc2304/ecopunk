#include "MotionExtraction.h"
#include <cmath>

void MotionExtraction::setup() {
    ofFbo::Settings accumSettings;
    accumSettings.width          = ACCUM_W;
    accumSettings.height         = ACCUM_H;
    accumSettings.internalformat = GL_RGB;
    accumSettings.useDepth       = false;
    accumSettings.useStencil     = false;

    fboAccumA.allocate(accumSettings);
    fboAccumB.allocate(accumSettings);

    fboAccumA.begin();
    ofClear(128, 128, 128, 255);
    fboAccumA.end();

    fboAccumB.begin();
    ofClear(128, 128, 128, 255);
    fboAccumB.end();

    ofFbo::Settings extractSettings;
    extractSettings.width          = ofGetWidth();
    extractSettings.height         = ofGetHeight();
    extractSettings.internalformat = GL_RGB;
    extractSettings.useDepth       = false;
    extractSettings.useStencil     = false;

    fboMotion.allocate(extractSettings);
    fboMotionDelayed.allocate(extractSettings);

    fboMotion.begin();
    ofClear(0, 0, 0, 255);
    fboMotion.end();

    fboMotionDelayed.begin();
    ofClear(0, 0, 0, 255);
    fboMotionDelayed.end();

    allocateHistory();

    accumShader.load("shaders/vert.glsl",   "shaders/motion_accum.glsl");
    extractShader.load("shaders/vert.glsl", "shaders/motion_extract.glsl");
}

void MotionExtraction::allocateHistory() {
    frameHistory.clear();
    frameHistory.resize(MAX_HISTORY_FRAMES);

    ofFbo::Settings historySettings;
    historySettings.width          = ACCUM_W;
    historySettings.height         = ACCUM_H;
    historySettings.internalformat = GL_RGB;
    historySettings.useDepth       = false;
    historySettings.useStencil     = false;

    for (int i = 0; i < MAX_HISTORY_FRAMES; ++i) {
        frameHistory[i].allocate(historySettings);

        frameHistory[i].begin();
        ofClear(128, 128, 128, 255);
        frameHistory[i].end();
    }

    historyWriteIndex = 0;
    historyCount      = 0;
}

void MotionExtraction::update(ofTexture& videoTex, float decayWeight, float sensitivity) {
    decayWeight = ofClamp(decayWeight, 0.0f, 0.999f);
    sensitivity = std::max(0.0f, sensitivity);

    /*
        Order: extract before updating accum or history, so both passes compare
        against prior state rather than the current frame.

        1. Accumulation-based extraction → fboMotion (fullscreen overlay source)
        2. Delayed-frame extraction       → fboMotionDelayed (quadrant variety)
        3. Update accumulation memory with the current frame.
        4. Store the current frame into history for future delayed-frame comparisons.
        5. Sample CPU control values (energy, centroid) from the updated accum.
    */
    updateExtract(videoTex, sensitivity, REFERENCE_ACCUMULATION,  fboMotion);
    updateExtract(videoTex, sensitivity, REFERENCE_DELAYED_FRAME, fboMotionDelayed);
    updateAccum(videoTex, decayWeight);
    updateFrameHistory(videoTex);
    sampleControlValues();
}

void MotionExtraction::updateExtract(ofTexture& videoTex, float sensitivity,
                                      int refMode, ofFbo& targetFbo) {
    targetFbo.begin();
    ofClear(0, 0, 0, 255);

    extractShader.begin();

    extractShader.setUniformTexture("currentFrame", videoTex,            0);
    extractShader.setUniformTexture("delayedFrame", getDelayedTexture(), 1);
    extractShader.setUniformTexture("accumFrame",   getAccumTexture(),   2);

    extractShader.setUniform1f("sensitivity", sensitivity);
    extractShader.setUniform1f("neutralGrey", extractNeutralGrey);
    extractShader.setUniform1f("boost", extractBoost);
    extractShader.setUniform1f("gamma", extractGamma);

    extractShader.setUniform1i("outputMode", outputMode);
    extractShader.setUniform1i("referenceMode", refMode);

    ofSetColor(255);

    // Draw current video as the geometry source so vTexCoord matches the output.
    videoTex.draw(0, 0, ofGetWidth(), ofGetHeight());

    extractShader.end();

    targetFbo.end();
}

void MotionExtraction::updateAccum(ofTexture& videoTex, float decayWeight) {
    // src: last result; dst: buffer to write into this frame
    ofFbo& src = pingPong ? fboAccumB : fboAccumA;
    ofFbo& dst = pingPong ? fboAccumA : fboAccumB;

    dst.begin();
    ofClear(128, 128, 128, 255);

    accumShader.begin();

    accumShader.setUniformTexture("currentFrame",  videoTex,         0);
    accumShader.setUniformTexture("previousAccum", src.getTexture(), 1);
    accumShader.setUniform1f("decayWeight", decayWeight);

    ofSetColor(255);

    // Draw the video as geometry source — this keeps videoTex bound on unit 0
    // (matching the currentFrame uniform) while src stays on unit 1.
    // Drawing src here would rebind it to unit 0 and silently override currentFrame.
    videoTex.draw(0, 0, ACCUM_W, ACCUM_H);

    accumShader.end();

    dst.end();

    pingPong = !pingPong;
}

void MotionExtraction::updateFrameHistory(ofTexture& videoTex) {
    if (frameHistory.empty()) {
        return;
    }

    ofFbo& dst = frameHistory[historyWriteIndex];

    dst.begin();
    ofClear(128, 128, 128, 255);

    ofSetColor(255);

    // Store a low-res copy of the current frame.
    // This keeps the delay buffer cheap and also smooths noisy video a bit.
    videoTex.draw(0, 0, ACCUM_W, ACCUM_H);

    dst.end();

    historyWriteIndex = (historyWriteIndex + 1) % MAX_HISTORY_FRAMES;
    historyCount = std::min(historyCount + 1, MAX_HISTORY_FRAMES);
}

void MotionExtraction::sampleControlValues() {
    ofFbo& accumFbo = pingPong ? fboAccumB : fboAccumA;

    ofPixels current;
    accumFbo.readToPixels(current);

    if (!prevAccumSet ||
        prevAccumPixels.getWidth()  != current.getWidth() ||
        prevAccumPixels.getHeight() != current.getHeight()) {

        prevAccumPixels = current;
        prevAccumSet = true;

        motionEnergy = 0.0f;
        motionCentroidX = 0.5f;
        motionCentroidY = 0.5f;

        return;
    }

    float totalDiff   = 0.f;
    float weightedX   = 0.f;
    float weightedY   = 0.f;
    float totalWeight = 0.f;

    int W = current.getWidth();
    int H = current.getHeight();

    const int STEP = 4;
    int sampleCount = 0;

    for (int y = 0; y < H; y += STEP) {
        for (int x = 0; x < W; x += STEP) {
            sampleCount++;

            ofColor c1 = current.getColor(x, y);
            ofColor c2 = prevAccumPixels.getColor(x, y);

            float diff = (std::fabs(c1.r - c2.r)
                        + std::fabs(c1.g - c2.g)
                        + std::fabs(c1.b - c2.b)) / (3.f * 255.f);

            totalDiff   += diff;
            weightedX   += diff * ((float)x / (float)W);
            weightedY   += diff * ((float)y / (float)H);
            totalWeight += diff;
        }
    }

    motionEnergy = sampleCount > 0 ? totalDiff / (float)sampleCount : 0.f;

    if (totalWeight > 0.001f) {
        motionCentroidX = weightedX / totalWeight;
        motionCentroidY = weightedY / totalWeight;
    }

    // Refresh snapshot every N frames so the slowly-decaying accum buffer has
    // enough time to accumulate change that survives 8-bit per-pixel rounding.
    if (++prevAccumSnapshotAge >= ENERGY_SNAPSHOT_INTERVAL) {
        prevAccumPixels      = current;
        prevAccumSnapshotAge = 0;
    }
}

void MotionExtraction::setOutputMode(int mode) {
    outputMode = ofClamp(mode, 0, 4);
}

void MotionExtraction::setReferenceMode(int mode) {
    referenceMode = ofClamp(mode, 0, 1);
}

void MotionExtraction::setDelayFrames(int frames) {
    delayFrames = ofClamp(frames, 1, MAX_HISTORY_FRAMES - 1);
}

ofTexture& MotionExtraction::getMotionTexture() {
    return fboMotion.getTexture();
}

ofTexture& MotionExtraction::getDelayedMotionTexture() {
    return fboMotionDelayed.getTexture();
}

ofTexture& MotionExtraction::getAccumTexture() {
    // Returns the most recently rendered accumulation buffer.
    return pingPong ? fboAccumB.getTexture() : fboAccumA.getTexture();
}

ofTexture& MotionExtraction::getDelayedTexture() {
    if (frameHistory.empty()) {
        return fboAccumA.getTexture();
    }

    /*
        historyWriteIndex points to the slot that will be written next.

        Most recently written frame is historyWriteIndex - 1.
        delayFrames = 1 means compare against the previous stored frame.
    */
    int availableDelay = std::min(delayFrames, std::max(1, historyCount));
    int index = historyWriteIndex - availableDelay;

    while (index < 0) {
        index += MAX_HISTORY_FRAMES;
    }

    index %= MAX_HISTORY_FRAMES;

    return frameHistory[index].getTexture();
}
