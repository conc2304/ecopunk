#pragma once

// ============================================================================
// BEExplodedView — the EXPLODED VIEW visual grammar of blueprint_emergence.
//
// One hero crop of the playing video is cut into parts that drift apart like
// an assembly drawing: dashed axes, elbowed leader lines to numbered callouts,
// a dimension bracket, an assembled reference thumbnail. The cycle is
// assemble -> explode -> hold (parts pull out and flash) -> collapse -> a
// NEW layout. Layouts vary every rebuild:
//   * kind     — ROWS (5 slabs), COLUMNS (5 slabs), GRID 2x2 (4 parts blown
//                outward from the centre), GRID 2x3 (two columns of three)
//   * anchor   — hero on the left, centre or right of the sheet
//   * format   — landscape or portrait (tall) hero
//   * labels   — on one side, or on BOTH sides (grids anchored centre)
// Parts keep sampling the live texture and can carry shared shader effects.
// Seeded, dt-driven, palette tokens only.
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

class BEExplodedView : public BEGrammar {
public:
	static constexpr int MAX_PARTS = 6;

	void setup(int canvasW, int canvasH, const std::string & fontPath, int fontSize);
	void beginLayout(uint32_t seed) override;
	void reset() override { built_ = false; }
	void update(float dt) override;
	void draw(const ofTexture * video) const override;
	const char * displayName() const override { return "EXPLODED VIEW"; }
	GrammarStats stats() const override;
	int eventCount() const { return eventCount_; }

	enum class Kind { Rows, Cols, Grid22, Grid23 };
	enum class Anchor { Left, Center, Right };
	struct Layout {
		Kind kind = Kind::Rows;
		Anchor anchor = Anchor::Center;
		bool portrait = false;
		bool labelsLeft = false; // single-sided labels
		bool bothSides = false; // labels on both sides (grid + centre anchor)
		int n = 5;
	};
	const Layout & layout() const { return lay_; }

private:
	enum class Phase { Assembled, Exploding, Held, Collapsing };
	struct Crop {
		float cx = 0.5f, cy = 0.5f, zoom = 1.0f;
	};

	float rnd(float a, float b);
	Crop newCrop();
	void pickLayout();
	ofColor col(hud::PaletteToken t) const;
	void drawText(const std::string & s, float x, float y) const;
	float textWidth(const std::string & s) const;
	ofRectangle heroRect() const;
	ofRectangle sourceRect(const ofTexture * v, const Crop & c, const ofRectangle & hero) const;
	ofRectangle partSource(const ofTexture * v, const Crop & c, const ofRectangle & hero, int i) const;
	ofRectangle partBase(int i, const ofRectangle & hero) const;
	glm::vec2 partOffset(int i) const;
	bool isGrid() const { return lay_.kind == Kind::Grid22 || lay_.kind == Kind::Grid23; }
	int gridRows() const { return lay_.kind == Kind::Grid22 ? 2 : 3; }
	bool partOnLeft(int i) const; // which side its callout sits on

	int canvasW_ = 1280, canvasH_ = 720;
	bool built_ = false;
	std::mt19937 rng_ { 1u };
	hud::HudCollagePalette pal_, prevPal_;
	float palMix_ = 1.0f;

	float time_ = 0, enter_ = 0;
	Phase phase_ = Phase::Assembled;
	float timer_ = 0, progress_ = 0;
	float expl_ = 0; // explosion amount 0..1(+breath)
	Layout lay_;
	bool haveLayout_ = false;
	float shearSign_ = 1.0f;
	Crop cur_, prev_;
	float mix_ = 1.0f;
	std::array<float, MAX_PARTS> pull_ {}, pullTarget_ {}, pullHold_ {}, flash_ {};
	std::array<BEFxSlot, MAX_PARTS> fx_;
	float nextPull_ = 3.0f;
	int assy_ = 1;
	int eventCount_ = 0;

	mutable ofTrueTypeFont font_;
	bool fontLoaded_ = false;
};
