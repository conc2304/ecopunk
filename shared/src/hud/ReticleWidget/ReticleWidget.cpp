#include "ReticleWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

static constexpr float kSpawnDur = 1.4f;
static constexpr float kPulseDur = 0.42f;
static constexpr float kDeathDur = kPulseDur * 2.0f + 0.6f;

// Sweep behavior: hold a heading for a long stretch, then ease into a new one —
// scanning/searchlight feel rather than the Wander behavior's constant-velocity bounce.
static constexpr float kSweepHoldMin     = 1.6f;
static constexpr float kSweepHoldMax     = 3.6f;
static constexpr float kSweepTurnRate    = 1.8f;
static constexpr float kSweepSpeed       = 0.05f;
static constexpr float kBehaviorBlendDur = 1.0f;

static const std::vector<std::string> kDefaultLabels = {"CANOPY", "FLOW", "SPORE", "ROOT", "SIGNAL", "WATER"};

const std::string& ReticleWidget::labelFor(int idx) const {
    const std::vector<std::string>& labels = options.labelOverride.empty() ? kDefaultLabels : options.labelOverride;
    // labels is guaranteed non-empty (kDefaultLabels has 6 entries), so % is always safe.
    return labels[((idx % (int)labels.size()) + (int)labels.size()) % (int)labels.size()];
}

void ReticleWidget::respawn(Target& t, int labelIdx) {
    t.p           = {ofRandom(0.15f, 0.85f), ofRandom(0.18f, 0.82f)};
    t.drawnP      = t.p;
    float angle   = ofRandom(TWO_PI);
    float speed   = ofRandom(0.025f, 0.065f);
    t.goalV       = {std::cos(angle) * speed, std::sin(angle) * speed};
    t.sweepHeading        = ofRandom(TWO_PI);
    t.sweepDesiredHeading = t.sweepHeading;
    t.sweepTimer          = ofRandom(kSweepHoldMin, kSweepHoldMax);
    t.phase       = ofRandom(TWO_PI);
    t.size        = 0.20f;
    t.label       = labelFor(labelIdx);
    t.lifecycle   = TargetLifecycle::Spawning;
    t.stateT      = 0.0f;
    t.lifetime    = options.trackingLifetime + ofRandom(-1.5f, 1.5f);
    t.jitter      = {0.0f, 0.0f};
    t.jitterTimer = ofRandom(0.08f, 0.22f);
}

void ReticleWidget::setup() { rebuild(); }
void ReticleWidget::randomize(int seed) { HudWidget::randomize(seed); rebuild(); }

void ReticleWidget::setOptions(const ReticleOptions& next) {
    bool needsRebuild = next.targetCount != options.targetCount
                     || next.preset != options.preset
                     || next.labelOverride != options.labelOverride;
    bool behaviorChanged = next.behavior != options.behavior;
    options = next;

    if (needsRebuild) {
        rebuild(); // resets behavior blend state too
    } else if (behaviorChanged) {
        fromBehavior = toBehavior;
        toBehavior   = options.behavior;
        behaviorMix  = 0.0f;
    }
}

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
            t.label = labelFor(i);
            targets.push_back(t);
        }
    }
    fromBehavior = toBehavior = options.behavior;
    behaviorMix  = 1.0f;
}

ofVec2f ReticleWidget::computeWanderVelocity(Target& t, float dt) {
    ofVec2f predicted = t.p + t.goalV * dt * motion.speed;
    if (predicted.x < 0.12f || predicted.x > 0.88f) t.goalV.x *= -1.0f;
    if (predicted.y < 0.14f || predicted.y > 0.86f) t.goalV.y *= -1.0f;
    return t.goalV * motion.speed;
}

