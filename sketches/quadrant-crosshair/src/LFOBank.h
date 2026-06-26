#pragma once
#include "ofMain.h"
#include <array>

enum LFOIndex {
    LFO_CROSSHAIR_PULSE = 0,
    LFO_ARM_H,
    LFO_ARM_V,
    LFO_THRESH_Q0,
    LFO_THRESH_Q1,
    LFO_SHIFT_Q2,
    LFO_TINT_HUE,
    LFO_DITHER_SCALE,
    LFO_SCAN_DARK,
    LFO_RD_FEED,
    LFO_RD_KILL,
    LFO_GRID_DECAY,
    LFO_COUNT = 12
};

class LFOBank {
public:
    void  setup();
    void  update(float dt);
    float get(int index) const;  // returns -1 to 1

private:
    struct Lane {
        float freq;
        float phase;
        float value;
    };
    std::array<Lane, LFO_COUNT> lanes;
    float timeAccum = 0.f;
};
