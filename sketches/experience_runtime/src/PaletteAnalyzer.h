// OC-003-F -- DIAGNOSTIC palette analyzer (OpenFrameworks-free, header-only, unit tested: test/palette_analyzer_tests.cpp).
//
// Operates ONLY on an already-acquired 160x90 (or any W x H) grid of LINEAR-LIGHT RGB cells -- the very grid the GPU samplers read back
// (collectLinear's pxBuf_) or the CPU RAW sampler built. It adds NO readback, owns NO GPU state, and does NOT redefine the production
// AVG / DOM / VIB: those are recorded unchanged beside the new metrics as reference values.
//
// Definitions (all documented in docs/reviews/OC-003-F-acquisition-qualification.md, "Palette metric definitions"):
//   colour space   cells are linear-light RGB in [0,1] (composited over black upstream). Every metric below is computed on the
//                  sRGB-ENCODED value e = OETF(linear) (what the viewer sees), via a 4096-entry table (deterministic, no per-cell pow).
//   chroma C       max(e) - min(e)                       (HSV chroma, 0..1)
//   hue H          HSV hue of e in degrees [0,360), defined only for a CHROMATIC cell: C >= kChromaMin (0.08)
//   family         12 equal 30-degree families centred on 0,30,...,330 deg: floor(((H + 15) mod 360) / 30)
//   luma Ye        0.2126 e.r + 0.7152 e.g + 0.0722 e.b  (Rec.709 weights on the ENCODED value)
//   hueOccupancy[f]    (cells that are chromatic and in family f) / (all cells)
//   chromaticFraction  chromatic cells / all cells
//   chromaticEnergy    mean over ALL cells of C
//   complexity         normalised Shannon entropy (base 12) of the CHROMA-WEIGHTED family histogram w[f] = sum C over chromatic cells in f,
//                      normalised to sum 1; 0 when there is no chromatic energy (a single hue family gives 0, 12 equal families give 1)
//   familiesActive     number of families with w[f] >= kFamilyShare (0.05)
//   highlightOcc       cells with Ye >= kHighlightY (0.85) / all cells
//   shadowOcc          cells with Ye <= kShadowY    (0.15) / all cells
//   drift (temporal)   between two CONSECUTIVE acquired samples of the SAME generation and stream, both with chromatic energy:
//                        driftHue = 0.5 * sum_f |w_t[f] - w_prev[f]|      (total-variation distance of the chroma-weighted histograms, 0..1)
//                        driftAvg = Euclidean distance of the two mean ENCODED RGBs
//                      otherwise NaN (never 0: an undefined drift is not "no drift")
//   resolutions    box-averaging (in LINEAR light) by an integer factor that divides W and H; for 160x90: 1 (160x90), 2 (80x45), 5 (32x18), 10 (16x9)
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace palette {

constexpr int kFamilies = 12;
constexpr float kChromaMin = 0.08f;
constexpr float kHighlightY = 0.85f;
constexpr float kShadowY = 0.15f;
constexpr float kFamilyShare = 0.05f;
constexpr float kEnergyEpsilon = 1e-6f; // "has chromatic energy" for the drift definition
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

struct Metrics {
	bool valid = false;           // false: no cells / non-finite input -> every field below is meaningless
	int cells = 0;
	int chromaticCells = 0;
	float chromaticFraction = 0.0f;
	float chromaticEnergy = 0.0f;
	float complexity = 0.0f;
	int familiesActive = 0;
	float highlightOcc = 0.0f;
	float shadowOcc = 0.0f;
	float meanR = 0.0f, meanG = 0.0f, meanB = 0.0f; // mean of the ENCODED cells (for driftAvg)
	std::array<float, kFamilies> hueOccupancy{};    // fraction of ALL cells
	std::array<float, kFamilies> hueWeight{};       // chroma-weighted, normalised to sum 1 (all 0 when no chromatic energy)
	float weightSum = 0.0f;                         // sum of un-normalised chroma weights (0 -> no chromatic energy)
};

struct Drift {
	float hue = kNaN;
	float avg = kNaN;
};

// linear -> sRGB-encoded, 4096-entry table (index = round(linear * 4095)).
inline const std::array<float, 4096>& oetfTable() {
	static const std::array<float, 4096> t = [] {
		std::array<float, 4096> a{};
		for (int i = 0; i < 4096; ++i) {
			const double c = static_cast<double>(i) / 4095.0;
			a[static_cast<std::size_t>(i)] = static_cast<float>(c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055);
		}
		return a;
	}();
	return t;
}

inline float encode(float linear) {
	if (!(linear > 0.0f)) return 0.0f; // also NaN -> 0 (callers reject non-finite input before this)
	if (linear >= 1.0f) return 1.0f;
	return oetfTable()[static_cast<std::size_t>(linear * 4095.0f + 0.5f)];
}

inline int familyOf(float hueDeg) {
	float h = std::fmod(hueDeg + 15.0f, 360.0f);
	if (h < 0.0f) h += 360.0f;
	int f = static_cast<int>(h / 30.0f);
	return f >= kFamilies ? kFamilies - 1 : f;
}

// `px`: n cells of `stride` floats (R,G,B,[A]) in linear light. Alpha is ignored (the upstream grid is opaque).
inline Metrics analyze(const float* px, std::size_t n, int stride) {
	Metrics m;
	if (px == nullptr || n == 0 || stride < 3) return m;
	double sumC = 0.0, sumR = 0.0, sumG = 0.0, sumB = 0.0;
	std::array<double, kFamilies> occ{}, w{};
	std::size_t chroma = 0, hi = 0, lo = 0;
	for (std::size_t i = 0; i < n; ++i) {
		const float* p = px + i * static_cast<std::size_t>(stride);
		if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2])) return m; // never analyse a poisoned grid
		const float r = encode(p[0]), g = encode(p[1]), b = encode(p[2]);
		const float mx = std::fmax(r, std::fmax(g, b)), mn = std::fmin(r, std::fmin(g, b));
		const float c = mx - mn;
		sumC += c; sumR += r; sumG += g; sumB += b;
		const float ye = 0.2126f * r + 0.7152f * g + 0.0722f * b;
		if (ye >= kHighlightY) ++hi;
		if (ye <= kShadowY) ++lo;
		if (c >= kChromaMin) {
			float h;
			if (mx == r) h = 60.0f * std::fmod((g - b) / c, 6.0f);
			else if (mx == g) h = 60.0f * ((b - r) / c + 2.0f);
			else h = 60.0f * ((r - g) / c + 4.0f);
			if (h < 0.0f) h += 360.0f;
			const int f = familyOf(h);
			occ[static_cast<std::size_t>(f)] += 1.0;
			w[static_cast<std::size_t>(f)] += c;
			++chroma;
		}
	}
	const double N = static_cast<double>(n);
	m.valid = true;
	m.cells = static_cast<int>(n);
	m.chromaticCells = static_cast<int>(chroma);
	m.chromaticFraction = static_cast<float>(chroma / N);
	m.chromaticEnergy = static_cast<float>(sumC / N);
	m.highlightOcc = static_cast<float>(hi / N);
	m.shadowOcc = static_cast<float>(lo / N);
	m.meanR = static_cast<float>(sumR / N); m.meanG = static_cast<float>(sumG / N); m.meanB = static_cast<float>(sumB / N);
	double wsum = 0.0;
	for (int f = 0; f < kFamilies; ++f) { m.hueOccupancy[static_cast<std::size_t>(f)] = static_cast<float>(occ[static_cast<std::size_t>(f)] / N); wsum += w[static_cast<std::size_t>(f)]; }
	m.weightSum = static_cast<float>(wsum);
	if (wsum > 0.0) {
		double ent = 0.0;
		for (int f = 0; f < kFamilies; ++f) {
			const double q = w[static_cast<std::size_t>(f)] / wsum;
			m.hueWeight[static_cast<std::size_t>(f)] = static_cast<float>(q);
			if (q > 0.0) ent -= q * std::log(q);
			if (q >= kFamilyShare) ++m.familiesActive;
		}
		m.complexity = static_cast<float>(ent / std::log(static_cast<double>(kFamilies)));
	}
	return m;
}

