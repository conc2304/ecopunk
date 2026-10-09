#include "BlueprintSemanticMapping.h"

#include <algorithm>

// Field table (source -> slot):
//   state.primaryStateId     | CompositionBase::CyclePhase: BLANK->blank, PLACEMENT->placing,
//                            |   DENSITY->building, DISSOLVE->dissolving, RESET_HOLD->hold
//                            |   ("settling" has no backing phase — never produced)
//   activity.overall         | fragmentCount / MAX_FRAGMENTS, clamped to [0,1] (ghost-layer
//                            |   and dissolving fragments can exceed the cap -> saturates at 1)
//   activity.motion/density/variation/transition | absent (no derivation defined yet)
//   timing.activeSeconds     | production scene's own timer — always present
//   timing.stateElapsedSeconds | CompositionBase::getPhaseElapsed() — always present
//   timing.stateProgress     | CompositionBase::getPhaseProgress() — only while the phase is
//                            |   timer-bounded (BLANK/RESET_HOLD; PLACEMENT/DENSITY in ghost-layers
//                            |   mode); absent during DISSOLVE and perpetual-mode placement
//   timing.generation        | production scene's own activation counter
//   scene.blueprint.metric.fragment_count | Count, Literal
//   scene.blueprint.metric.cycle_mode     | Identifier, scene.blueprint.metric.cycle_mode.<mode>
//   scene.blueprint.metric.zone_balance   | Ratio = zoneA / (zoneA + zoneB); absent with no fragments
//   scene.blueprint.metric.phase_progress | Ratio, same value as timing.stateProgress, same absence rule
//   scene.blueprint.metric.zone_a_density, zone_b_density | absent (§18.6 data gap: no normalization)
namespace blueprintsemantics {

namespace {

const char* stateIdForPhase(int phase) {
	switch (phase) {
		case 0: return "scene.blueprint.state.blank";
		case 1: return "scene.blueprint.state.placing";
		case 2: return "scene.blueprint.state.building";
		case 3: return "scene.blueprint.state.dissolving";
		case 4: return "scene.blueprint.state.hold";
	}
	return "scene.blueprint.state.blank";
}

SceneMetric numberMetric(const char* id, HudMetricValueType type, float value) {
	SceneMetric m;
	m.metricId = id;
	m.valueType = type;
	m.dataClass = (type == HudMetricValueType::Ratio) ? HudDataClass::Normalized : HudDataClass::Literal;
	m.value = value;
	return m;
}

} // namespace

SceneSemanticData compute(const Inputs& in) {
	SceneSemanticData data;
	data.schemaVersion = 1;

	data.state.primaryStateId = stateIdForPhase(in.phase);

	const int cap = std::max(1, in.maxFragments);
	data.activity.overall = std::clamp(static_cast<float>(in.fragmentCount) / static_cast<float>(cap), 0.0f, 1.0f);

	data.timing.activeSeconds = in.activeSeconds;
	data.timing.stateElapsedSeconds = in.phaseElapsedSeconds;
	data.timing.stateProgress = in.phaseProgress;
	data.timing.generation = in.generation;

	data.metrics.push_back(numberMetric("scene.blueprint.metric.fragment_count", HudMetricValueType::Count,
		static_cast<float>(in.fragmentCount)));

	if (!in.cycleModeId.empty()) {
		SceneMetric mode;
		mode.metricId = "scene.blueprint.metric.cycle_mode";
		mode.valueType = HudMetricValueType::Identifier;
		mode.dataClass = HudDataClass::Literal;
		mode.valueId = "scene.blueprint.metric.cycle_mode." + in.cycleModeId;
		data.metrics.push_back(mode);
	}

	const int zoned = in.zoneACount + in.zoneBCount;
	if (zoned > 0) {
		data.metrics.push_back(numberMetric("scene.blueprint.metric.zone_balance", HudMetricValueType::Ratio,
			static_cast<float>(in.zoneACount) / static_cast<float>(zoned)));
	}

	if (in.phaseProgress) {
		data.metrics.push_back(numberMetric("scene.blueprint.metric.phase_progress", HudMetricValueType::Ratio,
			*in.phaseProgress));
	}

	return data;
}

} // namespace blueprintsemantics
