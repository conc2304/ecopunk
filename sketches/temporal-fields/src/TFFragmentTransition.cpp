#include "TFFragmentTransition.h"
#include "TFRandom.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofMath.h"
#include <algorithm>
#include <cmath>

void TFFragmentTransition::begin(Style style_, float duration_, const ofRectangle& destBounds,
	const ofTexture& oldTex, const ofRectangle& oldSrcRect) {
	style = style_;
	duration = std::max(0.01f, duration_);
	elapsed = 0.0f;
	active = (style != Style::HARD_CUT);

	if (!active) {
		return;
	}

	int w = std::max(1, static_cast<int>(std::round(destBounds.width)));
	int h = std::max(1, static_cast<int>(std::round(destBounds.height)));

	if (!snapshotFbo.isAllocated() || static_cast<int>(snapshotFbo.getWidth()) != w
		|| static_cast<int>(snapshotFbo.getHeight()) != h) {
		ofFbo::Settings s;
		s.width = w;
		s.height = h;
		s.internalformat = GL_RGBA;
		s.useDepth = false;
		snapshotFbo.allocate(s);
	}

	snapshotFbo.begin();
	ofClear(0, 0, 0, 0);
	ofSetColor(255);
	oldTex.drawSubsection(0, 0, static_cast<float>(w), static_cast<float>(h),
		oldSrcRect.x, oldSrcRect.y, oldSrcRect.width, oldSrcRect.height);
	snapshotFbo.end();
}

void TFFragmentTransition::update(float dt) {
	if (!active) {
		return;
	}
	elapsed += dt;
	if (elapsed >= duration) {
		active = false;
	}
}

void TFFragmentTransition::draw(const ofRectangle& destBounds, const ofTexture& newTex, const ofRectangle& newSrcRect, float alpha) {
	int alpha255 = static_cast<int>(ofClamp(alpha, 0.0f, 1.0f) * 255);

	if (!active) {
		ofSetColor(255, 255, 255, alpha255);
		newTex.drawSubsection(destBounds.x, destBounds.y, destBounds.width, destBounds.height,
			newSrcRect.x, newSrcRect.y, newSrcRect.width, newSrcRect.height);
		return;
	}

	float progress = ofClamp(elapsed / duration, 0.0f, 1.0f);

	if (style == Style::CROSSFADE) {
		ofEnableAlphaBlending();

		ofSetColor(255, 255, 255, alpha255);
		newTex.drawSubsection(destBounds.x, destBounds.y, destBounds.width, destBounds.height,
			newSrcRect.x, newSrcRect.y, newSrcRect.width, newSrcRect.height);

		ofSetColor(255, 255, 255, static_cast<int>(alpha255 * (1.0f - progress)));
		snapshotFbo.getTexture().draw(destBounds.x, destBounds.y, destBounds.width, destBounds.height);
		ofSetColor(255);
		return;
	}

	// EROSION
	ofShader& shader = dissolveShader();
	float texW = newTex.getWidth();
	float texH = newTex.getHeight();

	ofSetColor(255, 255, 255, alpha255);
	shader.begin();
	// Explicit setUniformTexture for both — not a bind()+ofDrawRectangle()
	// pair. ofDrawRectangle() after a raw bind() doesn't reliably emit the
	// per-vertex UVs a shader needs (that's what ofTexture::draw() is for);
	// without them texCoordVarying degenerates and every sample in the
	// shader reads the same texel, which is exactly the flat-color bug
	// this replaced.
	shader.setUniformTexture("fromTex", snapshotFbo.getTexture(), 0);
	shader.setUniformTexture("toTex", newTex, 1);
	shader.setUniform4f("toUVRect", newSrcRect.x / texW, newSrcRect.y / texH,
		(newSrcRect.x + newSrcRect.width) / texW, (newSrcRect.y + newSrcRect.height) / texH);
	shader.setUniform1f("progress", progress);

	// Draws using fromTex's own texture (already bound to unit 0 above) so
	// the emitted quad carries fromTex-correct UVs in texCoordVarying.
	snapshotFbo.getTexture().draw(destBounds.x, destBounds.y, destBounds.width, destBounds.height);
	shader.end();
}

ofShader& TFFragmentTransition::dissolveShader() {
	static ofShader shader;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		if (!shader.load("shaders/fragmentDissolve.vert", "shaders/fragmentDissolve.frag")) {
			ofLogError("TFFragmentTransition") << "failed to load fragmentDissolve shader";
		}
	}
	return shader;
}

TFFragmentTransition::Style tfPickTransitionStyle(float hardCutWeight, float crossfadeWeight, float erosionWeight) {
	float total = std::max(0.0001f, hardCutWeight + crossfadeWeight + erosionWeight);
	float r = tfRandRangeF(0.0f, total);

	if (r < hardCutWeight) {
		return TFFragmentTransition::Style::HARD_CUT;
	}
	r -= hardCutWeight;
	if (r < crossfadeWeight) {
		return TFFragmentTransition::Style::CROSSFADE;
	}
	return TFFragmentTransition::Style::EROSION;
}
