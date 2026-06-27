#include "ScannerWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void ScannerWidget::update(float dt) {
    HudWidget::update(dt);
    rotation += dt * 18.0f * motion.speed;
    pulsePhase = std::fmod(pulsePhase + dt * 0.22f * motion.pulse, 1.0f);
}

void ScannerWidget::draw() {
    ofPushStyle();
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();

    frame.draw(bounds, theme.colors, theme.frame, time);

    ofVec2f c = bounds.center();
    float r = bounds.minDim() * 0.38f;
    float alpha = motion.opacity;

    ofNoFill();
    ofSetLineWidth(std::max(1.0f, su(bounds, 1.2f)));

    for (int i = 1; i <= options.rings; ++i) {
        float rr = r * (i / static_cast<float>(options.rings));
        ofSetColor(scaledAlpha(theme.colors.muted, alpha * (0.25f + 0.12f * i)));
        ofDrawCircle(c, rr);
    }

    ofPushMatrix();
    ofTranslate(c);
    ofRotateDeg(rotation);
    for (int i = 0; i < options.ticks; ++i) {
        float a = TWO_PI * i / static_cast<float>(options.ticks);
        float len = (i % 6 == 0) ? r * 0.08f : r * 0.035f;
        ofVec2f p0(std::cos(a) * (r - len), std::sin(a) * (r - len));
        ofVec2f p1(std::cos(a) * r, std::sin(a) * r);
        ofSetColor(scaledAlpha((i % 6 == 0) ? theme.colors.primary : theme.colors.muted, alpha * 0.65f));
        ofDrawLine(p0, p1);
    }
    ofPopMatrix();

    if (options.showCrosshair) {
        ofSetColor(scaledAlpha(theme.colors.muted, alpha * 0.55f));
        ofDrawLine(c.x - r, c.y, c.x + r, c.y);
        ofDrawLine(c.x, c.y - r, c.x, c.y + r);
    }

    if (options.showSweep) {
        ofPath sweep;
        sweep.setFilled(true);
        sweep.setColor(scaledAlpha(theme.colors.secondary, alpha * 0.24f));
        sweep.moveTo(c);
        float start = ofDegToRad(rotation);
        float span = ofDegToRad(42.0f);
        int steps = 18;
        for (int i = 0; i <= steps; ++i) {
            float a = start - span * (i / static_cast<float>(steps));
            sweep.lineTo(c.x + std::cos(a) * r, c.y + std::sin(a) * r);
        }
        sweep.close();
        sweep.draw();
    }

    if (options.showPulses) {
        float pr = r * pulsePhase;
        ofSetColor(scaledAlpha(theme.colors.accent, alpha * (1.0f - pulsePhase) * 0.7f));
        ofDrawCircle(c, pr);
    }

    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
