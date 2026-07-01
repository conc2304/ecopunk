#pragma once

#include "ofMain.h"
#include <vector>
#include <string>

// Ported from quadrant-crosshair/src/hud_elements/shared/HudTypes.h verbatim.
namespace hud {

enum class FrameStyle {
	None,
	Corners,
	Box,
	Brackets,
	Organic
};

struct FrameOptions {
	FrameStyle style = FrameStyle::Corners;
	bool showFrame = true;
	bool showTicks = true;
	bool showScanLines = false;
	float thickness = 1.25f;
	float cornerLength = 18.0f;
	float padding = 10.0f;
	float opacity = 0.75f;
};

struct WidgetColors {
	ofColor primary = ofColor(124, 232, 230, 220);
	ofColor secondary = ofColor(103, 255, 142, 190);
	ofColor accent = ofColor(244, 255, 106, 210);
	ofColor muted = ofColor(124, 232, 230, 70);
	ofColor background = ofColor(0, 20, 16, 36);
};

struct HudTheme {
	WidgetColors colors;
	FrameOptions frame;
	std::string fontPath = "";
	float textScale = 1.0f;
	bool additive = true;
};

struct HudBounds {
	float x = 0.0f;
	float y = 0.0f;
	float width = 200.0f;
	float height = 120.0f;

	ofRectangle rect() const { return ofRectangle(x, y, width, height); }
	ofVec2f center() const { return ofVec2f(x + width * 0.5f, y + height * 0.5f); }
	float minDim() const { return std::min(width, height); }
};

struct MotionSettings {
	float speed = 1.0f;
	float opacity = 1.0f;
	float drift = 1.0f;
	float pulse = 1.0f;
};

} // namespace hud
