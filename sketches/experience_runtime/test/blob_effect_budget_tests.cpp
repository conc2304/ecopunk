// Standalone, dependency-free tests for blobbudget::EffectBudget — Blob's
// production effect budget (design v0; see ../src/BlobEffectBudget.h).
// Covers every deterministic case the design's §18 lists, plus the
// accounting invariants. Same bare-compiler convention as the other
// blob_*_tests.cpp files.
//
// Build/run: make -C test -f Makefile.tests test

#include <iostream>
#include <map>
#include <set>
#include <string>

#include "../src/BlobEffectBudget.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define BUD_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using namespace blobbudget;

// The default table with only `id` weighted and eligible (forces the
// candidate; cost behavior is tested independently of visual eligibility).
std::vector<Treatment> only(const std::string& id) {
	std::vector<Treatment> t = defaultTable();
	for (Treatment& e : t) {
		e.weight = (e.effectId == id) ? 1 : 0;
		if (e.effectId == id) e.eligible = true;
	}
	return t;
}

void test_default_eligibility() {
	std::map<std::string, bool> eligible;
	for (const auto& t : defaultTable()) eligible[label(t.effectId)] = t.eligible;
	BUD_CHECK(eligible["original"] && eligible["recolor"] && eligible["heatmap_recolor"]);
	BUD_CHECK(!eligible["edge_glow"] && !eligible["water_refraction"] && !eligible["pixel_sorting"]);
	EffectBudget b;
	for (int id = 0; id < 2000; id++) {
		std::string c = b.candidateFor(id);
		BUD_CHECK(c.empty() || c == "recolor" || c == "heatmap_recolor");
	}
}

int sumActiveCost(const EffectBudget& b) {
	int sum = 0;
	for (const auto& kv : b.activeCostByEffect()) sum += kv.second;
	return sum;
}

void test_default_table_matches_design() {
	EffectBudget b;
	BUD_CHECK(b.maxCostCenti() == 1000);
	BUD_CHECK(b.costOf("") == 0);
	BUD_CHECK(b.costOf("recolor") == 100);
	BUD_CHECK(b.costOf("edge_glow") == 100);
	BUD_CHECK(b.costOf("water_refraction") == 100);
	BUD_CHECK(b.costOf("heatmap_recolor") == 125);
	BUD_CHECK(b.costOf("pixel_sorting") == 250);
	BUD_CHECK(b.costOf("ascii_solarpunk") == -1);
	std::map<std::string, int> w;
	for (const auto& t : defaultTable()) w[label(t.effectId)] = t.weight;
	BUD_CHECK(w["original"] == 5 && w["recolor"] == 3 && w["edge_glow"] == 2 && w["water_refraction"] == 2
		&& w["heatmap_recolor"] == 1 && w["pixel_sorting"] == 1);
}

void test_ten_recolor_accepted_eleventh_falls_back() {
	EffectBudget b(only("recolor"));
	for (int id = 1; id <= 10; id++) BUD_CHECK(b.assign(id) == "recolor");
	BUD_CHECK(b.usedCostCenti() == 1000);
	BUD_CHECK(b.fallbackCount() == 0);
	BUD_CHECK(b.assign(11).empty());
	BUD_CHECK(b.fallbackCount() == 1);
	BUD_CHECK(b.usedCostCenti() == 1000);
}

void test_eight_heatmap_accepted_ninth_falls_back() {
	EffectBudget b(only("heatmap_recolor"));
	for (int id = 1; id <= 8; id++) BUD_CHECK(b.assign(id) == "heatmap_recolor");
	BUD_CHECK(b.usedCostCenti() == 1000);
	BUD_CHECK(b.assign(9) != "heatmap_recolor");
	BUD_CHECK(b.usedCostCenti() <= 1000);
}

void test_four_pixel_sorting_accepted_fifth_falls_back() {
	EffectBudget b(only("pixel_sorting"));
	for (int id = 1; id <= 4; id++) BUD_CHECK(b.assign(id) == "pixel_sorting");
	BUD_CHECK(b.usedCostCenti() == 1000);
	BUD_CHECK(b.assign(5).empty());
	BUD_CHECK(b.fallbackCount() == 1);
}

void test_design_mixed_example_over_budget() {
	// 2 Pixel Sorting + 3 Heatmap + 2 Recolor = 10.75 -> the last one falls back.
	EffectBudget b(only("pixel_sorting"));
	BUD_CHECK(b.assign(1) == "pixel_sorting");
	BUD_CHECK(b.assign(2) == "pixel_sorting");
	b.setTable(only("heatmap_recolor"));
	for (int id = 3; id <= 5; id++) BUD_CHECK(b.assign(id) == "heatmap_recolor");
	b.setTable(only("recolor"));
	BUD_CHECK(b.assign(6) == "recolor"); // 9.75
	BUD_CHECK(b.assign(7).empty()); // 10.75 would exceed -> Original
	BUD_CHECK(b.usedCostCenti() == 975);
	BUD_CHECK(b.activeShaderCount() == 6 && b.activeOriginalCount() == 1);
	auto byEffect = b.activeCostByEffect();
	BUD_CHECK(byEffect["pixel_sorting"] == 500 && byEffect["heatmap_recolor"] == 375 && byEffect["recolor"] == 100);
}

