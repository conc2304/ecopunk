#include "LFOBank.h"

void LFOBank::setup() {
    lanes = {{
        { 0.031f, 0.00f, 0.f },   // LFO_CROSSHAIR_PULSE  ~32s cycle
        { 0.047f, 1.10f, 0.f },   // LFO_ARM_H            ~21s
        { 0.053f, 2.30f, 0.f },   // LFO_ARM_V            ~19s
        { 0.019f, 0.70f, 0.f },   // LFO_THRESH_Q0        ~53s
        { 0.023f, 3.50f, 0.f },   // LFO_THRESH_Q1        ~43s
        { 0.061f, 1.80f, 0.f },   // LFO_SHIFT_Q2         ~16s
        { 0.013f, 0.40f, 0.f },   // LFO_TINT_HUE         ~77s
        { 0.071f, 2.90f, 0.f },   // LFO_DITHER_SCALE     ~14s
        { 0.041f, 4.20f, 0.f },   // LFO_SCAN_DARK        ~24s
        { 0.007f, 1.60f, 0.f },   // LFO_RD_FEED          ~143s
        { 0.009f, 0.90f, 0.f },   // LFO_RD_KILL          ~111s
        { 0.017f, 3.10f, 0.f },   // LFO_GRID_DECAY       ~59s
    }};
}

void LFOBank::update(float dt) {
    timeAccum += dt;
    for (auto& lane : lanes)
        lane.value = sinf(timeAccum * lane.freq * TWO_PI + lane.phase);
}

float LFOBank::get(int index) const {
    return lanes[index].value;
}
