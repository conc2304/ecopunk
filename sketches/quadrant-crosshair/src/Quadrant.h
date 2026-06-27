#pragma once
#include "ofMain.h"
#include "ShaderLibrary.h"
#include <string>
#include <array>

struct ShaderSlot {
    enum class State { IDLE, FADE_IN, ACTIVE, FADE_OUT };
    std::string name;
    float alpha      = 0.f;
    float drawnAlpha = 0.f;  // noise-modulated alpha used for drawing
    float fadeDur    = 1.2f;
    float dwellDur   = 6.f;
    float dwellAcc   = 0.f;
    float noiseTime  = 0.f;
    float noiseSeed  = 0.f;
    float ditherArc  = 0.45f;
    float ditherPx   = 4.0f;
    State state      = State::IDLE;

    bool isIdle() const { return state == State::IDLE; }
    void clear() { name = ""; alpha = 0.f; drawnAlpha = 0.f; state = State::IDLE; dwellAcc = 0.f; noiseTime = 0.f; ditherArc = 0.45f; ditherPx = 4.0f; }
};

class Quadrant {
public:
    int         id;
    ofRectangle region;

    void setup(int id, ofRectangle region, ShaderLibrary* lib);
    void setScaleConfig(float min, float max, float seed);
    void setScaleTarget(float min, float max);
    void update(float dt, float cx, float cy);
    void draw(ofTexture& videoTex, glm::vec2 videoSize);

    bool pushShader(const std::string& name, float fadeSecs = 1.2f, float dwellSecs = 6.f);
    void clearShaders(float fadeSecs = 1.2f);
    void resetErosion();
    void drawDebugHUD(const std::string& phaseStr) const;

    bool allSlotsIdle() const {
        for (const auto& slot : slots)
            if (!slot.isIdle()) return false;
        return true;
    }

    std::string activeShaderName() const {
        for (const auto& slot : slots)
            if (!slot.isIdle()) return slot.name;
        return "";
    }

    float primaryDwellProgress() const {
        if (!slots[0].isIdle() && slots[0].dwellDur > 0.f)
            return ofClamp(slots[0].dwellAcc / slots[0].dwellDur, 0.f, 1.f);
        return 0.f;
    }

    bool getActivePlaying(std::string& outName, float& outDwellRemain) const {
        using State = ShaderSlot::State;
        for (const auto& slot : slots) {
            if (slot.state == State::ACTIVE) {
                outName        = slot.name;
                outDwellRemain = std::max(slot.dwellDur - slot.dwellAcc, 0.f);
                return true;
            }
            if (slot.state == State::FADE_IN) {
                outName        = slot.name;
                outDwellRemain = slot.dwellDur;
                return true;
            }
        }
        return false;
    }

    void setTint(glm::vec3 t)        { tint           = t; }
    void setThreshold(float t)       { threshold       = t; }
    void setShift(float s)           { shift           = s; }
    void setDecay(float d)           { decayRate       = d; }
    void setMaxPixelation(float m)   { maxPixelation   = m; }
    void setDitherParams(float arc, float px);
    void setRDTexture(ofTexture& t)          { rdTex            = &t; }
    void setGridTexture(ofTexture& t)        { gridTex          = &t; }
    void setMotionTexture(ofTexture& t)      { motionTex        = &t; }
    void setMotionDelayedTexture(ofTexture& t) { motionDelayedTex = &t; }
    void setMotionSourceMode(int m)          { motionSourceMode = m; }

private:
    ShaderLibrary* shaderLib = nullptr;

    std::array<ShaderSlot, 2> slots;

    float scaleNoiseSeed  = 0.0f;
    float currentScale    = 1.0f;
    float scaleMin        = 0.8f;
    float scaleMax        = 1.0f;
    float targetScaleMin  = 0.8f;
    float targetScaleMax  = 1.0f;

    float     lastCx = 640.f, lastCy = 360.f;
    glm::vec3 tint      = { 1.f, 0.78f, 0.25f };
    float     threshold      = 0.5f;
    float     shift          = 0.004f;
    float     maxPixelation  = 4.0f;
    float     timeAccum      = 0.f;

    float morphOffX = 0.f, morphOffY = 0.f;
    float morphW    = 0.f, morphH    = 0.f;
    float cropOffX  = 0.f, cropOffY  = 0.f;

    // Erosion / residue layer
    ofFbo      fbo_read, fbo_write;
    bool       erosionReady = false;
    float      decayRate    = 0.97f;
    float      videoAlpha   = 0.03f;
    ofTexture* rdTex            = nullptr;
    ofTexture* gridTex          = nullptr;
    ofTexture* motionTex        = nullptr;
    ofTexture* motionDelayedTex = nullptr;
    int        motionSourceMode = 0;  // 0=accum  1=delayed  2=blend

    void updateSlot(ShaderSlot& slot, float dt);
    void drawWithEffect(ofTexture& tex, glm::vec2 videoSize,
                        const std::string& effect, float alpha,
                        float cx, float cy,
                        float ditherArc, float ditherPx);
    void bindUniforms(ofShader& sh);
};
