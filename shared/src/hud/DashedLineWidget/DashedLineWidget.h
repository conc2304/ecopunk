#pragma once
#include "../shared/HudWidget.h"
#include <vector>

namespace hud {

struct DashedLineOptions {
	float drawOnDuration = 0.6f; // seconds to grow from A to B
	float holdDuration    = 1.6f; // seconds fully drawn before fading
	float fadeOutDuration = 0.3f;
	float dashLenNorm      = 0.012f; // fraction of bounds.minDim()
	float gapLenNorm       = 0.008f;
};

// One-shot: a dashed line that grows from point A to point B over
// drawOnDuration, holds, then fades — Handshake's connecting line between
// two reticles. Nothing existing in this library draws a growing dashed
// connector.
class DashedLineWidget : public HudWidget {
public:
	void setOptions(const DashedLineOptions& next) { options = next; }
	void triggerBetween(float nxA, float nyA, float nxB, float nyB);
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 40.0f, 10.0f }; }

private:
	struct Line {
		ofVec2f a, b;
		float age = 0.0f;
	};
	std::vector<Line> lines;
	DashedLineOptions options;
};

} // namespace hud
