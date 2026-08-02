#pragma once

#include "ofRectangle.h"
#include "ofTexture.h"
#include "ofGraphics.h"
#include "ofMath.h"

// "background-size: cover" for ofTexture — crops the longer axis, centered,
// never stretches. Copied from temporal-fields/src/TFTextureCropFill.h (same
// need here: FTBackgroundLayer's video/image sections aren't canvas-proportional
// like a fragment's own bounds, so a plain draw() would distort).
inline ofRectangle ftComputeCropFillSrcRect(float texW, float texH, const ofRectangle& destRect) {
	if (texW <= 0 || texH <= 0 || destRect.width <= 0 || destRect.height <= 0) {
		return ofRectangle(0, 0, texW, texH);
	}

	float texAspect = texW / texH;
	float destAspect = destRect.width / destRect.height;

	if (texAspect > destAspect) {
		float srcW = texH * destAspect;
		return ofRectangle((texW - srcW) * 0.5f, 0, srcW, texH);
	}

	float srcH = texW / destAspect;
	return ofRectangle(0, (texH - srcH) * 0.5f, texW, srcH);
}

inline void ftDrawTextureCroppedToFill(const ofTexture& tex, const ofRectangle& destRect, float alpha = 1.0f) {
	if (!tex.isAllocated()) {
		return;
	}
	ofRectangle src = ftComputeCropFillSrcRect(tex.getWidth(), tex.getHeight(), destRect);
	ofSetColor(255, 255, 255, static_cast<int>(ofClamp(alpha, 0.0f, 1.0f) * 255));
	tex.drawSubsection(destRect.x, destRect.y, destRect.width, destRect.height, src.x, src.y, src.width, src.height);
	ofSetColor(255);
}
