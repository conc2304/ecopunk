#pragma once

#include <functional>
#include <vector>
#include "TFPattern.h"
#include "TFShapeFragmentRenderer.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofRectangle.h"
#include "ofVec2f.h"

// Wedges (pie slices radiating from canvas center, full radius, randomized
// angular widths) or Rings (concentric full-sweep rings, randomized radial
// widths) — the two WEDGE-shape fragments this phase introduces (see
// TFFragmentShape.h). Regenerates wholesale on the shared Pattern Regen
// Rate clock, same as Bands/Column Grid — see docs/temporal-fields-new-
// patterns-brief.md Section 4.
class TFPatternRadialField : public TFPattern {
	public:
		enum class Mode { WEDGES, RINGS };

		struct Params {
			Mode mode = Mode::WEDGES;
			int segmentCount = 12; // clamped to [2, 24] — see Section 0's tessellation-cost caution
			float widthVariation = 0.3f; // 0 = even widths, 1 = highly uneven (angular for WEDGES, radial for RINGS)
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
		void regenerateField();

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		std::vector<TFPatternFragment> fragments;
		float regenTimer = 0.0f;
		float noiseTime = 0.0f;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
