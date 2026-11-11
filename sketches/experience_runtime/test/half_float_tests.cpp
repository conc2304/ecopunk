// OC-003 experimental RGBA16F readback: unit tests for the EXACT half -> float widening (OF-free) and for the
// claim that feeding the unchanged tracker a half-sourced buffer gives bit-identical AVG / DOM / VIB results.
// Build/run:  c++ -std=c++17 -Wall -Wextra -I../src half_float_tests.cpp -o half_float_tests && ./half_float_tests
#include "HalfFloat.h"
#include "PredominantColor.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int T = 0, F = 0;
#define CHECK(c, msg) do { ++T; if (!(c)) { ++F; std::printf("FAIL: %s (%d)\n", msg, __LINE__); } } while (0)

// Independent reference (no bit tricks): sign * mantissa * 2^exponent via ldexp.
static float refWiden(uint16_t h) {
	const int s = (h & 0x8000) ? -1 : 1;
	const int e = (h >> 10) & 0x1F, m = h & 0x3FF;
	if (e == 0) return s * std::ldexp(static_cast<float>(m), -24);
	if (e == 31) return m ? std::nanf("") : s * INFINITY;
	return s * std::ldexp(static_cast<float>(1024 + m), e - 25);
}
static uint32_t bitsOf(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

// Nearest half (positive, 0..1) to x, by binary search over the monotonic reference.
static uint16_t nearestHalf(float x) {
	uint16_t lo = 0, hi = 0x3C00;
	while (lo + 1 < hi) { const uint16_t mid = static_cast<uint16_t>((lo + hi) / 2); (refWiden(mid) <= x ? lo : hi) = mid; }
	return (x - refWiden(lo) <= refWiden(hi) - x) ? lo : hi;
}

struct Lcg { uint32_t s = 12345; uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; } float unit() { return static_cast<float>(next() & 0xFFFF) / 65535.0f; } };

// A 160x90 RGBA "frame" of half values: a few colour blobs + noise, like a real reduced scene.
static std::vector<uint16_t> makeFrame(Lcg& r, int variant) {
	std::vector<uint16_t> px(160 * 90 * 4);
	const float pal[4][3] = { {0.70f, 0.05f, 0.02f}, {0.02f, 0.40f, 0.10f}, {0.05f, 0.05f, 0.60f}, {0.30f, 0.30f, 0.30f} };
	for (int y = 0; y < 90; ++y)
		for (int x = 0; x < 160; ++x) {
			const int b = variant == 0 ? 3 : ((x / 40) + (y / 45)) % 4;           // variant 0: grey only -> vibrant invalid
			for (int c = 0; c < 3; ++c) {
				float v = pal[b][c] * (0.9f + 0.2f * r.unit());
				if (variant == 2 && (x + y) % 5 == 0) v = 0.0f;                       // exact zeros (+0)
				px[static_cast<size_t>((y * 160 + x) * 4 + c)] = nearestHalf(v > 1.0f ? 1.0f : v);
			}
			px[static_cast<size_t>((y * 160 + x) * 4 + 3)] = 0x3C00; // alpha = 1.0, as the reduction shader writes
		}
	return px;
}

