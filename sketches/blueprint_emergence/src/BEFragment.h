#pragma once

#include "Fragment.h"

enum class GeometryType { RECT, SLIVER, SQUARE }; // CIRCLE added in Phase 4

// Blueprint Emergence's geometry-specific arrival animations (§04): scan
// reveal for RECT/SQUARE, slide-in (with a fading afterimage) for SLIVER.
class BEFragment : public Fragment {
	public:
		void setupBE(const ofRectangle& bounds, const ofColor& placeholderColor, float phaseOffset,
			GeometryType geometryType, int canvasW, int canvasH, glm::vec2 driftAmp, glm::vec2 driftFreq);

	protected:
		void drawArrival(float t) const override;
		void drawStable() const override;

	private:
		void drawScanReveal(float elapsedSeconds) const;   // RECT & SQUARE
		void drawSlideIn(float elapsedSeconds) const;       // SLIVER
		void drawClockwiseBorder(glm::vec2 pos, float w, float h, float t) const;

		GeometryType geometryType = GeometryType::RECT;
		glm::vec2 slideStartPos{0, 0};
};
