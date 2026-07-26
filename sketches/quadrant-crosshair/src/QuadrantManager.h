#pragma once
#include "ofMain.h"
#include "Quadrant.h"
#include "ShaderLibrary.h"
#include "TriggerBus.h"
#include "LFOBank.h"
#include <array>
#include <utility>
#include <vector>
#include <string>

class QuadrantManager {
public:
    void setup(ShaderLibrary* lib);
    void update(float dt, const CrosshairState& state,
                const LFOBank& lfo, ofTexture& gridTex,
                ofTexture& motionTex, ofTexture& motionDelayedTex);
    void draw(ofTexture& videoTex, glm::vec2 videoSize);
    void drawHUD();
    void setVideoBrightness(float b) { videoBrightness = b; }
    void resetErosion() { for (auto& q : quads) q.resetErosion(); }
    void resize(int w, int h) { for (auto& q : quads) q.resize(w, h); }
    void onTrigger(const TriggerEvent& e);
    void setVideoPixels(const ofPixels* px);

    // Expansion contraction crossfade — called by ofApp when TRAVEL_BACK begins
    void beginContraction(int quadrantID);

    struct QuadrantTelemetry {
        std::string phase;        // "ONLINE", "QUIET PHASE", "STANDBY"
        std::string activeShader; // current shader name, or ""
        float dwellProgress;      // 0–1
    };
    QuadrantTelemetry getTelemetry(int quadId) const;

private:
    std::array<Quadrant, 4> quads;

    std::vector<std::string> shaderPool = {
        "desaturate", "invert", "recolor", "threshold",
        "dither", "solarize", "scanlines", "channelshift", "motion_effect",
        "hue_rotate", "ascii_solarpunk",
        // nature pack (temporal_trails excluded — needs per-quadrant FBO)
        "bioluminescence", "chromatic_aberration", "edge_glow",
        "ink_outlines", "pixel_drift", "pixel_sorting", "water_refraction",
        // cpu-side effects
        "ridgeline"
    };

    enum class CyclePhase { PLAYING, SILENCING, READY };

    struct CycleState {
        CyclePhase               phase            = CyclePhase::READY;
        float                    silenceAcc       = 0.f;
        float                    silenceDur       = 0.f;
        std::string              lastShader;
        std::vector<std::string> deck;             // shuffled queue — pop from back
        std::string              savedShader;      // shader interrupted by a trigger
        float                    savedDwellRemain = 0.f;
        bool                     resumePending    = false;
    };
    std::array<CycleState, 4> cycleStates;

    bool  velHighActive    = false;
    float videoBrightness  = 0.5f;
    const ofPixels* currentVideoPixels = nullptr; // non-owning; updated each frame via setVideoPixels

    // Expansion contraction crossfade state
    float contractionT        = 0.f;
    float contractionDuration = 3.f;
    bool  contracting         = false;
    int   expandedQuadrant    = -1;

    struct PendingEffect {
        float        delay;
        int          quadId;
        TriggerID    triggerId;
        bool         triggerActive;
        float        intensity;
        int          quadrantHint;
    };
    std::vector<PendingEffect> pending;

    std::array<float, 4>       scaleChangeAcc = {};
    std::array<float, 4>       scaleChangeDur = {};

    std::vector<int>           selectQuads(int primary);
    void                       scheduleEffect(const std::vector<int>& quads, const TriggerEvent& e);
    void                       applyTriggerToQuad(int quadId, const TriggerEvent& e);
    void                    kickRandomShader(int quadId);
    std::string             pickNextShader(int quadId);
    float                   computeSilenceDuration(int quadId);
    std::pair<float,float>  chooseDitherParams();
    std::pair<float,float>  chooseHueRotateParams();
    std::pair<float,float>  pickScaleRange();
};
