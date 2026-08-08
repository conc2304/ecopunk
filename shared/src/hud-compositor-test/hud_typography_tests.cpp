// Tests for HudTextMetricsCache — links the real openFrameworks bitmap
// font implementation (ofBitmapFont.cpp, part of libopenFrameworks.a),
// same convention as hud_media_viewport_tests.cpp (see that file's own
// header comment). No window/GL context created — ofBitmapFont's
// getBoundingBox() only measures, never draws.

#include <iostream>
#include <sstream>
#include <string>

#include "../hud-compositor/widgets/HudTextMetricsCache.h"

using namespace hudpresent;

namespace {
int g_total = 0;
int g_failures = 0;
void reportFailure(const char* file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}
} // namespace

#define HUD_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

namespace {

void test_width_is_deterministic_and_cached() {
	HudTextMetricsCache cache;
	float w1 = cache.widthOf("QUADRANT ARRAY");
	float w2 = cache.widthOf("QUADRANT ARRAY");
	HUD_CHECK(w1 > 0.0f);
	HUD_CHECK(w1 == w2); // identical input -> identical (cached) output
	HUD_CHECK(cache.widthCacheSize() == 1); // one entry, not two, despite two calls

	float wEmpty = cache.widthOf("");
	HUD_CHECK(wEmpty == 0.0f);

	float wShort = cache.widthOf("HI");
	float wLong = cache.widthOf("HI THERE THIS IS LONGER");
	HUD_CHECK(wLong > wShort); // longer text -> wider, monotonic in this fixed-width font
}

void test_no_truncation_when_it_fits() {
	HudTextMetricsCache cache;
	float fullWidth = cache.widthOf("READY");
	const auto& result = cache.truncateToWidth("READY", fullWidth + 50.0f);
	HUD_CHECK(result.text == "READY");
	HUD_CHECK(result.truncated == false);
}

void test_truncates_deterministically_when_too_narrow() {
	HudTextMetricsCache cache;
	std::string longText = "quadrant-crosshair_quadrant-crosshair_quadrant-crosshair"; // this task's own LongestStrings-shaped case
	float fullWidth = cache.widthOf(longText);
	const auto& result = cache.truncateToWidth(longText, fullWidth * 0.2f); // force truncation
	HUD_CHECK(result.truncated == true);
	HUD_CHECK(result.text.size() < longText.size());
	HUD_CHECK(result.widthLocal <= fullWidth * 0.2f + 0.5f);

	// Deterministic: repeated calls with the same inputs return the exact
	// same (cached) truncated string.
	const auto& result2 = cache.truncateToWidth(longText, fullWidth * 0.2f);
	HUD_CHECK(result.text == result2.text);
	HUD_CHECK(cache.truncateCacheSize() >= 1);
}

void test_too_narrow_for_even_ellipsis_is_empty_not_garbled() {
	HudTextMetricsCache cache;
	const auto& result = cache.truncateToWidth("SOME LABEL", 0.5f);
	HUD_CHECK(result.truncated == true);
	HUD_CHECK(result.text.empty() || result.text == "..."); // never a partial/garbled glyph
}

} // namespace

int main() {
	test_width_is_deterministic_and_cached();
	test_no_truncation_when_it_fits();
	test_truncates_deterministically_when_too_narrow();
	test_too_narrow_for_even_ellipsis_is_empty_not_garbled();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed.\n";
	if (g_failures > 0) {
		std::cerr << g_failures << " FAILURE(S)\n";
		return 1;
	}
	return 0;
}
