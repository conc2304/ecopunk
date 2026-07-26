#include "TFPatternTelescopingFrames.h"
#include "TFFragmentShape.h"
#include "TFRandom.h"
#include "ofGraphics.h"
#include "ofMath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
	constexpr float NOISE_SCALE = 2.2f;
	// ~3x slower for the "airy and calm" pass (was 0.02) — matches BSP.
	constexpr float NOISE_TIME_SPEED = 0.0067f;
	constexpr float MIN_THICKNESS = 4.0f; // px — keeps every strip's fragment visibly drawable
}

void TFPatternTelescopingFrames::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternTelescopingFrames::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFPatternTelescopingFrames::reset(int seed) {
	std::srand(seed);
	noiseTime = tfRandRangeF(0.0f, 1000.0f);
	regenTimer = 0.0f;
	regenerateFrames();
	tfAssignFragmentPlayheadsAndTransitions(*videoBuffer, canvasW, canvasH, noiseTime, NOISE_SCALE,
		params.transitionDuration, params.hardCutWeight, params.crossfadeWeight, params.erosionWeight,
		fragments, onFragmentReassignedCb);
}

void TFPatternTelescopingFrames::update(float dt) {
	noiseTime += dt * NOISE_TIME_SPEED;

	regenTimer += dt;
	if (regenTimer >= params.patternRegenRate) {
		regenTimer -= params.patternRegenRate;
		regenerateFrames();
	}

	tfAssignFragmentPlayheadsAndTransitions(*videoBuffer, canvasW, canvasH, noiseTime, NOISE_SCALE,
		params.transitionDuration, params.hardCutWeight, params.crossfadeWeight, params.erosionWeight,
		fragments, onFragmentReassignedCb);
	tfUpdateFragmentTransitions(fragments, dt);
}

void TFPatternTelescopingFrames::draw() {
	for (auto& f : fragments) {
		tfDrawPatternFragment(f, *videoBuffer, canvasW, canvasH);
	}
}

void TFPatternTelescopingFrames::regenerateFrames() {
	fragments.clear();

	ofRectangle current(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
	int n = std::max(1, params.ringCount);
	float variation = ofClamp(params.thicknessVariation, 0.0f, 1.0f);

	auto pushRect = [this](const ofRectangle& r) {
		TFPatternFragment f;
		f.shape.kind = TFShapeKind::RECT;
		f.shape.bounds = r;
		f.samplePos = ofVec2f(r.x + r.width * 0.5f, r.y + r.height * 0.5f);
		fragments.push_back(std::move(f));
	};

	for (int i = 0; i < n; i++) {
		float minEdge = std::min(current.width, current.height);
		float maxThickness = minEdge * 0.5f - 2.0f; // leaves at least a few px for the remainder
		if (maxThickness < MIN_THICKNESS) {
			break; // no room for another ring — `current` becomes the solid center below
		}

		float remainingRings = static_cast<float>(n - i);
		float baseThickness = maxThickness / remainingRings;
		float w = 1.0f + tfRandRangeF(-variation, variation);
		float thickness = ofClamp(baseThickness * std::max(0.15f, w), MIN_THICKNESS, maxThickness);

		ofRectangle inner(current.x + thickness, current.y + thickness,
			current.width - thickness * 2.0f, current.height - thickness * 2.0f);

		for (const ofRectangle& strip : tfDecomposeFrameToRects(current, inner)) {
			pushRect(strip);
		}

		current = inner;
	}

	// The innermost remaining area — a plain solid rectangle fragment, same
	// as any other rect-shaped fragment elsewhere in this sketch.
	pushRect(current);
}

std::vector<ofVec2f> TFPatternTelescopingFrames::getActiveFragmentCenters() const {
	return tfCollectFragmentCenters(fragments, canvasW, canvasH);
}
