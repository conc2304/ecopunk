#include "ReticleWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

static constexpr float kSpawnDur = 1.4f;
static constexpr float kPulseDur = 0.42f;
static constexpr float kDeathDur = kPulseDur * 2.0f + 0.6f;

static const std::vector<std::string> kLabels = {"CANOPY", "FLOW", "SPORE", "ROOT", "SIGNAL", "WATER"};

void ReticleWidget::respawn(Target& t, int labelIdx) {
    t.p           = {ofRandom(0.15f, 0.85f), ofRandom(0.18f, 0.82f)};
    t.drawnP      = t.p;
    float angle   = ofRandom(TWO_PI);
    float speed   = ofRandom(0.025f, 0.065f);
    t.goalV       = {std::cos(angle) * speed, std::sin(angle) * speed};
    t.phase       = ofRandom(TWO_PI);
    t.size        = 0.20f;
    t.label       = kLabels[labelIdx % (int)kLabels.size()];
    t.lifecycle   = TargetLifecycle::Spawning;
    t.stateT      = 0.0f;
    t.lifetime    = options.trackingLifetime + ofRandom(-1.5f, 1.5f);
    t.jitter      = {0.0f, 0.0f};
    t.jitterTimer = ofRandom(0.08f, 0.22f);
}

void ReticleWidget::setup() { rebuild(); }
void ReticleWidget::randomize(int seed) { HudWidget::randomize(seed); rebuild(); }

void ReticleWidget::rebuild() {
    targets.clear();
    if (options.preset == ReticlePreset::Tracking) {
        for (int i = 0; i < options.targetCount; ++i) {
            Target t;
            respawn(t, i);
            t.stateT = ofRandom(0.0f, kSpawnDur); // stagger initial spawns
            targets.push_back(t);
        }
    } else {
        for (int i = 0; i < options.targetCount; ++i) {
            Target t;
            t.p     = {ofRandom(0.15f, 0.85f), ofRandom(0.18f, 0.82f)};
            t.phase = ofRandom(TWO_PI);
            t.size  = ofRandom(0.06f, 0.14f);
            t.label = kLabels[i % (int)kLabels.size()];
            targets.push_back(t);
        }
    }
}

void ReticleWidget::updateTracking(Target& t, float dt) {
    t.stateT += dt;

    if (t.lifecycle == TargetLifecycle::Spawning) {
        t.drawnP += (t.p - t.drawnP) * (1.0f - std::exp(-5.0f * dt));
        if (t.stateT >= kSpawnDur) { t.stateT = 0.0f; t.lifecycle = TargetLifecycle::Tracking; }

    } else if (t.lifecycle == TargetLifecycle::Tracking) {
        t.p += t.goalV * dt * motion.speed;
        if (t.p.x < 0.12f || t.p.x > 0.88f) { t.goalV.x *= -1.0f; t.p.x = ofClamp(t.p.x, 0.12f, 0.88f); }
        if (t.p.y < 0.14f || t.p.y > 0.86f) { t.goalV.y *= -1.0f; t.p.y = ofClamp(t.p.y, 0.14f, 0.86f); }

        // Drawn position lags behind goal — the "tracking" feel
        t.drawnP += (t.p - t.drawnP) * (1.0f - std::exp(-6.0f * dt));

        // Periodic micro-correction snaps
        t.jitterTimer -= dt;
        if (t.jitterTimer <= 0.0f) {
            t.jitter      = {ofRandom(-0.018f, 0.018f), ofRandom(-0.018f, 0.018f)};
            t.jitterTimer = ofRandom(0.08f, 0.22f);
        }
        t.jitter *= std::exp(-10.0f * dt);

        if (t.stateT >= t.lifetime) { t.stateT = 0.0f; t.lifecycle = TargetLifecycle::Dying; }

    } else { // Dying
        if (t.stateT >= kDeathDur) respawn(t, (int)(&t - targets.data()));
    }
}

void ReticleWidget::update(float dt) {
    HudWidget::update(dt);
    if (options.preset == ReticlePreset::Tracking) {
        for (auto& t : targets) updateTracking(t, dt);
    }
}

void ReticleWidget::draw() {
    ofPushStyle();
    if (theme.additive) ofEnableBlendMode(OF_BLENDMODE_ADD); else ofEnableAlphaBlending();
    frame.draw(bounds, theme.colors, theme.frame, time);
    ofNoFill();
    ofSetLineWidth(std::max(1.0f, su(bounds, 1.0f) * 0.8f));

    for (const auto& t : targets) {
        ofVec2f p;
        float scaleFactor = 1.0f;
        float alphaScale  = motion.opacity;

        if (options.preset == ReticlePreset::Tracking) {
            p = pointInBounds(bounds, t.drawnP.x + t.jitter.x, t.drawnP.y + t.jitter.y);

            if (t.lifecycle == TargetLifecycle::Spawning) {
                float u = ofClamp(t.stateT / kSpawnDur, 0.0f, 1.0f);
                scaleFactor = u * u; // ease in from 0

            } else if (t.lifecycle == TargetLifecycle::Dying) {
                if (t.stateT < kPulseDur) {
                    float u = t.stateT / kPulseDur;
                    scaleFactor = 1.0f + 0.45f * std::sin(u * PI);
                } else if (t.stateT < kPulseDur * 2.0f) {
                    float u = (t.stateT - kPulseDur) / kPulseDur;
                    scaleFactor = 1.0f + 0.45f * std::sin(u * PI);
                } else {
                    float u  = (t.stateT - kPulseDur * 2.0f) / (kDeathDur - kPulseDur * 2.0f);
                    scaleFactor = 1.0f - u;
                    alphaScale  = motion.opacity * (1.0f - u);
                }
            }
        } else {
            p = pointInBounds(bounds, t.p.x, t.p.y);
        }

        float s = bounds.minDim() * t.size * breathe(time + t.phase, 0.35f, 0.82f, 1.16f) * scaleFactor * 0.2f;

        ofSetColor(scaledAlpha(theme.colors.primary, alphaScale * 0.72f));
        ofDrawLine(p.x - s, p.y - s, p.x - s * 0.45f, p.y - s);
        ofDrawLine(p.x - s, p.y - s, p.x - s, p.y - s * 0.45f);
        ofDrawLine(p.x + s, p.y - s, p.x + s * 0.45f, p.y - s);
        ofDrawLine(p.x + s, p.y - s, p.x + s, p.y - s * 0.45f);
        ofDrawLine(p.x - s, p.y + s, p.x - s * 0.45f, p.y + s);
        ofDrawLine(p.x - s, p.y + s, p.x - s, p.y + s * 0.45f);
        ofDrawLine(p.x + s, p.y + s, p.x + s * 0.45f, p.y + s);
        ofDrawLine(p.x + s, p.y + s, p.x + s, p.y + s * 0.45f);
        ofSetColor(scaledAlpha(theme.colors.accent, alphaScale * 0.85f));
        ofDrawCircle(p, std::max(1.2f, su(bounds, 1.5f)));
        if (options.showLabels) {
            ofSetColor(scaledAlpha(theme.colors.secondary, alphaScale * 0.75f));
            drawTextFallback(t.label, p.x + s + 4.0f, p.y - s * 0.25f, theme.textScale * su(bounds, 0.5f));
        }
    }

    ofDisableBlendMode();
    ofPopStyle();
}

} // namespace hud
