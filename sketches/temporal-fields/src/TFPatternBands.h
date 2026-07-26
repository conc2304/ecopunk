#pragma once

#include <functional>
#include <vector>
#include "TFPattern.h"
#include "TFShapeFragmentRenderer.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofRectangle.h"
#include "ofVec2f.h"

// Parallel strips (vertical, horizontal, an axis-flip split into one zone
// of each, or diagonal via a cheap matrix rotation) with randomized widths.
// Regenerates wholesale on a shared Pattern Regen Rate clock, unlike BSP's
// piecemeal per-leaf resplitting — see docs/temporal-fields-new-patterns-
// brief.md Section 2. Fragments are plain TFPatternFragment RECTs; the only
// genuinely new mechanic here is the Strata offset mode (see assignOffsets()
// in the .cpp), everything else reuses TFShapeFragmentRenderer as-is.
class TFPatternBands : public TFPattern {
	public:
		enum class Orientation { VERTICAL, HORIZONTAL, AXIS_FLIP, DIAGONAL };

		// DYNAMIC: every band's offset drifts/reassigns from the shared
		// noise field, same as every other pattern. STRATA: each band's
		// offset is fixed at regen time, proportional to its own index
		// (offset = bandIndex / (bandCount - 1)) and never reassigned
		// afterward — an assignment-time policy difference only, not a
		// rendering change (Section 2).
		enum class OffsetMode { DYNAMIC, STRATA };

		struct Params {
			Orientation orientation = Orientation::VERTICAL;
			int bandCount = 10; // per zone, for AXIS_FLIP
			float widthVariation = 0.3f; // 0 = even widths, 1 = highly uneven
			float diagonalAngleDeg = 30.0f;
			OffsetMode offsetMode = OffsetMode::DYNAMIC;
			float patternRegenRate = 12.0f; // seconds between wholesale regenerations

			// Transition system — each pattern owns its own copy of the same
			// shared tunables, matching BSP/Blob Grid's precedent.
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
		void regenerateBands();
		void appendBandStrip(const ofRectangle& zone, bool vertical);
		void assignOffsets();

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		std::vector<TFPatternFragment> fragments;
		float regenTimer = 0.0f;
		float noiseTime = 0.0f;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
