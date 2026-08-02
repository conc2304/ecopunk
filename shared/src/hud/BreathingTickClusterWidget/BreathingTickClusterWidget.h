#pragma once
#include "../shared/HudWidget.h"
#include <string>
#include <vector>

namespace hud {

struct BreathingTickClusterOptions {
	int   ticksPerAnchor = 4;
	float breathPeriod   = 2.5f;  // seconds per opacity/scale pulse cycle
	float driftPeriod    = 10.0f; // seconds per slow positional drift loop
	float driftAmountNorm = 0.01f;
};

// Continuous ambient atom: fixed anchor points, each a small cluster of
// parallel ticks of increasing length that pulse opacity/scale on a
// sine-like cycle (staggered per tick), plus a static zone label and a
// slow whole-cluster positional drift separate from the per-tick breathing.
class BreathingTickClusterWidget : public HudWidget {
public:
	void setup() override;
	void setOptions(const BreathingTickClusterOptions& next) { options = next; }
	// Anchor positions, normalized bounds-relative, one zone label each.
	// Replaces any anchors set by setup()'s default layout.
	void setAnchors(const std::vector<ofVec2f>& positionsNorm, const std::vector<std::string>& labels);
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 100.0f, 100.0f }; }

private:
	struct Anchor {
		ofVec2f posNorm;
		std::string label;
		float driftPhase = 0.0f;
	};
	std::vector<Anchor> anchors;
	BreathingTickClusterOptions options;
};

} // namespace hud
