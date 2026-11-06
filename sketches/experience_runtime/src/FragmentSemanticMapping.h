#pragma once

#include "SceneSemanticTypes.h"

#include <cstdint>
#include <string>

// Pure mapping from real FragmentSceneCore state to the frozen
// HUD-Semantic-Slot-Model-v1 §18.4 slot IDs. Only values with a real,
// documented derivation are published; see the field table in the .cpp.
namespace fragmentsemantics {

struct Inputs {
	int crosshairPreset = 0;        // CrosshairSystem::getCurrentPreset() (0 DRIFT .. 4 ORBIT)
	float presetElapsedSeconds = 0; // FragmentSceneCore::presetElapsedSeconds()
	int fragmentCount = 0;          // FTFragmentPool::getFragmentCount()
	int maxFragments = 1;           // FTParameterPanel::getMaxFragments() — the pool's hard cap
	float crosshairEnergy01 = 0.0f; // FragmentSceneCore::crosshairEnergy01()
	float trailLifetimeSeconds = 0; // sustain + decay (FTFragment total lifetime)
	float spawnsPerSecond = 0.0f;   // FragmentProductionScene's measured spawn rate
	std::string contentModeId;      // "time_slice" / "effect_varied" / "effect_param_variant" / "backdrop_effect"
	float activeSeconds = 0.0f;     // FragmentProductionScene's own elapsed-active timer
	uint64_t generation = 0;        // FragmentProductionScene's own activation counter
};

// Lowercase preset key in CrosshairSystem's preset order ("drift", "scan",
// "hunt", "nervous", "orbit"); empty for an out-of-range index.
std::string presetKey(int crosshairPreset);

SceneSemanticData compute(const Inputs& in);

} // namespace fragmentsemantics
