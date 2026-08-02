#pragma once
#include "ofMain.h"

// System I — Compositor. Consumes the live video texture (System F) and the
// reveal mask texture (System H) and draws the freshness-driven color/
// desaturation composite (color_reveal.frag).
class RPCompositor {
public:
	void setup();
	// Stretches videoTex to exactly (width, height) — the same coordinate
	// space the mask/pulse emitter are sized to — rather than the video's
	// own native resolution, so the reveal mask lines up with the video
	// regardless of the clip's actual dimensions/aspect ratio. Same
	// stretch-to-canvas convention as
	// QuadrantManager.cpp's videoTex.draw(0, 0, ofGetWidth(), ofGetHeight()).
	void draw(ofTexture & videoTex, ofTexture & maskTex, float width, float height);

	void setColorThreshold(float t) { colorThreshold = t; }

private:
	ofShader revealShader;
	float colorThreshold = 0.55f;
};
