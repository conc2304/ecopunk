// fragmentsemantics::compute() — RT-006. OF-free, same bare-compiler
// convention as blueprint_semantic_mapping_tests.cpp.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <string>

#include "../src/FragmentSemanticMapping.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

const SceneMetric* findMetric(const SceneSemanticData& d, const std::string& id) {
	for (const auto& m : d.metrics) {
		if (m.metricId == id) return &m;
	}
	return nullptr;
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

} // namespace

#define SEM_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using fragmentsemantics::Inputs;
using fragmentsemantics::compute;

int main() {
	// HUD-Semantic-Slot-Model-v1 §18.4 — the only IDs this scene may emit.
	const std::set<std::string> kFrozenStates = {
		"scene.fragment.state.drifting", "scene.fragment.state.scanning", "scene.fragment.state.hunting",
		"scene.fragment.state.nervous", "scene.fragment.state.orbiting", "scene.fragment.state.decaying"};
	const std::set<std::string> kFrozenMetrics = {
		"scene.fragment.metric.movement_preset", "scene.fragment.metric.fragment_count",
		"scene.fragment.metric.trail_persistence", "scene.fragment.metric.spawn_cadence",
		"scene.fragment.metric.content_mode"};

	// Preset index -> state ID and preset valueId, in CrosshairSystem's order.
	const char* states[] = {"scene.fragment.state.drifting", "scene.fragment.state.scanning",
		"scene.fragment.state.hunting", "scene.fragment.state.nervous", "scene.fragment.state.orbiting"};
	const char* keys[] = {"drift", "scan", "hunt", "nervous", "orbit"};
	for (int i = 0; i < 5; ++i) {
		Inputs in;
		in.crosshairPreset = i;
		in.contentModeId = "effect_varied";
		const SceneSemanticData d = compute(in);
		SEM_CHECK(d.state.primaryStateId == states[i]);
		SEM_CHECK(kFrozenStates.count(d.state.primaryStateId) == 1);
		SEM_CHECK(fragmentsemantics::presetKey(i) == keys[i]);
		const SceneMetric* preset = findMetric(d, "scene.fragment.metric.movement_preset");
		SEM_CHECK(preset != nullptr && preset->valueType == HudMetricValueType::Identifier
			&& preset->valueId == std::string("scene.fragment.metric.movement_preset.") + keys[i]);
		for (const auto& m : d.metrics) SEM_CHECK(kFrozenMetrics.count(m.metricId) == 1);
		SEM_CHECK(d.state.primaryStateId != "scene.fragment.state.decaying"); // no backing state
	}

	// Out-of-range preset: no movement_preset metric, safe fallback state.
	{
		Inputs in;
		in.crosshairPreset = 7;
		const SceneSemanticData d = compute(in);
		SEM_CHECK(findMetric(d, "scene.fragment.metric.movement_preset") == nullptr);
		SEM_CHECK(kFrozenStates.count(d.state.primaryStateId) == 1);
		SEM_CHECK(fragmentsemantics::presetKey(7).empty() && fragmentsemantics::presetKey(-1).empty());
	}

	// Density / overall / motion, clamped to [0,1].
	{
		Inputs in;
		in.crosshairPreset = 2;
		in.fragmentCount = 9;
		in.maxFragments = 18;
		in.crosshairEnergy01 = 0.25f;
		SceneSemanticData d = compute(in);
		SEM_CHECK(d.activity.overall && near(*d.activity.overall, 0.5f));
		SEM_CHECK(d.activity.density && near(*d.activity.density, 0.5f));
		SEM_CHECK(d.activity.motion && near(*d.activity.motion, 0.25f));
		SEM_CHECK(!d.activity.variation && !d.activity.transition);

		in.fragmentCount = 40;
		in.crosshairEnergy01 = 3.0f;
		d = compute(in);
		SEM_CHECK(near(*d.activity.overall, 1.0f) && near(*d.activity.motion, 1.0f));

		in.maxFragments = 0; // never divide by zero
		in.fragmentCount = 0;
		d = compute(in);
		SEM_CHECK(near(*d.activity.overall, 0.0f));
	}

	// Metric types/values and timing.
	{
		Inputs in;
		in.crosshairPreset = 2;
		in.fragmentCount = 11;
		in.trailLifetimeSeconds = 3.2f;
		in.spawnsPerSecond = 2.5f;
		in.contentModeId = "time_slice";
		in.activeSeconds = 4.0f;
		in.presetElapsedSeconds = 4.0f;
		in.generation = 3;
		const SceneSemanticData d = compute(in);
		const SceneMetric* count = findMetric(d, "scene.fragment.metric.fragment_count");
		SEM_CHECK(count && count->valueType == HudMetricValueType::Count && count->value && near(*count->value, 11.0f));
		const SceneMetric* trail = findMetric(d, "scene.fragment.metric.trail_persistence");
		SEM_CHECK(trail && trail->valueType == HudMetricValueType::DurationSeconds && near(*trail->value, 3.2f));
		const SceneMetric* cadence = findMetric(d, "scene.fragment.metric.spawn_cadence");
		SEM_CHECK(cadence && cadence->valueType == HudMetricValueType::Scalar && near(*cadence->value, 2.5f));
		const SceneMetric* mode = findMetric(d, "scene.fragment.metric.content_mode");
		SEM_CHECK(mode && mode->valueType == HudMetricValueType::Identifier && mode->valueId == std::string("time_slice"));
		SEM_CHECK(near(d.timing.activeSeconds, 4.0f) && d.timing.stateElapsedSeconds && d.timing.generation == 3);
		SEM_CHECK(!d.timing.stateProgress); // presets are not timer-bounded in production
		SEM_CHECK(d.schemaVersion == 1);

		Inputs neg = in;
		neg.spawnsPerSecond = -1.0f;
		SEM_CHECK(near(*findMetric(compute(neg), "scene.fragment.metric.spawn_cadence")->value, 0.0f));
		neg.contentModeId.clear();
		SEM_CHECK(findMetric(compute(neg), "scene.fragment.metric.content_mode") == nullptr);
	}

	if (g_failures == 0) {
		std::cout << g_total << "/" << g_total << " checks passed\n";
		return 0;
	}
	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return 1;
}
