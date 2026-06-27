#include "NodeNetworkWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

void NodeNetworkWidget::setup() { rebuild(); }
void NodeNetworkWidget::randomize(int seed) { HudWidget::randomize(seed); rebuild(); }

void NodeNetworkWidget::rebuild() {
    nodes.clear();
    for (int i = 0; i < options.nodeCount; ++i) {
        Node n;
        n.p = {ofRandom(0.08f, 0.92f), ofRandom(0.12f, 0.88f)};
        float a = ofRandom(TWO_PI);
        n.v = {std::cos(a) * ofRandom(0.003f, 0.018f), std::sin(a) * ofRandom(0.003f, 0.018f)};
        n.phase = ofRandom(TWO_PI);
        n.energy = ofRandom(0.45f, 1.0f);
        nodes.push_back(n);
    }
}

ofVec2f NodeNetworkWidget::toScreen(const ofVec2f& p) const { return pointInBounds(bounds, p.x, p.y); }

void NodeNetworkWidget::update(float dt) {
    HudWidget::update(dt);
    for (auto& n : nodes) {
        n.p += n.v * dt * motion.drift;
        if (options.wrap) {
            if (n.p.x < 0.02f) n.p.x = 0.98f; if (n.p.x > 0.98f) n.p.x = 0.02f;
            if (n.p.y < 0.02f) n.p.y = 0.98f; if (n.p.y > 0.98f) n.p.y = 0.02f;
        } else {
            if (n.p.x < 0.05f || n.p.x > 0.95f) n.v.x *= -1.0f;
            if (n.p.y < 0.05f || n.p.y > 0.95f) n.v.y *= -1.0f;
        }
    }
}

void NodeNetworkWidget::draw() {
    ofPushStyle();
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
    frame.draw(bounds, theme.colors, theme.frame, time);

    float maxD = bounds.minDim() * options.connectionDistance;
    ofSetLineWidth(std::max(1.0f, su(bounds, 0.8f)));
    for (size_t i = 0; i < nodes.size(); ++i) {
        for (size_t j = i + 1; j < nodes.size(); ++j) {
            ofVec2f a = toScreen(nodes[i].p);
            ofVec2f b = toScreen(nodes[j].p);
            float d = a.distance(b);
            if (d < maxD) {
                float u = 1.0f - d / maxD;
                ofSetColor(scaledAlpha(theme.colors.muted, motion.opacity * u * 0.7f));
                ofDrawLine(a, b);
                if (options.showPackets && ((i + j) % 7 == 0)) {
                    float k = std::fmod(time * 0.18f + i * 0.13f + j * 0.07f, 1.0f);
                    ofVec2f p = a.getInterpolated(b, k);
                    ofSetColor(scaledAlpha(theme.colors.accent, motion.opacity * u));
                    ofDrawCircle(p, std::max(1.5f, su(bounds, 1.8f)));
                }
            }
        }
    }

    for (const auto& n : nodes) {
        float pulse = breathe(time + n.phase, 0.4f, 0.65f, 1.0f);
        ofSetColor(scaledAlpha(theme.colors.secondary, motion.opacity * n.energy * pulse));
        ofDrawCircle(toScreen(n.p), std::max(1.5f, su(bounds, 2.2f) * pulse));
    }

    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