ofVec2f ReticleWidget::computeSweepVelocity(Target& t, float dt) {
    t.sweepTimer -= dt;
    if (t.sweepTimer <= 0.0f) {
        bool nearEdge = t.p.x < 0.18f || t.p.x > 0.82f || t.p.y < 0.20f || t.p.y > 0.80f;
        if (nearEdge) {
            // Steer the next heading back toward center instead of bouncing off the wall
            ofVec2f toCenter   = ofVec2f(0.5f, 0.5f) - t.p;
            float   centerAngle = std::atan2(toCenter.y, toCenter.x);
            t.sweepDesiredHeading = centerAngle + ofRandom(-PI * 0.3f, PI * 0.3f);
        } else {
            t.sweepDesiredHeading = t.sweepHeading + ofRandom(-PI * 0.8f, PI * 0.8f);
        }
        t.sweepTimer = ofRandom(kSweepHoldMin, kSweepHoldMax);
    }

    // Ease heading toward the desired one along the shortest angular path — this is
    // what turns the direction change into a smooth arc instead of a snap.
    float diff = std::atan2(std::sin(t.sweepDesiredHeading - t.sweepHeading),
                             std::cos(t.sweepDesiredHeading - t.sweepHeading));
    t.sweepHeading += diff * (1.0f - std::exp(-kSweepTurnRate * dt));

    float speed = kSweepSpeed * motion.speed;
    return { std::cos(t.sweepHeading) * speed, std::sin(t.sweepHeading) * speed };
}

void ReticleWidget::updateTracking(Target& t, float dt, float mix) {
    t.stateT += dt;

    if (t.lifecycle == TargetLifecycle::Spawning) {
        t.drawnP += (t.p - t.drawnP) * (1.0f - std::exp(-5.0f * dt));
        if (t.stateT >= kSpawnDur) { t.stateT = 0.0f; t.lifecycle = TargetLifecycle::Tracking; }

    } else if (t.lifecycle == TargetLifecycle::Tracking) {
        // Both behaviors are always ticked (even when not active) so a mid-flight
        // switch blends into state that's already evolving, not a frozen start.
        ofVec2f wanderVel = computeWanderVelocity(t, dt);
        ofVec2f sweepVel  = computeSweepVelocity(t, dt);
        ofVec2f vFrom = (fromBehavior == ReticleBehavior::Sweep) ? sweepVel : wanderVel;
        ofVec2f vTo   = (toBehavior   == ReticleBehavior::Sweep) ? sweepVel : wanderVel;
        ofVec2f vel   = vFrom + (vTo - vFrom) * mix;

        t.p += vel * dt;
        t.p.x = ofClamp(t.p.x, 0.12f, 0.88f);
        t.p.y = ofClamp(t.p.y, 0.14f, 0.86f);

        // Drawn position lags behind goal — the "tracking" feel
        t.drawnP += (t.p - t.drawnP) * (1.0f - std::exp(-6.0f * dt));

        // Periodic micro-correction snaps — dialed down for Sweep, which should read
        // as a steady scan rather than a twitchy tracker.
        float jitterFrom  = (fromBehavior == ReticleBehavior::Sweep) ? 0.15f : 1.0f;
        float jitterTo    = (toBehavior   == ReticleBehavior::Sweep) ? 0.15f : 1.0f;
        float jitterScale = jitterFrom + (jitterTo - jitterFrom) * mix;

        t.jitterTimer -= dt;
        if (t.jitterTimer <= 0.0f) {
            t.jitter      = {ofRandom(-0.018f, 0.018f) * jitterScale, ofRandom(-0.018f, 0.018f) * jitterScale};
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
        if (behaviorMix < 1.0f) {
            behaviorMix = ofClamp(behaviorMix + dt / kBehaviorBlendDur, 0.0f, 1.0f);
            if (behaviorMix >= 1.0f) fromBehavior = toBehavior;
        }
        float mixEased = behaviorMix * behaviorMix * (3.0f - 2.0f * behaviorMix); // smoothstep
        for (auto& t : targets) updateTracking(t, dt, mixEased);
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
