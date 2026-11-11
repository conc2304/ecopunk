// OC-003-F -- CpuRawAcquisition.h (OF-free): grid reduction vs an independent reference, view validation, cadence, generation isolation,
// gap handling, and agreement with the production linear-light average.
#include "CpuRawAcquisition.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace cpuraw;

namespace {
int g_total = 0, g_failures = 0;
void check(bool cond, const std::string& what) {
	g_total++;
	if (!cond) { g_failures++; std::cerr << "FAIL: " << what << "\n"; }
}
bool near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) < eps; }

std::vector<uint8_t> image(int w, int h, int ch, uint32_t seed) {
	std::vector<uint8_t> v(static_cast<std::size_t>(w) * h * ch);
	for (auto& b : v) { seed = seed * 1664525u + 1013904223u; b = static_cast<uint8_t>(seed >> 24); }
	return v;
}
PixelView view(const std::vector<uint8_t>& v, int w, int h, int ch) { PixelView p; p.data = v.data(); p.w = w; p.h = h; p.channels = ch; p.rowBytes = static_cast<std::size_t>(w) * ch; return p; }

// independent, deliberately naive reference of the DOCUMENTED arithmetic (double precision, no shared code)
void reference(const std::vector<uint8_t>& px, int w, int h, int ch, std::vector<double>& out) {
	out.assign(static_cast<std::size_t>(kGridW) * kGridH * 3, 0.0);
	auto eotf = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
	for (int oy = 0; oy < kGridH; ++oy) for (int ox = 0; ox < kGridW; ++ox) {
		const double bx = static_cast<double>(w) / kGridW, by = static_cast<double>(h) / kGridH;
		int x0 = static_cast<int>(std::floor(ox * bx)), y0 = static_cast<int>(std::floor(oy * by));
		int x1 = std::min(w, std::max(x0 + 1, static_cast<int>(std::floor((ox + 1) * bx + 0.5)))), y1 = std::min(h, std::max(y0 + 1, static_cast<int>(std::floor((oy + 1) * by + 0.5))));
		x1 = std::min(x1, x0 + 24); y1 = std::min(y1, y0 + 24);
		double s[3] = {0, 0, 0}; int n = 0;
		for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
			const uint8_t* p = &px[(static_cast<std::size_t>(y) * w + x) * ch];
			const double a = ch == 4 ? p[3] / 255.0 : 1.0;
			for (int c = 0; c < 3; ++c) s[c] += eotf(p[c] / 255.0 * a);
			++n;
		}
		for (int c = 0; c < 3; ++c) out[(static_cast<std::size_t>(oy) * kGridW + ox) * 3 + c] = s[c] / n;
	}
}
}

