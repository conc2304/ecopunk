#include "DebugMode.h"
#include <cctype>
#include <algorithm>

const std::vector<std::string> DebugMode::SHADER_NAMES = {
    "desaturate", "invert", "recolor", "threshold",
    "dither", "solarize", "scanlines", "channelshift", "motion_effect"
};

// ─────────────────────────────────────────────────────────────────────────────

void DebugMode::setup(ShaderLibrary* lib, VideoSystem* vid, MotionExtraction* motion) {
    shaderLib = lib;
    video     = vid;
    motionEx  = motion;

    triggers[0] = { TriggerID::EDGE_PROXIMITY,  'E', "EDGE_PROXIMITY",  false, false, 0.f };
    triggers[1] = { TriggerID::VELOCITY_HIGH,   'V', "VELOCITY_HIGH",   false, false, 0.f };
    triggers[2] = { TriggerID::VELOCITY_LOW,    'L', "VELOCITY_LOW",    false, true,  0.f };
    triggers[3] = { TriggerID::QUADRANT_CENTER, 'C', "QUADRANT_CENTER", false, false, 0.f };
    triggers[4] = { TriggerID::CORNER_NEAR,     'K', "CORNER_NEAR",     false, false, 0.f };
    triggers[5] = { TriggerID::DWELL,           'W', "DWELL",           false, true,  0.f };
}

// ─────────────────────────────────────────────────────────────────────────────

