#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"
#include <vector>

namespace hud {

struct HexGridOptions {
    float cellSize = 18.0f;
    float activation = 0.18f;
    bool filledCells = true;

    // Ripple tuning — see pulseAt().
    float rippleLifetime = 2.5f; // seconds a ripple stays influential
    float rippleSpeed = 0.9f;    // normalized units/sec the ripple front expands
    float rippleWidth = 0.12f;   // normalized thickness of the active ring
    float rippleBoost = 0.6f;    // added to a cell's noise value at the ring
};

class HexGridWidget : public HudWidget {
public:
    void setOptions(const HexGridOptions& next) { options = next; }
    void update(float dt) override;
    void draw() override;
    ofVec2f getMinSize() const override { return { 120.0f, 80.0f }; }

    // Triggers a ring of boosted cell activation expanding outward from a
    // normalized [0,1] origin (relative to this widget's own bounds), so a
    // consumer can tie the grid's activation to a real event instead of
    // only its own ambient ofNoise(x, y, time) clock.
    void pulseAt(float nx, float ny);

private:
    struct Ripple { float nx = 0.5f, ny = 0.5f; float age = 0.0f; };
    std::vector<Ripple> ripples;
    HexGridOptions options;
    HudFrameRenderer frame;
    void drawHex(float cx, float cy, float r, bool fill, const ofColor& c) const;
};

} // namespace hud
