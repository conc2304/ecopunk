#pragma once
#include "ofMain.h"

namespace quadrantcrosshair {

class MotionExtraction {
public:
    enum ReferenceMode {
        REFERENCE_DELAYED_FRAME = 0,
        REFERENCE_ACCUMULATION  = 1
    };

    // Shader file paths, set before setup(). Defaults are this sketch's own
    // bin/data layout; ExperienceRuntime passes the paths its shared
    // MotionExtraction (Blueprint) already loads.
    void setShaderPaths(const std::string& vert, const std::string& accum, const std::string& extract) {
        vertShaderPath = vert; accumShaderPath = accum; extractShaderPath = extract;
    }
    void setup();
    // Clears accumulated motion (accum, extraction and history FBOs; energy/
    // centroid) without reallocating — a fresh start with no trails from
    // earlier media or an earlier activation.
    void reset();
    void update(const ofTexture& videoTex, float decayWeight, float sensitivity);

    void setOutputMode(int mode);
    void setReferenceMode(int mode);
    void setDelayFrames(int frames);

    // Accumulation-based extraction (original behavior, used by fullscreen overlay)
    ofTexture& getMotionTexture();
    // Delayed-frame extraction (current - N frames ago, available to quadrants)
    ofTexture& getDelayedMotionTexture();
    ofTexture& getAccumTexture();
    ofTexture& getDelayedTexture();

    int   getOutputMode()      const { return outputMode; }
    int   getReferenceMode()   const { return referenceMode; }
    int   getDelayFrames()     const { return delayFrames; }

    float getMotionEnergy()    const { return motionEnergy; }
    float getMotionCentroidX() const { return motionCentroidX; }
    float getMotionCentroidY() const { return motionCentroidY; }

    // Extraction tuning dials
    float extractNeutralGrey = 0.5f;   // baseline grey for stylized modes
    float extractBoost       = 1.0f;   // multiplier on diff magnitude
    float extractGamma       = 1.0f;   // power curve on extraction output

    static constexpr int ACCUM_W = 160;
    static constexpr int ACCUM_H = 90;

#ifdef PLATFORM_PI
    static constexpr int MAX_HISTORY_FRAMES = 15;
#else
    static constexpr int MAX_HISTORY_FRAMES = 60;
#endif

private:
    std::string vertShaderPath = "shaders/vert.glsl";
    std::string accumShaderPath = "shaders/motion_accum.glsl";
    std::string extractShaderPath = "shaders/motion_extract.glsl";

    ofFbo    fboAccumA, fboAccumB;
    bool     pingPong = false;
    ofShader accumShader;

    ofFbo    fboMotion;         // accumulation-based extraction (fullscreen overlay)
    ofFbo    fboMotionDelayed;  // delayed-frame extraction (quadrant variety)
    ofShader extractShader;

    int outputMode    = 0;
    int referenceMode = REFERENCE_ACCUMULATION;  // default for API/DebugMode callers
    int delayFrames   = 1;

    // Low-res history ring buffer for current - N frames ago.
    std::vector<ofFbo> frameHistory;
    int historyWriteIndex = 0;
    int historyCount      = 0;

    float motionEnergy    = 0.f;
    float motionCentroidX = 0.5f;
    float motionCentroidY = 0.5f;

    // Accum snapshot diff for motion energy — updated every N frames so the
    // slowly-changing accum accumulates enough change to survive 8-bit rounding.
    ofPixels prevAccumPixels;
    bool     prevAccumSet         = false;
    int      prevAccumSnapshotAge = 0;
    int      sampleThrottleCount  = 0;
    static constexpr int ENERGY_SNAPSHOT_INTERVAL = 8;

    void allocateHistory();

    void updateAccum(const ofTexture& videoTex, float decayWeight);
    // Renders one extraction pass into targetFbo using the given reference mode.
    void updateExtract(const ofTexture& videoTex, float sensitivity, int refMode, ofFbo& targetFbo);
    void updateFrameHistory(const ofTexture& videoTex);
    void sampleControlValues();
};

} // namespace quadrantcrosshair

#if !defined(QC_PRODUCTION_RUNTIME)
// Standalone build only: this sketch excludes shared/src's top level, so the
// shared video-effects tree it compiles (effects/MotionExtractionEffect.h)
// has always resolved "MotionExtraction" to this local class. Keep that
// global name here. ExperienceRuntime (QC_PRODUCTION_RUNTIME) links the
// shared ::MotionExtraction too, so there the fork stays namespaced only.
using quadrantcrosshair::MotionExtraction;
#endif
