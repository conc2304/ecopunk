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
                const LFOBank& lfo, ofTexture& rdTex, ofTexture& gridTex,
                ofTexture& motionTex);
    void draw(ofTexture& videoTex, glm::vec2 videoSize);
    void drawHUD();
    void setVideoBrightness(float b) { videoBrightness = b; }
    void resetErosion() { for (auto& q : quads) q.resetErosion(); }
    void onTrigger(const TriggerEvent& e);

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
        "dither", "solarize", "scanlines", "channelshift", "motion_effect"
    };

    enum class CyclePhase { PLAYING, SILENCING, READY };

    struct CycleState {
        CyclePhase               phase      = CyclePhase::READY;
        float                    silenceAcc = 0.f;
        float                    silenceDur = 0.f;
        std::string              lastShader;
        std::vector<std::string> deck;       // shuffled queue — pop from back
    };
    std::array<CycleState, 4> cycleStates;

    bool  velHighActive    = false;
    float videoBrightness  = 0.5f;  // updated each frame from ofApp

    struct PendingEffect {
        float        delay;
        int          quadId;
        TriggerID    triggerId;
        bool         triggerActive;
        float        intensity;
        int          quadrantHint;
    };
    std::vector<PendingEffect> pending;

    std::vector<int>           selectQuads(int primary);
    void                       scheduleEffect(const std::vector<int>& quads, const TriggerEvent& e);
    void                       applyTriggerToQuad(int quadId, const TriggerEvent& e);
    void                    kickRandomShader(int quadId);
    std::string             pickNextShader(int quadId);
    float                   computeSilenceDuration(int quadId);
    std::pair<float,float>  chooseDitherParams();
};
