#pragma once

#include "ofMain.h"
#include "HudTypes.h"

// sx/sy/su are the responsive-scaling helpers that make widgets render
// relative to their actual HudBounds rather than a fixed screen size.
namespace hud {

inline float sx(const HudBounds & b, float valueAtBaseWidth, float baseWidth = 200.0f) {
	return valueAtBaseWidth * (b.width / baseWidth);
}

inline float sy(const HudBounds & b, float valueAtBaseHeight, float baseHeight = 120.0f) {
	return valueAtBaseHeight * (b.height / baseHeight);
}

inline float su(const HudBounds & b, float valueAtBase, float base = 160.0f) {
	return valueAtBase * (b.minDim() / base);
}

inline ofColor withAlpha(ofColor c, float alpha01) {
	c.a = static_cast<unsigned char>(ofClamp(alpha01, 0.0f, 1.0f) * 255.0f);
	return c;
}

inline ofColor scaledAlpha(ofColor c, float scale) {
	c.a = static_cast<unsigned char>(ofClamp((c.a / 255.0f) * scale, 0.0f, 1.0f) * 255.0f);
	return c;
}

inline float breathe(float t, float rate = 1.0f, float low = 0.75f, float high = 1.0f) {
	float u = 0.5f + 0.5f * std::sin(t * rate * TWO_PI);
	return ofLerp(low, high, u);
}

inline ofVec2f pointInBounds(const HudBounds & b, float nx, float ny) {
	return ofVec2f(b.x + nx * b.width, b.y + ny * b.height);
}

inline void drawTextFallback(const std::string & text, float x, float y, float scale = 1.0f) {
	ofPushMatrix();
	ofTranslate(x, y);
	ofScale(scale, scale);
	ofDrawBitmapString(text, 0, 0);
	ofPopMatrix();
}

} // namespace hud
