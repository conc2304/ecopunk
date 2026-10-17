// Standalone, dependency-free tests for blobeffects::MediaGeneration — the
// media-change signal that starts a new effect-budget palette generation in
// BlobProductionScene::update() (see ../src/BlobEffectPolicy.h). Same
// bare-compiler convention as blob_semantic_mapping_tests.cpp.
//
// Build/run: make -C test -f Makefile.tests test

#include <iostream>
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

using blobeffects::MediaGeneration;
using blobeffects::backgroundEffectId;

void test_background_is_desaturate() {
	POL_CHECK(backgroundEffectId() == "desaturate");
}

void test_no_media_never_advances() {
	MediaGeneration g;
	for (int i = 0; i < 100; i++) {
		POL_CHECK(!g.observeMedia(std::nullopt));
	}
	POL_CHECK(g.generation() == 0);
}

void test_first_media_keeps_generation_zero() {
	MediaGeneration g;
	POL_CHECK(!g.observeMedia(std::string("clip-a")));
	POL_CHECK(g.generation() == 0);
}

void test_same_media_never_advances_per_frame() {
	MediaGeneration g;
	g.observeMedia(std::string("clip-a"));
	for (int i = 0; i < 1000; i++) {
		POL_CHECK(!g.observeMedia(std::string("clip-a")));
	}
	POL_CHECK(g.generation() == 0);
}

void test_media_change_advances_once() {
	MediaGeneration g;
	g.observeMedia(std::string("clip-a"));
	POL_CHECK(g.observeMedia(std::string("clip-b")));
	POL_CHECK(g.generation() == 1);
	for (int i = 0; i < 50; i++) {
		POL_CHECK(!g.observeMedia(std::string("clip-b")));
	}
	POL_CHECK(g.generation() == 1);
}

void test_transient_no_media_does_not_count_as_change() {
	MediaGeneration g;
	g.observeMedia(std::string("clip-a"));
	POL_CHECK(!g.observeMedia(std::nullopt));
	POL_CHECK(!g.observeMedia(std::string("clip-a")));
	POL_CHECK(g.generation() == 0);
}

void test_every_change_is_one_generation() {
	MediaGeneration g;
	g.observeMedia(std::string("m0"));
	for (int i = 1; i <= 30; i++) {
		POL_CHECK(g.observeMedia("m" + std::to_string(i % 3)) == (i % 3 != (i - 1) % 3));
	}
	POL_CHECK(g.generation() == 30);
}

int main() {
	test_background_is_desaturate();
	test_no_media_never_advances();
	test_first_media_keeps_generation_zero();
	test_same_media_never_advances_per_frame();
	test_media_change_advances_once();
	test_transient_no_media_does_not_count_as_change();
	test_every_change_is_one_generation();

	std::cout << "blob_effect_policy_tests: " << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
