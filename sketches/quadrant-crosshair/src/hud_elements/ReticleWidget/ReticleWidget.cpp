#include "ReticleWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void ReticleWidget::setup() { rebuild(); }
void ReticleWidget::randomize(int seed) { HudWidget::randomize(seed); rebuild(); }

void ReticleWidget::rebuild() {
    static const std::vector<std::string> kDefaultLabels = {"CANOPY", "FLOW", "SPORE", "ROOT", "SIGNAL", "WATER"};
    const std::vector<std::string>& labels =
        options.labelOverride.empty() ? kDefaultLabels : options.labelOverride;
    targets.clear();
    for (int i = 0; i < options.targetCount; ++i) {
        Target t;
        t.p = {ofRandom(0.15f, 0.85f), ofRandom(0.18f, 0.82f)};
        t.phase = ofRandom(TWO_PI);
        t.size = ofRandom(0.06f, 0.14f);
        t.label = labels[i % (int)labels.size()];
        targets.push_back(t);
    }
}

void ReticleWidget::update(float dt) { HudWidget::update(dt); }

void ReticleWidget::draw() {
    ofPushStyle();
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
    frame.draw(bounds, theme.colors, theme.frame, time);
    ofNoFill();
    ofSetLineWidth(std::max(1.0f, su(bounds, 1.0f) * 0.8f));
    for (const auto& t : targets) {
        ofVec2f p = pointInBounds(bounds, t.p.x, t.p.y);
        float s = bounds.minDim() * t.size * breathe(time + t.phase, 0.35f, 0.82f, 1.16f) * 0.2f;
        ofSetColor(scaledAlpha(theme.colors.primary, motion.opacity * 0.72f));
        ofDrawLine(p.x - s, p.y - s, p.x - s * 0.45f, p.y - s);
        ofDrawLine(p.x - s, p.y - s, p.x - s, p.y - s * 0.45f);
        ofDrawLine(p.x + s, p.y - s, p.x + s * 0.45f, p.y - s);
        ofDrawLine(p.x + s, p.y - s, p.x + s, p.y - s * 0.45f);
        ofDrawLine(p.x - s, p.y + s, p.x - s * 0.45f, p.y + s);
        ofDrawLine(p.x - s, p.y + s, p.x - s, p.y + s * 0.45f);
        ofDrawLine(p.x + s, p.y + s, p.x + s * 0.45f, p.y + s);
        ofDrawLine(p.x + s, p.y + s, p.x + s, p.y + s * 0.45f);
        ofSetColor(scaledAlpha(theme.colors.accent, motion.opacity * 0.85f));
        ofDrawCircle(p, std::max(1.2f, su(bounds, 1.5f)));
        if (options.showLabels) {
            ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity * 0.75f));
            drawTextFallback(t.label, p.x + s + 4.0f, p.y - s * 0.25f, theme.textScale * su(bounds, 0.5f));
        }
    }
    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
