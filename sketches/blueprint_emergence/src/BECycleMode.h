#pragma once
#include "ofMath.h"

enum class CycleMode {
    GHOST_LAYERS,
    PERPETUAL
};

// Called at each transition point to pick the next classic mode. PERPETUAL was
// retired from the Blueprint mode selection (too similar to GHOST_LAYERS); its
// enum value and update path are dormant and no longer reachable from here,
// from NEXT MODE, or from the cycle's own transitions.
inline CycleMode selectNextMode() {
    return CycleMode::GHOST_LAYERS;
}
