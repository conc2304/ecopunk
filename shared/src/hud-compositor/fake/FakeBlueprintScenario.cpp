#include "FakeBlueprintScenario.h"

#include "FakeFrameMath.h"

namespace hudpresent {

using namespace fakemath;

FakeBlueprintScenario::FakeBlueprintScenario() : FakeHudScenarioBase("blueprint_emergence", "scene.blueprint_emergence.title") {}

FakeHudScenarioBase::SceneBaseline FakeBlueprintScenario::buildBaseline(float t, uint64_t seed) const {
	SceneBaseline b;
	static const std::vector<std::string> states = {
		"scene.blueprint.state.blank", "scene.blueprint.state.placing",
		"scene.blueprint.state.building", "scene.blueprint.state.settling",
		"scene.blueprint.state.dissolving", "scene.blueprint.state.hold"};
	b.alternateStateIds = states;
	size_t idx = static_cast<size_t>(smoothUnit(seed, 960, t, 0.14f) * states.size());
	if (idx >= states.size()) idx = states.size() - 1;
	b.primaryStateId = states[idx];

	SceneMetric cycleMode;
	cycleMode.metricId = "scene.blueprint.metric.cycle_mode";
	cycleMode.valueType = HudMetricValueType::Identifier;
	cycleMode.dataClass = HudDataClass::Literal;
	cycleMode.valueId = "perpetual";
	b.metrics.push_back(cycleMode);

	SceneMetric fragmentCount;
	fragmentCount.metricId = "scene.blueprint.metric.fragment_count";
	fragmentCount.valueType = HudMetricValueType::Count;
	fragmentCount.dataClass = HudDataClass::Literal;
	fragmentCount.value = std::round(1.0f + smoothUnit(seed, 961, t, 0.25f) * 3.0f);
	b.metrics.push_back(fragmentCount);

	SceneMetric zoneA;
	zoneA.metricId = "scene.blueprint.metric.zone_a_density";
	zoneA.valueType = HudMetricValueType::Ratio;
	zoneA.dataClass = HudDataClass::Normalized;
	zoneA.value = clamp01(smoothUnit(seed, 962, t, 0.2f));
	b.metrics.push_back(zoneA);

	SceneMetric zoneB;
	zoneB.metricId = "scene.blueprint.metric.zone_b_density";
	zoneB.valueType = HudMetricValueType::Ratio;
	zoneB.dataClass = HudDataClass::Normalized;
	zoneB.value = clamp01(smoothUnit(seed, 963, t, 0.2f));
	b.metrics.push_back(zoneB);

	SceneMetric zoneBalance;
	zoneBalance.metricId = "scene.blueprint.metric.zone_balance";
	zoneBalance.valueType = HudMetricValueType::Ratio;
	zoneBalance.dataClass = HudDataClass::Derived;
	zoneBalance.value = clamp01(0.5f + (*zoneA.value - *zoneB.value) * 0.5f);
	b.metrics.push_back(zoneBalance);

	SceneMetric phaseProgress;
	phaseProgress.metricId = "scene.blueprint.metric.phase_progress";
	phaseProgress.valueType = HudMetricValueType::Ratio;
	phaseProgress.dataClass = HudDataClass::Normalized;
	phaseProgress.value = clamp01(smoothUnit(seed, 964, t, 0.3f));
	b.metrics.push_back(phaseProgress);

	b.effectIds = {"Bioluminescence", "Hue Rotate"}; // already-curated display text — see FakeHudSemanticTypes.h
	return b;
}

} // namespace hudpresent
