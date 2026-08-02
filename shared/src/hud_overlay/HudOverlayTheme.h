#pragma once
#include "HudElements.h"
#include "HudOverlayDialState.h"

namespace hudoverlay {

// Never more than one accent color active at once: ground (near-black,
// applied as the harness's clear color, not here), one accent, white/gray
// for structural marks — design doc Section 05's Palette dial. No other
// color logic.
inline hud::HudTheme themeForPalette(Palette palette) {
	hud::HudTheme theme;
	theme.colors.background = ofColor(13, 13, 13, 40);
	theme.colors.muted      = ofColor(200, 200, 200, 60);
	theme.colors.primary    = ofColor(225, 225, 225, 210);

	switch (palette) {
		case Palette::Orange:
			theme.colors.secondary = ofColor(255, 150, 60, 200);
			theme.colors.accent    = ofColor(255, 150, 60, 220);
			break;
		case Palette::Lime:
			theme.colors.secondary = ofColor(170, 255, 60, 200);
			theme.colors.accent    = ofColor(170, 255, 60, 220);
			break;
		case Palette::Mono:
		default:
			theme.colors.secondary = ofColor(225, 225, 225, 190);
			theme.colors.accent    = ofColor(255, 255, 255, 220);
			break;
	}

	theme.frame.style = hud::FrameStyle::None; // atoms draw their own marks; no widget frame chrome
	theme.additive     = true;
	return theme;
}

} // namespace hudoverlay
