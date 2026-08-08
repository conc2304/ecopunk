#include "FakeQuadrantScenario.h"

#include "FakeFrameMath.h"

namespace hudpresent {

using namespace fakemath;

FakeQuadrantScenario::FakeQuadrantScenario() : FakeHudScenarioBase("quadrant-crosshair", "scene.quadrant-crosshair.title") {}

FakeHudScenarioBase::SceneBaseline FakeQuadrantScenario::buildBaseline(float t, uint64_t seed) const {
	SceneBaseline b;
	static const std::vector<std::string> states = {
		"scene.quadrant.state.online", "scene.quadrant.state.quiet",
		"scene.quadrant.state.standby", "scene.quadrant.state.expanding",
		"scene.quadrant.state.reconfiguring"};
	b.alternateStateIds = states;
	size_t idx = static_cast<size_t>(smoothUnit(seed, 940, t, 0.16f) * states.size());
	if (idx >= states.size()) idx = states.size() - 1;
	b.primaryStateId = states[idx];

	SceneMetric crosshairPreset;
	crosshairPreset.metricId = "scene.quadrant.metric.crosshair_preset";
	crosshairPreset.valueType = HudMetricValueType::Identifier;
	crosshairPreset.dataClass = HudDataClass::Literal;
	crosshairPreset.valueId = "scene.fragment.metric.movement_preset.drift"; // reuses the same preset vocabulary as Fragment Trail
	b.metrics.push_back(crosshairPreset);

	SceneMetric expansionState;
	expansionState.metricId = "scene.quadrant.metric.expansion_state";
	expansionState.valueType = HudMetricValueType::Identifier;
	expansionState.dataClass = HudDataClass::Literal;
	expansionState.valueId = "idle";
	b.metrics.push_back(expansionState);

	SceneMetric expansionProgress;
	expansionProgress.metricId = "scene.quadrant.metric.expansion_progress";
	expansionProgress.valueType = HudMetricValueType::Ratio;
	expansionProgress.dataClass = HudDataClass::Normalized;
	expansionProgress.value = clamp01(smoothUnit(seed, 941, t, 0.2f));
	b.metrics.push_back(expansionProgress);

	// Four-channel telemetry — already visitor-shaped strings per the
	// probe's own finding (Observability Profile Part 5), reused directly
	// as the per-channel state vocabulary.
	static const std::vector<std::string> channelStates = {
		"scene.quadrant.state.online", "scene.quadrant.state.quiet", "scene.quadrant.state.standby"};
	for (int i = 0; i < 4; ++i) {
		SceneMetric channel;
		channel.metricId = "scene.quadrant.metric.channel_" + std::to_string(i) + "_state";
		channel.valueType = HudMetricValueType::Identifier;
		channel.dataClass = HudDataClass::Literal;
		size_t cidx = static_cast<size_t>(smoothUnit(seed, 950 + static_cast<uint32_t>(i), t, 0.22f) * channelStates.size());
		if (cidx >= channelStates.size()) cidx = channelStates.size() - 1;
		channel.valueId = channelStates[cidx];
		b.metrics.push_back(channel);

		SceneMetric channelEffect;
		channelEffect.metricId = "scene.quadrant.metric.channel_" + std::to_string(i) + "_effect";
		channelEffect.valueType = HudMetricValueType::Identifier;
		channelEffect.dataClass = HudDataClass::Literal;
		static const std::vector<std::string> channelEffects = {
			"heatmap_recolor", "edge_glow", "bioluminescence", "water_refraction"};
		channelEffect.valueId = channelEffects[static_cast<size_t>(i) % channelEffects.size()];
		b.metrics.push_back(channelEffect);
	}

	b.effectIds = {"Heatmap Recolor", "Edge Glow", "Bioluminescence", "Water Refraction"}; // already-curated display text — see FakeHudSemanticTypes.h
	return b;
}

} // namespace hudpresent
