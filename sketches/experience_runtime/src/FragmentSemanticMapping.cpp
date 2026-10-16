#include "FragmentSemanticMapping.h"

#include <algorithm>

// Field table (source -> slot):
//   state.primaryStateId     | crosshair movement preset: DRIFT->drifting, SCAN->scanning,
//                            |   HUNT->hunting, NERVOUS->nervous, ORBIT->orbiting
//                            |   ("decaying" has no backing state — never produced)
//   activity.overall         | fragmentCount / maxFragments, clamped to [0,1]
//   activity.density         | same ratio (§18.4: active fragment count / configured maximum)
//   activity.motion          | crosshair speed through TriggerBus's own low/high thresholds
//                            |   (the same 0..1 "energy" the spawn-cadence speed response uses)
//   activity.variation/transition | absent (no derivation defined)
//   timing.activeSeconds     | production scene's own timer — always present
//   timing.stateElapsedSeconds | seconds since the current preset was applied
//   timing.stateProgress     | absent (presets are not timer-bounded in production: auto-cycle off)
//   timing.generation        | production scene's own activation counter
//   scene.fragment.metric.movement_preset  | Identifier, scene.fragment.metric.movement_preset.<key>
//   scene.fragment.metric.fragment_count   | Count, Literal
//   scene.fragment.metric.trail_persistence | DurationSeconds = sustain + decay (fragment lifetime)
//   scene.fragment.metric.spawn_cadence    | Scalar, spawns per second (measured, smoothed)
//   scene.fragment.metric.content_mode     | Identifier, bare FTContentMode key (FakeFragmentScenario's
//                                          |   "time_slice" convention); absent if unknown
namespace fragmentsemantics {

namespace {

const char* const kPresetKeys[] = {"drift", "scan", "hunt", "nervous", "orbit"};
const char* const kStateIds[] = {
	"scene.fragment.state.drifting", "scene.fragment.state.scanning", "scene.fragment.state.hunting",
	"scene.fragment.state.nervous", "scene.fragment.state.orbiting"};
constexpr int kPresetCount = 5;

SceneMetric numberMetric(const char* id, HudMetricValueType type, float value) {
	SceneMetric m;
	m.metricId = id;
	m.valueType = type;
	m.dataClass = HudDataClass::Literal;
	m.value = value;
	return m;
}

SceneMetric identifierMetric(const char* id, const std::string& valueId) {
	SceneMetric m;
	m.metricId = id;
	m.valueType = HudMetricValueType::Identifier;
	m.dataClass = HudDataClass::Literal;
	m.valueId = valueId;
	return m;
}

} // namespace

std::string presetKey(int crosshairPreset) {
	if (crosshairPreset < 0 || crosshairPreset >= kPresetCount) return std::string();
	return kPresetKeys[crosshairPreset];
}

SceneSemanticData compute(const Inputs& in) {
	SceneSemanticData data;
	data.schemaVersion = 1;

	const bool presetKnown = in.crosshairPreset >= 0 && in.crosshairPreset < kPresetCount;
	data.state.primaryStateId = presetKnown ? kStateIds[in.crosshairPreset] : kStateIds[2];

	const int cap = std::max(1, in.maxFragments);
	const float density = std::clamp(static_cast<float>(in.fragmentCount) / static_cast<float>(cap), 0.0f, 1.0f);
	data.activity.overall = density;
	data.activity.density = density;
	data.activity.motion = std::clamp(in.crosshairEnergy01, 0.0f, 1.0f);

	data.timing.activeSeconds = in.activeSeconds;
	data.timing.stateElapsedSeconds = in.presetElapsedSeconds;
	data.timing.generation = in.generation;

	if (presetKnown) {
		data.metrics.push_back(identifierMetric("scene.fragment.metric.movement_preset",
			"scene.fragment.metric.movement_preset." + presetKey(in.crosshairPreset)));
	}
	data.metrics.push_back(numberMetric("scene.fragment.metric.fragment_count", HudMetricValueType::Count,
		static_cast<float>(in.fragmentCount)));
	data.metrics.push_back(numberMetric("scene.fragment.metric.trail_persistence",
		HudMetricValueType::DurationSeconds, in.trailLifetimeSeconds));
	data.metrics.push_back(numberMetric("scene.fragment.metric.spawn_cadence", HudMetricValueType::Scalar,
		std::max(0.0f, in.spawnsPerSecond)));
	if (!in.contentModeId.empty()) {
		data.metrics.push_back(identifierMetric("scene.fragment.metric.content_mode", in.contentModeId));
	}
	return data;
}

} // namespace fragmentsemantics
