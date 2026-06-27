#include "Quadrant.h"
#include <algorithm>

void Quadrant::setup(int id_, ofRectangle region_, ShaderLibrary* lib_) {
    id        = id_;
    region    = region_;
    shaderLib = lib_;
    lastCx    = region.x + region.width  * 0.5f;
    lastCy    = region.y + region.height * 0.5f;

    ofFbo::Settings s;
    s.width          = 640;
    s.height         = 360;
    s.internalformat = GL_RGBA;
    s.useDepth       = false;
    fbo_read.allocate(s);
    fbo_write.allocate(s);
    fbo_read.begin();  ofClear(0, 0, 0, 255); fbo_read.end();
    fbo_write.begin(); ofClear(0, 0, 0, 255); fbo_write.end();
    erosionReady = true;
}

void Quadrant::setScaleConfig(float min, float max, float seed) {
    scaleMin       = min;
    scaleMax       = max;
    targetScaleMin = min;
    targetScaleMax = max;
    scaleNoiseSeed = seed;
    currentScale   = (min + max) * 0.5f;
}

void Quadrant::setScaleTarget(float min, float max) {
    targetScaleMin = min;
    targetScaleMax = max;
}

void Quadrant::updateSlot(ShaderSlot& slot, float dt) {
    using State = ShaderSlot::State;
    switch (slot.state) {
        case State::FADE_IN:
            slot.alpha += dt / slot.fadeDur;
            if (slot.alpha >= 1.f) {
                slot.alpha    = 1.f;
                slot.state    = State::ACTIVE;
                slot.dwellAcc = 0.f;
            }
            break;
        case State::ACTIVE:
            slot.dwellAcc  += dt;
            slot.noiseTime += dt;
            if (slot.dwellAcc >= slot.dwellDur)
                slot.state = State::FADE_OUT;
            break;
        case State::FADE_OUT:
            slot.alpha -= dt / slot.fadeDur;
            if (slot.alpha <= 0.f)
                slot.clear();
            break;
        case State::IDLE:
            break;
    }
    if (slot.state == State::ACTIVE) {
        // Noise is loud at the dwell edges (first/last 20%) and silent at centre,
        // so the full-opacity "rest" window feels genuinely still.
        float dwellProg = slot.dwellAcc / slot.dwellDur;
        float edgeFactor;
        if (dwellProg < 0.2f)
            edgeFactor = 1.f - dwellProg / 0.2f;
        else if (dwellProg > 0.8f)
            edgeFactor = (dwellProg - 0.8f) / 0.2f;
        else
            edgeFactor = 0.f;

        float noise    = ofNoise(slot.noiseSeed + slot.noiseTime * 0.08f);
        float noiseAmt = ofMap(noise, 0.f, 1.f, -0.15f * edgeFactor, 0.15f * edgeFactor);
        slot.drawnAlpha = ofClamp(slot.alpha + noiseAmt, 0.05f, 1.f);
    } else {
        // Smooth ease-in-out on the linear fade progress
        float p = slot.alpha;
        slot.drawnAlpha = p * p * (3.f - 2.f * p);
    }
}

void Quadrant::update(float dt, float cx, float cy) {
    lastCx = cx;
    lastCy = cy;

    timeAccum += dt;
    float t = timeAccum;

    scaleMin += (targetScaleMin - scaleMin) * 0.5f * dt;
    scaleMax += (targetScaleMax - scaleMax) * 0.5f * dt;

    currentScale = ofMap(
        ofNoise(t * 0.03f + scaleNoiseSeed),
        0, 1, scaleMin, scaleMax
    );

    morphOffX = ofMap(ofNoise(t * 0.018f + id * 9.1f),         0, 1, -25.f,  25.f);
    morphOffY = ofMap(ofNoise(t * 0.018f + id * 9.1f + 77.f),  0, 1, -25.f,  25.f);
    morphW    = ofMap(ofNoise(t * 0.012f + id * 6.3f + 155.f), 0, 1, -50.f,  50.f);
    morphH    = ofMap(ofNoise(t * 0.012f + id * 6.3f + 233.f), 0, 1, -50.f,  50.f);
    cropOffX  = ofMap(ofNoise(t * 0.022f + id * 4.7f + 311.f), 0, 1, -70.f,  70.f);
    cropOffY  = ofMap(ofNoise(t * 0.022f + id * 4.7f + 389.f), 0, 1, -70.f,  70.f);

    for (auto& slot : slots)
        updateSlot(slot, dt);
}

