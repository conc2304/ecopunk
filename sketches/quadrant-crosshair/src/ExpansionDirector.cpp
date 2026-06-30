#include "ExpansionDirector.h"

void ExpansionDirector::setup() {
    state           = ExpansionState::IDLE;
    expansionT      = 0.f;
    uiFadeAlpha     = 1.f;
    travelledPos    = { ofGetWidth() * 0.5f, ofGetHeight() * 0.5f };
    nextTriggerTime = ofRandom(120.f, 300.f);
    timerAccum      = 0.f;
}

// ── Update ─────────────────────────────────────────────────────────────────────

void ExpansionDirector::update(float dt, glm::vec2 currentCrosshairPos, float crosshairSpeed) {
    lastKnownSpeed = crosshairSpeed;

    if (state == ExpansionState::IDLE) {
        timerAccum += dt;
        if (timerAccum >= nextTriggerTime) {
            timerAccum      = 0.f;
            nextTriggerTime = ofRandom(120.f, 300.f);
            trigger(currentCrosshairPos);
        }
    }

    switch (state) {
        case ExpansionState::TRAVEL_OUT:  updateTravelOut(dt);  break;
        case ExpansionState::HOLD:        updateHold(dt);       break;
        case ExpansionState::TRAVEL_BACK: updateTravelBack(dt); break;
        case ExpansionState::IDLE:                              break;
    }
}

// ── Trigger ────────────────────────────────────────────────────────────────────

void ExpansionDirector::trigger(glm::vec2 currentPos) {
    if (state != ExpansionState::IDLE) return;
    triggerQuadrant(randomQuadrant(), currentPos);
}

void ExpansionDirector::triggerQuadrant(int quadrantID, glm::vec2 currentPos) {
    if (state != ExpansionState::IDLE) return;
    seq.targetQuadrant = quadrantID;
    seq.cornerPos      = cornerPosition(quadrantID);
    seq.originPos      = currentPos;

    // Travel at the crosshair's current natural speed (px/frame → px/sec at 24fps).
    // Floor at 1.0 px/frame so a dwelling crosshair still completes in finite time.
    // Clamp the resulting duration to [6, 20] seconds.
    float dist           = glm::length(seq.cornerPos - seq.originPos);
    float speedPxSec     = std::max(lastKnownSpeed, 1.0f) * 24.f;
    seq.travelDuration   = ofClamp(dist / speedPxSec, 6.f, 20.f);

    seq.holdDuration   = ofRandom(60.f, 90.f);
    seq.elapsed        = 0.f;
    enterTravelOut(currentPos);
}

int ExpansionDirector::randomQuadrant() {
    return (int)ofRandom(0, 4);  // 0–3
}

// ── State transitions ──────────────────────────────────────────────────────────

void ExpansionDirector::enterTravelOut(glm::vec2 currentPos) {
    state        = ExpansionState::TRAVEL_OUT;
    seq.elapsed  = 0.f;
    travelledPos = currentPos;
}

void ExpansionDirector::enterHold() {
    state        = ExpansionState::HOLD;
    seq.elapsed  = 0.f;
    expansionT   = 1.f;
    uiFadeAlpha  = 0.f;
    travelledPos = seq.cornerPos;
}

void ExpansionDirector::enterTravelBack() {
    state       = ExpansionState::TRAVEL_BACK;
    seq.elapsed = 0.f;
    // Quadrant contracts independently via QuadrantManager::beginContraction()
}

void ExpansionDirector::enterIdle() {
    state       = ExpansionState::IDLE;
    expansionT  = 0.f;
    uiFadeAlpha = 1.f;
}

// ── State updates ──────────────────────────────────────────────────────────────

void ExpansionDirector::updateTravelOut(float dt) {
    seq.elapsed += dt;
    float t     = ofClamp(seq.elapsed / seq.travelDuration, 0.f, 1.f);
    float eased = easeInOut(t);

    travelledPos = glm::mix(seq.originPos, seq.cornerPos, eased);
    expansionT   = eased;

    // UI fades in the last 10% of travel
    const float FADE_START = 0.9f;
    if (t >= FADE_START)
        uiFadeAlpha = ofMap(t, FADE_START, 1.f, 1.f, 0.f);

    if (t >= 1.f) enterHold();
}

void ExpansionDirector::updateHold(float dt) {
    seq.elapsed += dt;
    if (seq.elapsed >= seq.holdDuration) enterTravelBack();
}

void ExpansionDirector::updateTravelBack(float dt) {
    seq.elapsed += dt;
    float t     = ofClamp(seq.elapsed / seq.travelDuration, 0.f, 1.f);
    float eased = easeInOut(t);

    travelledPos = glm::mix(seq.cornerPos, seq.originPos, eased);

    // UI fades back in during first 40% of return travel
    const float FADE_END = 0.5f;
    if (t >= 0.1f)
        uiFadeAlpha = ofMap(t, 0.1f, FADE_END, 0.f, 1.f, true);

    // expansionT contracts independently in QuadrantManager — not driven here
    if (t >= 1.f) enterIdle();
}

// ── Helpers ────────────────────────────────────────────────────────────────────

glm::vec2 ExpansionDirector::cornerPosition(int quadrantID) {
    float W = ofGetWidth(), H = ofGetHeight();
    // The quadrant regions are defined by (cx,cy):
    //   Q0 TL: w=cx, h=cy       → fills screen when cx→W and cy→H (crosshair at BR)
    //   Q1 TR: w=W-cx, h=cy     → fills screen when cx→0 and cy→H (crosshair at BL)
    //   Q2 BL: w=cx, h=H-cy     → fills screen when cx→W and cy→0 (crosshair at TR)
    //   Q3 BR: w=W-cx, h=H-cy   → fills screen when cx→0 and cy→0 (crosshair at TL)
    // So to expand quadrant N, the crosshair must go to the diagonally opposite corner.
    switch (quadrantID) {
        case 0: return { W,   H   };  // TL expands → crosshair → BR corner
        case 1: return { 0.f, H   };  // TR expands → crosshair → BL corner
        case 2: return { W,   0.f };  // BL expands → crosshair → TR corner
        case 3: return { 0.f, 0.f };  // BR expands → crosshair → TL corner
        default: return { W * 0.5f, H * 0.5f };
    }
}

float ExpansionDirector::easeInOut(float t) {
    return t < 0.5f ? 4.f * t * t * t : 1.f - powf(-2.f * t + 2.f, 3.f) / 2.f;
}
