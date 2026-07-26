#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"

namespace hud {

enum class ReticlePreset { Standard, Tracking };

// Motion bank for the Tracking preset. setOptions() blends between behaviors
// over time rather than cutting instantly, so scenes can swap behavior mid-flight.
enum class ReticleBehavior { Wander, Locate };

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

    // Locate behavior's waypoint pool, normalized [0,1]. When randomTargets
    // is false and this is non-empty, pickWaypoint() travels between these
    // points (e.g. a host sketch's live fragment centers) instead of
    // uniformly random locations. Safe to call every frame — pickWaypoint()
    // only consults it when starting a new leg, so updating it mid-travel
    // doesn't retarget a reticle already in flight.
    void setLocateTargets(const std::vector<ofVec2f>& targetsNorm) { locateTargets = targetsNorm; }

    void update(float dt) override;
    void draw() override;
    ofVec2f getMinSize() const override { return { 150.0f, 100.0f }; }
private:
    enum class TargetLifecycle { Spawning, Tracking, Dying };
    struct Target {
        ofVec2f p;                              // goal position (normalized)
        ofVec2f drawnP;                         // actual drawn position
        ofVec2f goalV;                          // Wander behavior: current wander velocity
        ofVec2f spotTarget      = {0.5f, 0.5f}; // Locate behavior: waypoint being traveled to
        bool    spotDwelling    = true;         // Locate behavior: holding at the waypoint vs. traveling
        float   spotDwellTimer  = 0.0f;         // Locate behavior: time left in the current dwell
        float   spotTravelT     = 0.0f;         // Locate behavior: time since the current leg started (for ease-in)
        float   spotSpeedScale  = 1.0f;         // Locate behavior: per-leg cruise speed multiplier
        int     spotLegsTotal   = 1;            // Locate behavior: targets to visit this life (1-3)
        int     spotLegsDone    = 0;            // Locate behavior: targets visited so far
        bool    spotFinished    = false;        // Locate behavior: sequence complete, ready to retire
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
    std::vector<ofVec2f> locateTargets; // see setLocateTargets()

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
    ofVec2f computeLocateVelocity(Target& t, float dt);
    void pickWaypoint(Target& t) const;
    const std::string& labelFor(int idx) const;
};

} // namespace hud
