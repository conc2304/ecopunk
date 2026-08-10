// Standalone, dependency-free tests for blobsemantics::compute() (Blob
// First Production Acceptance narrow patch) — the pure derivation logic
// BlobProductionScene::buildSemanticData() forwards to (see
// ../src/BlobSemanticMapping.h's header comment). Deliberately does NOT
// link any part of openFrameworks or BlobSceneCore/VideoPlaybackService —
// same rationale as lifecycle_state_tests.cpp/session2_integration_tests.cpp
// in this directory: exercises the exact real production derivation code
// with controlled inputs instead of only via live video, so every semantic/
// metric value Blob populates is deterministically, reviewably tested.
//
// Build/run: make -C test -f Makefile.tests test

#include <iostream>
#include <string>

#include "../src/BlobSemanticMapping.h"

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

using blobsemantics::Inputs;
using blobsemantics::compute;

// -- state.primaryStateId bucketing -----------------------------------------

void test_zero_regions_is_analyzing() {
	// regionCount == 0 always wins regardless of fragmentCount, matching
	// BlobSceneCore's own invariant that fragments never exist without a
	// tracked region behind them — but this function doesn't assume that;
	// it only tests its own bucketing rule directly.
	Inputs in;
	in.regionCount = 0;
	in.fragmentCount = 0;
	in.maxActiveFragments = 10;

	SceneSemanticData data = compute(in);
	SEM_CHECK(data.state.primaryStateId == "scene.blob.state.analyzing");
	SEM_CHECK(!data.state.secondaryStateId.has_value());
}

void test_one_region_low_fragments_is_stable() {
	Inputs in;
	in.regionCount = 1;
	in.fragmentCount = 1;
	in.maxActiveFragments = 10;

	SceneSemanticData data = compute(in);
	SEM_CHECK(data.state.primaryStateId == "scene.blob.state.stable");
}

void test_fragmenting_threshold_boundary() {
	// kFragmentingThreshold == 4 (BlobSemanticMapping.cpp) — 3 is "stable",
	// 4 is "fragmenting". Both sides of the boundary tested explicitly.
	Inputs below;
	below.regionCount = 3;
	below.fragmentCount = 3;
	below.maxActiveFragments = 10;
	SEM_CHECK(compute(below).state.primaryStateId == "scene.blob.state.stable");

	Inputs at;
	at.regionCount = 4;
	at.fragmentCount = 4;
	at.maxActiveFragments = 10;
	SEM_CHECK(compute(at).state.primaryStateId == "scene.blob.state.fragmenting");

	Inputs above;
	above.regionCount = 8;
	above.fragmentCount = 8;
	above.maxActiveFragments = 10;
	SEM_CHECK(compute(above).state.primaryStateId == "scene.blob.state.fragmenting");
}

void test_degraded_state_is_never_reachable() {
	// No combination of real Inputs fields can produce
	// "scene.blob.state.degraded" — there is no genuine scene-owned
	// degraded condition in this pipeline today (see BlobSemanticMapping.cpp's
	// own comment). Swept across a representative grid rather than proven
	// exhaustively, since the function's own source (read directly, only
	// two branches other than analyzing/stable/fragmenting exist: none)
	// already proves this by inspection — this is a regression guard, not
	// the primary proof.
	int regionCounts[] = {0, 1, 2, 4, 100};
	int fragmentCounts[] = {0, 1, 3, 4, 50};
	for (int r : regionCounts) {
		for (int f : fragmentCounts) {
			Inputs in;
			in.regionCount = r;
			in.fragmentCount = f;
			in.maxActiveFragments = 10;
			SEM_CHECK(compute(in).state.primaryStateId != "scene.blob.state.degraded");
		}
	}
}

// -- activity.overall / activity.density -------------------------------------

void test_overall_activity_ratio_and_clamp() {
	Inputs half;
	half.regionCount = 1;
	half.fragmentCount = 5;
	half.maxActiveFragments = 10;
	SceneSemanticData halfData = compute(half);
	SEM_CHECK(halfData.activity.overall.has_value());
	SEM_CHECK(*halfData.activity.overall > 0.49f && *halfData.activity.overall < 0.51f);

	// Exactly at the configured maximum -> exactly 1.0, not >1.0.
	Inputs atMax;
	atMax.regionCount = 1;
	atMax.fragmentCount = 10;
	atMax.maxActiveFragments = 10;
	SEM_CHECK(*compute(atMax).activity.overall == 1.0f);

	// Beyond the configured maximum (shouldn't happen from real
	// VideoRegionController state, which enforces the cap itself, but the
	// clamp must still hold defensively) -> clamps to exactly 1.0, not >1.0.
	Inputs overMax;
	overMax.regionCount = 1;
	overMax.fragmentCount = 15;
	overMax.maxActiveFragments = 10;
	SEM_CHECK(*compute(overMax).activity.overall == 1.0f);

	// maxActiveFragments <= 0 (shouldn't happen from real config, which
	// has a GUI-enforced minimum of 1, but the function defends against
	// divide-by-zero anyway) -> treated as 1, never NaN/inf.
	Inputs zeroMax;
	zeroMax.regionCount = 1;
	zeroMax.fragmentCount = 0;
	zeroMax.maxActiveFragments = 0;
	SceneSemanticData zeroMaxData = compute(zeroMax);
	SEM_CHECK(zeroMaxData.activity.overall.has_value());
	SEM_CHECK(*zeroMaxData.activity.overall == 0.0f);
}

