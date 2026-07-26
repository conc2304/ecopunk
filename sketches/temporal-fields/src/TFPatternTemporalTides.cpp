#include "TFPatternTemporalTides.h"
#include "TFRandom.h"
#include "TFPlayheadAssignment.h"
#include "ofMath.h"
#include "ofGraphics.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

void TFPatternTemporalTides::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternTemporalTides::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFPatternTemporalTides::reset(int seed) {
	std::srand(seed);
	// A random phase offset (not a position/content seed — the wave formula
	// itself is fully deterministic from t and position) so consecutive
	// cycles of this pattern don't all start at the same tide phase.
	elapsedTime = tfRandRangeF(0.0f, 1000.0f);
	rebuildGrid();
}

void TFPatternTemporalTides::rebuildGrid() {
	gridCols = std::max(1, params.gridResolution);
	gridRows = std::max(1, params.gridResolution);
	float cellW = static_cast<float>(canvasW) / gridCols;
	float cellH = static_cast<float>(canvasH) / gridRows;

	cells.clear();
	cells.reserve(gridCols * gridRows);
	for (int gy = 0; gy < gridRows; gy++) {
		for (int gx = 0; gx < gridCols; gx++) {
			Cell c;
			c.bounds = ofRectangle(gx * cellW, gy * cellH, cellW, cellH);
			cells.push_back(c);
		}
	}
}

void TFPatternTemporalTides::update(float dt) {
	elapsedTime += dt;

	std::vector<int> submergedIndices;
	std::vector<float> desiredOffsets;
	submergedIndices.reserve(cells.size());
	desiredOffsets.reserve(cells.size());

	for (size_t i = 0; i < cells.size(); i++) {
		Cell& c = cells[i];
		float nx = (c.bounds.x + c.bounds.width * 0.5f) / canvasW;
		float ny = (c.bounds.y + c.bounds.height * 0.5f) / canvasH;

		float phaseCoord;
		switch (params.waveDirection) {
			case WaveDirection::VERTICAL: phaseCoord = ny; break;
			case WaveDirection::DIAGONAL: phaseCoord = (nx + ny) * 0.70710678f; break;
			case WaveDirection::HORIZONTAL:
			default: phaseCoord = nx; break;
		}

		float waveValue = ofClamp(
			0.5f + 0.5f * std::sin(elapsedTime * params.tideSpeed - phaseCoord * params.waveLength * TWO_PI) * params.amplitude,
			0.0f, 1.0f);

		if (waveValue < params.exposedThreshold) {
			// Exposed/dry ground this frame — a real hole, excluded before
			// the pool assignment below entirely so it doesn't consume a
			// shared playhead slot a submerged cell could use instead (same
			// policy as TFPatternBSP's transparency-noise exclusion).
			c.playheadIndex = -1;
			continue;
		}

		submergedIndices.push_back(static_cast<int>(i));
		desiredOffsets.push_back(videoBuffer->quantize(waveValue));
	}

	std::vector<int> playheadIndices;
	tfAssignPlayheadsByDesiredOffsets(*videoBuffer, desiredOffsets, playheadIndices);

	for (size_t k = 0; k < submergedIndices.size(); k++) {
		cells[submergedIndices[k]].playheadIndex = playheadIndices[k];
	}
}

void TFPatternTemporalTides::draw() {
	ofEnableAlphaBlending();
	ofSetColor(255);

	float bufW = static_cast<float>(videoBuffer->getBufferWidth());
	float bufH = static_cast<float>(videoBuffer->getBufferHeight());

	for (const auto& c : cells) {
		if (c.playheadIndex < 0) {
			// Exposed ground — skip the draw call entirely (Section 0),
			// showing whatever's already drawn beneath composition.draw() in
			// ofApp::draw() through the gap.
			continue;
		}

		const ofTexture& tex = videoBuffer->getPlayheadTexture(c.playheadIndex);
		if (!tex.isAllocated()) {
			continue;
		}

		ofRectangle srcRect(
			(c.bounds.x / canvasW) * bufW,
			(c.bounds.y / canvasH) * bufH,
			(c.bounds.width / canvasW) * bufW,
			(c.bounds.height / canvasH) * bufH);

		// Direct draw, no TFFragmentTransition — there is no discrete "cut"
		// moment for this pattern to animate between (see the class comment).
		tex.drawSubsection(c.bounds.x, c.bounds.y, c.bounds.width, c.bounds.height,
			srcRect.x, srcRect.y, srcRect.width, srcRect.height);
	}
}

std::vector<ofVec2f> TFPatternTemporalTides::getActiveFragmentCenters() const {
	std::vector<ofVec2f> centers;
	centers.reserve(cells.size());
	for (const auto& c : cells) {
		if (c.playheadIndex < 0) {
			continue;
		}
		centers.push_back(ofVec2f(
			(c.bounds.x + c.bounds.width * 0.5f) / canvasW,
			(c.bounds.y + c.bounds.height * 0.5f) / canvasH));
	}
	return centers;
}
