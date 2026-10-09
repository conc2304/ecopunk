// blueprintsemantics::compute() — BLUE-001. OF-free, same bare-compiler
// convention as blob_semantic_mapping_tests.cpp.

#include <iostream>
#include <string>

#include "../src/BlueprintSemanticMapping.h"

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

} // namespace

#define SEM_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using blueprintsemantics::Inputs;
using blueprintsemantics::compute;

int main() {
	// Phase -> state ID, every CyclePhase value.
	const char* expected[] = {
		"scene.blueprint.state.blank", "scene.blueprint.state.placing", "scene.blueprint.state.building",
		"scene.blueprint.state.dissolving", "scene.blueprint.state.hold" };
	for (int phase = 0; phase < 5; ++phase) {
		Inputs in;
		in.phase = phase;
		SEM_CHECK(compute(in).state.primaryStateId == expected[phase]);
	}

	// Activity: fragments over the real cap, clamped; ghost overflow saturates.
	{
		Inputs in;
		in.maxFragments = 4;
		in.fragmentCount = 2;
		SEM_CHECK(compute(in).activity.overall.value() == 0.5f);
		in.fragmentCount = 9;
		SEM_CHECK(compute(in).activity.overall.value() == 1.0f);
		in.maxFragments = 0; // never divides by zero
		in.fragmentCount = 0;
		SEM_CHECK(compute(in).activity.overall.value() == 0.0f);
	}

	// Undefined activity channels, zone densities, and progress for a phase
	// that is not timer-bounded stay absent.
	{
		Inputs in;
		in.fragmentCount = 3;
		in.zoneACount = 1;
		in.zoneBCount = 2;
		SceneSemanticData d = compute(in);
		SEM_CHECK(!d.activity.motion && !d.activity.density && !d.activity.variation && !d.activity.transition);
		SEM_CHECK(!d.timing.stateProgress);
		SEM_CHECK(findMetric(d, "scene.blueprint.metric.phase_progress") == nullptr);
		SEM_CHECK(findMetric(d, "scene.blueprint.metric.zone_a_density") == nullptr);
		SEM_CHECK(findMetric(d, "scene.blueprint.metric.zone_b_density") == nullptr);
	}

	// Metrics.
	{
		Inputs in;
		in.fragmentCount = 3;
		in.zoneACount = 1;
		in.zoneBCount = 3;
		in.cycleModeId = "ghost_layers";
		SceneSemanticData d = compute(in);
		const SceneMetric* count = findMetric(d, "scene.blueprint.metric.fragment_count");
		SEM_CHECK(count && count->valueType == HudMetricValueType::Count && count->value.value() == 3.0f);
		const SceneMetric* mode = findMetric(d, "scene.blueprint.metric.cycle_mode");
		SEM_CHECK(mode && mode->valueType == HudMetricValueType::Identifier
			&& mode->valueId.value() == "scene.blueprint.metric.cycle_mode.ghost_layers");
		const SceneMetric* balance = findMetric(d, "scene.blueprint.metric.zone_balance");
		SEM_CHECK(balance && balance->valueType == HudMetricValueType::Ratio && balance->value.value() == 0.25f);
	}

	// Phase progress: published (stateProgress + metric) only when bounded.
	{
		Inputs in;
		in.phaseProgress = 0.25f;
		SceneSemanticData d = compute(in);
		SEM_CHECK(d.timing.stateProgress.value() == 0.25f);
		const SceneMetric* progress = findMetric(d, "scene.blueprint.metric.phase_progress");
		SEM_CHECK(progress && progress->valueType == HudMetricValueType::Ratio && progress->value.value() == 0.25f);
	}

	// Zone balance absent with no zoned fragments; cycle mode absent when unknown.
	{
		Inputs in;
		SceneSemanticData d = compute(in);
		SEM_CHECK(findMetric(d, "scene.blueprint.metric.zone_balance") == nullptr);
		SEM_CHECK(findMetric(d, "scene.blueprint.metric.cycle_mode") == nullptr);
	}

	// Timing passthrough.
	{
		Inputs in;
		in.activeSeconds = 12.5f;
		in.phaseElapsedSeconds = 3.0f;
		in.generation = 7;
		SceneSemanticData d = compute(in);
		SEM_CHECK(d.timing.activeSeconds == 12.5f);
		SEM_CHECK(d.timing.stateElapsedSeconds.value() == 3.0f);
		SEM_CHECK(d.timing.generation == 7);
	}

	if (g_failures == 0) {
		std::cout << g_total << "/" << g_total << " checks passed\n";
		return 0;
	}
	std::cout << g_failures << " of " << g_total << " checks FAILED\n";
	return 1;
}
