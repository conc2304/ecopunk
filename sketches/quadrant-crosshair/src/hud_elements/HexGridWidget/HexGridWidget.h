#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"

namespace hud {

struct HexGridOptions {
    float cellSize = 18.0f;
    float activation = 0.18f;
    bool filledCells = true;
};

class HexGridWidget : public HudWidget {
public:
    void setOptions(const HexGridOptions& next) { options = next; }
    void update(float dt) override;
    void draw() override;
private:
    HexGridOptions options;
    HudFrameRenderer frame;
    void drawHex(float cx, float cy, float r, bool fill, const ofColor& c) const;
};

} // namespace hud
