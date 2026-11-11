// OC-003-F -- PaletteAnalyzer.h (OF-free): definitions, boundaries, determinism, drift, multi-resolution box averaging.
#include "PaletteAnalyzer.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace palette;

namespace {
int g_total = 0, g_failures = 0;
void check(bool cond, const std::string& what) {
	g_total++;
	if (!cond) { g_failures++; std::cerr << "FAIL: " << what << "\n"; }
}
bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

// linear cell from an sRGB-ENCODED triple (so tests state what the viewer sees)
void putEnc(std::vector<float>& g, std::size_t i, float er, float eg, float eb) {
	auto lin = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
	g[i * 4] = lin(er); g[i * 4 + 1] = lin(eg); g[i * 4 + 2] = lin(eb); g[i * 4 + 3] = 1.0f;
}
std::vector<float> filled(std::size_t n, float er, float eg, float eb) {
	std::vector<float> g(n * 4);
	for (std::size_t i = 0; i < n; ++i) putEnc(g, i, er, eg, eb);
	return g;
}
// HSV (h deg, s=1, v) -> encoded rgb
void hsv(float h, float s, float v, float& r, float& g, float& b) {
	const float c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1)), m = v - c;
	float rr = 0, gg = 0, bb = 0;
	if (h < 60) { rr = c; gg = x; } else if (h < 120) { rr = x; gg = c; } else if (h < 180) { gg = c; bb = x; }
	else if (h < 240) { gg = x; bb = c; } else if (h < 300) { rr = x; bb = c; } else { rr = c; bb = x; }
	r = rr + m; g = gg + m; b = bb + m;
}
}

