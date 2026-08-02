#include "PulseEmitterWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

float PulseEmitterWidget::jitteredInterval() const {
    float jitter = ofRandom(-0.15f, 0.15f);
    return config.frequencyMs * (1.0f + jitter);
}

int PulseEmitterWidget::addEmitter(float x, float y) {
    Emitter e;
    e.id = nextEmitterId++;
    e.x = x;
    e.y = y;
    e.nextSpawnMs = jitteredInterval();
    emitters.push_back(e);
    return e.id;
}

void PulseEmitterWidget::update(float dt) {
    HudWidget::update(dt);
    float dtMs = dt * 1000.0f;

    for (auto & e : emitters) {
        e.nextSpawnMs -= dtMs;
        if (e.nextSpawnMs <= 0.0f) {
            int active = 0;
            for (auto & p : pulses) if (p.emitterId == e.id) ++active;
            if (active < config.concurrency) {
                pulses.push_back({ e.id, e.x, e.y, 0.0f });
            }
            e.nextSpawnMs = jitteredInterval();
        }
    }

    std::vector<Pulse> alive;
    alive.reserve(pulses.size());
    for (auto & p : pulses) {
        p.ageMs += dtMs;
        float radius = (p.ageMs / 1000.0f) * config.speed;
        if (radius < config.range) {
            alive.push_back(p);
            if (pulseCallback) {
                pulseCallback(PulseInfo{ p.x, p.y, radius, p.ageMs, p.emitterId });
            }
        }
    }
    pulses = std::move(alive);
}

std::vector<PulseInfo> PulseEmitterWidget::getActivePulses() const {
    std::vector<PulseInfo> out;
    out.reserve(pulses.size());
    for (const auto & p : pulses) {
        float radius = (p.ageMs / 1000.0f) * config.speed;
        out.push_back(PulseInfo{ p.x, p.y, radius, p.ageMs, p.emitterId });
    }
    return out;
}

void PulseEmitterWidget::draw() {
    ofPushStyle();
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
    frame.draw(bounds, theme.colors, theme.frame, time);

    ofNoFill();
    float lineW = std::max(1.0f, su(bounds, 1.4f));
    ofSetLineWidth(lineW);

    for (const auto & p : pulses) {
        float radius = (p.ageMs / 1000.0f) * config.speed;
        float lifeT = ofClamp(radius / config.range, 0.0f, 1.0f);
        float px = bounds.x + p.x;
        float py = bounds.y + p.y;

        // Soft trailing band: a few concentric rings fading from the
        // leading edge inward across bandWidth, instead of one hard circle.
        const int bands = 6;
        for (int i = 0; i < bands; ++i) {
            float t = i / static_cast<float>(bands - 1); // 0 at leading edge, 1 at trailing
            float bandRadius = radius - t * config.bandWidth;
            if (bandRadius <= 0.0f) continue;
            float bandAlpha = (1.0f - t) * (1.0f - lifeT) * motion.opacity;
            ofSetColor(scaledAlpha(theme.colors.accent, bandAlpha));
            ofDrawCircle(px, py, bandRadius);
        }
    }

    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
