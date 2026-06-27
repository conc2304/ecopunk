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
};

class ScannerWidget : public HudWidget {
public:
    void setOptions(const ScannerOptions& next) { options = next; }
    void update(float dt) override;
    void draw() override;

    // ── In-code dials ──────────────────────────────────────────────────
    float scaleMultiplier = 1.25f;   // 0.5 = half size, 2.0 = double
    float globalOpacity   = 1.0f;   // 0–1 master alpha multiplier
    float lineWidthScale  = 1.0f;   // line thickness multiplier
    float hueShift        = 0.0f;   // hue rotation in degrees (0–360)

private:
    ScannerOptions options;
    HudFrameRenderer frame;
    float rotation      = 0.0f;
    float pulsePhase    = 0.0f;
    // Sweep arc — independent from tick ring
    float sweepAngle    = 0.0f;
    float sweepDir      = 1.0f;   // +1 or -1
    float sweepSpeed    = 14.0f;  // deg/s, re-randomised on each flip
    float sweepDirTimer = 0.0f;   // counts down to next direction change
};

} // namespace hud
