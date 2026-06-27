#include "GaugeWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void GaugeWidget::update(float dt) { HudWidget::update(dt); }

void GaugeWidget::draw() {
    ofPushStyle();
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
    frame.draw(bounds, theme.colors, theme.frame, time);
    ofVec2f c = bounds.center();
    float r = bounds.minDim() * 0.32f;
    float start = (options.style == GaugeStyle::SemiCircle) ? 200.0f : -90.0f;
    float span = (options.style == GaugeStyle::SemiCircle) ? 140.0f : 360.0f;

    ofNoFill();
    ofSetLineWidth(std::max(2.0f, su(bounds, 3.0f)));
    if (options.style == GaugeStyle::Segmented) {
        for (int i = 0; i < options.segments; ++i) {
            float u0 = i / static_cast<float>(options.segments);
            float u1 = (i + 0.55f) / static_cast<float>(options.segments);
            ofSetColor(scaledAlpha(u0 <= options.value ? theme.colors.secondary : theme.colors.muted, motion.opacity * (u0 <= options.value ? 0.85f : 0.35f)));
            ofDrawArc(c, r, r, start + span * u0, start + span * u1);
        }
    } else {
        ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity * 0.35f));
        ofDrawArc(c, r, r, start, start + span);
        ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity * 0.9f));
        ofDrawArc(c, r, r, start, start + span * options.value);
    }

    ofSetColor(scaledAlpha(theme.colors.primary, motion.opacity));
    drawTextFallback(options.label, bounds.x + bounds.width * 0.10f, bounds.y + bounds.height * 0.20f, theme.textScale * su(bounds, 0.7f));
    if (options.showValue) {
        int pct = static_cast<int>(std::round(options.value * 100.0f));
        drawTextFallback(ofToString(pct) + options.units, c.x - su(bounds, 17.0f), c.y + su(bounds, 4.0f), theme.textScale * su(bounds, 1.1f));
    }
    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
