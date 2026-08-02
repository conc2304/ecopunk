#pragma once
#include "ofMain.h"
#include "RidgelineRenderer.h"
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
    float pHueOffset       = 0.0f;
    float pHueSpeed        = 20.0f;
    float pHueSaturationMult = 1.0f;
    float pHueValueMult    = 1.0f;
    // motion_effect controls (fed into MotionExtraction::update each frame)
    float pMotionDecay       = 0.95f;
    float pMotionSensitivity = 4.0f;
    float pMotionMode        = 0.0f;   // 0/1/2 stored as float for param system
    // extraction tuning
    float pMotionNeutralGrey = 0.5f;
    float pMotionBoost       = 1.0f;
    float pMotionGamma       = 1.0f;
    // composite tuning
    float pEffectBlendMode   = 0.0f;   // 0/1/2 stored as float
    float pEffectMotionGamma = 1.0f;
    float pMotionSourceMode  = 0.0f;   // 0=accum  1=delayed  2=blend

    // nature pack params
    float pBioThreshold      = 0.3f;
    float pBioIntensity      = 1.0f;
    float pBioColorR         = 0.1f;
    float pBioColorG         = 1.0f;
    float pBioColorB         = 0.75f;

    float pChromAmount       = 1.5f;
    float pChromRadial       = 0.5f;

    float pEdgeStrength      = 1.5f;
    float pGlowStrength      = 1.0f;
    float pGlowColorR        = 0.3f;
    float pGlowColorG        = 1.0f;
    float pGlowColorB        = 0.55f;

    float pInkThreshold      = 0.2f;
    float pInkStrength       = 1.0f;
    float pPosterizeLevels   = 6.0f;

    float pDriftAmount       = 6.0f;
    float pDriftScale        = 0.03f;
    float pDriftSpeed        = 0.5f;

    float pSortThreshold     = 0.5f;
    float pSortRangePx       = 12.0f;
    float pSortDirection     = 0.0f;
    float pSortIntensity     = 1.0f;

    float pTrailDecay        = 0.92f;
    float pTrailCurrentWeight= 0.25f;
    float pTrailBrighten     = 1.05f;

    float pWaterAmplitude    = 6.0f;
    float pWaterFrequency    = 0.02f;
    float pWaterSpeed        = 1.0f;

    // heatmap_recolor params (alpha/mix reuses pAlpha)
    float pHeatmapIntensity     = 1.0f;
    float pHeatmapGamma         = 0.9f;
    float pHeatmapMinLuminance  = 0.05f;
    float pHeatmapMaxLuminance  = 0.95f;
    float pHeatmapPalette       = 0.0f;  // 0=thermal 1=bioluminescent 2=solarpunk 3=infrared
    float pHeatmapReverse       = 0.0f;  // 0/1

    // ascii_solarpunk params
    float pAsciiCellSize        = 12.0f;
    float pAsciiThreshMin       = 0.55f;
    float pAsciiThreshMax       = 1.0f;
    float pAsciiThreshMode      = 1.0f;  // 0=none 1=aboveMin 2=belowMax 3=between
    float pAsciiOpacity         = 1.0f;
    float pAsciiContrast        = 1.15f;
    float pAsciiBias            = 0.0f;
    float pAsciiSoftness        = 0.03f;
    float pAsciiColorMode       = 0.0f;  // 0=sampledSourceColor 1=blackAndWhite
    float pAsciiInvertMono      = 0.0f;  // 0=white glyphs 1=black glyphs
    float pAsciiBackgroundMode  = 0.0f;  // 0=originalImage 1=transparent

    ofFbo trailPrevFbo;   // temporal_trails: previous accumulated frame
    ofFbo trailOutFbo;    // temporal_trails: current output (ping-pong target)

    // ridgeline effect
    RidgelineRenderer ridgelineRenderer;
    int ridgelineCanvasW = 0;
    int ridgelineCanvasH = 0;
    float pRidgeNumLines      = 80.0f;
    float pRidgeSamplesPerLine= 128.0f;
    float pRidgeAmplitude     = 150.0f;
    float pRidgeSpacingPct    = 0.020f;
    float pRidgeCenterYPct    = 0.470f;
    float pRidgeMarginXPct    = -0.020f;
    float pRidgeOverlay       = 0.0f;  // 0=black ground  1=source image under
    float pRidgeFlipX         = 0.0f;  // 0=normal  1=mirror horizontally
    float pRidgeFlipY         = 1.0f;  // 0=normal  1=mirror vertically

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