void Quadrant::draw(ofTexture& videoTex, glm::vec2 videoSize) {
    float rx = region.x + morphOffX;
    float ry = region.y + morphOffY;
    float rw = std::max(region.width  + morphW, 1.f);
    float rh = std::max(region.height + morphH, 1.f);

    // ── Erosion accumulation pass (off-screen) ────────────────────────────
    if (erosionReady && shaderLib->has("erosion")) {
        fbo_write.begin();
        ofShader& er = shaderLib->get("erosion");
        er.begin();
        er.setUniformTexture("accumulated", fbo_read.getTexture(), 0);
        er.setUniformTexture("videoFrame",  videoTex,              1);
        er.setUniform1f("decayRate",  decayRate);
        er.setUniform1f("videoAlpha", videoAlpha);
        // Draw via accumulated texture so vert.glsl receives correct texcoords
        ofSetColor(255);
        fbo_read.getTexture().draw(0, 0, fbo_write.getWidth(), fbo_write.getHeight());
        er.end();
        fbo_write.end();
        std::swap(fbo_read, fbo_write);
    }

    // ── Draw to screen (scissor-clipped) ─────────────────────────────────
    // Use exact region bounds (not morphed) so scissor edges stay flush with crosshair lines.
    // Morph offsets still animate the draw position inside the clipped area.
    glEnable(GL_SCISSOR_TEST);
    glScissor(
        (GLint)region.x,
        (GLint)(ofGetHeight() - region.y - region.height),
        (GLint)region.width,
        (GLint)region.height
    );

    // Layer 1: Erosion FBO — ghost/accumulation base
    if (erosionReady) {
        ofPushMatrix();
        ofTranslate(lastCx, lastCy);
        ofScale(currentScale);
        ofTranslate(-lastCx, -lastCy);
        ofSetColor(255);
        fbo_read.getTexture().draw(rx, ry, rw, rh);
        ofPopMatrix();
    }

    // Layer 2: Raw video — same transform as shaders, sits cleanly between ghost and effect
    ofPushMatrix();
    ofTranslate(lastCx, lastCy);
    ofScale(currentScale);
    ofTranslate(-lastCx, -lastCy);
    ofSetColor(255);
    videoTex.draw(cropOffX, cropOffY, videoSize.x, videoSize.y);
    ofPopMatrix();

    // Layer 3: Shader effects alpha-fade in over the raw video
    ofEnableAlphaBlending();
    for (auto& slot : slots) {
        if (!slot.isIdle() && slot.drawnAlpha > 0.f)
            drawWithEffect(videoTex, videoSize, slot.name, slot.drawnAlpha, lastCx, lastCy, slot.ditherArc, slot.ditherPx);
    }
    ofDisableAlphaBlending();

    glDisable(GL_SCISSOR_TEST);
}

bool Quadrant::pushShader(const std::string& name, float fadeSecs, float dwellSecs) {
    using State = ShaderSlot::State;
    for (auto& slot : slots) {
        if (slot.state == State::IDLE) {
            slot.name       = name;
            slot.fadeDur    = fadeSecs;
            slot.dwellDur   = dwellSecs;
            slot.alpha      = 0.f;
            slot.drawnAlpha = 0.f;
            slot.dwellAcc   = 0.f;
            slot.noiseTime  = 0.f;
            slot.noiseSeed  = ofRandom(0.f, 1000.f);
            slot.state      = State::FADE_IN;
            return true;
        }
    }
    for (auto& slot : slots) {
        if (slot.state == State::FADE_OUT) {
            slot.name       = name;
            slot.fadeDur    = fadeSecs;
            slot.dwellDur   = dwellSecs;
            slot.alpha      = 0.f;
            slot.drawnAlpha = 0.f;
            slot.dwellAcc   = 0.f;
            slot.noiseTime  = 0.f;
            slot.noiseSeed  = ofRandom(0.f, 1000.f);
            slot.state      = State::FADE_IN;
            return true;
        }
    }
    return false;
}

void Quadrant::clearShaders(float fadeSecs) {
    using State = ShaderSlot::State;
    for (auto& slot : slots) {
        if (slot.state == State::FADE_IN || slot.state == State::ACTIVE) {
            slot.state   = State::FADE_OUT;
            slot.fadeDur = fadeSecs;
        }
    }
}

