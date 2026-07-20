#include "HexGridWidget.h"
#include "HudUtils.h"

namespace hud {

void HexGridWidget::update(float dt) { HudWidget::update(dt); }

void HexGridWidget::drawHex(float cx, float cy, float r, bool fill, const ofColor& c) const {
    ofPath path;
    path.setFilled(fill);
    path.setStrokeWidth(1.0f);
    path.setStrokeColor(c);
    path.setFillColor(c);
    for (int i = 0; i < 6; ++i) {
        float a = DEG_TO_RAD * (60.0f * i + 30.0f);
        float x = cx + std::cos(a) * r;
        float y = cy + std::sin(a) * r;
        if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
    }
    path.close();
    path.draw();
}

void HexGridWidget::draw() {
    ofPushStyle();
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
    frame.draw(bounds, theme.colors, theme.frame, time);
    float r = su(bounds, options.cellSize);
    float dx = r * 1.5f;
    float dy = r * std::sqrt(3.0f);
    int cols = static_cast<int>(bounds.width / dx) + 3;
    int rows = static_cast<int>(bounds.height / dy) + 3;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            float cx = bounds.x + x * dx + ((y % 2) ? dx * 0.5f : 0.0f);
            float cy = bounds.y + y * dy;
            float n = ofNoise(x * 0.19f, y * 0.23f, time * 0.08f * motion.speed);
            bool active = n > (1.0f - options.activation);
            ofColor c = active ? scaledAlpha(theme.colors.secondary, motion.opacity * 0.45f) : scaledAlpha(theme.colors.muted, motion.opacity * 0.18f);
            drawHex(cx, cy, r * 0.92f, options.filledCells && active, c);
        }
    }
    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
