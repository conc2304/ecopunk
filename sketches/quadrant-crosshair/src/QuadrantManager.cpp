#include "QuadrantManager.h"
#include <set>

void QuadrantManager::setup(ShaderLibrary* lib) {
    float W = ofGetWidth(), H = ofGetHeight();
    float hw = W * 0.5f, hh = H * 0.5f;

    quads[0].setup(0, ofRectangle(0,  0,  hw, hh), lib);
    quads[1].setup(1, ofRectangle(hw, 0,  hw, hh), lib);
    quads[2].setup(2, ofRectangle(0,  hh, hw, hh), lib);
    quads[3].setup(3, ofRectangle(hw, hh, hw, hh), lib);

    quads[0].setScaleConfig(0.80f, 1.00f,   0.f);
    quads[1].setScaleConfig(1.20f, 1.60f,  50.f);
    quads[2].setScaleConfig(0.50f, 0.70f, 100.f);
    quads[3].setScaleConfig(1.80f, 2.40f, 150.f);

    // Pre-shuffle an independent deck for each quadrant
    float staggerBase[4] = { 0.f, 12.f, 24.f, 36.f };
    for (int i = 0; i < 4; i++) {
        cycleStates[i].deck = shaderPool;
        for (int j = (int)cycleStates[i].deck.size() - 1; j > 0; j--)
            std::swap(cycleStates[i].deck[j], cycleStates[i].deck[(int)ofRandom(j + 1)]);

        cycleStates[i].phase      = CyclePhase::SILENCING;
        cycleStates[i].silenceAcc = 0.f;
        cycleStates[i].silenceDur = staggerBase[i] + ofRandom(0.f, 5.f);
    }
}

// ── Silence duration ──────────────────────────────────────────────────────────
// Always includes a short breath (3–5 s). 20% chance of a long gap (20–40 s),
// but suppressed when 2+ other quadrants are already silencing, so the composition
// never goes fully dark at once.

float QuadrantManager::computeSilenceDuration(int quadId) {
    int silentCount = 0;
    for (int i = 0; i < 4; i++) {
        if (i != quadId && cycleStates[i].phase == CyclePhase::SILENCING)
            silentCount++;
    }

    float dur = ofRandom(3.f, 5.f);

    if (silentCount < 2 && ofRandom(1.f) < 0.20f)
        dur = ofRandom(20.f, 40.f);

    return dur;
}

// ── Deck + cross-quadrant selection ───────────────────────────────────────────
// Each quadrant owns an independently-shuffled deck (all shaders, no repeats until
// the whole pool has played). Before popping, we also skip any shader currently
// PLAYING in another quadrant, so effects are always spread across the screen.

std::string QuadrantManager::pickNextShader(int quadId) {
    // Shaders currently active in other quadrants
    std::set<std::string> activeElsewhere;
    for (int i = 0; i < 4; i++) {
        if (i != quadId && cycleStates[i].phase == CyclePhase::PLAYING)
            activeElsewhere.insert(cycleStates[i].lastShader);
    }

    auto& deck = cycleStates[quadId].deck;

    auto refill = [&]() {
        deck = shaderPool;
        for (int i = (int)deck.size() - 1; i > 0; i--)
            std::swap(deck[i], deck[(int)ofRandom(i + 1)]);
    };

    if (deck.empty()) refill();

    // Walk the deck (back = next up) looking for a shader not active elsewhere
    for (int i = (int)deck.size() - 1; i >= 0; i--) {
        if (activeElsewhere.count(deck[i]) == 0) {
            std::string chosen = deck[i];
            deck.erase(deck.begin() + i);
            return chosen;
        }
    }

    // All remaining deck entries conflict — just pop the next one anyway
    std::string chosen = deck.back();
    deck.pop_back();
    return chosen;
}

void QuadrantManager::kickRandomShader(int quadId) {
    if (shaderPool.empty()) return;

    std::string chosen = pickNextShader(quadId);
    cycleStates[quadId].lastShader = chosen;

    float fade  = ofRandom(5.f,  10.f);
    float dwell = ofRandom(18.f, 24.f);
    quads[quadId].pushShader(chosen, fade, dwell);

    if (chosen == "dither") {
        float arc = 0.45f, px = 4.f;
        for (int attempt = 0; attempt < 5; attempt++) {
            arc = ofRandom(0.15f, 0.85f);
            px  = ofRandom(2.f, 10.f);
            // Whiteout risk peaks when arc is near 0.5 (max pixelation phase)
            // and the video is bright. peakProx: 0 at arc edges, 1 at arc centre.
            float peakProx = 1.f - 2.f * std::abs(arc - 0.5f);
            float risk     = peakProx * (px / 10.f) * videoBrightness;
            if (risk < 0.45f || attempt == 4) break;
        }
        quads[quadId].setDitherParams(arc, px);
    }
}

// ── Update ────────────────────────────────────────────────────────────────────

