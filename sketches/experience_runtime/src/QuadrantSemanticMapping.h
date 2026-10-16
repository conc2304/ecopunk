#pragma once

#include "SceneSemanticTypes.h"

#include <array>
#include <cstdint>
#include <string>

// Pure mapping from real QuadrantSceneCore state to the frozen
// HUD-Semantic-Slot-Model-v1 §18.5 slot IDs. Only values with a real,
// documented derivation are published; see the field table in the .cpp.
namespace quadrantsemantics {

// QuadrantManager's per-quadrant cycle phase (getTelemetry().phase).
enum class ChannelPhase { Online, Quiet, Standby };

// ExpansionDirector's state, in its own enum order.
enum class Expansion { Idle, TravelOut, Hold, TravelBack };

struct Inputs {
	std::array<ChannelPhase, 4> channelPhase{ChannelPhase::Standby, ChannelPhase::Standby,
		ChannelPhase::Standby, ChannelPhase::Standby};
	std::array<std::string, 4> channelEffect; // active shader name per quadrant; empty = none
	Expansion expansion = Expansion::Idle;
	float expansionT = 0.0f;       // ExpansionDirector::getExpansionT() (eased travel-out, 1 while held)
	int crosshairPreset = 0;       // CrosshairSystem::getCurrentPreset() (0 DRIFT .. 4 ORBIT)
	float motionEnergy = 0.0f;     // MotionExtraction::getMotionEnergy()
	float stateElapsedSeconds = 0; // production scene: time since primaryStateId last changed
	float activeSeconds = 0.0f;    // production scene's own elapsed-active timer
	uint64_t generation = 0;       // production scene's own activation counter
};

// QuadrantManager::QuadrantTelemetry::phase text -> ChannelPhase
// ("ONLINE", "QUIET PHASE", anything else -> Standby).
ChannelPhase channelPhaseFromTelemetry(const std::string& phase);

// The state ID compute() would publish as primaryStateId.
std::string primaryStateId(const Inputs& in);

SceneSemanticData compute(const Inputs& in);

} // namespace quadrantsemantics
