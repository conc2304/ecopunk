#pragma once
#include "../shared/HudFrameRenderer.h"
#include "../shared/HudWidget.h"

namespace hud {

enum class GaugeStyle { Ring,
	Segmented,
	SemiCircle };
struct GaugeOptions {
	GaugeStyle style = GaugeStyle::Ring;
	float value = 0.72f;
	int segments = 28;
	std::string label = "BIO SIGNAL";
	std::string units = "%";
	bool showValue = true;
};

class GaugeWidget : public HudWidget {
public:
	void setOptions(const GaugeOptions & next) { options = next; }
	void setValue(float v) { options.value = ofClamp(v, 0.0f, 1.0f); }
	void update(float dt) override;
	void draw() override;
	ofVec2f getMinSize() const override { return { 100.0f, 100.0f }; }

private:
	GaugeOptions options;
	HudFrameRenderer frame;
};

} // namespace hud
