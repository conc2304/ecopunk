#pragma once
#include "HudWidget.h"
#include "HudFrameRenderer.h"

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
    ofVec2f getMinSize() const override { return { 150.0f, 100.0f }; }
private:
    enum class TargetLifecycle { Spawning, Tracking, Dying };
    struct Target {
        ofVec2f p;
        ofVec2f drawnP;
        ofVec2f goalV;
        ofVec2f jitter;
        float jitterTimer = 0.0f;
        float phase       = 0.0f;
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
