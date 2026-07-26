#include "TFPatternColumnGrid.h"
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

	// Randomized-height-with-floor, same conceptual approach as Bands' width
	// variation (see TFPatternBands.cpp) — kept as its own small local copy
	// rather than a shared helper, since each is a handful of lines.
	std::vector<float> randomizedHeightWeights(int n, float variation) {
		std::vector<float> weights(n);
		constexpr float floorWeight = 0.15f;
		for (int i = 0; i < n; i++) {
			float w = 1.0f + tfRandRangeF(-variation, variation);
			weights[i] = std::max(floorWeight, w);
		}
		return weights;
	}

	// Cumulative row boundaries [0, ..., totalHeight] from a fresh
	// randomized-weight draw.
	std::vector<float> buildRowBoundaries(int n, float variation, float totalHeight) {
		std::vector<float> weights = randomizedHeightWeights(std::max(1, n), variation);
		float sum = 0.0f;
		for (float w : weights) {
			sum += w;
		}

		std::vector<float> boundaries;
		boundaries.reserve(weights.size() + 1);
		boundaries.push_back(0.0f);

		float cursor = 0.0f;
		for (float w : weights) {
			cursor += (w / sum) * totalHeight;
			boundaries.push_back(cursor);
		}
		boundaries.back() = totalHeight; // avoid float drift at the far edge
		return boundaries;
	}

	// Shifts every interior boundary down by half the first row's height,
	// wrapping anything past totalHeight back to the top — the masonry/
	// running-bond stagger Brick Offset asks for, built by cyclically
	// offsetting one shared boundary sequence rather than drawing a second
	// independent one.
	std::vector<float> staggerBoundaries(const std::vector<float>& boundaries, float totalHeight) {
		float shift = (boundaries.size() >= 2) ? (boundaries[1] - boundaries[0]) * 0.5f : 0.0f;

		std::vector<float> result;
		result.push_back(0.0f);
		for (size_t i = 1; i + 1 < boundaries.size(); i++) {
			float b = boundaries[i] + shift;
			if (b > totalHeight) {
				b -= totalHeight;
			}
			result.push_back(b);
		}
		result.push_back(totalHeight);

		std::sort(result.begin() + 1, result.end() - 1);
		return result;
	}
}

void TFPatternColumnGrid::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternColumnGrid::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFPatternColumnGrid::reset(int seed) {
	std::srand(seed);
	noiseTime = tfRandRangeF(0.0f, 1000.0f);
	regenTimer = 0.0f;
	regenerateColumns();
	tfAssignFragmentPlayheadsAndTransitions(*videoBuffer, canvasW, canvasH, noiseTime, NOISE_SCALE,
		params.transitionDuration, params.hardCutWeight, params.crossfadeWeight, params.erosionWeight,
		fragments, onFragmentReassignedCb);
}

void TFPatternColumnGrid::update(float dt) {
	noiseTime += dt * NOISE_TIME_SPEED;

	regenTimer += dt;
	if (regenTimer >= params.patternRegenRate) {
		regenTimer -= params.patternRegenRate;
		regenerateColumns();
	}

	tfAssignFragmentPlayheadsAndTransitions(*videoBuffer, canvasW, canvasH, noiseTime, NOISE_SCALE,
		params.transitionDuration, params.hardCutWeight, params.crossfadeWeight, params.erosionWeight,
		fragments, onFragmentReassignedCb);
	tfUpdateFragmentTransitions(fragments, dt);
}

void TFPatternColumnGrid::draw() {
	for (auto& f : fragments) {
		tfDrawPatternFragment(f, *videoBuffer, canvasW, canvasH);
	}
}

void TFPatternColumnGrid::regenerateColumns() {
	fragments.clear();

	int cols = std::max(1, params.columnCount);
	float colW = static_cast<float>(canvasW) / cols;
	float variation = ofClamp(params.rowHeightVariation, 0.0f, 1.0f);

	std::vector<float> sharedBoundaries;
	std::vector<float> shiftedBoundaries;
	if (params.brickOffset) {
		sharedBoundaries = buildRowBoundaries(params.rowsPerColumn, variation, static_cast<float>(canvasH));
		shiftedBoundaries = staggerBoundaries(sharedBoundaries, static_cast<float>(canvasH));
	}

	for (int c = 0; c < cols; c++) {
		std::vector<float> boundaries;
		if (params.brickOffset) {
			boundaries = (c % 2 == 0) ? sharedBoundaries : shiftedBoundaries;
		} else {
			// Fully independent per-column draw — the ledger/spreadsheet look.
			boundaries = buildRowBoundaries(params.rowsPerColumn, variation, static_cast<float>(canvasH));
		}

		float x = c * colW;
		for (size_t r = 0; r + 1 < boundaries.size(); r++) {
			TFPatternFragment f;
			f.shape.kind = TFShapeKind::RECT;
			f.shape.bounds = ofRectangle(x, boundaries[r], colW, boundaries[r + 1] - boundaries[r]);
			f.samplePos = ofVec2f(f.shape.bounds.x + f.shape.bounds.width * 0.5f, f.shape.bounds.y + f.shape.bounds.height * 0.5f);
			fragments.push_back(std::move(f));
		}
	}
}

std::vector<ofVec2f> TFPatternColumnGrid::getActiveFragmentCenters() const {
	return tfCollectFragmentCenters(fragments, canvasW, canvasH);
}
