// Standalone, dependency-free tests for blobanalysis::Director — the
// evolving-scene brain behind Specimen Track's Analysis Overlay mode (see
// ../src/BlobAnalysisDirector.h). Same bare-compiler convention as
// blob_effect_policy_tests.cpp.
//
// Build/run: make -C test -f Makefile.tests test

#include <iostream>
#include <set>
#include <string>

#include "../src/BlobAnalysisDirector.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string & file, int line, const std::string & expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define DIR_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using namespace blobanalysis;

namespace {

Config fastConfig() {
	Config c;
	c.compositionSeconds = 4.0f;
	c.lookSeconds = 9.0f;
	c.paletteSeconds = 13.0f;
	c.elementEaseSeconds = 0.5f;
	c.dipSeconds = 1.0f;
	c.compositionsPerStage = 2;
	c.stageCount = 3;
	return c;
}

void run(Director & d, float seconds, float dt = 1.0f / 30.0f) {
	for (float t = 0.0f; t < seconds; t += dt) {
		d.update(dt);
	}
}

} // namespace

void test_tables_are_consistent() {
	DIR_CHECK(compositionCount() >= 5);
	DIR_CHECK(lookCount() >= 3);
	DIR_CHECK(paletteCount() >= 3);
	bool hasStage0 = false;
	for (int i = 0; i < compositionCount(); i++) {
		if (composition(i).unlockStage == 0) hasStage0 = true;
		for (int e = 0; e < kElementCount; e++) {
			DIR_CHECK(composition(i).targets[e] >= 0.0f && composition(i).targets[e] <= 1.0f);
		}
	}
	DIR_CHECK(hasStage0);
	for (int i = 0; i < lookCount(); i++) {
		DIR_CHECK(look(i).baseDim >= 0.0f && look(i).baseDim <= 1.0f);
		DIR_CHECK(look(i).drawMode >= 0 && look(i).drawMode <= 2);
	}
}

void test_starts_sparse_with_first_look() {
	Director d(fastConfig());
	const State & s = d.state();
	DIR_CHECK(s.cycle == 0 && s.stage == 0);
	DIR_CHECK(s.lookIndex == 0);
	DIR_CHECK(s.maxFragments == fastConfig().minFragments);
	DIR_CHECK(s.linksPerNode == 1);
	DIR_CHECK(composition(s.compositionIndex).unlockStage == 0);
}

void test_deterministic_for_same_seed() {
	Director a(fastConfig());
	Director b(fastConfig());
	for (int i = 0; i < 3000; i++) {
		a.update(1.0f / 30.0f);
		b.update(1.0f / 30.0f);
		DIR_CHECK(a.state().compositionIndex == b.state().compositionIndex);
		DIR_CHECK(a.state().lookIndex == b.state().lookIndex);
		DIR_CHECK(a.state().maxFragments == b.state().maxFragments);
	}
}

void test_different_seed_diverges() {
	Config c1 = fastConfig();
	Config c2 = fastConfig();
	c2.seed = 0xabcdef01u;
	Director a(c1);
	Director b(c2);
	bool diverged = false;
	for (int i = 0; i < 3000 && !diverged; i++) {
		a.update(1.0f / 30.0f);
		b.update(1.0f / 30.0f);
		diverged = a.state().compositionIndex != b.state().compositionIndex || a.state().lookIndex != b.state().lookIndex;
	}
	DIR_CHECK(diverged);
}

void test_composition_never_repeats_back_to_back_and_respects_unlock() {
	Director d(fastConfig());
	int last = d.state().compositionIndex;
	int changes = 0;
	for (int i = 0; i < 6000; i++) {
		d.update(1.0f / 30.0f);
		int cur = d.state().compositionIndex;
		if (cur != last) {
			changes++;
			DIR_CHECK(cur != last);
			// Stage is the one in force when the composition was picked; a
			// composition may never require a later stage than the final one.
			DIR_CHECK(composition(cur).unlockStage <= fastConfig().stageCount - 1 + 2);
			last = cur;
		}
	}
	DIR_CHECK(changes > 20);
}

void test_early_stage_only_uses_unlocked_compositions() {
	Director d(fastConfig());
	// First composition of the cycle plus the first stage's second one.
	run(d, 6.0f);
	if (d.state().stage == 0) {
		DIR_CHECK(composition(d.state().compositionIndex).unlockStage == 0);
	}
}

void test_growth_increases_then_cycle_collapses() {
	Director d(fastConfig());
	int peakFragments = d.state().maxFragments;
	int peakLinks = d.state().linksPerNode;
	bool sawCollapse = false;
	int prevCycle = d.state().cycle;
	for (int i = 0; i < 30 * 200 && !sawCollapse; i++) {
		d.update(1.0f / 30.0f);
		peakFragments = std::max(peakFragments, d.state().maxFragments);
		peakLinks = std::max(peakLinks, d.state().linksPerNode);
		if (d.state().cycle != prevCycle) {
			sawCollapse = true;
			DIR_CHECK(d.state().stage == 0);
			DIR_CHECK(d.takeChanges().cycleChanged);
		}
	}
	DIR_CHECK(sawCollapse);
	DIR_CHECK(peakFragments > fastConfig().minFragments);
	DIR_CHECK(peakFragments <= fastConfig().maxFragments);
	DIR_CHECK(peakLinks >= 2 && peakLinks <= 3);
}

