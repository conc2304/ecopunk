#pragma once

#include <functional>
#include <vector>
#include "TFPattern.h"
#include "TFShapeFragmentRenderer.h"
#include "TimeOffsetVideoBuffer.h"
#include "ofRectangle.h"
#include "ofVec2f.h"

// Concentric rectangular rings nested from full-canvas size inward toward a
// solid center rectangle, each ring's thickness randomized. Every ring is
// decomposed into up to four plain RECT fragments (tfDecomposeFrameToRects()
// in TFFragmentShape.h) rather than drawn as a true annulus shape — cheaper
// and simpler than path winding, and since the decomposition IS the ring's
// exact silhouette, transitions/erosion never need a bounding-box fallback
// here (unlike a WEDGE shape would). Regenerates wholesale on the shared
// Pattern Regen Rate clock — see docs/temporal-fields-new-patterns-brief.md
// Section 5.
class TFPatternTelescopingFrames : public TFPattern {
	public:
		struct Params {
			int ringCount = 6;
			float thicknessVariation = 0.35f; // 0 = even ring thickness, 1 = highly uneven
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
		void regenerateFrames();

		TimeOffsetVideoBuffer* videoBuffer = nullptr;
		int canvasW = 0;
		int canvasH = 0;
		Params params{};

		std::vector<TFPatternFragment> fragments;
		float regenTimer = 0.0f;
		float noiseTime = 0.0f;

		std::function<void(float nx, float ny)> onFragmentReassignedCb;
};
