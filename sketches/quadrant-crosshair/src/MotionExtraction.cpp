#include "MotionExtraction.h"
#include <cmath>

void MotionExtraction::setup() {
    ofFbo::Settings accumSettings;
    accumSettings.width          = ACCUM_W;
    accumSettings.height         = ACCUM_H;
    accumSettings.internalformat = GL_RGB;
    accumSettings.useDepth       = false;
    fboAccumA.allocate(accumSettings);
    fboAccumB.allocate(accumSettings);
    fboAccumA.begin(); ofClear(128, 128, 128, 255); fboAccumA.end();
    fboAccumB.begin(); ofClear(128, 128, 128, 255); fboAccumB.end();

    ofFbo::Settings extractSettings;
    extractSettings.width          = ofGetWidth();
    extractSettings.height         = ofGetHeight();
    extractSettings.internalformat = GL_RGB;
    extractSettings.useDepth       = false;
    fboMotion.allocate(extractSettings);
    fboMotion.begin(); ofClear(128, 128, 128, 255); fboMotion.end();

    accumShader.load("shaders/vert.glsl",   "shaders/motion_accum.glsl");
    extractShader.load("shaders/vert.glsl", "shaders/motion_extract.glsl");
}

void MotionExtraction::update(ofTexture& videoTex, float decayWeight, float sensitivity) {
    updateAccum(videoTex, decayWeight);
    updateExtract(videoTex, sensitivity);
    sampleControlValues();
}

void MotionExtraction::updateAccum(ofTexture& videoTex, float decayWeight) {
    // src: last result; dst: buffer to write into this frame
    ofFbo& src = pingPong ? fboAccumB : fboAccumA;
    ofFbo& dst = pingPong ? fboAccumA : fboAccumB;

    dst.begin();
    accumShader.begin();
    accumShader.setUniformTexture("currentFrame",  videoTex,           0);
    accumShader.setUniformTexture("previousAccum", src.getTexture(),   1);
    accumShader.setUniform1f("decayWeight", decayWeight);
    ofSetColor(255);
    // Draw src geometry so vert.glsl gets correct [0,1] texcoords
    src.getTexture().draw(0, 0, ACCUM_W, ACCUM_H);
    accumShader.end();
    dst.end();

    pingPong = !pingPong;
}

void MotionExtraction::updateExtract(ofTexture& videoTex, float sensitivity) {
    fboMotion.begin();
    extractShader.begin();
    extractShader.setUniformTexture("currentFrame", videoTex,            0);
    extractShader.setUniformTexture("accumFrame",   getAccumTexture(),   1);
    extractShader.setUniform1f("sensitivity",  sensitivity);
    extractShader.setUniform1f("neutralGrey",  extractNeutralGrey);
    extractShader.setUniform1f("boost",        extractBoost);
    extractShader.setUniform1f("gamma",        extractGamma);
    extractShader.setUniform1i("outputMode",   outputMode);
    ofSetColor(255);
    videoTex.draw(0, 0, ofGetWidth(), ofGetHeight());
    extractShader.end();
    fboMotion.end();
}

void MotionExtraction::sampleControlValues() {
    ofFbo& accumFbo = pingPong ? fboAccumB : fboAccumA;

    ofPixels current;
    accumFbo.readToPixels(current);

    if (!prevAccumSet) {
        prevAccumPixels = current;
        prevAccumSet = true;
        return;
    }

    float totalDiff   = 0.f;
    float weightedX   = 0.f;
    float weightedY   = 0.f;
    float totalWeight = 0.f;
    int   W = current.getWidth(), H = current.getHeight();

    const int STEP = 4;
    for (int y = 0; y < H; y += STEP) {
        for (int x = 0; x < W; x += STEP) {
            ofColor c1 = current.getColor(x, y);
            ofColor c2 = prevAccumPixels.getColor(x, y);
            float diff = (std::fabs(c1.r - c2.r)
                        + std::fabs(c1.g - c2.g)
                        + std::fabs(c1.b - c2.b)) / (3.f * 255.f);
            totalDiff   += diff;
            weightedX   += diff * (float)x / W;
            weightedY   += diff * (float)y / H;
            totalWeight += diff;
        }
    }

    int sampleCount = (W / STEP) * (H / STEP);
    motionEnergy = totalDiff / sampleCount;

    if (totalWeight > 0.001f) {
        motionCentroidX = weightedX / totalWeight;
        motionCentroidY = weightedY / totalWeight;
    }

    prevAccumPixels = current;
}

void MotionExtraction::setOutputMode(int mode) {
    outputMode = ofClamp(mode, 0, 2);
}

ofTexture& MotionExtraction::getMotionTexture() {
    return fboMotion.getTexture();
}

// Returns the most recently rendered accumulation buffer
ofTexture& MotionExtraction::getAccumTexture() {
    return pingPong ? fboAccumB.getTexture() : fboAccumA.getTexture();
}
