#pragma once

#include <functional>
#include <vector>
#include "TFPattern.h"
#include "TFShapeFragmentRenderer.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofRectangle.h"
#include "ofVec2f.h"

// Fixed equal-width vertical columns, each independently subdivided into
// randomized-height row cells. Regenerates wholesale on the shared Pattern
// Regen Rate clock, same as Bands — see docs/temporal-fields-new-patterns-
// brief.md Section 3. Plain RECT TFPatternFragments throughout; the only
// pattern-specific logic is how row boundaries are generated (see
// regenerateColumns() in the .cpp).
class TFPatternColumnGrid : public TFPattern {
	public:
		struct Params {
			int columnCount = 6;
			int rowsPerColumn = 6; // target row count; brickOffset's stagger can add one extra partial row
			float rowHeightVariation = 0.35f; // 0 = even rows, 1 = highly uneven
			// true: row boundaries stagger between adjacent columns by half
			// a row-height, sharing one base row-height sequence across
			// columns (masonry/running-bond). false: each column generates
			// a fully independent row-height sequence (ledger/spreadsheet
			// look).
			bool brickOffset = true;
			float patternRegenRate = 12.0f;

			float transitionDuration = 0.8f;
			float hardCutWeight = 33.0f;
			float crossfadeWeight = 34.0f;
			float erosionWeight = 33.0f;
		};

		void setup(TimeOffsetVideoBuffer* videoBuffer, int canvasW, int canvasH, const Params& params);
		void setParams(const Params& p) { params = p; }

		void reset(int seed) override;
		void update(float dt) override;
		void draw() override;
		void resizeCanvas(int canvasW, int canvasH) override;

		void setOnFragmentReassigned(std::function<void(float nx, float ny)> cb) override { onFragmentReassignedCb = std::move(cb); }
		std::vector<ofVec2f> getActiveFragmentCenters() const override;

	private:
		void regenerateColumns();

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		std::vector<TFPatternFragment> fragments;
		float regenTimer = 0.0f;
		float noiseTime = 0.0f;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
