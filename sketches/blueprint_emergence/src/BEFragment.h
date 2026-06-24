#pragma once

#include "Fragment.h"
#include "glm/vec2.hpp"

// If your project already defines GeometryType somewhere else, keep that one
// and remove this enum from this header. In the original Blueprint Emergence
// layout, BEFragment.h is the natural place for it.
enum class GeometryType {
	RECT,
	CIRCLE,
	SLIVER,
	SQUARE
};

class BEFragment : public Fragment {
public:
	void setupBE(Fragment::Params params, GeometryType geometryType_, int canvasW, int canvasH);

protected:
	void drawArrival(float t) const override;
	void drawStable() const override;

private:
	void drawScanReveal(float elapsed) const;
	void drawClockwiseBorder(glm::vec2 pos, float w, float h, float t) const;
	void drawSlideIn(float elapsed) const;
	void drawIrisOpen(float elapsed) const;

	GeometryType geometryType = GeometryType::RECT;
	glm::vec2 slideStartPos { 0, 0 };
	float targetRadius = 0.0f;
};
