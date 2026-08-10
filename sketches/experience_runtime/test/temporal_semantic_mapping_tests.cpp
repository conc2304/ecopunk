// Standalone, dependency-free tests for temporalsemantics::compute()
// (Temporal Production Scene #2 Migration) — the pure derivation logic
// TemporalProductionScene::buildSemanticData() forwards to (see
// ../src/TemporalSemanticMapping.h's header comment). Deliberately does NOT
// link any part of openFrameworks or TemporalSceneCore/VideoPlaybackService
// — same rationale as blob_semantic_mapping_tests.cpp in this directory:
// exercises the exact real production derivation code with controlled
// inputs, so every semantic/metric value Temporal populates (and every one
// it deliberately leaves absent) is deterministically, reviewably tested.
//
// Build/run: make -C test -f Makefile.tests test

#include <iostream>
#include <string>

#include "../src/TemporalSemanticMapping.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define SEM_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using temporalsemantics::Inputs;
using temporalsemantics::compute;

namespace {
	const SceneMetric* findMetric(const SceneSemanticData& data, const std::string& id) {
		for (const SceneMetric& m : data.metrics) {
			if (m.metricId == id) return &m;
		}
		return nullptr;
	}
} // namespace

// -- state.primaryStateId mapping --------------------------------------------

void test_running_state() {
	Inputs in;
	in.isTransitioning = false;
	SceneSemanticData data = compute(in);
	SEM_CHECK(data.state.primaryStateId == "scene.temporal.state.running");
	SEM_CHECK(!data.state.secondaryStateId.has_value());
}

void test_transitioning_state() {
	Inputs in;
	in.isTransitioning = true;
	SceneSemanticData data = compute(in);
	SEM_CHECK(data.state.primaryStateId == "scene.temporal.state.transitioning");
}

// -- timing -------------------------------------------------------------------

void test_timing_fields_forwarded_literally() {
	Inputs in;
	in.activeSeconds = 42.5f;
	in.phaseElapsedSeconds = 3.25f;
	in.generation = 7;
	SceneSemanticData data = compute(in);
	SEM_CHECK(data.timing.activeSeconds == 42.5f);
	SEM_CHECK(data.timing.stateElapsedSeconds.has_value());
	SEM_CHECK(*data.timing.stateElapsedSeconds == 3.25f);
	SEM_CHECK(!data.timing.stateProgress.has_value()); // no exposed cycle/transition duration — always absent
	SEM_CHECK(data.timing.generation == 7);
}

// -- scene.temporal.metric.pattern --------------------------------------------

void test_pattern_identifier_forwarded() {
	Inputs in;
	in.activePatternId = "particle_field";
	SceneSemanticData data = compute(in);
	const SceneMetric* m = findMetric(data, "scene.temporal.metric.pattern");
	SEM_CHECK(m != nullptr);
	SEM_CHECK(m->valueType == HudMetricValueType::Identifier);
	SEM_CHECK(m->dataClass == HudDataClass::Literal);
	SEM_CHECK(m->valueId.has_value() && *m->valueId == "particle_field");
}

// -- scene.temporal.metric.history_fill ---------------------------------------

void test_history_fill_absent_when_zero_capacity() {
	Inputs in;
	in.historyFrameCount = 0;
	in.historyCapacityFrames = 0;
	SceneSemanticData data = compute(in);
	SEM_CHECK(findMetric(data, "scene.temporal.metric.history_fill") == nullptr);
}

void test_history_fill_zero_when_empty() {
	Inputs in;
	in.historyFrameCount = 0;
	in.historyCapacityFrames = 100;
	SceneSemanticData data = compute(in);
	const SceneMetric* m = findMetric(data, "scene.temporal.metric.history_fill");
	SEM_CHECK(m != nullptr);
	SEM_CHECK(m->valueType == HudMetricValueType::Ratio);
	SEM_CHECK(m->dataClass == HudDataClass::Normalized);
	SEM_CHECK(m->value.has_value() && *m->value == 0.0f);
}

void test_history_fill_partial() {
	Inputs in;
	in.historyFrameCount = 25;
	in.historyCapacityFrames = 100;
	SceneSemanticData data = compute(in);
	const SceneMetric* m = findMetric(data, "scene.temporal.metric.history_fill");
	SEM_CHECK(m != nullptr);
	SEM_CHECK(m->value.has_value() && *m->value == 0.25f);
}

void test_history_fill_clamped_at_full() {
	// count can transiently exceed capacity as *assumedSourceFps* estimate
	// drifts from the real decoded frame rate — clamp01() must still cap
	// the ratio at 1.0, never report >100% fill.
	Inputs in;
	in.historyFrameCount = 150;
	in.historyCapacityFrames = 100;
	SceneSemanticData data = compute(in);
	const SceneMetric* m = findMetric(data, "scene.temporal.metric.history_fill");
	SEM_CHECK(m != nullptr);
	SEM_CHECK(m->value.has_value() && *m->value == 1.0f);
}

// -- scene.temporal.metric.playhead_count -------------------------------------

void test_playhead_count_literal_not_hardcoded() {
	Inputs in;
	in.playheadCount = 4; // deliberately NOT 6, to prove this isn't a hard-coded constant
	SceneSemanticData data = compute(in);
	const SceneMetric* m = findMetric(data, "scene.temporal.metric.playhead_count");
	SEM_CHECK(m != nullptr);
	SEM_CHECK(m->valueType == HudMetricValueType::Count);
	SEM_CHECK(m->dataClass == HudDataClass::Literal);
	SEM_CHECK(m->value.has_value() && *m->value == 4.0f);
}

// -- deliberately-absent metrics (§10.5/§10.6/§10.7 — no settled formula) ----

void test_no_fabricated_metrics() {
	Inputs in;
	in.isTransitioning = false;
	in.activePatternId = "bsp";
	in.historyFrameCount = 10;
	in.historyCapacityFrames = 20;
	in.playheadCount = 6;
	SceneSemanticData data = compute(in);
	SEM_CHECK(findMetric(data, "scene.temporal.metric.temporal_depth") == nullptr);
	SEM_CHECK(findMetric(data, "scene.temporal.metric.field_activity") == nullptr);
	SEM_CHECK(findMetric(data, "scene.temporal.metric.evolution_state") == nullptr);
	// Only the 3 populated metrics (pattern, history_fill, playhead_count)
	// should be present — no more, no fewer.
	SEM_CHECK(data.metrics.size() == 3);
	// SceneActivity fields are never fabricated either.
	SEM_CHECK(!data.activity.overall.has_value());
	SEM_CHECK(!data.activity.motion.has_value());
	SEM_CHECK(!data.activity.density.has_value());
	SEM_CHECK(!data.activity.variation.has_value());
	SEM_CHECK(!data.activity.transition.has_value());
}

int main() {
	test_running_state();
	test_transitioning_state();
	test_timing_fields_forwarded_literally();
	test_pattern_identifier_forwarded();
	test_history_fill_absent_when_zero_capacity();
	test_history_fill_zero_when_empty();
	test_history_fill_partial();
	test_history_fill_clamped_at_full();
	test_playhead_count_literal_not_hardcoded();
	test_no_fabricated_metrics();

	std::cout << (g_failures == 0 ? "PASS" : "FAIL") << " — " << g_total << " checks, " << g_failures << " failures\n";
	return g_failures == 0 ? 0 : 1;
}
