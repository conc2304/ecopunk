#include "FakeBlobScenario.h"

#include "FakeFrameMath.h"

namespace hudpresent {

using namespace fakemath;

FakeBlobScenario::FakeBlobScenario() : FakeHudScenarioBase("blob-region-prototype", "scene.blob-region-prototype.title") {}

FakeHudScenarioBase::SceneBaseline FakeBlobScenario::buildBaseline(float t, uint64_t seed) const {
	SceneBaseline b;
	static const std::vector<std::string> states = {
		"scene.blob.state.analyzing", "scene.blob.state.fragmenting",
		"scene.blob.state.stable", "scene.blob.state.degraded"};
	b.alternateStateIds = states;
	size_t idx = static_cast<size_t>(smoothUnit(seed, 900, t, 0.15f) * states.size());
	if (idx >= states.size()) idx = states.size() - 1;
	b.primaryStateId = states[idx];

	SceneMetric regionCount;
	regionCount.metricId = "scene.blob.metric.region_count";
	regionCount.valueType = HudMetricValueType::Count;
	regionCount.dataClass = HudDataClass::Literal;
	regionCount.value = std::round(smoothUnit(seed, 901, t, 0.25f) * 10.0f);
	b.metrics.push_back(regionCount);

	SceneMetric fragmentCount;
	fragmentCount.metricId = "scene.blob.metric.fragment_count";
	fragmentCount.valueType = HudMetricValueType::Count;
	fragmentCount.dataClass = HudDataClass::Literal;
	fragmentCount.value = std::round(smoothUnit(seed, 902, t, 0.3f) * 12.0f);
	b.metrics.push_back(fragmentCount);

	SceneMetric occupiedArea;
	occupiedArea.metricId = "scene.blob.metric.occupied_area";
	occupiedArea.valueType = HudMetricValueType::Ratio;
	occupiedArea.dataClass = HudDataClass::Normalized;
	occupiedArea.normalizedValue = clamp01(smoothUnit(seed, 903, t, 0.2f));
	occupiedArea.value = occupiedArea.normalizedValue;
	b.metrics.push_back(occupiedArea);

	SceneMetric trackingStability;
	trackingStability.metricId = "scene.blob.metric.tracking_stability";
	trackingStability.valueType = HudMetricValueType::Ratio;
	trackingStability.dataClass = HudDataClass::Derived;
	trackingStability.value = clamp01(smoothUnit(seed, 904, t, 0.1f));
	b.metrics.push_back(trackingStability);

	b.effectIds = {"Chromatic Aberration", "Bioluminescence", "Edge Glow"}; // already-curated display text (SceneHudStatus::activeEffects shape) — see FakeHudSemanticTypes.h
	return b;
}

} // namespace hudpresent