// Temporal comparison. The CALLER guarantees "consecutive, same generation, same stream" (it resets its previous-metrics holder on a
// generation change, a gap or an invalid sample); this function only enforces validity and chromatic energy.
inline Drift drift(const Metrics& prev, const Metrics& cur) {
	Drift d;
	if (!prev.valid || !cur.valid) return d;
	if (!(prev.weightSum > kEnergyEpsilon) || !(cur.weightSum > kEnergyEpsilon)) return d;
	double tv = 0.0;
	for (std::size_t f = 0; f < static_cast<std::size_t>(kFamilies); ++f) tv += std::fabs(static_cast<double>(cur.hueWeight[f]) - prev.hueWeight[f]);
	d.hue = static_cast<float>(0.5 * tv);
	const double dr = cur.meanR - prev.meanR, dg = cur.meanG - prev.meanG, db = cur.meanB - prev.meanB;
	d.avg = static_cast<float>(std::sqrt(dr * dr + dg * dg + db * db));
	return d;
}

// Box average (LINEAR light) by an integer `factor` that divides w and h; out is (w/factor)*(h/factor) cells of 3 floats.
// Returns false (out untouched) when the factor does not divide the grid.
inline bool boxDownsample(const float* px, int w, int h, int stride, int factor, std::vector<float>& out) {
	if (px == nullptr || factor < 1 || stride < 3 || w <= 0 || h <= 0 || w % factor != 0 || h % factor != 0) return false;
	const int ow = w / factor, oh = h / factor;
	out.assign(static_cast<std::size_t>(ow) * static_cast<std::size_t>(oh) * 3u, 0.0f);
	const double k = 1.0 / (static_cast<double>(factor) * factor);
	for (int y = 0; y < oh; ++y)
		for (int x = 0; x < ow; ++x) {
			double r = 0, g = 0, b = 0;
			for (int j = 0; j < factor; ++j)
				for (int i = 0; i < factor; ++i) {
					const float* p = px + (static_cast<std::size_t>(y * factor + j) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x * factor + i)) * static_cast<std::size_t>(stride);
					r += p[0]; g += p[1]; b += p[2];
				}
			float* o = &out[(static_cast<std::size_t>(y) * static_cast<std::size_t>(ow) + static_cast<std::size_t>(x)) * 3u];
			o[0] = static_cast<float>(r * k); o[1] = static_cast<float>(g * k); o[2] = static_cast<float>(b * k);
		}
	return true;
}

} // namespace palette
