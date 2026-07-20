#include "ContourWidget.h"
#include "HudUtils.h"

namespace hud {

void ContourWidget::update(float dt) { HudWidget::update(dt); }

void ContourWidget::draw() {
    ofPushStyle();
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
    frame.draw(bounds, theme.colors, theme.frame, time);
    ofNoFill();
    ofSetLineWidth(std::max(1.0f, su(bounds, 0.7f)));
    for (int i = 0; i < options.contourCount; ++i) {
        float baseY = ofMap(i, 0, std::max(1, options.contourCount - 1), 0.12f, 0.88f);
        ofPolyline line;
        for (int s = 0; s < options.samples; ++s) {
            float u = s / static_cast<float>(options.samples - 1);
            float n = ofNoise(u * options.noiseScale, i * 0.17f, time * 0.025f * motion.speed);
            float y = baseY + (n - 0.5f) * 0.18f;
            auto p = pointInBounds(bounds, u, y); line.addVertex(p.x, p.y);
        }
        float a = ofMap(i, 0, options.contourCount - 1, 0.28f, 0.72f);
        ofSetColor(scaledAlpha((i % 4 == 0) ? theme.colors.secondary : theme.colors.muted, motion.opacity * a));
        line.draw();
    }
    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
