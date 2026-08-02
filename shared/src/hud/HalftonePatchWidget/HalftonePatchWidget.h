#pragma once
#include "../shared/HudWidget.h"
#include <vector>

namespace hud {

struct HalftonePatchOptions {
	float duration    = 0.25f;
	float cellSizeNorm = 0.018f; // fraction of bounds.minDim()
};

// One-shot: a halftone/checkerboard patch that flickers over a region for a
// short duration — Fault Cascade's "halftone/checkerboard patch flickers
// over the same region, overlapping the tear" component. Nothing existing
// in this library draws a flickering dither patch.
class HalftonePatchWidget : public HudWidget {
public:
	void setOptions(const HalftonePatchOptions& next) { options = next; }
	// Region is normalized, bounds-relative.
	void triggerAt(float nx, float ny, float wNorm, float hNorm);
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 20.0f, 20.0f }; }

private:
	struct Patch {
		float nx, ny, w, h;
		float age = 0.0f;
	};
	std::vector<Patch> patches;
	HalftonePatchOptions options;
};

} // namespace hud