int main() {
	// 1. Exhaustive: all 65536 binary16 patterns widen exactly like the independent reference.
	{
		uint32_t bad = 0, nanCount = 0, infCount = 0, subCount = 0;
		for (uint32_t u = 0; u < 65536; ++u) {
			const uint16_t h = static_cast<uint16_t>(u);
			const float a = halffloat::toFloat(h), b = refWiden(h);
			if (std::isnan(b)) { ++nanCount; if (!std::isnan(a)) ++bad; continue; }
			if (std::isinf(b)) ++infCount;
			if (((h >> 10) & 0x1F) == 0 && (h & 0x3FF)) ++subCount;
			if (bitsOf(a) != bitsOf(b)) ++bad;
		}
		CHECK(bad == 0, "all 65536 half patterns widen bit-exactly (NaN stays NaN)");
		CHECK(nanCount == 2046 && infCount == 2 && subCount == 2046, "NaN / Inf / subnormal counts as expected");
		CHECK(bitsOf(halffloat::toFloat(0x8000)) == 0x80000000u, "-0 keeps its sign");
		CHECK(halffloat::toFloat(0x3C00) == 1.0f && halffloat::toFloat(0x0001) == std::ldexp(1.0f, -24), "1.0 and the smallest subnormal");
	}

	// 2. widen() == element-wise toFloat on a buffer.
	{
		std::vector<uint16_t> h(4096);
		for (size_t i = 0; i < h.size(); ++i) h[i] = static_cast<uint16_t>(i * 16 + 3);
		std::vector<float> f(h.size());
		halffloat::widen(h.data(), f.data(), h.size());
		bool same = true;
		for (size_t i = 0; i < h.size(); ++i) {
			const float ref = refWiden(h[i]);
			same = same && (std::isnan(ref) ? std::isnan(f[i]) : bitsOf(f[i]) == bitsOf(ref)); // NaN payloads are not compared
		}
		CHECK(same, "bulk widen matches the reference");
	}

	// 3. Tracker + AVG on a half-sourced buffer are bit-identical to the reference-widened buffer, over a frame sequence
	//    (hysteresis + EMA state included), for scenes with valid DOM/VIB, with VIB invalid, and with exact zeros.
	{
		Lcg r;
		predominant::Tracker ta, tb;
		int identical = 0, frames = 0, vibInvalid = 0, vibValid = 0;
		for (int i = 0; i < 18; ++i) {
			const int variant = (i % 6 == 2) ? 0 : (i % 6 == 4 ? 2 : 1);
			const std::vector<uint16_t> half = makeFrame(r, variant);
			std::vector<float> a(half.size()), b(half.size());
			for (size_t k = 0; k < half.size(); ++k) a[k] = refWiden(half[k]);
			halffloat::widen(half.data(), b.data(), half.size());
			const size_t n = 160 * 90;
			double ar = 0, br = 0;
			for (size_t k = 0; k < n; ++k) { ar += a[k * 4]; br += b[k * 4]; }
			const predominant::Result ra = ta.step(a.data(), n, 4), rb = tb.step(b.data(), n, 4);
			const bool same = bitsOf(static_cast<float>(ar / n)) == bitsOf(static_cast<float>(br / n))
				&& ra.dom.valid == rb.dom.valid && ra.vib.valid == rb.vib.valid
				&& bitsOf(ra.dom.rgb.r) == bitsOf(rb.dom.rgb.r) && bitsOf(ra.dom.rgb.g) == bitsOf(rb.dom.rgb.g) && bitsOf(ra.dom.rgb.b) == bitsOf(rb.dom.rgb.b)
				&& bitsOf(ra.vib.rgb.r) == bitsOf(rb.vib.rgb.r) && bitsOf(ra.vib.rgb.g) == bitsOf(rb.vib.rgb.g) && bitsOf(ra.vib.rgb.b) == bitsOf(rb.vib.rgb.b)
				&& ta.histogram().count == tb.histogram().count && ta.histogram().vcount == tb.histogram().vcount;
			identical += same; ++frames;
			(rb.vib.valid ? vibValid : vibInvalid)++;
		}
		CHECK(identical == frames, "AVG, DOM, VIB and histograms are bit-identical for every frame of the sequence");
		CHECK(vibInvalid > 0 && vibValid > 0, "the sequence exercises both valid and invalid vibrant outcomes");
	}

	// 4. A non-finite half (Inf / NaN) is still non-finite after widening, so the existing finite check invalidates the sample.
	{
		const uint16_t h[3] = { 0x7C00, 0xFC00, 0x7E00 };
		float f[3];
		halffloat::widen(h, f, 3);
		CHECK(!std::isfinite(f[0]) && !std::isfinite(f[1]) && !std::isfinite(f[2]), "Inf / -Inf / NaN stay non-finite");
	}

	std::printf("%d/%d checks passed\n", T - F, T);
	return F == 0 ? 0 : 1;
}
