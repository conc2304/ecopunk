// OC-003A diagnostic: predominant-colour measurements over a LINEAR-light RGBA float image (the
// SceneColorSampler's 160x90 or 16x9 reduction targets). OpenFrameworks-free and allocation-free on
// the hot path so it can be unit-tested and timed in isolation. DIAGNOSTIC ONLY: nothing here is
// published to the HUD and the ratified scenecolor::SceneColorSnapshot contract is untouched.
//
// Three measurements, all fed the same pixels (already composited over opaque black, linear light):
//   Average   linear-light spatial mean (the ratified reference).
//   Dominant  most POPULATED 8x8x8 cluster. Pixels are binned in the DISPLAY-ENCODED (sRGB) domain,
//             because bin edges should follow perceived colour steps; the representative colour is the
//             LINEAR-light mean of the pixels assigned to the winning bin (never the bin centre).
//   Vibrant   most prominent SATURATED cluster. Per pixel (encoded domain) s = (max-min)/max and
//             v = max. A pixel is eligible only when v >= minV and s >= minS (dark noise and near-grey
//             pixels cannot vote). Eligible pixels vote with weight w = s * v into the same 8x8x8 bins.
//             A bin qualifies only if it holds at least max(minPop, ceil(minPopFrac * N)) eligible
//             pixels, i.e. a vibrant feature must cover ~1% of the frame; isolated saturated pixels
//             cannot win. The winner is the qualifying bin with the largest total weight; the
//             representative is the weight-averaged linear-light colour of its pixels. No qualifying bin
//             => the vibrant measurement is explicitly INVALID (never a substitute colour).
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace predominant {

constexpr int kLevels = 8;
constexpr int kBins = kLevels * kLevels * kLevels; // 512

struct Rgb {
	float r = 0.0f, g = 0.0f, b = 0.0f; // linear light
};

struct Tuning {
	float vibMinV = 0.15f;        // encoded value floor
	float vibMinS = 0.20f;        // encoded saturation floor
	float vibMinPopFrac = 0.01f;  // qualifying cluster covers >= 1% of the pixels ...
	uint32_t vibMinPop = 2;       // ... and never fewer than this many
	float hystMargin = 1.3f;      // challenger must beat the held bin's population by 30%
	float hystFloorFrac = 0.005f; // a held bin below 0.5% of the pixels is released
	float emaAlpha = 0.4f;        // "modest" smoothing of the representative colour
};

// sRGB OETF (linear -> encoded) through a 4096-entry table; accuracy is ample for 8-level binning.
inline float encode(float lin) {
	static const std::array<float, 4096> lut = [] {
		std::array<float, 4096> t{};
		for (int i = 0; i < 4096; ++i) {
			const float c = static_cast<float>(i) / 4095.0f;
			t[static_cast<std::size_t>(i)] = c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
		}
		return t;
	}();
	const float c = lin < 0.0f ? 0.0f : (lin > 1.0f ? 1.0f : lin);
	return lut[static_cast<std::size_t>(c * 4095.0f + 0.5f)];
}

inline int levelOf(float e) {
	const int l = static_cast<int>(e * kLevels);
	return l < 0 ? 0 : (l >= kLevels ? kLevels - 1 : l);
}
inline int binOf(float er, float eg, float eb) { return (levelOf(er) * kLevels + levelOf(eg)) * kLevels + levelOf(eb); }

struct Cluster {
	bool valid = false;
	Rgb rgb;              // representative colour, linear light
	int bin = -1;
	uint32_t population = 0; // pixels assigned to the bin (dominant) / eligible pixels in the bin (vibrant)
	float share = 0.0f;      // population / N
	float score = 0.0f;      // vibrant: summed weight; dominant: == population
};

// Per-bin accumulators, kept by the caller so hysteresis can query the held bin's CURRENT population.
struct Histogram {
	std::array<uint32_t, kBins> count{};
	std::array<double, kBins> sr{}, sg{}, sb{};            // linear sums of the assigned pixels
	std::array<uint32_t, kBins> vcount{};                  // eligible (vibrant) pixels
	std::array<double, kBins> vw{}, vr{}, vg{}, vb{};      // weight sum and weighted linear sums
	uint32_t n = 0;
	Rgb average;
	void clear() {
		count.fill(0); sr.fill(0); sg.fill(0); sb.fill(0);
		vcount.fill(0); vw.fill(0); vr.fill(0); vg.fill(0); vb.fill(0);
		n = 0; average = {};
	}
};

// `px` is `n` pixels of `stride` floats (R,G,B,...), linear light.
inline void accumulate(const float* px, std::size_t n, int stride, const Tuning& tu, Histogram& h) {
	h.clear();
	h.n = static_cast<uint32_t>(n);
	double ar = 0, ag = 0, ab = 0;
	for (std::size_t i = 0; i < n; ++i) {
		const float lr = px[i * stride], lg = px[i * stride + 1], lb = px[i * stride + 2];
		const float er = encode(lr), eg = encode(lg), eb = encode(lb);
		const int bin = binOf(er, eg, eb);
		h.count[bin]++; h.sr[bin] += lr; h.sg[bin] += lg; h.sb[bin] += lb;
		ar += lr; ag += lg; ab += lb;
		const float vmax = er > eg ? (er > eb ? er : eb) : (eg > eb ? eg : eb);
		const float vmin = er < eg ? (er < eb ? er : eb) : (eg < eb ? eg : eb);
		if (vmax >= tu.vibMinV && vmax > 0.0f) {
			const float s = (vmax - vmin) / vmax;
			if (s >= tu.vibMinS) {
				const double w = static_cast<double>(s) * vmax;
				h.vcount[bin]++; h.vw[bin] += w; h.vr[bin] += w * lr; h.vg[bin] += w * lg; h.vb[bin] += w * lb;
			}
		}
	}
	if (n > 0) { const double k = 1.0 / static_cast<double>(n); h.average = {static_cast<float>(ar * k), static_cast<float>(ag * k), static_cast<float>(ab * k)}; }
}

inline Cluster dominantOfBin(const Histogram& h, int bin) {
	Cluster c;
	if (bin < 0 || bin >= kBins || h.count[bin] == 0) return c;
	const double k = 1.0 / static_cast<double>(h.count[bin]);
	c.valid = true; c.bin = bin; c.population = h.count[bin]; c.score = static_cast<float>(h.count[bin]);
	c.share = h.n ? static_cast<float>(h.count[bin]) / static_cast<float>(h.n) : 0.0f;
	c.rgb = {static_cast<float>(h.sr[bin] * k), static_cast<float>(h.sg[bin] * k), static_cast<float>(h.sb[bin] * k)};
	return c;
}

// Most populated bin. Ties resolve to the lowest bin index (deterministic).
inline Cluster dominant(const Histogram& h) {
	int best = -1; uint32_t bc = 0;
	for (int b = 0; b < kBins; ++b) if (h.count[b] > bc) { bc = h.count[b]; best = b; }
	return dominantOfBin(h, best);
}

inline uint32_t vibrantMinPopulation(uint32_t n, const Tuning& tu) {
	const uint32_t byFrac = static_cast<uint32_t>(std::ceil(tu.vibMinPopFrac * static_cast<float>(n)));
	return byFrac > tu.vibMinPop ? byFrac : tu.vibMinPop;
}

inline Cluster vibrant(const Histogram& h, const Tuning& tu) {
	const uint32_t minPop = vibrantMinPopulation(h.n, tu);
	int best = -1; double bs = 0.0;
	for (int b = 0; b < kBins; ++b) if (h.vcount[b] >= minPop && h.vw[b] > bs) { bs = h.vw[b]; best = b; }
	Cluster c;
	if (best < 0) return c;
	c.valid = true; c.bin = best; c.population = h.vcount[best]; c.score = static_cast<float>(bs);
	c.share = h.n ? static_cast<float>(h.vcount[best]) / static_cast<float>(h.n) : 0.0f;
	c.rgb = {static_cast<float>(h.vr[best] / bs), static_cast<float>(h.vg[best] / bs), static_cast<float>(h.vb[best] / bs)};
	return c;
}

// Vibrant cluster for a specific bin (invalid unless that bin currently qualifies).
inline Cluster vibrantOfBin(const Histogram& h, int bin, const Tuning& tu) {
	Cluster c;
	if (bin < 0 || bin >= kBins) return c;
	if (h.vcount[bin] < vibrantMinPopulation(h.n, tu) || h.vw[bin] <= 0.0) return c;
	c.valid = true; c.bin = bin; c.population = h.vcount[bin]; c.score = static_cast<float>(h.vw[bin]);
	c.share = h.n ? static_cast<float>(h.vcount[bin]) / static_cast<float>(h.n) : 0.0f;
	c.rgb = {static_cast<float>(h.vr[bin] / h.vw[bin]), static_cast<float>(h.vg[bin] / h.vw[bin]), static_cast<float>(h.vb[bin] / h.vw[bin])};
	return c;
}

// Vibrant winner-bin hysteresis: keep `heldBin` while it still qualifies unless a challenger's summed
// weight exceeds the held bin's CURRENT weight by hystMargin. Returns -1 when NO bin qualifies: the
// vibrant measurement is then explicitly invalid and the caller must drop its held state (nothing is
// ever carried across an invalid observation).
inline int vibrantWithHysteresis(const Histogram& h, int heldBin, const Tuning& tu) {
	const Cluster top = vibrant(h, tu);
	if (!top.valid) return -1;
	const Cluster held = vibrantOfBin(h, heldBin, tu);
	if (!held.valid) return top.bin;
	if (top.bin == heldBin) return heldBin;
	return top.score > held.score * tu.hystMargin ? top.bin : heldBin;
}

// Cluster-winner hysteresis: keep `heldBin` unless a challenger is clearly more populated, or the
// held bin has all but vanished. Returns the bin to report (-1 when there is no valid dominant).
inline int dominantWithHysteresis(const Histogram& h, int heldBin, const Tuning& tu) {
	const Cluster top = dominant(h);
	if (!top.valid) return -1;
	if (heldBin < 0 || heldBin >= kBins) return top.bin;
	const float floorPop = tu.hystFloorFrac * static_cast<float>(h.n);
	const float heldPop = static_cast<float>(h.count[heldBin]);
	if (heldPop <= floorPop) return top.bin;
	if (top.bin == heldBin) return heldBin;
	return static_cast<float>(top.population) > heldPop * tu.hystMargin ? top.bin : heldBin;
}

// EMA of a representative colour in linear light; `have` false (first sample / after a gap or scene
// change) starts a fresh state, so missing data is never bridged by smoothing.
inline Rgb emaStep(const Rgb& prev, bool have, const Rgb& x, float alpha) {
	if (!have) return x;
	return {prev.r + alpha * (x.r - prev.r), prev.g + alpha * (x.g - prev.g), prev.b + alpha * (x.b - prev.b)};
}

} // namespace predominant
