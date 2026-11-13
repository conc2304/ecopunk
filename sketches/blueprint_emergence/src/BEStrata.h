#pragma once

// ============================================================================
// BEStrata — the STRATA visual grammar of blueprint_emergence.
//
// The frame is cut into NUM_LANES horizontal slices (one per composition
// slot). Each lane samples its own row of the one canonical playing texture
// and slides it sideways at its own speed and direction, so the slices drift
// against each other like geological strata. Differences from the other
// Blueprint modes: no divider, no grid cells, no ghost residue, no rect
// composition — linear, scrolling and rhythmic.
//
// Lanes carry independent segments (video windows that wipe in/out), a
// scrolling tick rail, a label cell and a speed bar. Director events (seeded,
// dt-driven): speed ramp, freeze, row swap, segment turnover, group sync.
// Palette tokens only (<= 2 chromatic accents), shared HUD primitives only.
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

class BEStrata : public BEGrammar {
public:
	static constexpr int NUM_LANES = 6;
	static constexpr int MAX_SEGS = 3;

	using Stats = GrammarStats;

	void setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize);
	// First call builds; later calls recompose (new palette cross-fade, lanes re-rolled).
	void beginLayout(uint32_t seed) override;
	void reset() override { built_ = false; }
	void update(float dt) override;
	void draw(const ofTexture * video) const override;

	const char * displayName() const override { return "STRATA"; }
	GrammarStats stats() const override;
	uint32_t stateHash() const;
	int eventCount() const { return eventCount_; }

private:
	struct Segment {
		bool alive = false;
		float x0 = 0, x1 = 0; // lane-relative [0,1]
		float uOffset = 0; // source-u offset added to the lane scroll
		float reveal = 0; // wipe in/out amount
		float revealTarget = 0;
		float hold = 0; // seconds left before it starts leaving
		bool leftToRight = true;
		bool framed = false;
	};
	struct Lane {
		float speed = 0, speedTarget = 0, speedRate = 1; // source-widths / s
		float scroll = 0; // source-u scroll
		float tickPhase = 0;
		int row = 0; // which of the NUM_LANES source rows this lane shows
		float rowWipe = 1; // 0..1 re-reveal after a row swap
		float freeze = 0; // seconds remaining frozen
		float tint = 0; // overprint strength
		bool useSecondary = false;
		float nextRamp = 0;
		float nextTurnover = 0;
		float markPulse = 0;
		float enter = 0; // lane entry reveal 0..1
		float enterDelay = 0;
		std::array<Segment, MAX_SEGS> segs;
	};

	float rnd(float a, float b);
	int rndi(int a, int b);
	void buildLane(int i, bool initial);
	void spawnSegment(Lane & l, int slot);
	void directorTick(float dt);
	ofRectangle laneRect(int i) const;
	void drawText(const std::string & s, float x, float y) const;
	void drawSegment(const Lane & l, int laneIdx, const Segment & s, const ofRectangle & lr, const ofTexture * video) const;

	int canvasW_ = 1280, canvasH_ = 720;
	bool built_ = false;
	uint32_t seed_ = 0;
	std::mt19937 rng_ { 1u };
	std::array<Lane, NUM_LANES> lanes_;
	std::array<BEFxSlot, NUM_LANES> fx_;

	hud::HudCollagePalette pal_;
	hud::HudCollagePalette prevPal_;
	float palMix_ = 1.0f;
	ofColor col(hud::PaletteToken t) const;

	float time_ = 0;
	float syncTimer_ = 0, syncHold_ = 0, swapTimer_ = 0;
	float syncSpeed_ = 0;
	float scanX_ = 0, scanSpeed_ = 0.05f;
	float nextRecompose_ = 0;
	int eventCount_ = 0;

	mutable ofMesh tickMesh_;
	mutable ofTrueTypeFont font_;
	bool fontLoaded_ = false;
};
