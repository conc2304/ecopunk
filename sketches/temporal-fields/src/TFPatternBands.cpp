#include "TFPatternBands.h"
#include "TFRandom.h"
#include "ofGraphics.h"
#include "ofMath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
	constexpr float NOISE_SCALE = 2.2f;
	constexpr float NOISE_TIME_SPEED = 0.0067f; // matches BSP's calmer drift speed

	// Randomized-width-with-floor: same conceptual approach as Blob Grid's
	// size variation (TFPatternBlobGrid::Params::sizeVariation) — 0 gives
	// perfectly even bands, 1 gives highly uneven ones. Not shared code with
	// Blob Grid since that variation chooses a merge count, not a continuous
	// width weight, but the "randomize with a floor so nothing collapses to
	// zero" idea is the same.
	std::vector<float> randomizedWidthWeights(int n, float variation) {
		std::vector<float> weights(n);
		constexpr float floorWeight = 0.15f;
		for (int i = 0; i < n; i++) {
			float w = 1.0f + tfRandRangeF(-variation, variation);
			weights[i] = std::max(floorWeight, w);
		}
		return weights;
	}
}

void TFPatternBands::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternBands::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFPatternBands::reset(int seed) {
	std::srand(seed);
	noiseTime = tfRandRangeF(0.0f, 1000.0f);
	regenTimer = 0.0f;
	regenerateBands();
	assignOffsets();
}

void TFPatternBands::update(float dt) {
	noiseTime += dt * NOISE_TIME_SPEED;

	regenTimer += dt;
	if (regenTimer >= params.patternRegenRate) {
		regenTimer -= params.patternRegenRate;
		regenerateBands();
	}

	assignOffsets();
	tfUpdateFragmentTransitions(fragments, dt);
}

void TFPatternBands::draw() {
	bool diagonal = params.orientation == Orientation::DIAGONAL;

	if (diagonal) {
		// Cheap GPU-side rotation, not a per-vertex computation — bands are
		// generated oversized (see regenerateBands()) so this never leaves
		// gaps at the corners. See Section 2 of the brief.
		ofPushMatrix();
		ofTranslate(canvasW * 0.5f, canvasH * 0.5f);
		ofRotateDeg(params.diagonalAngleDeg);
		ofTranslate(-canvasW * 0.5f, -canvasH * 0.5f);
	}

	for (auto& f : fragments) {
		tfDrawPatternFragment(f, *videoBuffer, canvasW, canvasH);
	}

	if (diagonal) {
		ofPopMatrix();
	}
}

void TFPatternBands::regenerateBands() {
	fragments.clear();

	switch (params.orientation) {
		case Orientation::VERTICAL:
			appendBandStrip(ofRectangle(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH)), true);
			break;

		case Orientation::HORIZONTAL:
			appendBandStrip(ofRectangle(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH)), false);
			break;

		case Orientation::AXIS_FLIP: {
			// Split down the middle: left zone vertical-banded, right zone
			// horizontal-banded, meeting at the boundary.
			float splitX = canvasW * 0.5f;
			appendBandStrip(ofRectangle(0, 0, splitX, static_cast<float>(canvasH)), true);
			appendBandStrip(ofRectangle(splitX, 0, canvasW - splitX, static_cast<float>(canvasH)), false);
			break;
		}

		case Orientation::DIAGONAL: {
			// Oversized virtual canvas (1.3x the diagonal), centered on the
			// real canvas, generated BEFORE the draw()-time rotation is
			// applied — guarantees no corner gaps once rotated.
			float side = std::hypot(static_cast<float>(canvasW), static_cast<float>(canvasH)) * 1.3f;
			float offsetX = canvasW * 0.5f - side * 0.5f;
			float offsetY = canvasH * 0.5f - side * 0.5f;
			appendBandStrip(ofRectangle(offsetX, offsetY, side, side), true);
			break;
		}
	}
}

void TFPatternBands::appendBandStrip(const ofRectangle& zone, bool vertical) {
	int n = std::max(1, params.bandCount);
	std::vector<float> weights = randomizedWidthWeights(n, ofClamp(params.widthVariation, 0.0f, 1.0f));

	float sum = 0.0f;
	for (float w : weights) {
		sum += w;
	}

	float extent = vertical ? zone.width : zone.height;
	float cursor = vertical ? zone.x : zone.y;

	for (int i = 0; i < n; i++) {
		float w = (weights[i] / sum) * extent;

		TFPatternFragment f;
		f.shape.kind = TFShapeKind::RECT;
		if (vertical) {
			f.shape.bounds = ofRectangle(cursor, zone.y, w, zone.height);
		} else {
			f.shape.bounds = ofRectangle(zone.x, cursor, zone.width, w);
		}
		cursor += w;
		f.samplePos = ofVec2f(f.shape.bounds.x + f.shape.bounds.width * 0.5f, f.shape.bounds.y + f.shape.bounds.height * 0.5f);

		fragments.push_back(std::move(f));
	}
}

void TFPatternBands::assignOffsets() {
	if (params.offsetMode == OffsetMode::STRATA) {
		int n = static_cast<int>(fragments.size());
		std::vector<float> desiredOffsets(n);
		for (int i = 0; i < n; i++) {
			desiredOffsets[i] = (n > 1) ? static_cast<float>(i) / static_cast<float>(n - 1) : 0.0f;
		}
		tfAssignFragmentPlayheadsAndTransitionsWithOffsets(
			*videoBuffer, canvasW, canvasH, desiredOffsets,
			params.transitionDuration, params.hardCutWeight, params.crossfadeWeight, params.erosionWeight,
			fragments, onFragmentReassignedCb);
		return;
	}

	tfAssignFragmentPlayheadsAndTransitions(
		*videoBuffer, canvasW, canvasH, noiseTime, NOISE_SCALE,
		params.transitionDuration, params.hardCutWeight, params.crossfadeWeight, params.erosionWeight,
		fragments, onFragmentReassignedCb);
}

std::vector<ofVec2f> TFPatternBands::getActiveFragmentCenters() const {
	std::vector<ofVec2f> centers = tfCollectFragmentCenters(fragments, canvasW, canvasH);

	if (params.orientation != Orientation::DIAGONAL) {
		return centers;
	}

	// Diagonal bands are drawn under an ofRotateDeg() transform around
	// canvas center (see draw()) — rotate each normalized center the same
	// way so a HUD ripple originates from where the band actually appears
	// on screen, not its pre-rotation local position.
	float rad = ofDegToRad(params.diagonalAngleDeg);
	float cosA = std::cos(rad);
	float sinA = std::sin(rad);

	for (auto& c : centers) {
		float px = (c.x - 0.5f) * canvasW;
		float py = (c.y - 0.5f) * canvasH;
		float rx = px * cosA - py * sinA;
		float ry = px * sinA + py * cosA;
		c.x = 0.5f + rx / canvasW;
		c.y = 0.5f + ry / canvasH;
	}
	return centers;
}
