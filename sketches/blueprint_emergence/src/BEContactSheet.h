#pragma once

// ============================================================================
// BEContactSheet — the CONTACT SHEET visual grammar of blueprint_emergence.
//
// A fixed 3x2 proof sheet: six numbered cells (one per composition slot), each
// sampling its own crop/zoom of the one canonical playing texture. The grid is
// rigid; the *contents* are marked up like a photographer's proof: cells
// recrop through a cross-fade, get circled (SELECT, the cell lifts), struck
// through (REJECT, it drains and refills), swap contents, renumber, and a
// scan highlight sweeps row by row. Registration marks, tick rails and a
// title strip frame it. Palette tokens only (<= 2 accents), shared HUD
// primitives only. Seeded and dt-driven like the other grammars.
// ============================================================================

#include "BEGrammar.h"
#include "hud/graphics/GraphicPrimitives.h"
#include "ofRectangle.h"
#include "ofTexture.h"
#include "ofTrueTypeFont.h"

#include <array>
#include <cstdint>
#include <random>
#include <string>

class BEContactSheet : public BEGrammar {
public:
	static constexpr int COLS = 3;
	static constexpr int ROWS = 2;
	static constexpr int CELLS = COLS * ROWS;

	using Stats = GrammarStats;

	void setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize);
	void beginLayout(uint32_t seed) override; // first call builds, later calls recompose
	void reset() override { built_ = false; }
	void update(float dt) override;
	void draw(const ofTexture * video) const override;

	const char * displayName() const override { return "CONTACT SHEET"; }
	GrammarStats stats() const override;
	int eventCount() const { return eventCount_; }

private:
	struct Crop {
		float cx = 0.5f, cy = 0.5f; // source centre, normalised
		float zoom = 1.0f; // 1 = whole frame, >1 = tighter
	};
	struct Cell {
		Crop cur, prev;
		float mix = 1.0f; // prev -> cur cross-fade
		float enter = 0.0f, enterDelay = 0.0f;
		float select = 0.0f, selectTarget = 0.0f; // circled + lifted
		float reject = 0.0f, rejectTarget = 0.0f; // struck + drained
		float flash = 0.0f; // scan/event highlight
		int frameNo = 1;
		float nextRecrop = 0.0f;
		bool tinted = false;
	};

	float rnd(float a, float b);
	int rndi(int a, int b);
	Crop newCrop(int cell);
	void directorTick(float dt);
	ofRectangle cellRect(int i) const;
	void drawText(const std::string & s, float x, float y) const;
	void drawCrop(const ofTexture * v, const ofRectangle & r, const Crop & c, float alpha) const;
	ofRectangle cropSource(const ofTexture * v, const ofRectangle & r, const Crop & c) const;
	ofColor col(hud::PaletteToken t) const;

	int canvasW_ = 1280, canvasH_ = 720;
	bool built_ = false;
	std::mt19937 rng_ { 1u };
	std::array<Cell, CELLS> cells_;
	std::array<BEFxSlot, CELLS> fx_;
	hud::HudCollagePalette pal_, prevPal_;
	float palMix_ = 1.0f;

	float time_ = 0;
	float selectTimer_ = 0, rejectTimer_ = 0, swapTimer_ = 0, sweepTimer_ = 0, renumberTimer_ = 0;
	float sweepPos_ = -1.0f; // row-major sweep position in cells, -1 = idle
	int eventCount_ = 0;
	int frameCounter_ = 100;

	mutable ofTrueTypeFont font_;
	bool fontLoaded_ = false;
};
