#include "FakeFragmentScenario.h"

#include "FakeFrameMath.h"

namespace hudpresent {

using namespace fakemath;

FakeFragmentScenario::FakeFragmentScenario() : FakeHudScenarioBase("fragment-trail", "scene.fragment-trail.title") {}

FakeHudScenarioBase::SceneBaseline FakeFragmentScenario::buildBaseline(float t, uint64_t seed) const {
	SceneBaseline b;
	static const std::vector<std::string> states = {
		"scene.fragment.state.drifting", "scene.fragment.state.scanning",
		"scene.fragment.state.hunting", "scene.fragment.state.nervous",
		"scene.fragment.state.orbiting", "scene.fragment.state.decaying"};
	b.alternateStateIds = states;
	size_t idx = static_cast<size_t>(smoothUnit(seed, 930, t, 0.18f) * states.size());
	if (idx >= states.size()) idx = states.size() - 1;
	b.primaryStateId = states[idx];

	SceneMetric movementPreset;
	movementPreset.metricId = "scene.fragment.metric.movement_preset";
	movementPreset.valueType = HudMetricValueType::Identifier;
	movementPreset.dataClass = HudDataClass::Literal;
	static const std::vector<std::string> presets = {"drift", "scan", "hunt"};
	size_t presetIdx = static_cast<size_t>(smoothUnit(seed, 931, t, 0.08f) * presets.size());
	if (presetIdx >= presets.size()) presetIdx = presets.size() - 1;
	movementPreset.valueId = "scene.fragment.metric.movement_preset." + presets[presetIdx];
	b.metrics.push_back(movementPreset);

	SceneMetric fragmentCount;
	fragmentCount.metricId = "scene.fragment.metric.fragment_count";
	fragmentCount.valueType = HudMetricValueType::Count;
	fragmentCount.dataClass = HudDataClass::Literal;
	fragmentCount.value = std::round(smoothUnit(seed, 932, t, 0.3f) * 40.0f);
	b.metrics.push_back(fragmentCount);

	SceneMetric trailPersistence;
	trailPersistence.metricId = "scene.fragment.metric.trail_persistence";
	trailPersistence.valueType = HudMetricValueType::DurationSeconds;
	trailPersistence.dataClass = HudDataClass::Literal;
	trailPersistence.value = 1.0f + smoothUnit(seed, 933, t, 0.15f) * 5.0f;
	b.metrics.push_back(trailPersistence);

	SceneMetric spawnCadence;
	spawnCadence.metricId = "scene.fragment.metric.spawn_cadence";
	spawnCadence.valueType = HudMetricValueType::Scalar;
	spawnCadence.dataClass = HudDataClass::Literal;
	spawnCadence.value = smoothUnit(seed, 934, t, 0.4f) * 3.0f;
	b.metrics.push_back(spawnCadence);

	SceneMetric contentMode;
	contentMode.metricId = "scene.fragment.metric.content_mode";
	contentMode.valueType = HudMetricValueType::Identifier;
	contentMode.dataClass = HudDataClass::Literal;
	contentMode.valueId = "time_slice";
	b.metrics.push_back(contentMode);

	b.effectIds = {"Pixel Drift", "Chromatic Aberration"}; // already-curated display text — see FakeHudSemanticTypes.h
	return b;
}

} // namespace hudpresent
