#include "BlobSemanticMapping.h"

namespace blobsemantics {

SceneSemanticData compute(const Inputs & in) {
	SceneSemanticData data;
	data.schemaVersion = 1;

	// Simple, honest bucketing of real counts into the four cinematic
	// state IDs the HUD vocabulary/profile layer already registers for
	// this scene (HudVocabularyResolver.cpp's "Blob state IDs" table:
	// scene.blob.state.{analyzing,fragmenting,stable,degraded}) — a
	// scene-local heuristic, not verified against a formal product-design
	// spec (none exists yet for Blob's cinematic states; see the
	// migration report's "Deviations"/"Newly discovered risks"). Copied
	// unchanged from the original BlobProductionScene::buildSemanticData().
	// "degraded" is reserved for a genuine scene-owned degraded condition,
	// which does not currently exist in this pipeline — no input
	// combination below ever produces it (see
	// blob_semantic_mapping_tests.cpp's own note on this).
	constexpr int kFragmentingThreshold = 4;
	if (in.regionCount == 0) {
		data.state.primaryStateId = "scene.blob.state.analyzing";
	} else if (in.fragmentCount >= kFragmentingThreshold) {
		data.state.primaryStateId = "scene.blob.state.fragmenting";
	} else {
		data.state.primaryStateId = "scene.blob.state.stable";
	}

	int maxFragments = in.maxActiveFragments;
	if (maxFragments < 1) {
		maxFragments = 1;
	}
	float overall = static_cast<float>(in.fragmentCount) / static_cast<float>(maxFragments);
	if (overall < 0.0f) overall = 0.0f;
	if (overall > 1.0f) overall = 1.0f;
	data.activity.overall = overall;
	data.activity.density = in.occupiedAreaFraction;
	// motion/variation/transition: no cheap, honest source exists in the
	// current detector/tracker/region-controller state — left absent
	// rather than fabricated (see migration report).

	data.timing.activeSeconds = in.activeSeconds;
	data.timing.generation = in.generation;

	SceneMetric regionMetric;
	regionMetric.metricId = "scene.blob.metric.region_count";
	regionMetric.valueType = HudMetricValueType::Count;
	regionMetric.dataClass = HudDataClass::Literal;
	regionMetric.value = static_cast<float>(in.regionCount);
	data.metrics.push_back(regionMetric);

	SceneMetric fragmentMetric;
	fragmentMetric.metricId = "scene.blob.metric.fragment_count";
	fragmentMetric.valueType = HudMetricValueType::Count;
	fragmentMetric.dataClass = HudDataClass::Literal;
	fragmentMetric.value = static_cast<float>(in.fragmentCount);
	data.metrics.push_back(fragmentMetric);

	SceneMetric areaMetric;
	areaMetric.metricId = "scene.blob.metric.occupied_area";
	areaMetric.valueType = HudMetricValueType::Ratio;
	areaMetric.dataClass = HudDataClass::Derived;
	areaMetric.normalizedValue = in.occupiedAreaFraction;
	data.metrics.push_back(areaMetric);

	return data;
}

} // namespace blobsemantics
