#pragma once

#include "ofMain.h"
#include "TimeOffsetVideoBuffer.h"
#include "FTImageCycler.h"
#include "HudElements.h"
#include <string>

// Full-canvas backdrop drawn beneath the fragment pool, so the travelling
// fragments read as bright peepholes over a coherent field instead of flat
// black — the same role temporal-fields' TFBackgroundLayer + TFHudLayer's
// underlay widgets play (see that sketch's src/TFBackgroundLayer.cpp,
// src/TFHudLayer.cpp's drawUnderlay()). Deliberately trimmed to what this
// brief asked for, matching FTParameterPanel's "much smaller than the
// original" precedent: no per-effect cycling, no multi-widget rotation —
// just an axis-aligned split (dimmed raw video on one side always; the
// other side alternates between video and a background image on a timer,
// when any images are present — see FTImageCycler::hasImages()) plus one
// fixed ambient HUD widget drawn full-canvas at low opacity.
class FTBackgroundLayer {
	public:
		void setup(TimeOffsetVideoBuffer* videoBuffer, const std::string& imagesFolder, int canvasW, int canvasH);
		// draw() derives every rect from canvasW/canvasH fresh each call, so
		// this is the only state a resize needs to update.
		void resizeCanvas(int canvasW_, int canvasH_);
		void update(float dt);
		void draw();

	private:
		void pickNextAltSection();
		void drawVideoDimmed(const ofTexture& tex, const ofRectangle& destRect);

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		FTImageCycler imageCycler;
		hud::HexGridWidget hexGrid;
		ofShader bgShader; // desaturates + dims the background video (bg_dim.glsl) — kept
		                    // separate from ShaderLibrary's fragment effects, whose single
		                    // `alpha` uniform conflates mix-amount and output opacity, which
		                    // doesn't work here since saturation and opacity are independent.

		int canvasW = 0;
		int canvasH = 0;

		// Video always dimmed, not full-strength — keeps fragments reading as
		// the brighter, sharper foreground layer. 0.35 base, dimmed a further
		// 20% per user request (0.35 * 0.8 = 0.28).
		static constexpr float kVideoAlpha = 0.28f;
		// 50% desaturated per user request — 1.0 would be original color, 0.0 fully grayscale.
		static constexpr float kVideoSaturation = 0.5f;

		bool splitIsVertical = true;
		float splitRatio = 0.62f;
		bool altSectionShowsImage = false;
		float sectionTimer = 0.0f;
		float sectionInterval = 18.0f;
};
