#pragma once
#include "../shared/HudWidget.h"
#include "../shared/HudFrameRenderer.h"
#include <vector>
#include <functional>

namespace hud {

struct PulseEmitterConfig {
    float frequencyMs = 900.0f;  // mean interval between emissions, per emitter (±15% jitter)
    float speed = 90.0f;         // px/sec radius growth @ 1280x720
    float range = 260.0f;        // px, max radius before a pulse retires
    int concurrency = 4;         // max simultaneous pulses per emitter
    // Width of the soft gradient trailing a pulse's leading edge, used by
    // this widget's own draw(). Radar Pulse's engineering handoff flags this
    // as possibly reveal-mask-specific rather than universal; kept here as
    // the widget's own rendering dial. A consumer stamping pulses into its
    // own mask shader is free to apply a different falloff to the same
    // position/radius/age data instead of relying on this value.
    float bandWidth = 60.0f;
};

// x/y/radius are in this widget's bounds-local pixel space (origin at the
// widget's top-left, same convention HudBounds already uses elsewhere) —
// not normalized [0,1] — so a consumer can size the widget to match a video
// frame's resolution and use these values directly as mask-stamp coordinates.
struct PulseInfo {
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    float ageMs = 0.0f;
    int emitterId = -1;
};

class PulseEmitterWidget : public HudWidget {
public:
    void setConfig(const PulseEmitterConfig & next) { config = next; }
    const PulseEmitterConfig & getConfig() const { return config; }

    // Registers a new emitter origin in bounds-local pixel space. Returns the
    // emitter's id, used to tag its pulses in getActivePulses()/onPulseUpdate().
    int addEmitter(float x, float y);

    void update(float dt) override;
    void draw() override;
    ofVec2f getMinSize() const override { return { 100.0f, 100.0f }; }

    std::vector<PulseInfo> getActivePulses() const;

    // Invoked once per active pulse, per frame, from update() — preferred
    // over polling getActivePulses() when a consumer needs to react to
    // pulse state (e.g. stamping a reveal mask) as it changes.
    void onPulseUpdate(std::function<void(const PulseInfo &)> callback) { pulseCallback = std::move(callback); }

private:
    struct Emitter {
        int id = -1;
        float x = 0.0f, y = 0.0f;
        float nextSpawnMs = 0.0f;
    };
    struct Pulse {
        int emitterId = -1;
        float x = 0.0f, y = 0.0f;
        float ageMs = 0.0f;
    };

    PulseEmitterConfig config;
    HudFrameRenderer frame;
    std::vector<Emitter> emitters;
    std::vector<Pulse> pulses;
    std::function<void(const PulseInfo &)> pulseCallback;
    int nextEmitterId = 0;

    float jitteredInterval() const;
};

} // namespace hud
