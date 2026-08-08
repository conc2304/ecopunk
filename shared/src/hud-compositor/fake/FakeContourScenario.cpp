#include "FakeContourScenario.h"

#include "FakeFrameMath.h"

namespace hudpresent {

using namespace fakemath;

FakeContourScenario::FakeContourScenario() : FakeHudScenarioBase("contour-portrait", "scene.contour-portrait.title") {}

FakeHudScenarioBase::SceneBaseline FakeContourScenario::buildBaseline(float t, uint64_t seed) const {
	SceneBaseline b;
	// "reforming" deliberately left out of HudVocabularyResolver's
	// canonical table (see that file's comment) — included here so it's
	// a real, in-use ID this fake data exercises the vocabulary fallback
	// tier against.
	static const std::vector<std::string> states = {
		"scene.contour.state.tracing", "scene.contour.state.displacing",
		"scene.contour.state.breaking", "scene.contour.state.reforming"};
	b.alternateStateIds = states;
	size_t idx = static_cast<size_t>(smoothUnit(seed, 910, t, 0.2f) * states.size());
	if (idx >= states.size()) idx = states.size() - 1;
	b.primaryStateId = states[idx];

	SceneMetric lineDensity;
	lineDensity.metricId = "scene.contour.metric.line_density";
	lineDensity.valueType = HudMetricValueType::Count;
	lineDensity.dataClass = HudDataClass::Literal;
	lineDensity.value = std::round(40.0f + smoothUnit(seed, 911, t, 0.15f) * 360.0f);
	b.metrics.push_back(lineDensity);

	SceneMetric displacementEnergy;
	displacementEnergy.metricId = "scene.contour.metric.displacement_energy";
	displacementEnergy.valueType = HudMetricValueType::Ratio;
	displacementEnergy.dataClass = HudDataClass::Normalized;
	displacementEnergy.value = clamp01(smoothUnit(seed, 912, t, 0.3f));
	b.metrics.push_back(displacementEnergy);

	SceneMetric complexity;
	complexity.metricId = "scene.contour.metric.contour_complexity";
	complexity.valueType = HudMetricValueType::Ratio;
	complexity.dataClass = HudDataClass::Derived;
	complexity.value = clamp01(smoothUnit(seed, 913, t, 0.1f));
	b.metrics.push_back(complexity);

	SceneMetric breakupProgress;
	breakupProgress.metricId = "scene.contour.metric.breakup_progress";
	breakupProgress.valueType = HudMetricValueType::Ratio;
	breakupProgress.dataClass = HudDataClass::Normalized;
	breakupProgress.value = clamp01(smoothUnit(seed, 914, t, 0.25f));
	b.metrics.push_back(breakupProgress);

	SceneMetric preset;
	preset.metricId = "scene.contour.metric.preset";
	preset.valueType = HudMetricValueType::Identifier;
	preset.dataClass = HudDataClass::Literal;
	preset.valueId = "clean_portrait";
	b.metrics.push_back(preset);

	// contour-portrait is not on the shared shader catalog today (per
	// the probe's §9/CLAUDE.md finding) — this pool is illustrative fake
	// content for exercising EffectChips, not a claim about real effect
	// usage.
	b.effectIds = {"Edge Glow"}; // already-curated display text — see FakeHudSemanticTypes.h
	return b;
}

} // namespace hudpresent