void Quadrant::drawWithEffect(ofTexture& tex, glm::vec2 videoSize,
                               const std::string& effect, float alpha,
                               float cx, float cy,
                               float ditherArc, float ditherPx) {
    bool useShader = (effect != "passthrough" && shaderLib->has(effect));

    if (useShader) {
        ofShader& sh = shaderLib->get(effect);
        sh.begin();
        sh.setUniformTexture("tex", tex, 0);
        sh.setUniform2f("resolution", ofGetWidth(), ofGetHeight());
        if (effect == "dither") {
            // arc and px are frozen per-slot at push time; only opacity follows the lifecycle
            sh.setUniform1f("alpha",         ditherArc);
            sh.setUniform1f("opacity",       alpha);
            sh.setUniform1f("maxPixelation", ditherPx);
        } else {
            sh.setUniform1f("alpha", alpha);
        }
        if (rdTex)            sh.setUniformTexture("rdState",          *rdTex,            2);
        if (gridTex)          sh.setUniformTexture("gridState",        *gridTex,          3);
        if (motionTex)        sh.setUniformTexture("motionTex",        *motionTex,        4);
        if (motionDelayedTex) sh.setUniformTexture("motionDelayedTex", *motionDelayedTex, 5);
        sh.setUniform1f("motionGamma",      1.0f);
        sh.setUniform1i("blendMode",        0);
        sh.setUniform1i("motionSourceMode", motionSourceMode);
        bindUniforms(sh);
    }

    ofPushMatrix();
    ofTranslate(cx, cy);
    ofScale(currentScale);
    ofTranslate(-cx, -cy);
    ofSetColor(255, 255, 255, (int)(alpha * 255));
    tex.draw(cropOffX, cropOffY, videoSize.x, videoSize.y);
    ofPopMatrix();

    if (useShader) shaderLib->get(effect).end();
}

void Quadrant::drawDebugHUD(const std::string& phaseStr) const {
    using State = ShaderSlot::State;
    static const char* stateStr[] = { "IDLE", "FADE_IN", "ACTIVE", "FADE_OUT" };

    const float PAD       = 8.f;
    const float LH        = 14.f;
    const float PW        = 224.f;
    const int   lineCount = 2 + (int)slots.size();  // header + slots + settings
    const float PH        = PAD * 2.f + LH * lineCount;

    // Position panel in the outer corner of the quadrant (away from the crosshair centre)
    float px, py;
    switch (id) {
        case 0: px = region.x + 6.f;                     py = region.y + 6.f;                      break;
        case 1: px = region.x + region.width  - PW - 6.f; py = region.y + 6.f;                     break;
        case 2: px = region.x + 6.f;                     py = region.y + region.height - PH - 6.f; break;
        default:px = region.x + region.width  - PW - 6.f; py = region.y + region.height - PH - 6.f;
    }

    ofEnableAlphaBlending();

    ofFill();
    ofSetColor(0, 0, 0, 160);
    ofDrawRectangle(px, py, PW, PH);

    ofNoFill();
    ofSetColor(255, 255, 255, 220);
    ofSetLineWidth(1.f);
    ofDrawRectangle(px, py, PW, PH);

    ofDisableAlphaBlending();

    ofSetColor(255);
    float tx = px + PAD;
    float ty = py + PAD + 10.f;

    ofDrawBitmapString("Q" + ofToString(id) + "  " + phaseStr, tx, ty);
    ty += LH;

    for (const auto& s : slots) {
        std::string line;
        if (s.state == State::IDLE) {
            line = "  ---  idle";
        } else {
            int pct = (int)(s.drawnAlpha * 100.f + 0.5f);
            line    = "  " + s.name + "  " + stateStr[(int)s.state] + "  " + ofToString(pct) + "%";
            if (s.state == State::ACTIVE) {
                float rem = s.dwellDur - s.dwellAcc;
                line += "  (" + ofToString(rem, 0) + "s)";
            }
        }
        ofDrawBitmapString(line, tx, ty);
        ty += LH;
    }

    // Quadrant parameter snapshot
    ofDrawBitmapString(
        "  thr:" + ofToString(threshold, 2)
      + "  shf:" + ofToString(shift, 3)
      + "  px:"  + ofToString((int)maxPixelation),
        tx, ty
    );
}

void Quadrant::resetErosion() {
    fbo_read.begin();  ofClear(0, 0, 0, 255); fbo_read.end();
    fbo_write.begin(); ofClear(0, 0, 0, 255); fbo_write.end();
}

void Quadrant::setDitherParams(float arc, float px) {
    using State = ShaderSlot::State;
    for (auto& slot : slots) {
        if (slot.name == "dither" &&
            (slot.state == State::FADE_IN || slot.state == State::ACTIVE)) {
            slot.ditherArc = arc;
            slot.ditherPx  = px;
            return;
        }
    }
}

void Quadrant::bindUniforms(ofShader& sh) {
    sh.setUniform3f("tint",      tint);
    sh.setUniform1f("threshold", threshold);
    sh.setUniform1f("shift",     shift);
}