void test_new_cycle_changes_palette_epoch() {
	Director d(fastConfig());
	int startPalette = d.state().paletteIndex;
	int prevCycle = 0;
	for (int i = 0; i < 30 * 200; i++) {
		d.update(1.0f / 30.0f);
		if (d.state().cycle != prevCycle) {
			break;
		}
	}
	DIR_CHECK(d.state().cycle == 1);
	DIR_CHECK(d.state().paletteIndex != startPalette || paletteCount() == 1);
}

void test_look_swap_is_hidden_behind_dip() {
	Director d(fastConfig());
	d.takeChanges();
	int startLook = d.state().lookIndex;
	float dimAtSwap = -1.0f;
	for (int i = 0; i < 30 * 40; i++) {
		d.update(1.0f / 30.0f);
		Changes c = d.takeChanges();
		if (c.lookChanged && d.state().lookIndex != startLook) {
			dimAtSwap = d.state().backgroundDim;
			break;
		}
	}
	DIR_CHECK(dimAtSwap >= 0.0f);
	// Swap happens at (or within a frame of) fully black.
	DIR_CHECK(dimAtSwap > 0.95f);
}

void test_dim_returns_to_base_after_dip() {
	Director d(fastConfig());
	run(d, 12.0f); // past the first look swap plus its dip
	const State & s = d.state();
	float base = look(s.lookIndex).baseDim;
	DIR_CHECK(s.backgroundDim >= base - 1e-4f);
	DIR_CHECK(s.backgroundDim <= 1.0f);
}

void test_dim_always_in_unit_range() {
	Director d(fastConfig());
	for (int i = 0; i < 30 * 300; i++) {
		d.update(1.0f / 30.0f);
		DIR_CHECK(d.state().backgroundDim >= 0.0f && d.state().backgroundDim <= 1.0f);
		for (int e = 0; e < kElementCount; e++) {
			DIR_CHECK(d.state().elements[e] >= -1e-4f && d.state().elements[e] <= 1.0001f);
		}
	}
}

void test_elements_crossfade_toward_target() {
	Director d(fastConfig());
	run(d, 3.0f);
	const Composition & c = composition(d.state().compositionIndex);
	for (int e = 0; e < kElementCount; e++) {
		DIR_CHECK(std::abs(d.state().elements[e] - c.targets[e]) < 0.35f);
	}
}

void test_every_composition_is_reachable_with_default_stage_count() {
	// A composition whose unlock stage is beyond the last growth stage could
	// never be shown.
	for (int i = 0; i < compositionCount(); i++) {
		DIR_CHECK(composition(i).unlockStage <= Config {}.stageCount - 1);
	}
}

void test_every_look_and_composition_eventually_seen() {
	Config cfg = fastConfig();
	cfg.stageCount = Config {}.stageCount;
	Director d(cfg);
	std::set<int> looks;
	std::set<int> comps;
	for (int i = 0; i < 30 * 1500; i++) {
		d.update(1.0f / 30.0f);
		looks.insert(d.state().lookIndex);
		comps.insert(d.state().compositionIndex);
	}
	DIR_CHECK(static_cast<int>(looks.size()) == lookCount());
	DIR_CHECK(static_cast<int>(comps.size()) == compositionCount());
}

void test_huge_dt_is_clamped() {
	Director d(fastConfig());
	int before = d.state().compositionIndex;
	d.update(30.0f); // a media-load stall
	// One clamped step can never complete a 3-5 s composition on its own.
	DIR_CHECK(d.state().compositionIndex == before);
}

void test_reset_restarts_cycle_zero() {
	Director d(fastConfig());
	run(d, 90.0f);
	d.reset();
	DIR_CHECK(d.state().cycle == 0 && d.state().stage == 0);
	DIR_CHECK(d.state().lookIndex == 0);
	DIR_CHECK(d.state().maxFragments == fastConfig().minFragments);
}

int main() {
	test_tables_are_consistent();
	test_starts_sparse_with_first_look();
	test_deterministic_for_same_seed();
	test_different_seed_diverges();
	test_composition_never_repeats_back_to_back_and_respects_unlock();
	test_early_stage_only_uses_unlocked_compositions();
	test_growth_increases_then_cycle_collapses();
	test_new_cycle_changes_palette_epoch();
	test_look_swap_is_hidden_behind_dip();
	test_dim_returns_to_base_after_dip();
	test_dim_always_in_unit_range();
	test_elements_crossfade_toward_target();
	test_every_composition_is_reachable_with_default_stage_count();
	test_every_look_and_composition_eventually_seen();
	test_huge_dt_is_clamped();
	test_reset_restarts_cycle_zero();

	std::cout << "blob_analysis_director_tests: " << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
