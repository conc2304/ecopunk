#pragma once
#include "../shared/HudWidget.h"
#include <vector>

namespace hud {

struct TickBurstOptions {
	int   tickCount    = 4;     // strokes radiating from the trigger point
	float radiusStart  = 0.03f; // normalized (bounds.minDim()), where ticks begin
	float radiusEnd    = 0.09f; // where ticks end their outward travel
	float tickLength   = 0.025f;
	float duration     = 0.35f; // seconds a single burst stays visible
};

// One-shot: a small cluster of ticks radiates outward from a point and
// fades, e.g. Lock Sequence's "tick burst radiates outward" step. Nothing
// existing in this library covers a fire-once radial burst.
class TickBurstWidget : public HudWidget {
public:
	void setOptions(const TickBurstOptions& next) { options = next; }
	// Fires a burst immediately at (nx, ny), normalized bounds-relative.
	void triggerAt(float nx, float ny);
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 40.0f, 40.0f }; }

private:
	struct Burst {
		ofVec2f p;
		float age = 0.0f;
	};
	std::vector<Burst> bursts;
	TickBurstOptions options;
};

} // namespace hud
