#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"

namespace hud {

enum class ReticlePreset { Standard, Tracking };

// Motion bank for the Tracking preset. setOptions() blends between behaviors
// over time rather than cutting instantly, so scenes can swap behavior mid-flight.
enum class ReticleBehavior { Wander, Sweep };

struct ReticleOptions {
    int targetCount = 5;
    bool showLabels = true;
    bool randomTargets = true;
    ReticlePreset preset = ReticlePreset::Tracking;
    ReticleBehavior behavior = ReticleBehavior::Wander;
    float trackingLifetime = 5.0f; // seconds each reticle tracks before dying

    // When non-empty, overrides the built-in label table. Cycled modulo size
    // (safe for any non-empty size, including fewer entries than targetCount).
    std::vector<std::string> labelOverride;
};

class ReticleWidget : public HudWidget {
public:
    void setup() override;
    void randomize(int seed = -1) override;
    void setOptions(const ReticleOptions& next);
    void update(float dt) override;
    void draw() override;
    ofVec2f getMinSize() const override { return { 150.0f, 100.0f }; }
private:
    enum class TargetLifecycle { Spawning, Tracking, Dying };
    struct Target {
        ofVec2f p;                              // goal position (normalized)
        ofVec2f drawnP;                         // actual drawn position
        ofVec2f goalV;                          // Wander behavior: current wander velocity
        float sweepHeading        = 0.0f;      // Sweep behavior: current heading (radians)
        float sweepDesiredHeading = 0.0f;      // Sweep behavior: heading being eased toward
        float sweepTimer          = 0.0f;      // Sweep behavior: time left on the current heading
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

    // Behavior blend state: motion eases from `fromBehavior` to `toBehavior` over
    // kBehaviorBlendDur whenever setOptions() changes options.behavior, instead of
    // cutting instantly — lets a scene change swap behaviors without a visible snap.
    ReticleBehavior fromBehavior = ReticleBehavior::Wander;
    ReticleBehavior toBehavior   = ReticleBehavior::Wander;
    float behaviorMix = 1.0f; // 0 = fully fromBehavior, 1 = fully toBehavior

    void rebuild();
    void respawn(Target& t, int labelIdx);
    void updateTracking(Target& t, float dt, float mix);
    ofVec2f computeWanderVelocity(Target& t, float dt);
    ofVec2f computeSweepVelocity(Target& t, float dt);
    const std::string& labelFor(int idx) const;
};

} // namespace hud
