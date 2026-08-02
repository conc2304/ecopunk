#include "RPCompositor.h"

void RPCompositor::setup() {
	bool loaded = revealShader.load("shaders/color_reveal.vert", "shaders/color_reveal.frag");
	if (!loaded) {
		ofLogError("RPCompositor") << "failed to load color_reveal shader";
	}
}

void RPCompositor::draw(ofTexture & videoTex, ofTexture & maskTex, float width, float height) {
	ofSetColor(255);
	revealShader.begin();
	revealShader.setUniformTexture("videoTex", videoTex, 0);
	revealShader.setUniformTexture("maskTex", maskTex, 1);
	revealShader.setUniform1f("colorThreshold", colorThreshold);
	videoTex.draw(0, 0, width, height);
	revealShader.end();
}
