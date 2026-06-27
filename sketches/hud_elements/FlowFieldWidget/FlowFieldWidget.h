#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"

namespace hud {

struct FlowFieldOptions {
    int lineCount = 8;
    int particlesPerLine = 9;
    bool showCurves = true;
};

class FlowFieldWidget : public HudWidget {
public:
    void setup() override;
    void randomize(int seed = -1) override;
    void setOptions(const FlowFieldOptions& next) { options = next; rebuild(); }
    void update(float dt) override;
    void draw() override;
private:
    struct FlowLine { ofPolyline path; float phase = 0.0f; };
    std::vector<FlowLine> lines;
    FlowFieldOptions options;
    HudFrameRenderer frame;
    void rebuild();
};

} // namespace hud
