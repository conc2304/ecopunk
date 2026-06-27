#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"

namespace hud {

enum class ReticlePreset { Standard, Tracking };

struct ReticleOptions {
    int targetCount = 5;
    bool showLabels = true;
    bool randomTargets = true;
    ReticlePreset preset = ReticlePreset::Tracking;
    float trackingLifetime = 5.0f; // seconds each reticle tracks before dying
};

class ReticleWidget : public HudWidget {
public:
    void setup() override;
    void randomize(int seed = -1) override;
    void setOptions(const ReticleOptions& next) { options = next; rebuild(); }
    void update(float dt) override;
    void draw() override;
private:
    enum class TargetLifecycle { Spawning, Tracking, Dying };
    struct Target {
        ofVec2f p;                              // goal position (normalized)
        ofVec2f drawnP;                         // actual drawn position
        ofVec2f goalV;                          // goal wander velocity
        ofVec2f jitter;                         // micro-correction offset (normalized)
        float jitterTimer = 0.0f;
        float phase       = 0.0f;              // breathe phase offset
        float size        = 0.1f;
        std::string label;
        TargetLifecycle lifecycle = TargetLifecycle::Spawning;
        float stateT   = 0.0f;
        float lifetime = 5.0f;
    };
    std::vector<Target> targets;
    ReticleOptions options;
    HudFrameRenderer frame;
    void rebuild();
    void respawn(Target& t, int labelIdx);
    void updateTracking(Target& t, float dt);
};

} // namespace hud
