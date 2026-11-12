#pragma once

#include "GraphicBlock.h"

namespace hud {

struct TechnicalGridStyle {
	float majorSpacing = 80.0f; // px between major lines
	int minorDivisions = 4; // minor lines per major cell (1 = none)
	ofColor color = ofColor(0);
	float majorAlpha = 0.35f;
	float minorAlpha = 0.12f;
	float majorWidth = 1.0f;
	float minorWidth = 1.0f;
	float alpha = 1.0f; // overall multiplier (fade in/out)
	RevealDir revealDir = RevealDir::None; // wipe reveal
	float reveal = 1.0f;
	// Minor lines closer than this (px) are dropped so the grid survives 720p.
	float minMinorSpacing = 14.0f;
};

// Major/minor grid clipped to `rect` (lines are generated inside it, no GL
// scissor, so it's safe inside FBOs).
class TechnicalGrid {
public:
	static void draw(const ofRectangle & rect, const TechnicalGridStyle & style);
};

}
