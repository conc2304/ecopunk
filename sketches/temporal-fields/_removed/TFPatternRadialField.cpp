#include "TFPatternRadialField.h"
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

	// Randomized-extent-with-floor, same conceptual approach as Bands' width
	// variation — distributes `n` shares of `totalExtent` (360 degrees for
	// Wedges, the outer radius in pixels for Rings) with a floor so no
	// segment collapses to nothing.
	std::vector<float> randomizedBoundaries(int n, float variation, float totalExtent) {
		std::vector<float> weights(n);
		constexpr float floorWeight = 0.15f;
		for (int i = 0; i < n; i++) {
			float w = 1.0f + tfRandRangeF(-variation, variation);
			weights[i] = std::max(floorWeight, w);
		}
		float sum = 0.0f;
		for (float w : weights) {
			sum += w;
		}

		std::vector<float> boundaries;
		boundaries.reserve(n + 1);
		boundaries.push_back(0.0f);
		float cursor = 0.0f;
		for (float w : weights) {
			cursor += (w / sum) * totalExtent;
			boundaries.push_back(cursor);
		}
		boundaries.back() = totalExtent; // avoid float drift at the far edge
		return boundaries;
	}
}

void TFPatternRadialField::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternRadialField::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFPatternRadialField::reset(int seed) {
	std::srand(seed);
	noiseTime = tfRandRangeF(0.0f, 1000.0f);
	regenTimer = 0.0f;
	regenerateField();
	tfAssignFragmentPlayheadsAndTransitions(*videoBuffer, canvasW, canvasH, noiseTime, NOISE_SCALE,
		params.transitionDuration, params.hardCutWeight, params.crossfadeWeight, params.erosionWeight,
		fragments, onFragmentReassignedCb);
}

void TFPatternRadialField::update(float dt) {
	noiseTime += dt * NOISE_TIME_SPEED;

	regenTimer += dt;
	if (regenTimer >= params.patternRegenRate) {
		regenTimer -= params.patternRegenRate;
		regenerateField();
	}

	tfAssignFragmentPlayheadsAndTransitions(*videoBuffer, canvasW, canvasH, noiseTime, NOISE_SCALE,
		params.transitionDuration, params.hardCutWeight, params.crossfadeWeight, params.erosionWeight,
		fragments, onFragmentReassignedCb);
	tfUpdateFragmentTransitions(fragments, dt);
}

void TFPatternRadialField::draw() {
	for (auto& f : fragments) {
		tfDrawPatternFragment(f, *videoBuffer, canvasW, canvasH);
	}
}

void TFPatternRadialField::regenerateField() {
	fragments.clear();

	ofVec2f center(canvasW * 0.5f, canvasH * 0.5f);
	// Slightly past the corner distance so a full sweep never leaves a
	// sliver of canvas uncovered at the corners.
	float maxRadius = std::hypot(canvasW * 0.5f, canvasH * 0.5f) * 1.02f;
	int n = ofClamp(params.segmentCount, 2, 24);
	float variation = ofClamp(params.widthVariation, 0.0f, 1.0f);

	if (params.mode == Mode::WEDGES) {
		std::vector<float> angleBoundaries = randomizedBoundaries(n, variation, 360.0f);

		for (int i = 0; i < n; i++) {
			TFPatternFragment f;
			f.shape.kind = TFShapeKind::WEDGE;
			f.shape.center = center;
			f.shape.innerRadius = 0.0f;
			f.shape.outerRadius = maxRadius;
			f.shape.angleBeginDeg = angleBoundaries[i];
			f.shape.angleEndDeg = angleBoundaries[i + 1];
			// Bounding-box fallback for transitions (Section 7): the full
			// outer-radius square, same for every wedge regardless of its
			// own angular span.
			f.shape.bounds = ofRectangle(center.x - maxRadius, center.y - maxRadius, maxRadius * 2.0f, maxRadius * 2.0f);

			float midAngleRad = ofDegToRad((f.shape.angleBeginDeg + f.shape.angleEndDeg) * 0.5f);
			float sampleRadius = maxRadius * 0.6f;
			f.samplePos = ofVec2f(center.x + sampleRadius * std::cos(midAngleRad), center.y + sampleRadius * std::sin(midAngleRad));

			fragments.push_back(std::move(f));
		}
	} else {
		std::vector<float> radiusBoundaries = randomizedBoundaries(n, variation, maxRadius);

		for (int i = 0; i < n; i++) {
			TFPatternFragment f;
			f.shape.kind = TFShapeKind::WEDGE;
			f.shape.center = center;
			f.shape.innerRadius = radiusBoundaries[i];
			f.shape.outerRadius = radiusBoundaries[i + 1];
			f.shape.angleBeginDeg = 0.0f;
			f.shape.angleEndDeg = 360.0f;
			// Bounding-box fallback: this ring's own outer radius, not the
			// pattern-wide max — a tighter, still-square box.
			float r = f.shape.outerRadius;
			f.shape.bounds = ofRectangle(center.x - r, center.y - r, r * 2.0f, r * 2.0f);

			float sampleRadius = (f.shape.innerRadius + f.shape.outerRadius) * 0.5f;
			f.samplePos = ofVec2f(center.x + sampleRadius, center.y); // angle is meaningless for a full sweep — any point at the mean radius is representative

			fragments.push_back(std::move(f));
		}
	}
}

std::vector<ofVec2f> TFPatternRadialField::getActiveFragmentCenters() const {
	return tfCollectFragmentCenters(fragments, canvasW, canvasH);
}