int main() {
	std::vector<float> grid(static_cast<std::size_t>(kGridW) * kGridH * 4, -1.0f);

	// -- uniform sources
	{
		std::vector<uint8_t> white(960 * 540 * 3, 255);
		check(reduceToGrid(view(white, 960, 540, 3), grid.data()) && near(grid[0], 1.0f) && near(grid[4 * 777 + 1], 1.0f) && near(grid[3], 1.0f), "white 960x540 RGB -> grid 1.0 (A=1)");
		std::vector<uint8_t> black(960 * 540 * 3, 0);
		check(reduceToGrid(view(black, 960, 540, 3), grid.data()) && near(grid[0], 0.0f) && near(grid[4 * 14399 + 2], 0.0f), "black -> 0");
		std::vector<uint8_t> gray(960 * 540 * 3, 128);
		check(reduceToGrid(view(gray, 960, 540, 3), grid.data()) && near(grid[8], scenecolorsampling::srgbLut()[128]), "grey 128 -> sRGB EOTF(128/255), averaged in linear light");
	}

	// -- equals an independent reference on noisy RGB / RGBA of several sizes (incl. a non-multiple size and a cap case)
	for (const auto& dims : std::vector<std::vector<int>>{{960, 540, 3}, {1280, 720, 4}, {800, 450, 3}, {641, 361, 3}, {4000, 2250, 3}}) {
		const auto px = image(dims[0], dims[1], dims[2], static_cast<uint32_t>(dims[0] * 31 + dims[2]));
		std::vector<double> ref;
		reference(px, dims[0], dims[1], dims[2], ref);
		check(reduceToGrid(view(px, dims[0], dims[1], dims[2]), grid.data()), "reduce " + std::to_string(dims[0]) + "x" + std::to_string(dims[1]));
		double maxd = 0;
		for (std::size_t i = 0; i < static_cast<std::size_t>(kGridW) * kGridH; ++i) for (int c = 0; c < 3; ++c) maxd = std::max(maxd, std::fabs(grid[i * 4 + static_cast<std::size_t>(c)] - ref[i * 3 + static_cast<std::size_t>(c)]));
		check(maxd < 1e-4, "grid matches the documented arithmetic (max diff " + std::to_string(maxd) + ") for " + std::to_string(dims[0]) + "x" + std::to_string(dims[1]) + "x" + std::to_string(dims[2]));
	}

	// -- orientation: left half red, right half blue -> grid columns follow, row 0 = top of memory
	{
		std::vector<uint8_t> px(1280 * 720 * 3, 0);
		for (int y = 0; y < 720; ++y) for (int x = 0; x < 1280; ++x) { uint8_t* p = &px[(static_cast<std::size_t>(y) * 1280 + x) * 3]; if (x < 640) p[0] = 255; else p[2] = 255; }
		check(reduceToGrid(view(px, 1280, 720, 3), grid.data()), "reduce halves");
		auto at = [&](int x, int y, int c) { return grid[(static_cast<std::size_t>(y) * kGridW + x) * 4 + static_cast<std::size_t>(c)]; };
		check(near(at(79, 0, 0), 1) && near(at(79, 0, 2), 0) && near(at(80, 0, 2), 1) && near(at(80, 89, 0), 0), "x<640 red, x>=640 blue; grid x maps 8:1");
	}

	// -- stride (padding) is honoured
	{
		const int w = 320, h = 180;
		std::vector<uint8_t> padded(static_cast<std::size_t>(w + 16) * 3 * h, 7);
		for (int y = 0; y < h; ++y) for (int x = 0; x < w * 3; ++x) padded[static_cast<std::size_t>(y) * (w + 16) * 3 + x] = 255;
		PixelView v; v.data = padded.data(); v.w = w; v.h = h; v.channels = 3; v.rowBytes = static_cast<std::size_t>(w + 16) * 3;
		check(reduceToGrid(v, grid.data()) && near(grid[0], 1.0f), "row padding is skipped (white content, 7-valued padding)");
	}

	// -- alpha is composited over black before the EOTF
	{
		std::vector<uint8_t> px(160 * 90 * 4);
		for (std::size_t i = 0; i < 160 * 90; ++i) { px[i * 4] = 200; px[i * 4 + 1] = 100; px[i * 4 + 2] = 50; px[i * 4 + 3] = 128; }
		check(reduceToGrid(view(px, 160, 90, 4), grid.data()), "RGBA 160x90");
		check(near(grid[0], scenecolorsampling::srgbToLinear(200.0f / 255.0f * (128.0f / 255.0f)), 1e-5f), "alpha 128 composited over black in the encoded domain, then EOTF");
	}

	// -- unusable views leave the output untouched
	{
		std::vector<float> keep(grid);
		PixelView none; check(!reduceToGrid(none, grid.data()), "null data refused");
		std::vector<uint8_t> px(100, 0);
		PixelView bad2 = view(px, 5, 5, 2); check(!reduceToGrid(bad2, grid.data()), "2-channel refused");
		PixelView shortRow = view(px, 5, 5, 3); shortRow.rowBytes = 10; check(!reduceToGrid(shortRow, grid.data()), "rowBytes < w*channels refused");
		check(keep == grid, "refused views never write the output");
	}

	// -- agreement with the PRODUCTION linear-light average (SceneColorSeries.h) on a 160x90 RGBA source (block = 1)
	{
		const auto px = image(160, 90, 4, 99u);
		float er, eg, eb;
		check(scenecolorsampling::averageLinearRgba8OverBlack(px.data(), 160 * 90, er, eg, eb), "production average");
		Sampler s; s.beginGeneration(1, 0.0);
		const auto out = s.onFrame(view(px, 160, 90, 4), 160, 90, 0.0, 0.0, 0.0);
		check(out == Sampler::Outcome::Sampled, "sampled");
		check(near(s.last().r, er, 1e-4f) && near(s.last().g, eg, 1e-4f) && near(s.last().b, eb, 1e-4f), "CPU RAW AVG equals the production average on the same pixels");
	}

	// -- cadence, generation isolation, gaps, dimension mismatch, predominant
	{
		const auto red = image(960, 540, 3, 1u);
		std::vector<uint8_t> r(960 * 540 * 3, 0); for (std::size_t i = 0; i < 960 * 540; ++i) r[i * 3] = 230;
		std::vector<uint8_t> b(960 * 540 * 3, 0); for (std::size_t i = 0; i < 960 * 540; ++i) b[i * 3 + 2] = 230;
		Sampler s; s.setPredominant(true);
		check(!s.due(0.0), "not due before a generation exists");
		s.beginGeneration(1, 10.0);
		check(s.onFrame(view(r, 960, 540, 3), 960, 540, 9.9, 0, 0) == Sampler::Outcome::NotDue, "not due before the first due time");
		check(s.onFrame(view(r, 960, 540, 3), 960, 540, 10.0, 0.1, 0.1) == Sampler::Outcome::Sampled && s.stats().samples == 1, "first sample at the due time");
		check(s.onFrame(view(r, 960, 540, 3), 960, 540, 10.3, 0, 0) == Sampler::Outcome::NotDue, "2 Hz: not due again after 0.3 s");
		check(s.onFrame(view(r, 960, 540, 3), 960, 540, 10.5, 0, 0) == Sampler::Outcome::Sampled && s.stats().samples == 2, "due again after 0.5 s");
		check(s.last().valid && s.last().domValid && s.last().r > 0.5f && s.last().b < 0.01f, "red scene: valid AVG, DOM valid, red dominates");
		check(s.series().generation() == 1 && s.series().size() == 2, "two samples in generation 1");
		// new generation: history cleared, tracker reset, blue never blends with red
		s.beginGeneration(2, 20.0);
		check(s.series().generation() == 2 && s.series().size() == 0, "new generation clears history");
		check(s.onFrame(view(b, 960, 540, 3), 960, 540, 20.0, 0, 0) == Sampler::Outcome::Sampled, "sample in generation 2");
		check(s.last().generation == 2 && s.last().b > 0.5f && s.last().r < 0.01f && s.last().domValid && s.last().domB > s.last().domR, "generation 2 sample is blue only: no stale red, DOM tracker restarted");
		check(s.series().size() == 1, "exactly one sample in generation 2");
		// dimension mismatch -> explicit gap, counted, tracker reset
		check(s.onFrame(view(r, 960, 540, 3), 1280, 720, 20.5, 0, 0) == Sampler::Outcome::Gap && s.stats().dimMismatch == 1 && !s.last().valid, "dimension mismatch is a gap, not a guess");
		// null pixels while eligible -> gap
		check(s.onFrame(PixelView{}, 960, 540, 21.0, 0, 0) == Sampler::Outcome::Gap && s.stats().noPixels == 1, "no pixels is a gap");
		// bad format (2 channels with data) -> gap + badFormat
		std::vector<uint8_t> two(960 * 540 * 2, 0);
		PixelView bf = view(two, 960, 540, 2);
		check(s.onFrame(bf, 960, 540, 21.5, 0, 0) == Sampler::Outcome::Gap && s.stats().badFormat == 1, "unsupported pixel format is refused and counted");
		// recordGap when due
		check(s.recordGap(21.6) == Sampler::Outcome::NotDue, "recordGap respects cadence");
		check(s.recordGap(22.0) == Sampler::Outcome::Gap && !s.last().valid, "recordGap writes an explicit invalid sample");
		// after a gap the first sample has no smoothing carried over
		check(s.onFrame(view(r, 960, 540, 3), 960, 540, 22.5, 0, 0) == Sampler::Outcome::Sampled && s.last().domValid && s.last().domR > s.last().domB, "sample after a gap is red again (DOM state restarted)");
		(void)red;
	}

	// -- timing helpers are monotone and cpu time never exceeds wall time by more than noise over a busy loop
	{
		const double w0 = wallMs(), c0 = threadCpuMs();
		volatile double x = 0; for (int i = 0; i < 2000000; ++i) x += std::sqrt(static_cast<double>(i));
		const double w1 = wallMs(), c1 = threadCpuMs();
		check(w1 > w0, "wall clock advances");
		check(c0 < 0 || (c1 >= c0 && (c1 - c0) <= (w1 - w0) + 5.0), "thread CPU time is bounded by wall time");
	}

	// -- Dist is bounded and nearest-rank
	{
		Dist d;
		for (int i = 1; i <= 1000; ++i) d.add(i);
		check(d.total() == 1000 && near(static_cast<float>(d.max()), 1000.0f, 1e-3f), "Dist counts everything");
		check(d.pct(0.5) >= 745 && d.pct(0.5) <= 885, "percentiles come from the bounded latest-256 ring");
	}

	std::cout << (g_failures == 0 ? "ALL PASS" : "FAILURES") << " (" << (g_total - g_failures) << "/" << g_total << ")\n";
	return g_failures == 0 ? 0 : 1;
}
