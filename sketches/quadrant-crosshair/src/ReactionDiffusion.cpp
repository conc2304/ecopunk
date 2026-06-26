#include "ReactionDiffusion.h"

void ReactionDiffusion::setup() {
    ofFbo::Settings s;
    s.width          = RD_W;
    s.height         = RD_H;
    s.internalformat = GL_RGBA;
    s.useDepth       = false;
    fboA.allocate(s);
    fboB.allocate(s);

    pingPong = false;
    seedInitialState();

    rdShader.load("shaders/vert.glsl", "shaders/rd_step.glsl");
    buildQuad();
}

void ReactionDiffusion::seedInitialState() {
    fboA.begin();
    ofClear(255, 0, 0, 255);       // R=U=1, G=V=0
    ofSetColor(0, 255, 0, 255);
    ofDrawRectangle(RD_W / 2 - 8, RD_H / 2 - 8, 16, 16);
    fboA.end();

    fboB.begin();
    ofClear(255, 0, 0, 255);
    fboB.end();
}

void ReactionDiffusion::buildQuad() {
    // unused — drawing via ofTexture::draw() to get correct texcoords from vert.glsl
}

void ReactionDiffusion::update(float feedRate, float killRate) {
    ofFbo& src = pingPong ? fboB : fboA;
    ofFbo& dst = pingPong ? fboA : fboB;

    // ofFbo::begin() sets viewport + projection automatically for RD_W×RD_H
    dst.begin();
    rdShader.begin();
    rdShader.setUniformTexture("rdState", src.getTexture(), 0);
    rdShader.setUniform2f("resolution",  RD_W, RD_H);
    rdShader.setUniform1f("feedRate",    feedRate);
    rdShader.setUniform1f("killRate",    killRate);
    // ofTexture::draw() feeds gl_MultiTexCoord0 that vert.glsl forwards as vTexCoord
    src.getTexture().draw(0, 0, RD_W, RD_H);
    rdShader.end();
    dst.end();

    pingPong = !pingPong;
}

ofTexture& ReactionDiffusion::getTexture() {
    return pingPong ? fboA.getTexture() : fboB.getTexture();
}

void ReactionDiffusion::readPixels(ofPixels& out) {
    ofFbo& cur = pingPong ? fboA : fboB;
    cur.readToPixels(out);
}