int main() {
	const std::size_t N = 160 * 90;

	// -- hue family boundaries (12 x 30 deg, centred on 0,30,...)
	check(familyOf(0.0f) == 0 && familyOf(14.999f) == 0, "0..14.999 deg -> family 0");
	check(familyOf(15.0f) == 1 && familyOf(44.999f) == 1, "15 deg is the first of family 1");
	check(familyOf(345.0f) == 0 && familyOf(344.999f) == 11, "345 deg wraps to family 0, 344.999 is family 11");
	check(familyOf(359.9f) == 0 && familyOf(180.0f) == 6, "359.9 -> 0, 180 -> 6");

	// -- neutral grey: no chroma, no hue, no complexity, no drift base
	{
		const auto g = filled(N, 0.5f, 0.5f, 0.5f);
		const Metrics m = analyze(g.data(), N, 4);
		check(m.valid && m.cells == static_cast<int>(N), "grey grid valid");
		check(m.chromaticCells == 0 && near(m.chromaticFraction, 0) && near(m.chromaticEnergy, 0) && near(m.complexity, 0) && m.familiesActive == 0, "grey has no chromatic content");
		check(m.weightSum == 0.0f, "grey has no chroma weight");
		check(near(m.highlightOcc, 0) && near(m.shadowOcc, 0), "mid grey is neither highlight nor shadow");
		const Drift d = drift(m, m);
		check(std::isnan(d.hue) && std::isnan(d.avg), "drift without chromatic energy is NaN, never 0");
	}

	// -- pure red: one family, full chroma, complexity 0
	{
		const auto g = filled(N, 1.0f, 0.0f, 0.0f);
		const Metrics m = analyze(g.data(), N, 4);
		check(m.chromaticCells == static_cast<int>(N) && near(m.chromaticFraction, 1) && near(m.chromaticEnergy, 1, 2e-3f), "red: all cells chromatic, energy ~1");
		check(near(m.hueOccupancy[0], 1) && m.familiesActive == 1 && near(m.complexity, 0, 1e-6f), "red: family 0 only, complexity 0");
		check(near(m.highlightOcc, 0) && near(m.shadowOcc, 0), "red Ye=0.2126: not highlight, not shadow");
	}

	// -- twelve equal hue families, equal chroma -> complexity 1, 12 active
	{
		std::vector<float> g(12 * 4);
		for (int k = 0; k < 12; ++k) { float r, gg, b; hsv(k * 30.0f, 1.0f, 1.0f, r, gg, b); putEnc(g, static_cast<std::size_t>(k), r, gg, b); }
		const Metrics m = analyze(g.data(), 12, 4);
		check(m.familiesActive == 12 && near(m.complexity, 1.0f, 2e-3f), "12 equal families -> complexity ~1 and 12 active");
		for (int f = 0; f < 12; ++f) check(near(m.hueOccupancy[static_cast<std::size_t>(f)], 1.0f / 12.0f, 1e-4f), "each family holds 1/12 of the cells");
	}

	// -- chroma threshold: 0.10 is chromatic, 0.04 is not
	{
		auto g = filled(2, 0.5f, 0.5f, 0.6f);               // C = 0.10
		putEnc(g, 1, 0.5f, 0.5f, 0.54f);                    // C = 0.04
		const Metrics m = analyze(g.data(), 2, 4);
		check(m.chromaticCells == 1 && near(m.chromaticFraction, 0.5f), "only the C>=0.08 cell is chromatic");
	}

	// -- highlight / shadow occupancy
	{
		std::vector<float> g(4 * 4);
		putEnc(g, 0, 1, 1, 1); putEnc(g, 1, 0, 0, 0); putEnc(g, 2, 0.9f, 0.9f, 0.9f); putEnc(g, 3, 0.5f, 0.5f, 0.5f);
		const Metrics m = analyze(g.data(), 4, 4);
		check(near(m.highlightOcc, 0.5f) && near(m.shadowOcc, 0.25f), "white+0.9 grey are highlights (2/4), black is shadow (1/4)");
	}

	// -- determinism: identical input -> bitwise identical output, repeated
	{
		std::vector<float> g(N * 4);
		uint32_t seed = 12345u;
		for (std::size_t i = 0; i < N; ++i) { for (int c = 0; c < 3; ++c) { seed = seed * 1664525u + 1013904223u; g[i * 4 + static_cast<std::size_t>(c)] = static_cast<float>(seed >> 8) / 16777216.0f; } g[i * 4 + 3] = 1; }
		const Metrics a = analyze(g.data(), N, 4), b = analyze(g.data(), N, 4);
		check(std::memcmp(&a, &b, sizeof(Metrics)) == 0, "analyze() is deterministic (bitwise)");
		float s = 0; for (float q : a.hueOccupancy) s += q;
		check(near(s, a.chromaticFraction, 1e-4f), "hue occupancies sum to the chromatic fraction");
		float w = 0; for (float q : a.hueWeight) w += q;
		check(near(w, 1.0f, 1e-4f), "chroma-weighted histogram is normalised");
	}

	// -- poisoned / empty input is rejected, never analysed
	{
		auto g = filled(4, 0.5f, 0.2f, 0.2f);
		g[1] = std::nanf("");
		check(!analyze(g.data(), 4, 4).valid, "a NaN cell invalidates the whole analysis");
		check(!analyze(nullptr, 4, 4).valid && !analyze(g.data(), 0, 4).valid && !analyze(g.data(), 4, 2).valid, "null / empty / stride<3 are invalid");
	}

	// -- temporal drift
	{
		const auto red = filled(N, 1, 0, 0), blue = filled(N, 0, 0, 1);
		const Metrics r = analyze(red.data(), N, 4), b = analyze(blue.data(), N, 4);
		const Drift same = drift(r, r), diff = drift(r, b);
		check(near(same.hue, 0) && near(same.avg, 0), "identical consecutive samples -> drift 0");
		check(near(diff.hue, 1.0f) && diff.avg > 1.0f, "red -> blue: total-variation distance 1, large mean shift");
		Metrics invalid;
		check(std::isnan(drift(invalid, r).hue) && std::isnan(drift(r, invalid).avg), "drift against an invalid sample is NaN");
	}

	// -- multi-resolution box averaging (linear light)
	{
		const auto g = filled(N, 0.8f, 0.4f, 0.2f);
		std::vector<float> out;
		for (int f : {1, 2, 5, 10}) {
			check(boxDownsample(g.data(), 160, 90, 4, f, out) && out.size() == static_cast<std::size_t>((160 / f) * (90 / f) * 3), "factor " + std::to_string(f) + " divides 160x90");
			const Metrics m = analyze(out.data(), out.size() / 3, 3);
			const Metrics ref = analyze(g.data(), N, 4);
			check(near(m.chromaticEnergy, ref.chromaticEnergy, 2e-3f) && near(m.complexity, ref.complexity, 1e-4f), "uniform image: metrics identical at factor " + std::to_string(f));
		}
		check(!boxDownsample(g.data(), 160, 90, 4, 3, out) && !boxDownsample(g.data(), 160, 90, 4, 4, out), "factors that do not divide both sides are refused");
		// 2x2 checker of black/white averages to mid linear 0.5
		std::vector<float> chk(2 * 2 * 4, 0.0f);
		chk[0] = chk[1] = chk[2] = 1; chk[12] = chk[13] = chk[14] = 1;
		check(boxDownsample(chk.data(), 2, 2, 4, 2, out) && near(out[0], 0.5f) && near(out[1], 0.5f), "box average is taken in LINEAR light");
	}

	std::cout << (g_failures == 0 ? "ALL PASS" : "FAILURES") << " (" << (g_total - g_failures) << "/" << g_total << ")\n";
	return g_failures == 0 ? 0 : 1;
}
