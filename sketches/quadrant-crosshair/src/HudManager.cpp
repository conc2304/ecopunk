#include "HudManager.h"
#include <algorithm>
#include <cmath>

// ── Theme helper ──────────────────────────────────────────────────────────────

hud::HudTheme HudManager::themedAt(float alpha) const {
    hud::HudTheme t = baseTheme;
    auto scale = [alpha](ofColor c) -> ofColor {
        c.a = (unsigned char)(c.a * ofClamp(alpha, 0.f, 1.f));
        return c;
    };
    t.colors.primary    = scale(baseTheme.colors.primary);
    t.colors.secondary  = scale(baseTheme.colors.secondary);
    t.colors.accent     = scale(baseTheme.colors.accent);
    t.colors.muted      = scale(baseTheme.colors.muted);
    t.colors.background = scale(baseTheme.colors.background);
    return t;
}

ofVec2f HudManager::panelBasePos(int q) const {
    float W = ofGetWidth(), H = ofGetHeight();
    switch (q) {
        case 0: return {        30.f,         30.f };
        case 1: return { W - 230.f,            30.f };
        case 2: return {        30.f,    H - 150.f  };
        case 3: return { W - 230.f,       H - 150.f };
        default: return { 0.f, 0.f };
    }
}

// ── Slot machinery ────────────────────────────────────────────────────────────

void HudManager::refillDeck() {
    // 5 drawable elements (0–4) + 3 silence entries (-1) → ~37% quiet probability
    deck = { 0, 1, 2, 3, 4, -1, -1, -1 };
    for (int i = (int)deck.size() - 1; i > 0; i--)
        std::swap(deck[i], deck[(int)ofRandom(i + 1)]);
}

void HudManager::startSlot(HudSlot& slot, int idx) {
    slot.elemIdx    = idx;
    slot.alpha      = 0.f;
    slot.drawnAlpha = 0.f;
    slot.dwellAcc   = 0.f;
    slot.dwellDur   = ofRandom(30.f, 60.f);
    // Silence uses near-zero fadeDur so it steps through FADE_IN/FADE_OUT instantly
    slot.fadeDur    = (idx < 0) ? 0.001f : ofRandom(5.f, 10.f);
    slot.state      = HudSlot::State::FADE_IN;
}

void HudManager::popNextToSlot(int slotIdx) {
    if (deck.empty()) refillDeck();

    // Avoid duplicating an element active in the other slot (silence never conflicts)
    int other     = 1 - slotIdx;
    int avoidElem = (!slots[other].isIdle() && slots[other].elemIdx >= 0)
                    ? slots[other].elemIdx : -999;

    for (int i = (int)deck.size() - 1; i >= 0; i--) {
        if (deck[i] != avoidElem) {
            int idx = deck[i];
            deck.erase(deck.begin() + i);
            startSlot(slots[slotIdx], idx);
            return;
        }
    }
    // All remaining entries conflict (rare) — pop next anyway
    int idx = deck.back();
    deck.pop_back();
    startSlot(slots[slotIdx], idx);
}

void HudManager::updateSlot(int slotIdx, float dt) {
    HudSlot& slot = slots[slotIdx];
    using State   = HudSlot::State;

    switch (slot.state) {
        case State::IDLE:
            return;

        case State::FADE_IN:
            slot.alpha += dt / slot.fadeDur;
            if (slot.alpha >= 1.f) {
                slot.alpha    = 1.f;
                slot.state    = State::ACTIVE;
                slot.dwellAcc = 0.f;
            }
            { float p = slot.alpha; slot.drawnAlpha = p * p * (3.f - 2.f * p); }
            break;

        case State::ACTIVE:
            slot.drawnAlpha = 1.f;
            slot.dwellAcc  += dt;
            if (slot.dwellAcc >= slot.dwellDur)
                slot.state = State::FADE_OUT;
            break;

        case State::FADE_OUT:
            slot.alpha -= dt / slot.fadeDur;
            if (slot.alpha <= 0.f) {
                slot.state = State::IDLE;
                popNextToSlot(slotIdx);
                return;
            }
            { float p = slot.alpha; slot.drawnAlpha = p * p * (3.f - 2.f * p); }
            break;
    }
}

