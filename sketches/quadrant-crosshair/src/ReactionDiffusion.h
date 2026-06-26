#pragma once
#include "ofMain.h"

class ReactionDiffusion {
public:
    static constexpr int RD_W = 160;
    static constexpr int RD_H = 90;

    void setup();
    void update(float feedRate, float killRate);
    ofTexture& getTexture();
    void readPixels(ofPixels& out);  // full 160×90 readback, ~0.5ms on Pi 3B

private:
    ofFbo  fboA, fboB;
    bool   pingPong = false;
    ofShader rdShader;
    ofMesh   fullscreenQuad;

    void buildQuad();
    void seedInitialState();
};
