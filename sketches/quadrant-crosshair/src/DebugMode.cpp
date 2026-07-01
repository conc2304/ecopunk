#include "DebugMode.h"
#include <cctype>
#include <algorithm>

const std::vector<std::string> DebugMode::SHADER_NAMES = {
    "desaturate", "invert", "recolor", "threshold",
    "dither", "solarize", "scanlines", "channelshift", "motion_effect",
    "ascii_solarpunk",
    // nature pack
    "bioluminescence", "chromatic_aberration", "edge_glow",
    "ink_outlines", "pixel_drift", "pixel_sorting", "temporal_trails",
    "water_refraction",
    // cpu-side effects (no shader)
    "ridgeline"
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
            p.push_back({ "motionSource",   &pMotionSourceMode,  1.0f,   0.0f,  2.0f   }); // 0=accum 1=delayed 2=blend
            // ── Extraction layer ──────────────────────────────────────────
            p.push_back({ "outputMode",     &pMotionMode,        1.0f,   0.0f,  4.0f   }); // 0=luma 1=chroma 2=signed 3=raw 4=ref
            p.push_back({ "sensitivity",    &pMotionSensitivity, 0.5f,   0.5f, 16.0f   });
            p.push_back({ "boost",          &pMotionBoost,       0.25f,  0.1f,  6.0f   });
            p.push_back({ "neutralGrey",    &pMotionNeutralGrey, 0.05f,  0.0f,  1.0f   });
            p.push_back({ "extractGamma",   &pMotionGamma,       0.1f,   0.2f,  4.0f   });
            p.push_back({ "decay",          &pMotionDecay,       0.005f, 0.85f, 0.999f });
            break;
        case 9: // ascii_solarpunk
            p.push_back({ "cellSize",        &pAsciiCellSize,       1.0f,  4.0f, 32.0f  });
            p.push_back({ "thresholdMin",    &pAsciiThreshMin,      0.02f, 0.0f,  1.0f  });
            p.push_back({ "thresholdMax",    &pAsciiThreshMax,      0.02f, 0.0f,  1.0f  });
            p.push_back({ "thresholdMode",   &pAsciiThreshMode,     1.0f,  0.0f,  3.0f  }); // 0=none 1=above 2=below 3=between
            p.push_back({ "opacity",         &pAsciiOpacity,        0.05f, 0.0f,  1.0f  });
            p.push_back({ "contrast",        &pAsciiContrast,       0.05f, 0.1f,  4.0f  });
            p.push_back({ "bias",            &pAsciiBias,           0.02f,-0.5f,  0.5f  });
            p.push_back({ "softness",        &pAsciiSoftness,       0.005f,0.01f, 0.15f });
            p.push_back({ "colorMode",       &pAsciiColorMode,      1.0f,  0.0f,  1.0f  }); // 0=sampled 1=B&W
            p.push_back({ "invertMono",      &pAsciiInvertMono,     1.0f,  0.0f,  1.0f  }); // 0=white  1=black
            p.push_back({ "backgroundMode",  &pAsciiBackgroundMode, 1.0f,  0.0f,  1.0f  }); // 0=image  1=transparent
            break;
        case 10: // bioluminescence
            p.push_back({ "threshold",     &pBioThreshold,   0.05f, 0.05f, 0.8f  });
            p.push_back({ "intensity",     &pBioIntensity,   0.1f,  0.0f,  3.0f  });
            p.push_back({ "glowColor.r",   &pBioColorR,      0.05f, 0.0f,  1.0f  });
            p.push_back({ "glowColor.g",   &pBioColorG,      0.05f, 0.0f,  1.0f  });
            p.push_back({ "glowColor.b",   &pBioColorB,      0.05f, 0.0f,  1.0f  });
            break;
        case 11: // chromatic_aberration
            p.push_back({ "amount",        &pChromAmount,  0.1f,  0.0f,  6.0f  });
            p.push_back({ "radial",        &pChromRadial,  0.05f, 0.0f,  1.0f  });
            break;
        case 12: // edge_glow
            p.push_back({ "edgeStrength",  &pEdgeStrength, 0.1f,  0.0f,  5.0f  });
            p.push_back({ "glowStrength",  &pGlowStrength, 0.1f,  0.0f,  4.0f  });
            p.push_back({ "glowColor.r",   &pGlowColorR,   0.05f, 0.0f,  1.0f  });
            p.push_back({ "glowColor.g",   &pGlowColorG,   0.05f, 0.0f,  1.0f  });
            p.push_back({ "glowColor.b",   &pGlowColorB,   0.05f, 0.0f,  1.0f  });
            break;
        case 13: // ink_outlines
            p.push_back({ "threshold",     &pInkThreshold,    0.01f, 0.01f, 0.5f   });
            p.push_back({ "inkStrength",   &pInkStrength,     0.05f, 0.0f,  1.0f   });
            p.push_back({ "posterizeLvls", &pPosterizeLevels, 1.0f,  2.0f,  16.0f  });
            break;
        case 14: // pixel_drift
            p.push_back({ "amount",        &pDriftAmount,  0.5f,  0.0f,  20.0f  });
            p.push_back({ "scale",         &pDriftScale,   0.005f,0.005f,0.1f   });
            p.push_back({ "speed",         &pDriftSpeed,   0.1f,  0.0f,  4.0f   });
            break;
        case 15: // pixel_sorting
            p.push_back({ "threshold",     &pSortThreshold,  0.05f, 0.0f,  1.0f  });
            p.push_back({ "rangePx",       &pSortRangePx,    1.0f,  1.0f,  48.0f });
            p.push_back({ "direction",     &pSortDirection,  1.0f,  0.0f,  1.0f  }); // 0=H 1=V
            p.push_back({ "intensity",     &pSortIntensity,  0.05f, 0.0f,  1.0f  });
            break;
        case 16: // temporal_trails
            p.push_back({ "decay",         &pTrailDecay,         0.005f, 0.5f,  0.99f });
            p.push_back({ "currentWeight", &pTrailCurrentWeight, 0.05f,  0.05f, 0.9f  });
            p.push_back({ "brighten",      &pTrailBrighten,      0.01f,  0.9f,  1.5f  });
            break;
        case 17: // water_refraction
            p.push_back({ "amplitude",     &pWaterAmplitude,  0.5f,   0.0f,  20.0f  });
            p.push_back({ "frequency",     &pWaterFrequency,  0.002f, 0.001f,0.08f  });
            p.push_back({ "speed",         &pWaterSpeed,      0.1f,   0.0f,  4.0f   });
            break;
        case 18: // ridgeline
            p.push_back({ "numLines",       &pRidgeNumLines,       1.0f,  10.0f, 80.0f  });
            p.push_back({ "samplesPerLine", &pRidgeSamplesPerLine, 2.0f,  16.0f, 128.0f });
            p.push_back({ "amplitude",      &pRidgeAmplitude,      5.0f,  0.0f,  300.0f });
            p.push_back({ "spacingPct",     &pRidgeSpacingPct,     0.005f,0.02f, 0.30f  });
            p.push_back({ "centerYPct",     &pRidgeCenterYPct,     0.02f, 0.1f,  0.9f   });
            p.push_back({ "marginXPct",     &pRidgeMarginXPct,     0.01f, 0.0f,  0.25f  });
            p.push_back({ "overlay (0/1)",  &pRidgeOverlay,        1.0f,  0.0f,  1.0f   });
            p.push_back({ "flipX (0/1)",    &pRidgeFlipX,          1.0f,  0.0f,  1.0f   });
            p.push_back({ "flipY (0/1)",    &pRidgeFlipY,          1.0f,  0.0f,  1.0f   });
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

    if (name == "ridgeline") {
        if ((int)W != ridgelineCanvasW || (int)H != ridgelineCanvasH) {
            ridgelineRenderer.setup((int)W, (int)H);
            ridgelineCanvasW = (int)W;
            ridgelineCanvasH = (int)H;
        }
        RidgelineRenderer::Params rp;
        rp.numLines       = (int)pRidgeNumLines;
        rp.samplesPerLine = (int)pRidgeSamplesPerLine;
        rp.amplitude      = pRidgeAmplitude;
        rp.spacingPct     = pRidgeSpacingPct;
        rp.centerYPct     = pRidgeCenterYPct;
        rp.marginXPct     = pRidgeMarginXPct;
        rp.overlayMode    = (pRidgeOverlay  > 0.5f);
        rp.flipX          = (pRidgeFlipX    > 0.5f);
        rp.flipY          = (pRidgeFlipY    > 0.5f);
        ridgelineRenderer.setParams(rp);
        ridgelineRenderer.update(video->getPixels());
        ridgelineRenderer.draw(rp.overlayMode ? &video->getTexture() : nullptr);
        return;
    }

    if (!shaderLib->has(name)) {
        ofSetColor(255);
        video->getTexture().draw(0, 0, W, H);
        return;
    }

    ofEnableAlphaBlending();

    ofShader& sh = shaderLib->get(name);
    sh.begin();
    sh.setUniformTexture("tex",  video->getTexture(), 0);  // legacy shaders
    sh.setUniformTexture("tex0", video->getTexture(), 0);  // nature pack shaders
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
        sh.setUniformTexture("motionTex",        motionEx->getMotionTexture(),        4);
        sh.setUniformTexture("motionDelayedTex", motionEx->getDelayedMotionTexture(), 5);
        sh.setUniform1f("motionGamma",      pEffectMotionGamma);
        sh.setUniform1i("blendMode",        (int)pEffectBlendMode);
        sh.setUniform1i("motionSourceMode", (int)pMotionSourceMode);
    }
    if (name == "recolor")
        sh.setUniform3f("tint", glm::vec3(pTintR, pTintG, pTintB));
    if (name == "threshold")
        sh.setUniform1f("threshold", pThreshold);
    if (name == "channelshift")
        sh.setUniform1f("shift", pShift);

    // nature pack uniforms
    float t = ofGetElapsedTimef();
    if (name == "bioluminescence") {
        sh.setUniform1f("time",      t);
        sh.setUniform1f("threshold", pBioThreshold);
        sh.setUniform1f("intensity", pBioIntensity);
        sh.setUniform3f("glowColor", glm::vec3(pBioColorR, pBioColorG, pBioColorB));
    }
    if (name == "chromatic_aberration") {
        sh.setUniform1f("amount", pChromAmount);
        sh.setUniform1f("radial", pChromRadial);
    }
    if (name == "edge_glow") {
        sh.setUniform1f("edgeStrength", pEdgeStrength);
        sh.setUniform1f("glowStrength", pGlowStrength);
        sh.setUniform3f("glowColor",    glm::vec3(pGlowColorR, pGlowColorG, pGlowColorB));
    }
    if (name == "ink_outlines") {
        sh.setUniform1f("threshold",      pInkThreshold);
        sh.setUniform1f("inkStrength",    pInkStrength);
        sh.setUniform1f("posterizeLevels",pPosterizeLevels);
    }
    if (name == "pixel_drift") {
        sh.setUniform1f("time",   t);
        sh.setUniform1f("amount", pDriftAmount);
        sh.setUniform1f("scale",  pDriftScale);
        sh.setUniform1f("speed",  pDriftSpeed);
    }
    if (name == "pixel_sorting") {
        sh.setUniform1f("threshold", pSortThreshold);
        sh.setUniform1f("rangePx",   pSortRangePx);
        sh.setUniform1f("direction", pSortDirection);
        sh.setUniform1f("intensity", pSortIntensity);
    }
    if (name == "water_refraction") {
        sh.setUniform1f("time",      t);
        sh.setUniform1f("amplitude", pWaterAmplitude);
        sh.setUniform1f("frequency", pWaterFrequency);
        sh.setUniform1f("speed",     pWaterSpeed);
    }
    if (name == "ascii_solarpunk") {
        sh.setUniform1f("cellSize",            pAsciiCellSize);
        sh.setUniform1f("thresholdMin",        pAsciiThreshMin);
        sh.setUniform1f("thresholdMax",        pAsciiThreshMax);
        sh.setUniform1i("thresholdMode",       (int)pAsciiThreshMode);
        sh.setUniform1f("opacity",             pAsciiOpacity);
        sh.setUniform1f("contrast",            pAsciiContrast);
        sh.setUniform1f("bias",                pAsciiBias);
        sh.setUniform1f("softness",            pAsciiSoftness);
        sh.setUniform1i("asciiColorMode",      (int)pAsciiColorMode);
        sh.setUniform1i("asciiInvertMono",     (int)pAsciiInvertMono);
        sh.setUniform1i("asciiBackgroundMode", (int)pAsciiBackgroundMode);
    }

    // temporal_trails needs its own ping-pong draw path
    if (name == "temporal_trails") {
        bool needsAlloc = !trailPrevFbo.isAllocated()
                       || trailPrevFbo.getWidth()  != W
                       || trailPrevFbo.getHeight() != H;
        if (needsAlloc) {
            trailPrevFbo.allocate(W, H, GL_RGB);
            trailOutFbo.allocate(W, H, GL_RGB);
            trailPrevFbo.begin(); ofClear(0); trailPrevFbo.end();
        }
        sh.setUniformTexture("currentTex",  video->getTexture(), 0);
        sh.setUniformTexture("previousTex", trailPrevFbo.getTexture(), 1);
        sh.setUniform1f("decay",         pTrailDecay);
        sh.setUniform1f("currentWeight", pTrailCurrentWeight);
        sh.setUniform1f("brighten",      pTrailBrighten);
        // render shader into trailOutFbo, then blit to screen and swap
        trailOutFbo.begin();
        ofClear(0);
        ofSetColor(255);
        video->getTexture().draw(0, 0, W, H);
        trailOutFbo.end();
        sh.end();
        ofSetColor(255);
        trailOutFbo.draw(0, 0, W, H);
        // update prev for next frame
        trailPrevFbo.begin();
        ofClear(0);
        trailOutFbo.draw(0, 0, W, H);
        trailPrevFbo.end();
        ofDisableAlphaBlending();
        return;
    }

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