// ── Draw element by pool index ────────────────────────────────────────────────

void HudManager::drawElement(int idx, float alpha) {
    if (idx < 0 || alpha <= 0.001f) return;

    switch (idx) {
        case 0: contours.setTheme(themedAt(0.40f * alpha));  contours.draw();  break;
        case 1: hexGrid.setTheme(themedAt(0.25f * alpha));   hexGrid.draw();   break;
        case 2: network.setTheme(themedAt(0.45f * alpha));   network.draw();   break;
        case 3: {
            // Registration corner marks on the reticle layer specifically —
            // matches temporal-fields' choice of pairing this frame style
            // with the reticle/tracking-brackets widget.
            hud::HudTheme t = themedAt(0.40f * alpha);
            t.frame.style = hud::FrameStyle::Registration;
            reticles.setTheme(t);
            reticles.draw();
            break;
        }
        case 4: flowField.setTheme(themedAt(0.40f * alpha)); flowField.draw(); break;
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void HudManager::setup(TriggerBus& tb, CrosshairSystem& ch, LFOBank& lfoRef,
                        QuadrantManager& qm, MotionExtraction& me) {
    triggerBus   = &tb;
    crosshairSys = &ch;
    lfo          = &lfoRef;
    quadMgr      = &qm;
    motionEx     = &me;

    baseTheme.colors.primary    = ofColor(124, 232, 230, 220);
    baseTheme.colors.secondary  = ofColor(103, 255, 142, 200);
    baseTheme.colors.accent     = ofColor(244, 255, 106, 220);
    baseTheme.colors.muted      = ofColor(124, 232, 230, 70);
    baseTheme.colors.background = ofColor(0,   20,  16,  18);
    baseTheme.frame.style       = hud::FrameStyle::Corners;
    baseTheme.frame.showTicks   = true;
    baseTheme.additive          = true;

    float W = ofGetWidth(), H = ofGetHeight();

    // Pool 0 — Contours
    hud::ContourOptions cOpts;
    cOpts.contourCount = 8;
    cOpts.samples      = 80;
    cOpts.noiseScale   = 1.5f;
    contours.setBounds(0, 0, W, H);
    contours.setTheme(themedAt(0.f));
    contours.setOptions(cOpts);
    contours.setup();

    // Pool 1 — HexGrid
    hud::HexGridOptions hOpts;
    hOpts.cellSize    = 26.f;
    hOpts.activation  = 0.12f;
    hOpts.filledCells = false;
    hexGrid.setBounds(0, 0, W, H);
    hexGrid.setTheme(themedAt(0.f));
    hexGrid.setOptions(hOpts);
    hexGrid.setup();

    // Pool 2 — NodeNetwork
    hud::NodeNetworkOptions nOpts;
    nOpts.nodeCount          = 28;
    nOpts.connectionDistance = 0.22f;
    nOpts.wrap               = true;
    nOpts.showPackets        = true;
    // Organic (root/mycelium) edges rather than the tech-mesh default — this
    // widget's own naming ("Mycelium/root communication mesh") always assumed
    // this look, even before the Straight/Organic option existed to draw it.
    nOpts.edgeStyle          = hud::NodeNetworkEdgeStyle::Organic;
    network.setBounds(0, 0, W, H);
    network.setTheme(themedAt(0.f));
    network.setOptions(nOpts);
    network.setup();

    // Always-on — Scanner (tracks crosshair, not in rotation pool)
    hud::ScannerOptions sOpts;
    sOpts.rings         = 3;
    sOpts.ticks         = 48;
    sOpts.showSweep     = true;
    sOpts.showCrosshair = false;
    sOpts.showPulses    = true;
    scanner.setBounds(W * 0.5f - 90.f, H * 0.5f - 90.f, 180.f, 180.f);
    scanner.setTheme(themedAt(0.45f));
    scanner.setOptions(sOpts);
    scanner.setup();

    // Pool 3 — Motion gauge (store opts as member so label can be swapped later)
    motionGaugeOpts_.style     = hud::GaugeStyle::Ring;
    motionGaugeOpts_.label     = "BIO SIGNAL";
    motionGaugeOpts_.units     = "%";
    motionGaugeOpts_.showValue = true;
    motionGaugeOpts_.value     = 0.f;
    motionGauge.setBounds(W * 0.5f - 135.f, 10.f, 110.f, 110.f);
    motionGauge.setTheme(themedAt(0.55f));
    motionGauge.setOptions(motionGaugeOpts_);
    motionGauge.setup();

    // Pool 4 — Dwell gauge (store opts as member so label can be swapped later)
    dwellGaugeOpts_.style     = hud::GaugeStyle::SemiCircle;
    dwellGaugeOpts_.label     = "DWELL";
    dwellGaugeOpts_.units     = "%";
    dwellGaugeOpts_.showValue = true;
    dwellGaugeOpts_.value     = 0.f;
    dwellGauge.setBounds(W * 0.5f + 25.f, 10.f, 110.f, 110.f);
    dwellGauge.setTheme(themedAt(0.55f));
    dwellGauge.setOptions(dwellGaugeOpts_);
    dwellGauge.setup();

    // Always-on — Telemetry cards (one per quadrant)
    for (int q = 0; q < 4; q++) {
        ofVec2f pos = panelBasePos(q);
        hud::DataCardOptions dcOpts;
        dcOpts.title         = "FIELD 0" + ofToString(q);
        dcOpts.value         = "STANDBY";
        dcOpts.subtitle      = "---";
        dcOpts.meter         = 0.f;
        dcOpts.showMeter     = true;
        dcOpts.showSparkline = true;
        quadCards[q].setBounds(pos.x, pos.y, 200.f, 120.f);
        quadCards[q].setTheme(themedAt(0.55f));
        quadCards[q].setOptions(dcOpts);
        quadCards[q].setup();
    }

    // Pool 5 — Reticles (store opts as member so labelOverride can be swapped later)
    reticleOpts_.targetCount   = 4;
    reticleOpts_.showLabels    = true;
    reticleOpts_.randomTargets = true;
    reticles.setBounds(0, 0, W, H);
    reticles.setTheme(themedAt(0.f));
    reticles.setOptions(reticleOpts_);
    reticles.setup();

    // Pool 6 — FlowField
    hud::FlowFieldOptions ffOpts;
    ffOpts.lineCount        = 6;
    ffOpts.particlesPerLine = 7;
    ffOpts.showCurves       = true;
    ffOpts.density          = 0.5f;  // in-code dial: 0–1 scales lines and particles
    flowField.setBounds(0, 0, W, H);
    flowField.setTheme(themedAt(0.f));
    flowField.setOptions(ffOpts);
    flowField.setup();

    // Always-on — media status light
    hud::StatusLightOptions slOpts;
    slOpts.label = "MEDIA";
    slOpts.state = hud::StatusState::Idle;
    mediaStatus.setBounds(W - 170.f, H - 40.f, 150.f, 24.f);
    mediaStatus.setTheme(themedAt(0.55f));
    mediaStatus.setOptions(slOpts);
    mediaStatus.setup();

    // Always-on — event log (video loads, motion readouts)
    hud::LogScrollOptions lOpts;
    lOpts.maxLines    = 24;
    lOpts.scrollSpeed = 9.f;
    lOpts.showFrame   = true;
    log.setBounds(W - 260.f, H - 200.f, 236.f, 150.f);
    log.setTheme(themedAt(0.55f));
    log.setOptions(lOpts);
    log.setup();

    glitch.setBounds(0, 0, W, H);
    glitch.setTheme(themedAt(1.f));
    glitch.setup();

    // Seed with NATURE fallback copy until first file-change notification arrives
    currentCopy_ = getCopyForCategory(NatureCategory::NATURE);

    // Kick off both slots
    refillDeck();
    popNextToSlot(0);
    popNextToSlot(1);
}

// ── Nature copy ───────────────────────────────────────────────────────────────

void HudManager::onVideoFileChanged(const std::string& basename) {
    currentCopy_ = getCopyForCategory(classifyFilename(basename));

    motionGaugeOpts_.label = currentCopy_.motionGaugeLabel;
    dwellGaugeOpts_.label  = currentCopy_.dwellGaugeLabel;
    motionGauge.setOptions(motionGaugeOpts_);
    dwellGauge.setOptions(dwellGaugeOpts_);

    reticleOpts_.labelOverride = currentCopy_.reticleLabels;
    reticles.setOptions(reticleOpts_);

    // Real event, real position — ripple from wherever the crosshair
    // actually is right now, not canvas center.
    glitch.trigger();
    CrosshairState cs = crosshairSys->getState();
    float W = ofGetWidth(), H = ofGetHeight();
    hexGrid.pulseAt(ofClamp(cs.cx / W, 0.f, 1.f), ofClamp(cs.cy / H, 0.f, 1.f));

    log.pushLine("> loaded " + basename + " [" + currentCopy_.categoryName + "]");
}

// ── Update helpers ────────────────────────────────────────────────────────────

void HudManager::updateScannerBounds(const CrosshairState& cs) {
    float lfoVal = lfo->get(LFO_CROSSHAIR_PULSE);
    float r = 90.f + lfoVal * 12.f;
    scanner.setBounds(cs.cx - r, cs.cy - r, r * 2.f, r * 2.f);

    float W = ofGetWidth(), H = ofGetHeight();

    // Gauges: centered top, recalculated every frame so resize stays correct
    motionGauge.setBounds(W * 0.5f - 135.f, 10.f, 110.f, 110.f);
    dwellGauge.setBounds( W * 0.5f +  25.f, 10.f, 110.f, 110.f);

    // Fullscreen widgets
    contours.setBounds(0, 0, W, H);
    hexGrid.setBounds( 0, 0, W, H);
    network.setBounds( 0, 0, W, H);
    reticles.setBounds(0, 0, W, H);
    flowField.setBounds(0, 0, W, H);
}

void HudManager::updateTelemetryCards() {
    for (int q = 0; q < 4; q++) {
        auto tel = quadMgr->getTelemetry(q);

        float driftX = (ofNoise(q * 17.3f, time / 120.f) * 2.f - 1.f) * 20.f;
        float driftY = (ofNoise(q * 31.7f + 5.f, time / 120.f) * 2.f - 1.f) * 20.f;
        ofVec2f base = panelBasePos(q);
        quadCards[q].setPosition(base.x + driftX, base.y + driftY);

        hud::DataCardOptions opts;
        opts.title         = currentCopy_.cardTitles[q];
        opts.value         = tel.phase;
        opts.subtitle      = tel.activeShader.empty() ? "---" : tel.activeShader;
        opts.meter         = tel.dwellProgress;
        opts.showMeter     = true;
        opts.showSparkline = true;
        quadCards[q].setOptions(opts);
    }
}

// ── Update ────────────────────────────────────────────────────────────────────

void HudManager::update(float dt) {
    time += dt;

    CrosshairState cs = crosshairSys->getState();
    updateScannerBounds(cs);
    updateTelemetryCards();

    // Live gauge values
    if (showBioGauge) {
        float energy = ofClamp(motionEx->getMotionEnergy() / 0.25f, 0.f, 1.f);
        motionGauge.setValue(energy);
        motionGauge.update(dt);
    }
    if (showDwellGauge) {
        float maxDwell = 0.f;
        for (int q = 0; q < 4; q++) {
            auto tel = quadMgr->getTelemetry(q);
            maxDwell = std::max(maxDwell, tel.dwellProgress);
        }
        dwellGauge.setValue(maxDwell);
        dwellGauge.update(dt);
    }

    // LFO → motion settings (keeps all widgets animated even when off-screen)
    hud::MotionSettings m;
    m.speed = 1.f + lfo->get(LFO_CROSSHAIR_PULSE) * 0.3f;
    m.drift = 1.f + lfo->get(LFO_GRID_DECAY) * 0.2f;
    m.pulse = 1.f + lfo->get(LFO_RD_FEED) * 0.2f;
    contours.setMotion(m);
    hexGrid.setMotion(m);
    network.setMotion(m);

    // Media status light + periodic motion readout — real signal
    // (MotionExtraction's energy is computed every frame regardless of
    // showBioGauge, which only gates the gauge widget's own visibility).
    float energy = motionEx->getMotionEnergy();
    mediaStatus.setState(energy > 0.15f ? hud::StatusState::Alert
        : energy > 0.02f ? hud::StatusState::Active
                          : hud::StatusState::Idle);
    logPushTimer += dt;
    if (logPushTimer >= 2.5f) {
        logPushTimer = 0.f;
        log.pushLine("bio_signal " + ofToString(energy, 3));
    }

    // Advance slot lifecycles
    updateSlot(0, dt);
    updateSlot(1, dt);

    // Update all widgets every frame to keep internal animation warm
    contours.update(dt);
    hexGrid.update(dt);
    network.update(dt);
    scanner.update(dt);
    for (int q = 0; q < 4; q++) quadCards[q].update(dt);
    reticles.update(dt);
    flowField.update(dt);
    mediaStatus.update(dt);
    log.update(dt);
    glitch.update(dt);
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void HudManager::draw(float expansionFade) {
    if (expansionFade <= 0.01f) return;

    // Always-on elements — theme alpha scaled by expansionFade
    scanner.setTheme(themedAt(0.45f * expansionFade)); scanner.draw();
    if (showBioGauge)   { motionGauge.setTheme(themedAt(0.55f * expansionFade)); motionGauge.draw(); }
    if (showDwellGauge) { dwellGauge.setTheme(themedAt(0.55f * expansionFade));  dwellGauge.draw();  }
    for (int q = 0; q < 4; q++) {
        quadCards[q].setTheme(themedAt(0.55f * expansionFade));
        quadCards[q].draw();
    }
    mediaStatus.setTheme(themedAt(0.55f * expansionFade)); mediaStatus.draw();
    log.setTheme(themedAt(0.55f * expansionFade));         log.draw();
    glitch.setTheme(themedAt(1.f * expansionFade));        glitch.draw();

    // Rotating slots — drawnAlpha scaled by expansionFade
    for (auto& slot : slots)
        drawElement(slot.elemIdx, slot.drawnAlpha * expansionFade);

    ofEnableAlphaBlending();

    // Bottom-center slot name labels — fade with their slot alpha and expansionFade
    static const char* elemNames[] = {
        "CONTOURS", "HEX GRID", "NODE NETWORK", "RETICLES", "FLOW FIELD"
    };
    static const float charW = 8.f;

    struct NameAlpha { std::string name; float alpha; };
    std::vector<NameAlpha> labels;
    for (auto& slot : slots) {
        if (!slot.isIdle() && slot.elemIdx >= 0 && slot.drawnAlpha > 0.001f)
            labels.push_back({ elemNames[slot.elemIdx], slot.drawnAlpha * expansionFade });
    }

    if (!labels.empty()) {
        const float gap = 32.f;
        float totalW = 0.f;
        for (auto& l : labels) totalW += l.name.size() * charW;
        if (labels.size() > 1) totalW += gap;

        float x = ofGetWidth() * 0.5f - totalW * 0.5f;
        float y = ofGetHeight() - 18.f;

        for (auto& l : labels) {
            ofColor c = themedAt(l.alpha * 0.65f).colors.muted;
            ofSetColor(c);
            ofDrawBitmapString(l.name, x, y);
            x += l.name.size() * charW + gap;
        }
    }
}
