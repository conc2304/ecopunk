#pragma once

#include "SceneSemanticTypes.h"

#include <cstdint>
#include <optional>
#include <string>

// Pure mapping from real BlueprintSceneCore/BEComposition state to the frozen
// HUD-Semantic-Slot-Model-v1 §18.6 slot IDs. Only values with a real,
// documented derivation are published; zone_a/zone_b_density stay absent
// (no normalization defined). phase_progress is published only while the
// current phase is timer-bounded (CompositionBase::getPhaseProgress()).
namespace blueprintsemantics {

struct Inputs {
	int phase = 0;              // CompositionBase::CyclePhase, cast to int
	float phaseElapsedSeconds = 0.0f; // CompositionBase::getPhaseElapsed()
	std::optional<float> phaseProgress; // CompositionBase::getPhaseProgress(); nullopt = not timer-bounded
	std::string cycleModeId;    // "ghost_layers" / "perpetual" (BEComposition::getCurrentMode())
	int fragmentCount = 0;      // BECompositionState::fragmentCount (incl. ghost/dissolving fragments)
	int zoneACount = 0;         // BECompositionState::zoneACount
	int zoneBCount = 0;         // BECompositionState::zoneBCount
	int maxFragments = 1;       // BESettings MAX_FRAGMENTS — the composition's own slot cap
	float activeSeconds = 0.0f; // BlueprintProductionScene's own elapsed-active timer
	uint64_t generation = 0;    // BlueprintProductionScene's own activation counter
};

SceneSemanticData compute(const Inputs& in);

} // namespace blueprintsemantics
