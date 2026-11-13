// Raw Video — OF-free tests for ../src/RawVideoAdjust.h (the BRIGHTNESS / CONTRAST / SATURATION dial model).
// GL behaviour (the adjust pass, the HUD rows) is covered by the real-runtime run, not here.

#include "../src/RawVideoAdjust.h"

#include <cmath>
#include <iostream>
#include <set>
#include <string>

namespace {
int g_total = 0, g_failures = 0;
}
#define RV_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) { g_failures++; std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; } \
	} while (0)

int main() {
	using namespace rawvideo;

	// --- identity: the centre detent of every dial is the untouched picture
	RV_CHECK(dialValue(kBrightness, kCentre) == 0.0f);
	RV_CHECK(dialValue(kContrast, kCentre) == 1.0f);
	RV_CHECK(dialValue(kSaturation, kCentre) == 1.0f);
	{
		Dials d;
		RV_CHECK(d.isIdentity());
		RV_CHECK(d.step(kContrast, +1));
		RV_CHECK(!d.isIdentity());
		RV_CHECK(d.step(kContrast, -1));
		RV_CHECK(d.isIdentity());
	}

	// --- documented ranges at both ends, monotonic in between
	RV_CHECK(std::fabs(dialValue(kBrightness, 0) + 0.25f) < 1e-6f && std::fabs(dialValue(kBrightness, kSteps - 1) - 0.25f) < 1e-6f);
	RV_CHECK(std::fabs(dialValue(kContrast, 0) - 0.5f) < 1e-6f && std::fabs(dialValue(kContrast, kSteps - 1) - 1.5f) < 1e-6f);
	RV_CHECK(std::fabs(dialValue(kSaturation, 0) - 0.0f) < 1e-6f && std::fabs(dialValue(kSaturation, kSteps - 1) - 2.0f) < 1e-6f);
	for (int dial = 0; dial < kDialCount; ++dial)
		for (int i = 1; i < kSteps; ++i) RV_CHECK(dialValue(dial, i) > dialValue(dial, i - 1));
	RV_CHECK(dialValue(kBrightness, -3) == dialValue(kBrightness, 0) && dialValue(kBrightness, 99) == dialValue(kBrightness, kSteps - 1)); // out-of-range index is clamped

	// --- stepping clamps at the ends and reports whether anything moved
	for (int dial = 0; dial < kDialCount; ++dial) {
		Dials d;
		for (int i = 0; i < kCentre; ++i) RV_CHECK(d.step(dial, +1));
		RV_CHECK(d.index[dial] == kSteps - 1);
		RV_CHECK(!d.step(dial, +1)); // at the top: no wrap, nothing moved
		RV_CHECK(d.index[dial] == kSteps - 1);
		for (int i = 0; i < kSteps - 1; ++i) RV_CHECK(d.step(dial, -1));
		RV_CHECK(d.index[dial] == 0);
		RV_CHECK(!d.step(dial, -1));
		// the other two dials were never touched
		for (int other = 0; other < kDialCount; ++other) if (other != dial) RV_CHECK(d.index[other] == kCentre);
	}
	{
		Dials d;
		RV_CHECK(!d.step(-1, +1) && !d.step(kDialCount, +1)); // unknown axis refused
		RV_CHECK(!d.step(kBrightness, 0) && !d.step(kBrightness, 2) && !d.step(kBrightness, -2)); // only +/-1
		RV_CHECK(d.isIdentity());
	}

	// --- reset returns every dial to its centre
	{
		Dials d;
		d.step(kBrightness, +1); d.step(kContrast, -1); d.step(kSaturation, -1); d.step(kSaturation, -1);
		RV_CHECK(!d.isIdentity());
		d.reset();
		RV_CHECK(d.isIdentity());
	}

	// --- HUD text: the identity reads 0% / 100% / 100%; extremes read honestly
	RV_CHECK(dialText(kBrightness, kCentre) == "0%");
	RV_CHECK(dialText(kContrast, kCentre) == "100%");
	RV_CHECK(dialText(kSaturation, kCentre) == "100%");
	RV_CHECK(dialText(kBrightness, kSteps - 1) == "+25%" && dialText(kBrightness, 0) == "-25%");
	RV_CHECK(dialText(kContrast, 0) == "50%" && dialText(kContrast, kSteps - 1) == "150%");
	RV_CHECK(dialText(kSaturation, 0) == "0%" && dialText(kSaturation, kSteps - 1) == "200%");
	RV_CHECK(dialText(kBrightness, kCentre + 1) == "+5%" && dialText(kBrightness, kCentre - 1) == "-5%");
	{ // every detent of a dial has a distinct label (the HUD never shows two positions as the same value)
		for (int dial = 0; dial < kDialCount; ++dial) {
			std::set<std::string> seen;
			for (int i = 0; i < kSteps; ++i) seen.insert(dialText(dial, i));
			RV_CHECK((int)seen.size() == kSteps);
		}
	}
	RV_CHECK(dialPosition(0) == "1/11" && dialPosition(kCentre) == "6/11" && dialPosition(kSteps - 1) == "11/11");

	// --- stable captions / ids, one per axis, all distinct
	{
		std::set<std::string> labels, ids;
		for (int dial = 0; dial < kDialCount; ++dial) { labels.insert(dialLabel(dial)); ids.insert(dialId(dial)); }
		RV_CHECK(labels.size() == (size_t)kDialCount && ids.size() == (size_t)kDialCount);
		RV_CHECK(std::string(dialLabel(kBrightness)) == "BRIGHTNESS" && std::string(dialLabel(kContrast)) == "CONTRAST" && std::string(dialLabel(kSaturation)) == "SATURATION");
	}

	std::cout << "raw_video_adjust_tests: " << (g_total - g_failures) << " passed, " << g_failures << " failed (" << g_total << " checks)\n";
	return g_failures == 0 ? 0 : 1;
}