void test_cheaper_fallback_before_original() {
	std::vector<Treatment> t = defaultTable();
	for (Treatment& e : t) e.weight = (e.effectId == "heatmap_recolor" || e.effectId == "recolor") ? 1 : 0;
	EffectBudget b(only("recolor"));
	for (int id = 1; id <= 9; id++) b.assign(id); // 900 used
	b.setTable(t);
	// Find an id whose candidate is heatmap (1.25 does not fit; 1.0 does).
	int id = 100;
	while (b.candidateFor(id) != "heatmap_recolor") id++;
	BUD_CHECK(b.assign(id) == "recolor");
	BUD_CHECK(b.usedCostCenti() == 1000);
	BUD_CHECK(b.fallbackCount() == 1);
}

void test_release_frees_budget() {
	EffectBudget b(only("pixel_sorting"));
	for (int id = 1; id <= 4; id++) b.assign(id);
	BUD_CHECK(b.assign(5).empty());
	b.release(2);
	BUD_CHECK(b.usedCostCenti() == 750);
	BUD_CHECK(b.assign(6) == "pixel_sorting");
	b.release(42); // unknown id: no effect
	BUD_CHECK(b.usedCostCenti() == 1000);
	b.release(5); // Original releases nothing
	BUD_CHECK(b.usedCostCenti() == 1000);
	b.releaseAll();
	BUD_CHECK(b.usedCostCenti() == 0 && b.activeCount() == 0);
}

void test_release_after_palette_change_uses_charged_cost() {
	EffectBudget b(only("pixel_sorting"));
	b.assign(1);
	b.setTable(only("recolor")); // pixel_sorting no longer in the palette
	b.release(1);
	BUD_CHECK(b.usedCostCenti() == 0);
}

void test_unknown_or_costless_effect_falls_back_to_original() {
	std::vector<Treatment> t = only("none");
	t.push_back({ "made_up_effect", -1, 100, true, "no cost metadata" });
	t.push_back({ "blocked_effect", 100, 100, false, "visually blocked" });
	EffectBudget b(t);
	for (int id = 1; id <= 20; id++) BUD_CHECK(b.assign(id).empty());
	BUD_CHECK(b.usedCostCenti() == 0);
	std::vector<std::string> ignored;
	withWeights(defaultTable(), "made_up:5,recolor:7", &ignored);
	BUD_CHECK(ignored.size() == 1 && ignored[0] == "made_up:5");
}

void test_assignment_is_deterministic_and_generation_scoped() {
	EffectBudget a, b;
	for (int id = 0; id < 500; id++) BUD_CHECK(a.candidateFor(id) == b.candidateFor(id));
	int differs = 0;
	EffectBudget c;
	c.setGeneration(1);
	for (int id = 0; id < 500; id++) differs += a.candidateFor(id) != c.candidateFor(id) ? 1 : 0;
	BUD_CHECK(differs > 100); // a new palette generation draws differently
}

void test_weights_shape_the_palette() {
	std::vector<Treatment> all = defaultTable();
	for (Treatment& e : all) e.eligible = true; // the design's full palette
	EffectBudget b(all);
	std::map<std::string, int> counts;
	const int n = 14000;
	for (int id = 0; id < n; id++) counts[label(b.candidateFor(id))]++;
	auto near = [&](const char* id, int weight) {
		double expected = n * weight / 14.0;
		return counts[id] > expected * 0.85 && counts[id] < expected * 1.15;
	};
	BUD_CHECK(near("original", 5));
	BUD_CHECK(near("recolor", 3));
	BUD_CHECK(near("edge_glow", 2));
	BUD_CHECK(near("water_refraction", 2));
	BUD_CHECK(near("heatmap_recolor", 1));
	BUD_CHECK(near("pixel_sorting", 1));
}

void test_budget_never_exceeded_under_churn() {
	EffectBudget b;
	std::set<int> live;
	unsigned seed = 12345;
	int nextId = 1;
	for (int step = 0; step < 20000; step++) {
		seed = seed * 1103515245u + 12345u;
		if (live.size() < 10 && (seed >> 16) % 3 != 0) {
			b.assign(nextId);
			live.insert(nextId++);
		} else if (!live.empty()) {
			auto it = live.begin();
			std::advance(it, (seed >> 8) % live.size());
			b.release(*it);
			live.erase(it);
		}
		if (step % 997 == 0) b.setGeneration(step);
		BUD_CHECK(b.usedCostCenti() <= b.maxCostCenti());
		BUD_CHECK(b.usedCostCenti() == sumActiveCost(b));
		BUD_CHECK(b.activeCount() == static_cast<int>(live.size()));
	}
}

int main() {
	test_default_table_matches_design();
	test_default_eligibility();
	test_ten_recolor_accepted_eleventh_falls_back();
	test_eight_heatmap_accepted_ninth_falls_back();
	test_four_pixel_sorting_accepted_fifth_falls_back();
	test_design_mixed_example_over_budget();
	test_cheaper_fallback_before_original();
	test_release_frees_budget();
	test_release_after_palette_change_uses_charged_cost();
	test_unknown_or_costless_effect_falls_back_to_original();
	test_assignment_is_deterministic_and_generation_scoped();
	test_weights_shape_the_palette();
	test_budget_never_exceeded_under_churn();

	std::cout << "blob_effect_budget_tests: " << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
