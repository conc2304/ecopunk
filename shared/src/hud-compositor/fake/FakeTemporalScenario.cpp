#include "FakeTemporalScenario.h"

#include "FakeFrameMath.h"

namespace hudpresent {

using namespace fakemath;

FakeTemporalScenario::FakeTemporalScenario() : FakeHudScenarioBase("temporal-fields", "scene.temporal-fields.title") {}

FakeHudScenarioBase::SceneBaseline FakeTemporalScenario::buildBaseline(float t, uint64_t seed) const {
	SceneBaseline b;
	static const std::vector<std::string> states = {
		"scene.temporal.state.running", "scene.temporal.state.transitioning",
		"scene.temporal.state.evolving", "scene.temporal.state.regenerating"};
	b.alternateStateIds = states;
	size_t idx = static_cast<size_t>(smoothUnit(seed, 920, t, 0.12f) * states.size());
	if (idx >= states.size()) idx = states.size() - 1;
	b.primaryStateId = states[idx];

	SceneMetric pattern;
	pattern.metricId = "scene.temporal.metric.pattern";
	pattern.valueType = HudMetricValueType::Identifier;
	pattern.dataClass = HudDataClass::Literal;
	pattern.valueId = "particle_field";
	b.metrics.push_back(pattern);

	SceneMetric temporalDepth;
	temporalDepth.metricId = "scene.temporal.metric.temporal_depth";
	temporalDepth.valueType = HudMetricValueType::Scalar;
	temporalDepth.dataClass = HudDataClass::Derived;
	temporalDepth.value = 2.0f + smoothUnit(seed, 921, t, 0.1f) * 6.0f;
	b.metrics.push_back(temporalDepth);

	SceneMetric historyFill;
	historyFill.metricId = "scene.temporal.metric.history_fill";
	historyFill.valueType = HudMetricValueType::Ratio;
	historyFill.dataClass = HudDataClass::Normalized;
	historyFill.value = clamp01(smoothUnit(seed, 922, t, 0.2f));
	b.metrics.push_back(historyFill);

	SceneMetric fieldActivity;
	fieldActivity.metricId = "scene.temporal.metric.field_activity";
	fieldActivity.valueType = HudMetricValueType::Ratio;
	fieldActivity.dataClass = HudDataClass::Normalized;
	fieldActivity.value = clamp01(smoothUnit(seed, 923, t, 0.35f));
	b.metrics.push_back(fieldActivity);

	SceneMetric evolutionState;
	evolutionState.metricId = "scene.temporal.metric.evolution_state";
	evolutionState.valueType = HudMetricValueType::Identifier;
	evolutionState.dataClass = HudDataClass::Literal;
	evolutionState.valueId = "drifting";
	b.metrics.push_back(evolutionState);

	SceneMetric playheadCount;
	playheadCount.metricId = "scene.temporal.metric.playhead_count";
	playheadCount.valueType = HudMetricValueType::Count;
	playheadCount.dataClass = HudDataClass::Literal;
	playheadCount.value = 6.0f;
	b.metrics.push_back(playheadCount);

	b.effectIds = {"Hue Rotate", "Water Refraction", "Ink Outlines"}; // already-curated display text — see FakeHudSemanticTypes.h
	return b;
}

} // namespace hudpresent
