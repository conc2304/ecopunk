#pragma once

// ============================================================================
// BESectionCut — the SECTION CUT visual grammar of blueprint_emergence.
//
// An architectural section drawing: a moving cut line slices the video frame
// into two pieces that pull apart. One piece is the plain view, the other is
// the "sectioned" side (palette overprint + diagonal hatching). The gap shows
// a dash-dot cut line with a dimension arrow, section markers (A—A) sit at
// the ends of the cut, and a second cross cut (B—B) opens on the sectioned
// piece now and then. The cut drifts, the gap breathes, the hatched side
// swaps, and every so often the cut closes, flips orientation, recrops and
// opens again. Pieces keep their true position in the picture (the gap hides
// a strip of the image), and keep sampling the live texture.
// Seeded, dt-driven, palette tokens only, shared HUD primitives only.
// ============================================================================

#include "BEGrammar.h"
#include "hud/graphics/GraphicPrimitives.h"
#include "ofMesh.h"
#include "ofRectangle.h"
#include "ofTexture.h"
#include "ofTrueTypeFont.h"

#include <array>
#include <cstdint>
#include <random>
#include <string>

class BESectionCut : public BEGrammar {
public:
	void setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize);
	void beginLayout(uint32_t seed) override;
	void reset() override { built_ = false; }
	void update(float dt) override;
	void draw(const ofTexture * video) const override;
	const char * displayName() const override { return "SECTION CUT"; }
	GrammarStats stats() const override;
	int eventCount() const { return eventCount_; }

private:
	float rnd(float a, float b);
	ofColor col(hud::PaletteToken t) const;
	void drawText(const std::string & s, float x, float y) const;
	ofRectangle area() const;
	void drawPiece(const ofTexture * v, const ofRectangle & area, const ofRectangle & d, float hatch, float alpha, int seedIdx, int fxIdx) const;
	void hatchRect(const ofRectangle & r, float spacing, const ofColor & c, float alpha) const;

	int canvasW_ = 1280, canvasH_ = 720;
	bool built_ = false;
	std::mt19937 rng_ { 1u };
	hud::HudCollagePalette pal_, prevPal_;
	float palMix_ = 1.0f;

	float time_ = 0, enter_ = 0;
	int orient_ = 0; // 0 = vertical cut (left/right pieces), 1 = horizontal cut (top/bottom)
	float cut_ = 0.5f, cutTarget_ = 0.5f; // cut position along its axis, 0..1
	float gap_ = 0.0f, gapTarget_ = 36.0f; // px
	float hatchMix_ = 1.0f, hatchTarget_ = 1.0f; // 0 = first piece hatched, 1 = second
	float sec_ = 0.0f, secTarget_ = 0.0f; // cross cut (B—B) amount on the second piece
	bool reorienting_ = false;
	float cx_ = 0.5f, cy_ = 0.5f, zoom_ = 1.1f, vx_ = 0.012f, vy_ = 0.008f;
	float moveTimer_ = 0, swapTimer_ = 0, secTimer_ = 0, reorientTimer_ = 0;
	int eventCount_ = 0;
	int sheetNo_ = 1;

	std::array<BEFxSlot, 3> fx_;
	mutable ofMesh hatchMesh_;
	mutable ofTrueTypeFont font_;
	bool fontLoaded_ = false;
};
