// Standalone, dependency-free tests for blobcapacity::Plan — Blob
// Experiment 2's deterministic per-track-id treatment assignment (see
// ../src/BlobCapacityPlan.h). Same bare-compiler convention as
// blob_effect_policy_tests.cpp.
//
// Build/run: make -C test -f Makefile.tests test

#include <iostream>
#include <map>
#include <set>
#include <string>

#include "../src/BlobCapacityPlan.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define CAP_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using namespace blobcapacity;

int shadedIn(const Plan& p) {
	int n = 0;
	for (const auto& s : p.pattern()) n += s.empty() ? 0 : 1;
	return n;
}

void test_effect_table() {
	CAP_CHECK(effectTable().size() == 18);
	std::set<std::string> candidates;
	for (const auto& e : effectTable()) {
		if (e.productionCandidate()) candidates.insert(e.id);
	}
	CAP_CHECK(candidates == (std::set<std::string>{ "recolor", "desaturate" }));
	for (const auto& set : effectSets()) {
		for (const auto& id : set.effects) CAP_CHECK(findEffect(id) != nullptr);
	}
}

void test_shader_slot_counts_exact() {
	for (int k = 0; k <= kSlots; k++) {
		Plan p;
		p.shaderSlots = k;
		CAP_CHECK(shadedIn(p) == k);
		CAP_CHECK(p.plannedShaderSlots() == k);
	}
}

void test_presets_match_matrix() {
	Plan p;
	CAP_CHECK(preset('A', p) && shadedIn(p) == 0);
	CAP_CHECK(preset('B', p) && shadedIn(p) == 10 && p.activeEffects() == std::vector<std::string>{ "recolor" });
	CAP_CHECK(preset('C', p) && shadedIn(p) == 5 && p.activeEffects().size() == 1);
	CAP_CHECK(preset('D', p) && shadedIn(p) == 5 && p.activeEffects().size() == 2);
	CAP_CHECK(preset('E', p) && shadedIn(p) == 8 && p.activeEffects().size() == 4);
	CAP_CHECK(preset('F', p) && shadedIn(p) == 10 && p.activeEffects().size() == 2);
	CAP_CHECK(preset('G', p) && shadedIn(p) == 10 && p.activeEffects().size() == 18);
	CAP_CHECK(!preset('Z', p));
}

void test_round_robin_uses_every_distinct_shader() {
	Plan p;
	preset('E', p);
	std::map<std::string, int> counts;
	for (const auto& s : p.pattern()) if (!s.empty()) counts[s]++;
	CAP_CHECK(counts.size() == 4);
	for (const auto& kv : counts) CAP_CHECK(kv.second == 2);
}

void test_single_mode_uses_only_first_effect() {
	Plan p;
	preset('E', p);
	p.mode = AssignmentMode::Single;
	for (const auto& s : p.pattern()) CAP_CHECK(s.empty() || s == "recolor");
}

void test_assignment_is_stable_per_id() {
	Plan p;
	preset('G', p);
	for (int id = -5; id < 200; id++) {
		std::string first = p.effectForId(id);
		for (int i = 0; i < 3; i++) CAP_CHECK(p.effectForId(id) == first);
		CAP_CHECK(p.effectForId(id + kSlots) == first); // periodic in the slot count
	}
}

void test_consecutive_ids_reach_planned_mix() {
	Plan p;
	preset('C', p);
	int shaded = 0;
	for (int id = 1; id <= 10; id++) shaded += p.effectForId(id).empty() ? 0 : 1;
	CAP_CHECK(shaded == 5);
}

void test_weighted() {
	Plan p;
	p.mode = AssignmentMode::Weighted;
	p.weights = parseWeights("original:6,recolor:2,scanlines:2,bogus:3,heatmap_recolor:0");
	CAP_CHECK(p.weights.size() == 3);
	std::map<std::string, int> counts;
	for (const auto& s : p.pattern()) counts[s.empty() ? "original" : s]++;
	CAP_CHECK(counts["original"] == 6);
	CAP_CHECK(counts["recolor"] == 2);
	CAP_CHECK(counts["scanlines"] == 2);
	CAP_CHECK(p.plannedShaderSlots() == 4);
	// Interleaved, not blocked: no 3 consecutive identical shaded slots.
	auto pat = p.pattern();
	CAP_CHECK(!(pat[0] == pat[1] && pat[1] == pat[2] && !pat[0].empty()));
}

void test_distinct_clamps() {
	Plan p;
	preset('B', p);
	p.distinct = 99;
	CAP_CHECK(p.activeEffects().size() == p.effectSet.size());
	p.distinct = 0;
	CAP_CHECK(p.activeEffects().empty());
	CAP_CHECK(shadedIn(p) == 0); // nothing to shade with -> Original
}

void test_script_parsing() {
	std::vector<ScriptStep> steps;
	std::string err;
	CAP_CHECK(parseScript("A:5,recolor@4:10,w#2@6:7", steps, err));
	CAP_CHECK(steps.size() == 3);
	CAP_CHECK(steps[1].plan.activeEffects() == std::vector<std::string>{ "recolor" } && shadedIn(steps[1].plan) == 4);
	CAP_CHECK(steps[2].plan.activeEffects().size() == 2 && shadedIn(steps[2].plan) == 6 && steps[2].seconds == 7.0f);
	CAP_CHECK(parseScript("matrix", steps, err) && steps.size() == 7);
	CAP_CHECK(parseScript("scaling", steps, err) && steps.size() == 25);
	CAP_CHECK(parseScript("distinct", steps, err) && steps.size() == 5);
	CAP_CHECK(parseScript("qualify", steps, err) && steps.size() == 8);
	CAP_CHECK(steps[6].plan.mode == AssignmentMode::Weighted && steps[6].plan.plannedShaderSlots() == 6);
	CAP_CHECK(steps[7].plan.mode == AssignmentMode::Budget && steps[7].plan.plannedShaderSlots() == 0);
	CAP_CHECK(parseScript("budget:30", steps, err) && steps[0].plan.mode == AssignmentMode::Budget);
	CAP_CHECK(steps[0].plan.banner().find("assignment=budget") != std::string::npos);
	CAP_CHECK(!parseScript("A", steps, err));
	CAP_CHECK(!parseScript("nosuch@3:10", steps, err));
	CAP_CHECK(!parseScript("recolor@11:10", steps, err));
	CAP_CHECK(!parseScript("", steps, err));
}

void test_banner_names_everything() {
	Plan p;
	preset('E', p);
	std::string b = p.banner();
	for (const char* needle : { "[BLOB EFFECT CAPACITY]", "case=E", "shaderTarget=8/10", "originalTarget=2/10",
			 "distinctShaders=4", "assignment=round_robin", "recolor[candidate]", "solarize[diagnostic:outside-tolerance]" }) {
		CAP_CHECK(b.find(needle) != std::string::npos);
	}
}

int main() {
	test_effect_table();
	test_shader_slot_counts_exact();
	test_presets_match_matrix();
	test_round_robin_uses_every_distinct_shader();
	test_single_mode_uses_only_first_effect();
	test_assignment_is_stable_per_id();
	test_consecutive_ids_reach_planned_mix();
	test_weighted();
	test_distinct_clamps();
	test_script_parsing();
	test_banner_names_everything();

	std::cout << "blob_capacity_plan_tests: " << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
