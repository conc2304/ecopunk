#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"

namespace hud {

struct ContourOptions {
    int contourCount = 12;
    int samples = 96;
    float noiseScale = 1.8f;
    bool fillBands = false;
};

class ContourWidget : public HudWidget {
public:
    void setOptions(const ContourOptions& next) { options = next; }
    void update(float dt) override;
    void draw() override;
    ofVec2f getMinSize() const override { return { 160.0f, 80.0f }; }
private:
    ContourOptions options;
    HudFrameRenderer frame;
};

} // namespace hud
