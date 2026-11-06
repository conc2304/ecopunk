// OC-003A: unit tests for the predominant-colour diagnostic algorithms (OF-free).
// Build/run:  c++ -std=c++17 -Wall -Wextra -I../src predominant_color_tests.cpp -o predominant_color_tests && ./predominant_color_tests
#include "PredominantColor.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace predominant;

static int T = 0, F = 0;
#define CHECK(c, msg) do { ++T; if (!(c)) { ++F; std::printf("FAIL: %s (%d)\n", msg, __LINE__); } } while (0)

static Rgb lin(float e) { // encoded -> linear (test helper)
	return {e <= 0.04045f ? e / 12.92f : std::pow((e + 0.055f) / 1.055f, 2.4f), 0, 0};
}
static void put(std::vector<float>& v, int n, float er, float eg, float eb) {
	for (int i = 0; i < n; ++i) { v.push_back(lin(er).r); v.push_back(lin(eg).r); v.push_back(lin(eb).r); v.push_back(1.0f); }
}
static Histogram H(const std::vector<float>& px, const Tuning& tu = {}) {
	Histogram h; accumulate(px.data(), px.size() / 4, 4, tu, h); return h;
}
static bool near(float a, float b, float e = 0.01f) { return std::fabs(a - b) <= e; }

