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

    for (int i = 0; i < 4; i++) {
        scaleChangeAcc[i] = 0.f;
        scaleChangeDur[i] = ofRandom(90.f, 180.f);
    }

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

    float dur = ofRandom(9.f, 15.f);

    if (silentCount < 2 && ofRandom(1.f) < 0.20f)
        dur = ofRandom(60.f, 120.f);

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

std::pair<float,float> QuadrantManager::pickScaleRange() {
    float center = ofRandom(0.45f, 2.40f);
    float spread = ofRandom(0.15f, 0.55f);
    float mn     = std::max(0.30f, center - spread * 0.5f);
    float mx     = std::min(2.80f, center + spread * 0.5f);
    return { mn, mx };
}

std::pair<float,float> QuadrantManager::chooseDitherParams() {
    float arc = 0.45f, px = 4.f;
    for (int attempt = 0; attempt < 5; attempt++) {
        arc = ofRandom(0.15f, 0.85f);
        px  = ofRandom(2.f, 10.f);
        // Risk peaks when arc ≈ 0.5 (max pixelation) and video is bright
        float peakProx = 1.f - 2.f * std::abs(arc - 0.5f);
        float risk     = peakProx * (px / 10.f) * videoBrightness;
        if (risk < 0.45f || attempt == 4) break;
    }
    return { arc, px };
}

void QuadrantManager::kickRandomShader(int quadId) {
    if (shaderPool.empty()) return;

    std::string chosen = pickNextShader(quadId);
    cycleStates[quadId].lastShader = chosen;

    float fade  = ofRandom(15.f, 30.f);
    float dwell = ofRandom(54.f, 72.f);
    quads[quadId].pushShader(chosen, fade, dwell);

    if (chosen == "dither") {
        auto [arc, px] = chooseDitherParams();
        quads[quadId].setDitherParams(arc, px);
    }
}

// ── Update ────────────────────────────────────────────────────────────────────

