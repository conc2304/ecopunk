#include "ErosionFBO.h"
#include "ofGraphics.h"
#include "ofLog.h"

void ErosionFBO::setup(int width_, int height_, float decayRate_) {
	width = width_;
	height = height_;
	decayRate = decayRate_;

	ofFbo::Settings s;
	s.width = width;
	s.height = height;
	s.internalformat = GL_RGBA;
	s.useDepth = false;

	captureFbo.allocate(s);
	fboA.allocate(s);
	fboB.allocate(s);

	captureFbo.begin();
	ofClear(0, 0, 0, 255);
	captureFbo.end();
	fboA.begin();
	ofClear(0, 0, 0, 255);
	fboA.end();
	fboB.begin();
	ofClear(0, 0, 0, 255);
	fboB.end();

	bool loaded = erosionShader.load("shaders/erosion.vert", "shaders/erosion.frag");
	if (!loaded) {
		ofLogError("ErosionFBO") << "failed to load erosion shader";
	}

	primed = false;
	ready = true;
}

void ErosionFBO::beginCapture() {
	captureFbo.begin();
	ofClear(0, 0, 0, 0);
}

void ErosionFBO::endCapture() {
	captureFbo.end();
}

void ErosionFBO::update() {
	if (!ready) {
		return;
	}

	if (!primed) {
		// First frame after setup()/clear(): copy straight through instead of
		// blending against the opaque-black initial history, which would
		// otherwise read as an extra ~0.5s fade-in from black on top of
		// whatever fade-in the scene itself is already doing.
		readFbo().begin();
		ofSetColor(255);
		captureFbo.getTexture().draw(0, 0, width, height);
		readFbo().end();
		primed = true;
	}

	writeFbo().begin();
	erosionShader.begin();
	erosionShader.setUniformTexture("history", readFbo().getTexture(), 0);
	erosionShader.setUniformTexture("current", captureFbo.getTexture(), 1);
	erosionShader.setUniform1f("decayRate", decayRate);
	erosionShader.setUniform1f("desatAmount", desatAmount);
	ofSetColor(255);
	captureFbo.getTexture().draw(0, 0, width, height);
	erosionShader.end();
	writeFbo().end();

	pingPong = !pingPong;
}

void ErosionFBO::draw(float x, float y) {
	draw(x, y, static_cast<float>(width), static_cast<float>(height));
}

void ErosionFBO::draw(float x, float y, float w, float h) {
	if (!ready) {
		return;
	}
	ofSetColor(255);
	readFbo().getTexture().draw(x, y, w, h);
}

void ErosionFBO::clear() {
	fboA.begin();
	ofClear(0, 0, 0, 255);
	fboA.end();
	fboB.begin();
	ofClear(0, 0, 0, 255);
	fboB.end();
	primed = false;
}
