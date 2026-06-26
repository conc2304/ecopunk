#pragma once
#include "ofMain.h"
#include "CrosshairSystem.h"
#include <functional>
#include <map>

enum class TriggerID {
    EDGE_PROXIMITY,
    VELOCITY_HIGH,
    VELOCITY_LOW,
    QUADRANT_CENTER,
    CORNER_NEAR,
    DWELL,
};

struct TriggerEvent {
    TriggerID id;
    bool      active;
    float     intensity;
    int       quadrantHint;
};

using TriggerCallback = std::function<void(const TriggerEvent&)>;

class TriggerBus {
public:
    void setup();
    void update(const CrosshairState& state, float dt);
    void addListener(TriggerCallback cb);

private:
    void fire(TriggerEvent e);
    void checkEdgeProximity(const CrosshairState& s);
    void checkVelocity(const CrosshairState& s);
    void checkQuadrantCenter(const CrosshairState& s);
    void checkCornerNear(const CrosshairState& s);
    void checkDwell(const CrosshairState& s, float dt);

    std::vector<TriggerCallback> listeners;

    bool edgeActive    = false;
    bool velHighActive = false;
    bool velLowActive  = false;
    bool centerActive  = false;
    bool cornerActive  = false;
    bool dwellActive   = false;

    float dwellAccum     = 0.f;
    float velLowAccum    = 0.f;
    float dwellTotalMove = 0.f;

    std::map<TriggerID, float> cooldowns;

    static constexpr float EDGE_ZONE    = 100.f;
    static constexpr float HIGH_THRESH  =   4.0f;
    static constexpr float LOW_THRESH   =   0.8f;
    static constexpr float LOW_SECS     =   3.0f;
    static constexpr float CENTER_ZONE  =  80.f;
    static constexpr float CORNER_ZONE  = 150.f;
    static constexpr float DWELL_MOVE   =  25.f;
    static constexpr float DWELL_SECS   =   6.0f;
    static constexpr float COOLDOWN_SEC =   1.5f;
};
