#include "ReticleWidget.h"
#include "../shared/HudUtils.h"

namespace hud {

static constexpr float kSpawnDur = 1.4f;
static constexpr float kPulseDur = 0.42f;
static constexpr float kDeathDur = kPulseDur * 2.0f + 0.6f;

// Locate behavior: spot a distant point, travel there with an accelerate/cruise/
// decelerate arc, hold on arrival, then spot a new point — a "found it, moving on"
// scanning feel rather than the Wander behavior's constant-velocity local bounce.
static constexpr float kLocateDwellMin     = 0.7f;
static constexpr float kLocateDwellMax     = 1.8f;
static constexpr float kLocateMinDist      = 0.4f;   // require a real medium/long haul
static constexpr float kLocateArriveDist   = 0.02f;
static constexpr float kLocateSpoolDur     = 0.6f;   // ease-in ramp at the start of a leg
static constexpr float kLocateCruiseSpeed  = 0.22f;  // normalized units/sec at full speed
static constexpr float kLocateApproachGain = 2.2f;   // decel factor as it nears the waypoint
static constexpr float kBehaviorBlendDur   = 1.0f;

// Shared roaming box for both behaviors (Wander's bounce walls, Locate's waypoint
// range). Widened past the old 0.12–0.88/0.14–0.86 box so Wander covers more
// ground — and Locate can reach farther — before it changes trajectory.
static constexpr float kBoundsMinX = 0.04f;
static constexpr float kBoundsMaxX = 0.96f;
static constexpr float kBoundsMinY = 0.05f;
static constexpr float kBoundsMaxY = 0.95f;

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
    // Start dwelling briefly so the Locate behavior doesn't dash off mid-spawn-ease;
    // it'll pick a real waypoint once the dwell timer runs out.
    t.spotTarget     = t.p;
    t.spotDwelling   = true;
    t.spotDwellTimer = ofRandom(0.2f, 1.0f);
    t.spotTravelT    = 0.0f;
    t.spotSpeedScale = 1.0f;
    t.spotLegsTotal  = (int)ofRandom(1.0f, 4.0f); // 1-3 targets before this one dies
    t.spotLegsDone   = 0;
    t.spotFinished   = false;
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
    if (predicted.x < kBoundsMinX || predicted.x > kBoundsMaxX) t.goalV.x *= -1.0f;
    if (predicted.y < kBoundsMinY || predicted.y > kBoundsMaxY) t.goalV.y *= -1.0f;
    return t.goalV * motion.speed;
}

void ReticleWidget::pickWaypoint(Target& t) const {
    ofVec2f candidate = t.spotTarget;

    if (!options.randomTargets && !locateTargets.empty()) {
        // Lock onto an actual target (e.g. a fragment center) rather than a
        // uniformly random point — walk the pool starting from a random
        // offset so repeated calls don't all favor index 0, and prefer one
        // far enough away to still read as a real haul.
        int startIdx = (int)ofRandom((float)locateTargets.size());
        for (int i = 0; i < (int)locateTargets.size(); ++i) {
            candidate = locateTargets[(startIdx + i) % (int)locateTargets.size()];
            if (candidate.distance(t.p) >= kLocateMinDist) break;
        }
        candidate.x = ofClamp(candidate.x, kBoundsMinX, kBoundsMaxX);
        candidate.y = ofClamp(candidate.y, kBoundsMinY, kBoundsMaxY);
    } else {
        for (int i = 0; i < 8; ++i) {
            candidate = {ofRandom(kBoundsMinX, kBoundsMaxX), ofRandom(kBoundsMinY, kBoundsMaxY)};
            if (candidate.distance(t.p) >= kLocateMinDist) break;
        }
    }

    t.spotTarget     = candidate;
    t.spotTravelT    = 0.0f;
    // Vary the pace leg-to-leg so consecutive hops don't all read at one speed.
    t.spotSpeedScale = ofRandom(0.6f, 1.5f);
}

ofVec2f ReticleWidget::computeLocateVelocity(Target& t, float dt) {
    if (t.spotDwelling) {
        t.spotDwellTimer -= dt;
        if (t.spotDwellTimer <= 0.0f) {
            if (t.spotLegsDone >= t.spotLegsTotal) {
                // Visited its full 1-3 target sequence — hold here; updateTracking
                // retires this target rather than starting another leg.
                t.spotFinished = true;
            } else {
                t.spotDwelling = false;
                pickWaypoint(t);
            }
        }
        return {0.0f, 0.0f};
    }

    ofVec2f toTarget = t.spotTarget - t.p;
    float   dist     = toTarget.length();
    if (dist <= kLocateArriveDist) {
        t.spotLegsDone++;
        t.spotDwelling   = true;
        t.spotDwellTimer = ofRandom(kLocateDwellMin, kLocateDwellMax);
        return {0.0f, 0.0f};
    }

    // Ease-in at the start of the leg, cruise, then ease-down as it nears the
    // waypoint — the accelerate/cruise/decelerate arc that reads as "traveling to it".
    t.spotTravelT += dt;
    float spoolUp = ofClamp(t.spotTravelT / kLocateSpoolDur, 0.0f, 1.0f);
    spoolUp = spoolUp * spoolUp * (3.0f - 2.0f * spoolUp); // smoothstep

    float cruise        = kLocateCruiseSpeed * t.spotSpeedScale;
    float approachSpeed = dist * kLocateApproachGain; // soft dock near arrival
    float speed = std::min(cruise, approachSpeed) * spoolUp * motion.speed;

    ofVec2f dir = toTarget / dist;
    return dir * speed;
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
        ofVec2f locateVel = computeLocateVelocity(t, dt);
        ofVec2f vFrom = (fromBehavior == ReticleBehavior::Locate) ? locateVel : wanderVel;
        ofVec2f vTo   = (toBehavior   == ReticleBehavior::Locate) ? locateVel : wanderVel;
        ofVec2f vel   = vFrom + (vTo - vFrom) * mix;

        t.p += vel * dt;
        t.p.x = ofClamp(t.p.x, kBoundsMinX, kBoundsMaxX);
        t.p.y = ofClamp(t.p.y, kBoundsMinY, kBoundsMaxY);

        // Drawn position lags behind goal — the "tracking" feel
        t.drawnP += (t.p - t.drawnP) * (1.0f - std::exp(-6.0f * dt));

        // Periodic micro-correction snaps — dialed down for Locate, which should read
        // as a steady scan rather than a twitchy tracker.
        float jitterFrom  = (fromBehavior == ReticleBehavior::Locate) ? 0.15f : 1.0f;
        float jitterTo    = (toBehavior   == ReticleBehavior::Locate) ? 0.15f : 1.0f;
        float jitterScale = jitterFrom + (jitterTo - jitterFrom) * mix;

        t.jitterTimer -= dt;
        if (t.jitterTimer <= 0.0f) {
            t.jitter      = {ofRandom(-0.018f, 0.018f) * jitterScale, ofRandom(-0.018f, 0.018f) * jitterScale};
            t.jitterTimer = ofRandom(0.08f, 0.22f);
        }
        t.jitter *= std::exp(-10.0f * dt);

        // Locate retires once it's visited its 1-3 target sequence, not on a fixed
        // timer — a sequence can easily run longer or shorter than trackingLifetime.
        bool locateSequenceDone = (toBehavior == ReticleBehavior::Locate) && t.spotFinished;
        bool timerExpired       = (toBehavior != ReticleBehavior::Locate) && (t.stateT >= t.lifetime);
        if (timerExpired || locateSequenceDone) { t.stateT = 0.0f; t.lifecycle = TargetLifecycle::Dying; }

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
