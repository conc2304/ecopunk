#pragma once

#include "Fragment.h"

enum class GeometryType { RECT, SLIVER, SQUARE, CIRCLE };

// Blueprint Emergence's geometry-specific arrival animations (§04): scan
// reveal for RECT/SQUARE, slide-in (with a fading afterimage) for SLIVER,
// iris-open (growing fill + leading outline) for CIRCLE.
class BEFragment : public Fragment {
	public:
		// params.arrivalDuration is overridden internally based on geometryType.
		// params.circularMask / maskRadius should be set by the caller for CIRCLE.
		void setupBE(Fragment::Params params, GeometryType geometryType, int canvasW, int canvasH);

	protected:
		void drawArrival(float t) const override;
		void drawStable() const override;

	private:
		void drawScanReveal(float elapsedSeconds) const;   // RECT & SQUARE
		void drawSlideIn(float elapsedSeconds) const;       // SLIVER
		void drawIrisOpen(float elapsedSeconds) const;      // CIRCLE
		void drawClockwiseBorder(glm::vec2 pos, float w, float h, float t) const;

		GeometryType geometryType = GeometryType::RECT;
		glm::vec2    slideStartPos{0, 0};
		float        targetRadius = 0.0f; // CIRCLE only: final radius, saved for ghost ring + drawStable
};