void DebugMode::update() {
    video->update();
    if (video->isFrameNew()) {
        motionEx->extractNeutralGrey = pMotionNeutralGrey;
        motionEx->extractBoost       = pMotionBoost;
        motionEx->extractGamma       = pMotionGamma;
        motionEx->update(video->getTexture(), pMotionDecay, pMotionSensitivity);
        motionEx->setOutputMode((int)pMotionMode);
    }

    float dt = ofGetLastFrameTime();
    for (auto& t : triggers) {
        if (t.fireOnce && t.state) {
            t.fireTimer -= dt;
            if (t.fireTimer <= 0.f) {
                t.state     = false;
                t.fireTimer = 0.f;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────

std::vector<DebugParam> DebugMode::buildParams() {
    std::vector<DebugParam> p;

    switch (shaderIndex) {
        case 0: // desaturate
            p.push_back({ "alpha",         &pAlpha,         0.05f, 0.f, 1.f });
            break;
        case 1: // invert
            p.push_back({ "alpha",         &pAlpha,         0.05f, 0.f, 1.f });
            break;
        case 2: // recolor
            p.push_back({ "tintR",         &pTintR,         0.05f, 0.f, 1.f });
            p.push_back({ "tintG",         &pTintG,         0.05f, 0.f, 1.f });
            p.push_back({ "tintB",         &pTintB,         0.05f, 0.f, 1.f });
            p.push_back({ "alpha",         &pAlpha,         0.05f, 0.f, 1.f });
            break;
        case 3: // threshold
            p.push_back({ "threshold",     &pThreshold,     0.05f, 0.f, 1.f });
            p.push_back({ "alpha",         &pAlpha,         0.05f, 0.f, 1.f });
            break;
        case 4: // dither — alpha is a transition-progress arc: 0.5=peak pixelation, 0/1=clean
            p.push_back({ "maxPixelation", &pMaxPixelation, 1.0f,  1.f, 32.f });
            p.push_back({ "alpha (arc)",   &pAlpha,         0.05f, 0.f, 1.f });
            break;
        case 5: // solarize
            p.push_back({ "alpha",         &pAlpha,         0.05f, 0.f, 1.f });
            break;
        case 6: // scanlines
            p.push_back({ "alpha",         &pAlpha,         0.05f, 0.f, 1.f });
            break;
        case 7: // channelshift
            p.push_back({ "shift",         &pShift,         0.002f, 0.f, 0.05f });
            p.push_back({ "alpha",         &pAlpha,         0.05f,  0.f, 1.f  });
            break;
        case 8: // motion_effect
            // ── Composite layer ───────────────────────────────────────────
            p.push_back({ "alpha",          &pAlpha,             0.05f,  0.f,   1.f    });
            p.push_back({ "blendMode",      &pEffectBlendMode,   1.0f,   0.0f,  2.0f   }); // 0=mix 1=add 2=screen
            p.push_back({ "effectGamma",    &pEffectMotionGamma, 0.1f,   0.2f,  4.0f   });
            // ── Extraction layer ──────────────────────────────────────────
            p.push_back({ "outputMode",     &pMotionMode,        1.0f,   0.0f,  2.0f   }); // 0=luma 1=chroma 2=signed
            p.push_back({ "sensitivity",    &pMotionSensitivity, 0.5f,   0.5f, 16.0f   });
            p.push_back({ "boost",          &pMotionBoost,       0.25f,  0.1f,  6.0f   });
            p.push_back({ "neutralGrey",    &pMotionNeutralGrey, 0.05f,  0.0f,  1.0f   });
            p.push_back({ "extractGamma",   &pMotionGamma,       0.1f,   0.2f,  4.0f   });
            p.push_back({ "decay",          &pMotionDecay,       0.005f, 0.85f, 0.999f });
            break;
        default:
            break;
    }
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────

void DebugMode::drawShaderFullScreen() {
    const std::string& name = SHADER_NAMES[shaderIndex];
    float W = ofGetWidth(), H = ofGetHeight();

    if (!shaderLib->has(name)) {
        ofSetColor(255);
        video->getTexture().draw(0, 0, W, H);
        return;
    }

    ofEnableAlphaBlending();

    ofShader& sh = shaderLib->get(name);
    sh.begin();
    sh.setUniformTexture("tex", video->getTexture(), 0);
    sh.setUniform2f("resolution", W, H);

    if (name == "dither") {
        // alpha = arc position (what you're tuning), opacity = full in debug
        sh.setUniform1f("alpha",         pAlpha);
        sh.setUniform1f("opacity",       1.0f);
        sh.setUniform1f("maxPixelation", pMaxPixelation);
    } else {
        sh.setUniform1f("alpha", pAlpha);
    }

    if (name == "motion_effect") {
        sh.setUniformTexture("motionTex", motionEx->getMotionTexture(), 4);
        sh.setUniform1f("motionGamma", pEffectMotionGamma);
        sh.setUniform1i("blendMode",   (int)pEffectBlendMode);
    }
    if (name == "recolor")
        sh.setUniform3f("tint", glm::vec3(pTintR, pTintG, pTintB));
    if (name == "threshold")
        sh.setUniform1f("threshold", pThreshold);
    if (name == "channelshift")
        sh.setUniform1f("shift", pShift);

    ofSetColor(255);
    video->getTexture().draw(0, 0, W, H);
    sh.end();

    ofDisableAlphaBlending();
}

// ─────────────────────────────────────────────────────────────────────────────

void DebugMode::drawPanel(const std::vector<DebugParam>& params) {
    const float PAD  = 10.f;
    const float LH   = 14.f;
    const float PW   = 300.f;
    const float COL2 = 190.f;   // x offset for value column

    const std::string& shaderName = SHADER_NAMES[shaderIndex];
    int n = (int)SHADER_NAMES.size();

    // Count total lines for dynamic height
    int totalLines = 2          // "DEBUG [D=exit]" + "EFFECT: name  N/9  [</> cycle]"
                   + 1          // separator
                   + 1          // "PARAMS  [UP/DN sel]  [+/- adj]"
                   + 1          // separator
                   + (int)params.size()
                   + 1          // separator
                   + 1          // "TRIGGERS  (toggle key)"
                   + 1          // separator
                   + 6;         // trigger rows

    float PH = PAD * 2.f + LH * totalLines;
    float px = 8.f;
    float py = ofGetHeight() - PH - 8.f;

    // Background
    ofEnableAlphaBlending();
    ofFill();
    ofSetColor(0, 0, 0, 170);
    ofDrawRectangle(px, py, PW, PH);

    // Border
    ofNoFill();
    ofSetColor(255, 255, 255, 220);
    ofSetLineWidth(1.f);
    ofDrawRectangle(px, py, PW, PH);
    ofDisableAlphaBlending();

    ofSetColor(255);
    float tx = px + PAD;
    float ty = py + PAD + 10.f;    // bitmap string baseline

    // Header
    ofDrawBitmapString("DEBUG  [D=exit]", tx, ty);
    ty += LH;
    ofDrawBitmapString("EFFECT: " + shaderName
                       + "  " + ofToString(shaderIndex + 1) + "/" + ofToString(n)
                       + "   [</> cycle]",
                       tx, ty);
    ty += LH;

    // Params section
    ofDrawBitmapString("----------------------------", tx, ty); ty += LH;
    ofDrawBitmapString("PARAMS  [UP/DN]  [= / - adj]  [+/_ x10]", tx, ty); ty += LH;
    ofDrawBitmapString("----------------------------", tx, ty); ty += LH;

    for (int i = 0; i < (int)params.size(); i++) {
        const DebugParam& dp = params[i];
        std::string prefix = (i == selectedParam) ? "> " : "  ";
        float step = dp.step;
        // Decide decimal places from step size
        int decimals = (step < 0.1f) ? 3 : 2;
        std::string valStr = ofToString(*dp.value, decimals);
        ofDrawBitmapString(prefix + dp.label, tx, ty);
        ofDrawBitmapString(valStr, tx + COL2, ty);
        ty += LH;
    }

    // Triggers section
    ofDrawBitmapString("----------------------------", tx, ty); ty += LH;
    ofDrawBitmapString("TRIGGERS  (toggle with key)", tx, ty); ty += LH;
    ofDrawBitmapString("----------------------------", tx, ty); ty += LH;

    for (const auto& t : triggers) {
        std::string stateStr;
        if (t.fireOnce) {
            stateStr = t.state ? "FIRE" : "off";
        } else {
            stateStr = t.state ? "ON" : "off";
        }
        std::string line = "  [" + std::string(1, t.key) + "] " + t.label;
        ofDrawBitmapString(line, tx, ty);
        ofDrawBitmapString(stateStr, tx + COL2, ty);
        ty += LH;
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void DebugMode::draw() {
    ofBackground(13, 13, 13);
    drawShaderFullScreen();
    auto params = buildParams();
    drawPanel(params);
}

// ─────────────────────────────────────────────────────────────────────────────

void DebugMode::cyclePrev() {
    shaderIndex   = (shaderIndex - 1 + (int)SHADER_NAMES.size()) % (int)SHADER_NAMES.size();
    selectedParam = 0;
    pAlpha        = 1.0f;
}

void DebugMode::cycleNext() {
    shaderIndex   = (shaderIndex + 1) % (int)SHADER_NAMES.size();
    selectedParam = 0;
    pAlpha        = 1.0f;
}

void DebugMode::adjustParam(int direction, bool coarse) {
    auto params = buildParams();
    if (params.empty()) return;
    selectedParam = ofClamp(selectedParam, 0, (int)params.size() - 1);
    DebugParam& dp = params[selectedParam];
    float delta = dp.step * (coarse ? 10.f : 1.f) * direction;
    *dp.value = ofClamp(*dp.value + delta, dp.min, dp.max);
}

void DebugMode::handleTriggerKey(int key) {
    int upper = toupper(key);
    for (auto& t : triggers) {
        if (upper == (int)t.key) {
            if (t.fireOnce) {
                t.state     = true;
                t.fireTimer = 0.5f;
            } else {
                t.state = !t.state;
            }
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────

bool DebugMode::keyPressed(int key) {
    auto params = buildParams();

    if (key == OF_KEY_LEFT)  { cyclePrev(); return true; }
    if (key == OF_KEY_RIGHT) { cycleNext(); return true; }

    if (key == OF_KEY_UP) {
        selectedParam = std::max(0, selectedParam - 1);
        return true;
    }
    if (key == OF_KEY_DOWN) {
        selectedParam = std::min((int)params.size() - 1, selectedParam + 1);
        return true;
    }

    if (key == '=' || key == '-' || key == '+' || key == '_') {
        bool coarse = (key == '+' || key == '_');
        int  dir    = (key == '=' || key == '+') ? +1 : -1;
        adjustParam(dir, coarse);
        return true;
    }

    // Trigger keys: E V L C K W
    int upper = toupper(key);
    if (upper == 'E' || upper == 'V' || upper == 'L' ||
        upper == 'C' || upper == 'K' || upper == 'W') {
        handleTriggerKey(key);
        return true;
    }

    return false;
}