int main() {
	const Tuning tu;
	{ // uniform colour: dominant == average == the colour
		std::vector<float> p; put(p, 144, 0.8f, 0.3f, 0.2f);
		auto h = H(p); auto d = dominant(h);
		CHECK(d.valid && d.share > 0.999f, "uniform: whole frame is the dominant cluster");
		CHECK(near(d.rgb.r, lin(0.8f).r) && near(h.average.r, d.rgb.r), "uniform: dominant == average");
	}
	{ // mixed colours: 50% red / 50% blue averages to purple, but the dominant is a real pixel colour
		std::vector<float> p; put(p, 60, 0.9f, 0.05f, 0.05f); put(p, 40, 0.05f, 0.05f, 0.9f);
		auto h = H(p); auto d = dominant(h);
		CHECK(near(d.rgb.r, lin(0.9f).r) && d.rgb.b < 0.05f, "mixed: dominant is the red majority, not the purple mean");
		CHECK(h.average.r > 0.3f && h.average.b > 0.2f, "mixed: the average blends both (reference behaviour)");
	}
	{ // representative is the mean of the assigned pixels, not the bin centre
		std::vector<float> p; put(p, 10, 0.52f, 0.52f, 0.52f); put(p, 10, 0.60f, 0.60f, 0.60f); // same 8-level bin [0.5,0.625)
		auto h = H(p); auto d = dominant(h);
		const float mean = 0.5f * (lin(0.52f).r + lin(0.60f).r);
		CHECK(d.population == 20 && near(d.rgb.g, mean, 1e-4f), "representative = linear mean of the cluster, not the bin centre");
		CHECK(!near(d.rgb.g, lin(0.5625f).r, 1e-4f), "representative differs from the bin centre");
	}
	{ // tie resolves to the lowest bin index
		std::vector<float> p; put(p, 10, 0.9f, 0.9f, 0.9f); put(p, 10, 0.1f, 0.1f, 0.1f);
		CHECK(dominant(H(p)).bin == binOf(0.1f, 0.1f, 0.1f), "tie -> lowest bin index");
	}
	{ // vibrant: isolated saturated pixels cannot win; a ~1% feature can; grey frames are invalid
		std::vector<float> p; put(p, 14398, 0.5f, 0.5f, 0.5f); put(p, 2, 1.0f, 0.0f, 0.0f);
		CHECK(!vibrant(H(p), tu).valid, "vibrant: 2 saturated pixels of 14400 do not qualify (needs >=1% area)");
		std::vector<float> q; put(q, 14200, 0.5f, 0.5f, 0.5f); put(q, 200, 1.0f, 0.1f, 0.1f);
		auto v = vibrant(H(q), tu);
		CHECK(v.valid && v.rgb.r > 0.9f && v.rgb.g < 0.2f, "vibrant: a 1.4% red feature wins over the grey majority");
		CHECK(dominant(H(q)).rgb.r < 0.3f, "dominant stays grey in the same frame");
		std::vector<float> g; put(g, 144, 0.4f, 0.4f, 0.4f);
		CHECK(!vibrant(H(g), tu).valid, "vibrant: pure grey frame has no vibrant colour (explicitly invalid)");
		std::vector<float> d; put(d, 100, 0.10f, 0.0f, 0.0f); // saturated but dark (v < 0.15)
		CHECK(!vibrant(H(d), tu).valid, "vibrant: dark saturated noise is ineligible");
	}
	{ // vibrant weighting: a larger but duller cluster loses to a smaller vivid cluster only if its weight is greater
		std::vector<float> p; put(p, 60, 0.50f, 0.40f, 0.35f); put(p, 40, 0.95f, 0.05f, 0.05f);
		auto v = vibrant(H(p), tu);
		CHECK(v.valid && v.rgb.r > 0.8f, "vibrant: vivid red outweighs the duller, more populous beige");
	}
	{ // hysteresis
		std::vector<float> p; put(p, 50, 0.9f, 0.1f, 0.1f); put(p, 55, 0.1f, 0.1f, 0.9f); // challenger only 10% larger
		auto h = H(p); const int red = binOf(0.9f, 0.1f, 0.1f), blue = binOf(0.1f, 0.1f, 0.9f);
		CHECK(dominant(h).bin == blue, "raw dominant picks blue");
		CHECK(dominantWithHysteresis(h, red, tu) == red, "hysteresis holds red against a 10% challenger");
		std::vector<float> q; put(q, 50, 0.9f, 0.1f, 0.1f); put(q, 80, 0.1f, 0.1f, 0.9f);
		CHECK(dominantWithHysteresis(H(q), red, tu) == blue, "hysteresis releases against a 60% challenger");
		std::vector<float> r; put(r, 100, 0.1f, 0.1f, 0.9f);
		CHECK(dominantWithHysteresis(H(r), red, tu) == blue, "a vanished held bin is released");
		CHECK(dominantWithHysteresis(H(r), -1, tu) == blue, "no held bin -> raw winner");
	}
	{ // vibrant hysteresis: holds a qualifying bin against a marginal challenger, releases on a clear one,
	  // and never holds anything across an invalid observation
		std::vector<float> p; put(p, 100, 0.95f, 0.05f, 0.05f); put(p, 110, 0.05f, 0.95f, 0.05f); // green only ~10% heavier
		auto h = H(p); const int red = binOf(0.95f, 0.05f, 0.05f), green = binOf(0.05f, 0.95f, 0.05f);
		CHECK(vibrant(h, tu).bin == green, "raw vibrant picks the heavier green");
		CHECK(vibrantWithHysteresis(h, red, tu) == red, "vibrant hysteresis holds red against a marginal challenger");
		std::vector<float> q; put(q, 100, 0.95f, 0.05f, 0.05f); put(q, 200, 0.05f, 0.95f, 0.05f);
		CHECK(vibrantWithHysteresis(H(q), red, tu) == green, "vibrant hysteresis releases against a clear challenger");
		std::vector<float> r; put(r, 100, 0.05f, 0.95f, 0.05f);
		CHECK(vibrantWithHysteresis(H(r), red, tu) == green, "a held bin that stopped qualifying is released");
		std::vector<float> g; put(g, 144, 0.4f, 0.4f, 0.4f);
		CHECK(vibrantWithHysteresis(H(g), red, tu) == -1, "no qualifying bin => invalid (-1), held state must be dropped");
		CHECK(!vibrantOfBin(H(g), red, tu).valid, "vibrantOfBin is invalid for a non-qualifying bin");
	}
	{ // EMA: fresh state after a gap / scene change
		const Rgb a{1, 0, 0}, b{0, 0, 0};
		Rgb s = emaStep({}, false, a, tu.emaAlpha);
		CHECK(s.r == 1.0f, "EMA starts at the first sample");
		s = emaStep(s, true, b, tu.emaAlpha);
		CHECK(near(s.r, 0.6f, 1e-5f), "EMA moves alpha toward the new sample");
		s = emaStep(s, false, a, tu.emaAlpha);
		CHECK(s.r == 1.0f, "EMA restarts (no bridging) when the state was reset");
	}
	{ // empty input never produces a measurement
		Histogram h; accumulate(nullptr, 0, 4, tu, h);
		CHECK(!dominant(h).valid && !vibrant(h, tu).valid, "empty input is invalid");
	}
	std::printf("%d/%d passed\n", T - F, T);
	return F == 0 ? 0 : 1;
}