void QuadrantManager::update(float dt, const CrosshairState& state,
                              const LFOBank& lfo, ofTexture& rdTex, ofTexture& gridTex,
                              ofTexture& motionTex) {
    float W  = ofGetWidth(),  H  = ofGetHeight();
    float cx = state.cx,      cy = state.cy;

    quads[0].region = ofRectangle(0,  0,  cx,     cy    );
    quads[1].region = ofRectangle(cx, 0,  W - cx, cy    );
    quads[2].region = ofRectangle(0,  cy, cx,     H - cy);
    quads[3].region = ofRectangle(cx, cy, W - cx, H - cy);

    quads[0].setThreshold(ofMap(lfo.get(LFO_THRESH_Q0), -1, 1, 0.3f, 0.7f));
    quads[1].setThreshold(ofMap(lfo.get(LFO_THRESH_Q1), -1, 1, 0.3f, 0.7f));
    quads[2].setShift(ofMap(lfo.get(LFO_SHIFT_Q2), -1, 1, 0.002f, 0.008f));

    float hue = ofMap(lfo.get(LFO_TINT_HUE), -1, 1, 0.f, 360.f);
    ofColor tintColor = ofColor::fromHsb((uint8_t)hue, 200, 255);
    quads[2].setTint({ tintColor.r / 255.f, tintColor.g / 255.f, tintColor.b / 255.f });

    float decayRate = ofMap(lfo.get(LFO_GRID_DECAY), -1, 1, 0.92f, 0.98f);
    for (auto& q : quads) {
        q.setDecay(decayRate);
        q.setRDTexture(rdTex);
        q.setGridTexture(gridTex);
        q.setMotionTexture(motionTex);
        q.update(dt, cx, cy);
    }

    if (velHighActive) return;

    for (int i = 0; i < 4; i++) {
        CycleState& cs = cycleStates[i];

        switch (cs.phase) {
            case CyclePhase::PLAYING:
                if (quads[i].allSlotsIdle()) {
                    cs.phase      = CyclePhase::SILENCING;
                    cs.silenceAcc = 0.f;
                    cs.silenceDur = computeSilenceDuration(i);
                }
                break;

            case CyclePhase::SILENCING:
                cs.silenceAcc += dt;
                if (cs.silenceAcc >= cs.silenceDur)
                    cs.phase = CyclePhase::READY;
                break;

            case CyclePhase::READY:
                kickRandomShader(i);
                cs.phase = CyclePhase::PLAYING;
                break;
        }
    }
}

void QuadrantManager::draw(ofTexture& videoTex, glm::vec2 videoSize) {
    for (auto& q : quads)
        q.draw(videoTex, videoSize);
}

void QuadrantManager::drawHUD() {
    static const char* phaseNames[] = { "PLAYING", "SILENCING", "READY" };

    for (int i = 0; i < 4; i++) {
        std::string phase = phaseNames[(int)cycleStates[i].phase];
        if (cycleStates[i].phase == CyclePhase::SILENCING) {
            float rem = cycleStates[i].silenceDur - cycleStates[i].silenceAcc;
            phase += " " + ofToString(rem, 1) + "s";
        }
        quads[i].drawDebugHUD(phase);
    }
}

void QuadrantManager::onTrigger(const TriggerEvent& e) {
    switch (e.id) {

        case TriggerID::VELOCITY_HIGH:
            velHighActive = e.active;
            if (e.active) {
                for (auto& q : quads) {
                    q.clearShaders(0.4f);
                    q.setThreshold(0.5f);
                    q.pushShader("threshold", 0.4f, 999.f);
                }
            } else {
                for (int i = 0; i < 4; i++) {
                    quads[i].clearShaders(0.8f);
                    // Let threshold fade out, then resume organic cycle after a short breath
                    cycleStates[i].phase      = CyclePhase::SILENCING;
                    cycleStates[i].silenceAcc = 0.f;
                    cycleStates[i].silenceDur = ofRandom(3.f, 6.f);
                }
            }
            break;

        case TriggerID::EDGE_PROXIMITY:
            if (e.active) {
                static const int edgeToQuad[4] = { 0, 1, 3, 2 };
                int qid = (e.quadrantHint >= 0 && e.quadrantHint < 4)
                          ? edgeToQuad[e.quadrantHint] : 0;
                quads[qid].setShift(e.intensity * 0.008f);
                quads[qid].pushShader("invert", 0.6f, 3.f);
            }
            break;

        case TriggerID::VELOCITY_LOW:
            if (!velHighActive)
                quads[3].pushShader("dither", 1.0f, 5.f);
            break;

        case TriggerID::QUADRANT_CENTER:
            if (e.active && !velHighActive) {
                for (auto& q : quads)
                    q.pushShader("invert", 0.3f, 0.5f);
            }
            break;

        case TriggerID::CORNER_NEAR:
            if (e.active && !velHighActive && e.quadrantHint >= 0) {
                quads[e.quadrantHint].setTint({ 1.0f, 0.5f, 0.1f });
                quads[e.quadrantHint].pushShader("recolor", 0.8f, 4.f);
            }
            break;

        case TriggerID::DWELL:
            if (e.active && !velHighActive) {
                for (auto& q : quads)
                    q.pushShader("solarize", 2.0f, 8.f);
            }
            break;
    }
}
