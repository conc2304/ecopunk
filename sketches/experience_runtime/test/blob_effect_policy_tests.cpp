// Standalone, dependency-free tests for blobeffects::MediaEffectPolicy —
// the curated, media-change-driven Blob fragment-treatment selection used by
// BlobProductionScene::update() (see ../src/BlobEffectPolicy.h). Same
// bare-compiler convention as blob_semantic_mapping_tests.cpp.
//
// Build/run: make -C test -f Makefile.tests test

#include <iostream>
#include <set>
#include <string>

#include "../src/BlobEffectPolicy.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define POL_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using blobeffects::MediaEffectPolicy;
using blobeffects::FragmentTreatment;
using blobeffects::backgroundEffectId;
using blobeffects::curatedFragmentTreatments;

std::set<std::string> curatedIds() {
	std::set<std::string> ids;
	for (const auto& t : curatedFragmentTreatments()) ids.insert(t.id());
	return ids;
}

void test_curated_set_shape() {
	const auto& treatments = curatedFragmentTreatments();
	POL_CHECK(treatments.size() == 2);
	POL_CHECK(treatments[0] == FragmentTreatment::original());
	POL_CHECK(treatments[1] == FragmentTreatment::shader("recolor"));
	POL_CHECK(curatedIds().size() == treatments.size());
	// desaturate would be invisible on the desaturated background.
	POL_CHECK(curatedIds().count("desaturate") == 0);
	POL_CHECK(backgroundEffectId() == "desaturate");
}

void test_original_is_not_a_shader() {
	FragmentTreatment o = FragmentTreatment::original();
	POL_CHECK(o.isOriginal());
	POL_CHECK(o.regionEffectName().empty()); // renderer's unshaded contract
	POL_CHECK(o.id() == "original");
	FragmentTreatment r = FragmentTreatment::shader("recolor");
	POL_CHECK(!r.isOriginal());
	POL_CHECK(r.regionEffectName() == "recolor");
	POL_CHECK(r.id() == "recolor");
	POL_CHECK(o != r);
}

void test_initial_treatment_is_original() {
	MediaEffectPolicy p;
	POL_CHECK(p.currentTreatment().isOriginal());
	POL_CHECK(p.selectionChangeCount() == 0);
}

void test_no_media_never_selects() {
	MediaEffectPolicy p;
	for (int i = 0; i < 100; i++) {
		POL_CHECK(!p.observeMedia(std::nullopt));
	}
	POL_CHECK(p.currentTreatment().isOriginal());
	POL_CHECK(p.selectionChangeCount() == 0);
}

void test_first_media_keeps_default() {
	MediaEffectPolicy p;
	POL_CHECK(!p.observeMedia(std::string("clip-a")));
	POL_CHECK(p.currentTreatment().isOriginal());
}

void test_same_media_never_reselects_per_frame() {
	MediaEffectPolicy p;
	p.observeMedia(std::string("clip-a"));
	for (int i = 0; i < 1000; i++) {
		POL_CHECK(!p.observeMedia(std::string("clip-a")));
	}
	POL_CHECK(p.selectionChangeCount() == 0);
}

void test_media_change_advances_once() {
	MediaEffectPolicy p;
	p.observeMedia(std::string("clip-a"));
	POL_CHECK(p.observeMedia(std::string("clip-b")));
	POL_CHECK(p.currentTreatment() == curatedFragmentTreatments()[1]);
	POL_CHECK(p.selectionChangeCount() == 1);
	for (int i = 0; i < 50; i++) {
		POL_CHECK(!p.observeMedia(std::string("clip-b")));
	}
	POL_CHECK(p.selectionChangeCount() == 1);
}

void test_transient_no_media_does_not_count_as_change() {
	// A media load gap (status briefly without an active item) must not
	// itself advance, and returning to the same media must not either.
	MediaEffectPolicy p;
	p.observeMedia(std::string("clip-a"));
	POL_CHECK(!p.observeMedia(std::nullopt));
	POL_CHECK(!p.observeMedia(std::string("clip-a")));
	POL_CHECK(p.selectionChangeCount() == 0);
}

void test_cycle_wraps_and_stays_in_curated_set() {
	MediaEffectPolicy p;
	const auto& effects = curatedFragmentTreatments();
	std::set<std::string> allowed = curatedIds();
	p.observeMedia(std::string("clip-0"));
	for (size_t i = 1; i <= effects.size() * 3; i++) {
		POL_CHECK(p.observeMedia("clip-" + std::to_string(i)));
		POL_CHECK(allowed.count(p.currentTreatment().id()) == 1);
		POL_CHECK(p.currentTreatment() == effects[i % effects.size()]);
	}
}

void test_consecutive_media_never_repeat_effect() {
	MediaEffectPolicy p;
	p.observeMedia(std::string("m0"));
	FragmentTreatment previous = p.currentTreatment();
	for (int i = 1; i < 20; i++) {
		p.observeMedia("m" + std::to_string(i));
		POL_CHECK(p.currentTreatment() != previous);
		previous = p.currentTreatment();
	}
}

int main() {
	test_curated_set_shape();
	test_original_is_not_a_shader();
	test_initial_treatment_is_original();
	test_no_media_never_selects();
	test_first_media_keeps_default();
	test_same_media_never_reselects_per_frame();
	test_media_change_advances_once();
	test_transient_no_media_does_not_count_as_change();
	test_cycle_wraps_and_stays_in_curated_set();
	test_consecutive_media_never_repeat_effect();

	std::cout << "blob_effect_policy_tests: " << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
