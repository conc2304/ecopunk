#pragma once
#include "ofMath.h"

enum class CycleMode {
    GHOST_LAYERS,
    PERPETUAL
};

// Called at each transition point to randomly pick the next mode (50/50).
inline CycleMode selectNextMode() {
    return (ofRandom(1.0f) < 0.5f) ? CycleMode::GHOST_LAYERS : CycleMode::PERPETUAL;
}
