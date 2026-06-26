#pragma once
#include "ofMain.h"

class MotionExtraction {
public:
    void setup();
    void update(ofTexture& videoTex, float decayWeight, float sensitivity);
    void setOutputMode(int mode);

    ofTexture& getMotionTexture();
    ofTexture& getAccumTexture();

    int   getOutputMode()    const { return outputMode; }
    float getMotionEnergy()    const { return motionEnergy; }
    float getMotionCentroidX() const { return motionCentroidX; }
    float getMotionCentroidY() const { return motionCentroidY; }

    static constexpr int ACCUM_W = 160;
    static constexpr int ACCUM_H = 90;

private:
    ofFbo    fboAccumA, fboAccumB;
    bool     pingPong = false;
    ofShader accumShader;

    ofFbo    fboMotion;
    ofShader extractShader;
    int      outputMode = 0;

    float motionEnergy    = 0.f;
    float motionCentroidX = 0.5f;
    float motionCentroidY = 0.5f;

    // Frame-to-frame accum diff for motion energy
    ofPixels prevAccumPixels;
    bool     prevAccumSet = false;

    void updateAccum(ofTexture& videoTex, float decayWeight);
    void updateExtract(ofTexture& videoTex, float sensitivity);
    void sampleControlValues();
};
