#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"

namespace hud {

struct ScannerOptions {
    int rings = 4;
    int ticks = 72;
    bool showSweep = true;
    bool showCrosshair = true;
    bool showPulses = true;
    bool showBackground = true;
};

class ScannerWidget : public HudWidget {
public:
    void setOptions(const ScannerOptions& next) { options = next; }
    void update(float dt) override;
    void draw() override;

private:
    ScannerOptions options;
    HudFrameRenderer frame;
    float rotation = 0.0f;
    float pulsePhase = 0.0f;
};

} // namespace hud