void QuadrantManager::update(float dt, const CrosshairState& state,
                              const LFOBank& lfo, ofTexture& gridTex,
                              ofTexture& motionTex, ofTexture& motionDelayedTex) {
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
        q.setGridTexture(gridTex);
        q.setMotionTexture(motionTex);
        q.setMotionDelayedTexture(motionDelayedTex);
        q.update(dt, cx, cy);
    }

    // ── Deferred / staggered effects ─────────────────────────────────────────
    for (auto& p : pending) p.delay -= dt;
    pending.erase(
        std::remove_if(pending.begin(), pending.end(), [&](PendingEffect& p) {
            if (p.delay <= 0.f) {
                TriggerEvent e;
                e.id            = p.triggerId;
                e.active        = p.triggerActive;
                e.intensity     = p.intensity;
                e.quadrantHint  = p.quadrantHint;
                applyTriggerToQuad(p.quadId, e);
                return true;
            }
            return false;
        }),
        pending.end()
    );

    // ── Scale personality rotation ─────────────────────────────────────────────
    for (int i = 0; i < 4; i++) {
        scaleChangeAcc[i] += dt;
        if (scaleChangeAcc[i] >= scaleChangeDur[i]) {
            auto [sMin, sMax] = pickScaleRange();
            quads[i].setScaleTarget(sMin, sMax);
            scaleChangeAcc[i] = 0.f;
            scaleChangeDur[i] = ofRandom(90.f, 180.f);
            ofLogNotice("QM") << "Q" << i << " scale target -> ["
                              << ofToString(sMin, 2) << ", " << ofToString(sMax, 2) << "]"
                              << "  next in " << (int)scaleChangeDur[i] << "s";
        }
    }

    // Contraction crossfade timer
    if (contracting) {
        contractionT -= dt / contractionDuration;
        if (contractionT <= 0.f) {
            contractionT     = 0.f;
            contracting      = false;
            expandedQuadrant = -1;
        }
    }

    if (velHighActive) return;

    for (int i = 0; i < 4; i++) {
        CycleState& cs = cycleStates[i];

        switch (cs.phase) {
            case CyclePhase::PLAYING:
                if (quads[i].allSlotsIdle()) {
                    if (cs.resumePending) {
                        float resumeDwell = std::max(cs.savedDwellRemain, 5.f);
                        quads[i].pushShader(cs.savedShader, 0.8f, resumeDwell);
                        if (cs.savedShader == "dither") {
                            auto [arc, px] = chooseDitherParams();
                            quads[i].setDitherParams(arc, px);
                        }
                        ofLogNotice("QM") << "Q" << i << " resuming '" << cs.savedShader << "' (" << resumeDwell << "s)";
                        cs.resumePending = false;
                        cs.savedShader   = "";
                    } else {
                        cs.phase      = CyclePhase::SILENCING;
                        cs.silenceAcc = 0.f;
                        cs.silenceDur = computeSilenceDuration(i);
                    }
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

void QuadrantManager::beginContraction(int quadrantID) {
    contracting      = true;
    contractionT     = 1.f;
    expandedQuadrant = quadrantID;
}

void QuadrantManager::draw(ofTexture& videoTex, glm::vec2 videoSize) {
    for (auto& q : quads) {
        if (contracting && q.id == expandedQuadrant) {
            // Fullscreen version fades out (no scissor — full canvas draw)
            ofEnableAlphaBlending();
            ofSetColor(255, 255, 255, (int)(contractionT * 255));
            videoTex.draw(0, 0, ofGetWidth(), ofGetHeight());
            ofDisableAlphaBlending();
            // Quarter-size version grows in underneath (scissored to crosshair region)
            q.draw(videoTex, videoSize);
        } else {
            q.draw(videoTex, videoSize);
        }
    }
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

// ── Staggered quad selection ──────────────────────────────────────────────────

std::vector<int> QuadrantManager::selectQuads(int primary) {
    int p = (primary >= 0 && primary < 4) ? primary : (int)ofRandom(4);
    std::vector<int> sel = { p };
    for (int q = 0; q < 4; q++) {
        if (q == p) continue;
        if (ofRandom(1.f) < 0.33f) sel.push_back(q);
    }
    return sel;
}

void QuadrantManager::scheduleEffect(const std::vector<int>& quads, const TriggerEvent& e) {
    float delay = 0.f;
    for (int i = 0; i < (int)quads.size(); i++) {
        pending.push_back({ delay, quads[i], e.id, e.active, e.intensity, e.quadrantHint });
        if (i + 1 < (int)quads.size())
            delay += ofRandom(0.1f, 0.5f);
    }
}

void QuadrantManager::applyTriggerToQuad(int qid, const TriggerEvent& e) {
    // Snapshot the actively playing shader so it can resume after the trigger effect
    if (cycleStates[qid].phase == CyclePhase::PLAYING && !cycleStates[qid].resumePending) {
        std::string name; float rem;
        if (quads[qid].getActivePlaying(name, rem)) {
            cycleStates[qid].savedShader      = name;
            cycleStates[qid].savedDwellRemain = rem;
            cycleStates[qid].resumePending    = true;
            ofLogNotice("QM") << "Q" << qid << " saved '" << name << "' (" << rem << "s rem) for resume";
        }
    }

    switch (e.id) {
        case TriggerID::VELOCITY_HIGH:
            ofLogNotice("QM") << "Q" << qid << " VELOCITY_HIGH -> threshold";
            quads[qid].clearShaders(0.4f);
            quads[qid].setThreshold(0.5f);
            quads[qid].pushShader("threshold", 0.4f, 999.f);
            break;

        case TriggerID::EDGE_PROXIMITY:
            ofLogNotice("QM") << "Q" << qid << " EDGE_PROXIMITY -> invert (intensity=" << e.intensity << ")";
            quads[qid].setShift(e.intensity * 0.008f);
            quads[qid].clearShaders(0.3f);
            quads[qid].pushShader("invert", 0.6f, 3.f);
            break;

        case TriggerID::VELOCITY_LOW: {
            ofLogNotice("QM") << "Q" << qid << " VELOCITY_LOW -> dither";
            quads[qid].clearShaders(0.4f);
            quads[qid].pushShader("dither", 1.0f, 5.f);
            auto [arc, px] = chooseDitherParams();
            quads[qid].setDitherParams(arc, px);
            break;
        }

        case TriggerID::QUADRANT_CENTER:
            ofLogNotice("QM") << "Q" << qid << " QUADRANT_CENTER -> invert";
            quads[qid].clearShaders(0.3f);
            quads[qid].pushShader("invert", 0.3f, 0.5f);
            break;

        case TriggerID::CORNER_NEAR:
            ofLogNotice("QM") << "Q" << qid << " CORNER_NEAR -> recolor";
            quads[qid].setTint({ 1.0f, 0.5f, 0.1f });
            quads[qid].clearShaders(0.4f);
            quads[qid].pushShader("recolor", 0.8f, 4.f);
            break;

        case TriggerID::DWELL:
            ofLogNotice("QM") << "Q" << qid << " DWELL -> solarize";
            quads[qid].clearShaders(0.5f);
            quads[qid].pushShader("solarize", 2.0f, 8.f);
            break;
    }
}

void QuadrantManager::onTrigger(const TriggerEvent& e) {
    switch (e.id) {

        case TriggerID::VELOCITY_HIGH:
            velHighActive = e.active;
            if (e.active) {
                int primary = (e.quadrantHint >= 0 && e.quadrantHint < 4)
                              ? e.quadrantHint : (int)ofRandom(4);
                scheduleEffect(selectQuads(primary), e);
            } else {
                // Deactivation: flush pending, clear all quads
                pending.clear();
                for (int i = 0; i < 4; i++) {
                    quads[i].clearShaders(0.8f);
                    if (cycleStates[i].resumePending) {
                        // Resume the interrupted shader once the fade-out clears
                        cycleStates[i].phase = CyclePhase::PLAYING;
                    } else {
                        cycleStates[i].phase      = CyclePhase::SILENCING;
                        cycleStates[i].silenceAcc = 0.f;
                        cycleStates[i].silenceDur = ofRandom(3.f, 6.f);
                    }
                }
            }
            break;

        case TriggerID::EDGE_PROXIMITY:
            if (e.active) {
                static const int edgeToQuad[4] = { 0, 1, 3, 2 };
                int primary = (e.quadrantHint >= 0 && e.quadrantHint < 4)
                              ? edgeToQuad[e.quadrantHint] : 0;
                TriggerEvent mapped = e;
                mapped.quadrantHint = primary;
                scheduleEffect(selectQuads(primary), mapped);
            }
            break;

        case TriggerID::VELOCITY_LOW:
            if (!velHighActive)
                scheduleEffect(selectQuads(-1), e);   // random primary
            break;

        case TriggerID::QUADRANT_CENTER:
            if (e.active && !velHighActive)
                scheduleEffect(selectQuads(-1), e);   // random primary
            break;

        case TriggerID::CORNER_NEAR:
            if (e.active && !velHighActive && e.quadrantHint >= 0)
                scheduleEffect(selectQuads(e.quadrantHint), e);
            break;

        case TriggerID::DWELL:
            if (e.active && !velHighActive)
                scheduleEffect(selectQuads(-1), e);   // random primary
            break;
    }
}

QuadrantManager::QuadrantTelemetry QuadrantManager::getTelemetry(int q) const {
    QuadrantTelemetry t;
    const auto& cs = cycleStates[q];
    switch (cs.phase) {
        case CyclePhase::PLAYING:   t.phase = "ONLINE";      break;
        case CyclePhase::SILENCING: t.phase = "QUIET PHASE"; break;
        case CyclePhase::READY:     t.phase = "STANDBY";     break;
    }
    t.activeShader  = quads[q].activeShaderName();
    t.dwellProgress = quads[q].primaryDwellProgress();
    return t;
}