void test_density_passthrough() {
	Inputs in;
	in.regionCount = 2;
	in.fragmentCount = 2;
	in.maxActiveFragments = 10;
	in.occupiedAreaFraction = 0.375f;

	SceneSemanticData data = compute(in);
	SEM_CHECK(data.activity.density.has_value());
	SEM_CHECK(*data.activity.density == 0.375f);
}

void test_unsupported_activity_signals_stay_absent() {
	// motion/variation/transition: no honest cheap source exists in the
	// current Blob pipeline — must remain std::nullopt (missing), never a
	// fabricated 0.0f. This is the one assertion this whole patch is built
	// to make reviewable/provable rather than only documented in prose.
	Inputs in;
	in.regionCount = 3;
	in.fragmentCount = 5;
	in.maxActiveFragments = 10;
	in.occupiedAreaFraction = 0.2f;

	SceneSemanticData data = compute(in);
	SEM_CHECK(!data.activity.motion.has_value());
	SEM_CHECK(!data.activity.variation.has_value());
	SEM_CHECK(!data.activity.transition.has_value());
}

// -- timing passthrough -------------------------------------------------------

void test_timing_passthrough() {
	Inputs in;
	in.regionCount = 1;
	in.fragmentCount = 1;
	in.maxActiveFragments = 10;
	in.activeSeconds = 12.5f;
	in.generation = 7;

	SceneSemanticData data = compute(in);
	SEM_CHECK(data.timing.activeSeconds == 12.5f);
	SEM_CHECK(data.timing.generation == 7);
	SEM_CHECK(!data.timing.stateElapsedSeconds.has_value());
	SEM_CHECK(!data.timing.stateProgress.has_value());
}

// -- metrics -------------------------------------------------------------------

void test_metrics_exact_three_entries() {
	Inputs in;
	in.regionCount = 3;
	in.fragmentCount = 2;
	in.maxActiveFragments = 10;
	in.occupiedAreaFraction = 0.125f;

	SceneSemanticData data = compute(in);
	SEM_CHECK(data.metrics.size() == 3);
	if (data.metrics.size() != 3) return;

	const SceneMetric& region = data.metrics[0];
	SEM_CHECK(region.metricId == "scene.blob.metric.region_count");
	SEM_CHECK(region.valueType == HudMetricValueType::Count);
	SEM_CHECK(region.dataClass == HudDataClass::Literal);
	SEM_CHECK(region.value.has_value() && *region.value == 3.0f);
	SEM_CHECK(!region.normalizedValue.has_value());

	const SceneMetric& fragment = data.metrics[1];
	SEM_CHECK(fragment.metricId == "scene.blob.metric.fragment_count");
	SEM_CHECK(fragment.valueType == HudMetricValueType::Count);
	SEM_CHECK(fragment.dataClass == HudDataClass::Literal);
	SEM_CHECK(fragment.value.has_value() && *fragment.value == 2.0f);

	const SceneMetric& area = data.metrics[2];
	SEM_CHECK(area.metricId == "scene.blob.metric.occupied_area");
	SEM_CHECK(area.valueType == HudMetricValueType::Ratio);
	SEM_CHECK(area.dataClass == HudDataClass::Derived);
	SEM_CHECK(area.normalizedValue.has_value() && *area.normalizedValue == 0.125f);
	SEM_CHECK(!area.value.has_value());
}

void test_zero_regions_zero_fragments_metrics_present_not_absent() {
	// "Genuinely zero" must still be a populated metric (value == 0), not
	// an absent one — SceneHudStatus::semantic itself is what's absent
	// (std::nullopt) when there's no honest data at all; once compute() is
	// called at all, its own metrics are always fully populated real
	// counts, even when those counts are zero.
	Inputs in;
	in.regionCount = 0;
	in.fragmentCount = 0;
	in.maxActiveFragments = 10;
	in.occupiedAreaFraction = 0.0f;

	SceneSemanticData data = compute(in);
	SEM_CHECK(data.metrics.size() == 3);
	SEM_CHECK(data.metrics[0].value.has_value() && *data.metrics[0].value == 0.0f);
	SEM_CHECK(data.metrics[1].value.has_value() && *data.metrics[1].value == 0.0f);
	SEM_CHECK(data.metrics[2].normalizedValue.has_value() && *data.metrics[2].normalizedValue == 0.0f);
	SEM_CHECK(data.activity.overall.has_value() && *data.activity.overall == 0.0f);
	SEM_CHECK(data.activity.density.has_value() && *data.activity.density == 0.0f);
}

void test_schema_version() {
	Inputs in;
	SEM_CHECK(compute(in).schemaVersion == 1);
}

int main() {
	test_zero_regions_is_analyzing();
	test_one_region_low_fragments_is_stable();
	test_fragmenting_threshold_boundary();
	test_degraded_state_is_never_reachable();
	test_overall_activity_ratio_and_clamp();
	test_density_passthrough();
	test_unsupported_activity_signals_stay_absent();
	test_timing_passthrough();
	test_metrics_exact_three_entries();
	test_zero_regions_zero_fragments_metrics_present_not_absent();
	test_schema_version();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
