#pragma once
#include "ofMain.h"
#include "ShaderLibrary.h"
#include "VideoSystem.h"
#include "MotionExtraction.h"
#include "TriggerBus.h"
#include <vector>
#include <array>
#include <string>

struct DebugParam {
    std::string label;
    float*      value;
    float       step;
    float       min, max;
};

class DebugMode {
public:
    bool active = false;

    void setup(ShaderLibrary* lib, VideoSystem* vid, MotionExtraction* motion);
    void update();
    void draw();
    bool keyPressed(int key);   // returns true if key was consumed

private:
    ShaderLibrary*    shaderLib = nullptr;
    VideoSystem*      video     = nullptr;
    MotionExtraction* motionEx  = nullptr;

    static const std::vector<std::string> SHADER_NAMES;
    int shaderIndex   = 0;
    int selectedParam = 0;

    // Per-param storage — persistent across shader switches
    float pAlpha           = 1.0f;
    float pThreshold       = 0.5f;
    float pShift           = 0.004f;
    float pMaxPixelation   = 8.0f;
    float pTintR           = 1.0f;
    float pTintG           = 0.78f;
    float pTintB           = 0.25f;
    // motion_effect controls (fed into MotionExtraction::update each frame)
    float pMotionDecay     = 0.95f;
    float pMotionSensitivity = 4.0f;
    float pMotionMode      = 0.0f;   // 0/1/2 stored as float for param system

    struct TriggerSim {
        TriggerID   id;
        char        key;
        std::string label;
        bool        state     = false;
        bool        fireOnce  = false;  // true = one-shot, auto-resets after 0.5s
        float       fireTimer = 0.f;
    };
    std::array<TriggerSim, 6> triggers;

    std::vector<DebugParam> buildParams();
    void drawShaderFullScreen();
    void drawPanel(const std::vector<DebugParam>& params);
    void cyclePrev();
    void cycleNext();
    void adjustParam(int direction, bool coarse);
    void handleTriggerKey(int key);
};
