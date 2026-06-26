#include "TriggerBus.h"

void TriggerBus::setup() {
    cooldowns[TriggerID::EDGE_PROXIMITY]  = 0.f;
    cooldowns[TriggerID::VELOCITY_HIGH]   = 0.f;
    cooldowns[TriggerID::VELOCITY_LOW]    = 0.f;
    cooldowns[TriggerID::QUADRANT_CENTER] = 0.f;
    cooldowns[TriggerID::CORNER_NEAR]     = 0.f;
    cooldowns[TriggerID::DWELL]           = 0.f;
}

void TriggerBus::update(const CrosshairState& state, float dt) {
    for (auto& kv : cooldowns)
        kv.second = std::max(0.f, kv.second - dt);

    checkEdgeProximity(state);
    checkVelocity(state);
    checkQuadrantCenter(state);
    checkCornerNear(state);
    checkDwell(state, dt);
}

void TriggerBus::fire(TriggerEvent e) {
    for (auto& cb : listeners) cb(e);
}

void TriggerBus::addListener(TriggerCallback cb) {
    listeners.push_back(cb);
}

void TriggerBus::checkEdgeProximity(const CrosshairState& s) {
    float W = ofGetWidth(), H = ofGetHeight();
    float minDist = std::min({ s.cx, s.cy, W - s.cx, H - s.cy });
    bool near = minDist < EDGE_ZONE;
    if (near != edgeActive) {
        edgeActive = near;
        float d[4] = { s.cx, s.cy, W - s.cx, H - s.cy };
        int edge = (int)(std::min_element(d, d + 4) - d);
        float intensity = near ? ofMap(minDist, EDGE_ZONE, 0, 0, 1, true) : 0.f;
        fire({ TriggerID::EDGE_PROXIMITY, near, intensity, near ? edge : -1 });
    }
}

void TriggerBus::checkVelocity(const CrosshairState& s) {
    bool high = s.speed > HIGH_THRESH;
    if (high != velHighActive) {
        velHighActive = high;
        fire({ TriggerID::VELOCITY_HIGH, high, s.speed / HIGH_THRESH, -1 });
    }

    if (s.speed < LOW_THRESH) velLowAccum += ofGetLastFrameTime();
    else                      velLowAccum  = 0.f;
    bool low = velLowAccum > LOW_SECS;
    if (low != velLowActive) {
        velLowActive = low;
        fire({ TriggerID::VELOCITY_LOW, low, 1.f, 3 });
    }
}

void TriggerBus::checkDwell(const CrosshairState& s, float dt) {
    dwellTotalMove += s.speed;
    dwellAccum     += dt;
    if (dwellAccum >= DWELL_SECS) {
        bool still = dwellTotalMove < DWELL_MOVE;
        if (still != dwellActive) {
            dwellActive = still;
            fire({ TriggerID::DWELL, still, 1.f, -1 });
        }
        dwellAccum     = 0.f;
        dwellTotalMove = 0.f;
    }
}

void TriggerBus::checkQuadrantCenter(const CrosshairState& s) {
    float W = ofGetWidth(), H = ofGetHeight();
    bool near = fabsf(s.cx - W * 0.5f) < CENTER_ZONE
             && fabsf(s.cy - H * 0.5f) < CENTER_ZONE;
    if (near != centerActive) {
        centerActive = near;
        fire({ TriggerID::QUADRANT_CENTER, near, 1.f, -1 });
    }
}

void TriggerBus::checkCornerNear(const CrosshairState& s) {
    float W = ofGetWidth(), H = ofGetHeight();
    bool tl = s.cx < CORNER_ZONE && s.cy < CORNER_ZONE;
    bool tr = s.cx > W - CORNER_ZONE && s.cy < CORNER_ZONE;
    bool bl = s.cx < CORNER_ZONE && s.cy > H - CORNER_ZONE;
    bool br = s.cx > W - CORNER_ZONE && s.cy > H - CORNER_ZONE;
    bool near = tl || tr || bl || br;
    int hint = tl ? 0 : tr ? 1 : bl ? 2 : br ? 3 : -1;
    if (near != cornerActive) {
        cornerActive = near;
        fire({ TriggerID::CORNER_NEAR, near, 1.f, hint });
    }
}
