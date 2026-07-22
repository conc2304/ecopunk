#include "FlowFieldWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void FlowFieldWidget::setup() { rebuild(); }
void FlowFieldWidget::randomize(int seed) { HudWidget::randomize(seed); rebuild(); }

void FlowFieldWidget::rebuild() {
    lines.clear();
    float density = ofClamp(options.density, 0.0f, 1.0f);
    int lineCount = std::max(1, (int)(options.lineCount * density));
    for (int i = 0; i < lineCount; ++i) {
        FlowLine f;
        float y = ofMap(i, 0, std::max(1, options.lineCount - 1), 0.18f, 0.82f);
        float amp = ofRandom(0.03f, 0.16f);
        int samples = 70;
        for (int s = 0; s < samples; ++s) {
            float u = s / static_cast<float>(samples - 1);
            float yy = y + std::sin(u * TWO_PI * ofRandom(0.8f, 1.8f) + i) * amp;
            f.path.addVertex(u, yy);
        }
        f.phase = ofRandom(1.0f);
        lines.push_back(f);
    }
}

void FlowFieldWidget::update(float dt) { HudWidget::update(dt); }

void FlowFieldWidget::draw() {
    ofPushStyle();
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
    frame.draw(bounds, theme.colors, theme.frame, time);
    ofSetLineWidth(std::max(1.0f, su(bounds, 1.0f)));

    float density = ofClamp(options.density, 0.0f, 1.0f);
    for (const auto& f : lines) {
        ofPolyline screen;
        for (const auto& v : f.path.getVertices()) { auto p = pointInBounds(bounds, v.x, v.y); screen.addVertex(p.x, p.y); }
        if (options.showCurves) {
            ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity * 0.45f));
            screen.draw();
        }
        int particlesPerLine = std::max(1, (int)(options.particlesPerLine * density));
        for (int p = 0; p < particlesPerLine; ++p) {
            float u = std::fmod(f.phase + time * 0.035f * motion.speed + p / static_cast<float>(options.particlesPerLine), 1.0f);
            ofVec3f pos = screen.getPointAtPercent(u);
            float r = std::max(1.4f, su(bounds, 1.6f + 1.6f * std::sin((u + time * 0.5f) * TWO_PI)));
            ofSetColor(scaledAlpha((p % 3 == 0) ? theme.colors.accent : theme.colors.secondary, motion.opacity * 0.85f));
            ofDrawCircle(pos.x, pos.y, r);
        }
    }
    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
