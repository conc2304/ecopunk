#pragma once

// PhosphorContoursParams — OF-free, GL-free parameter/preset model and CPU mirrors of the shader math that is
// worth unit-testing (band quantisation, hash, dropout, time-correct decay). Header-only so
// experience_runtime/test can build it with a bare compiler. The shaders in PhosphorContoursShaders.h are the
// rendering authority; the mirrors here exist to pin the intended math (keep them in sync when editing).

#include "glm/vec3.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace phc {

enum class Quality { Desktop = 0, PiSafe = 1 };
enum class View { Final = 0, Source, Luma, Contour, Raster };
// Structure generator. Isophote = quantised structural-luma iso-lines; RasterRelief = regularly spaced scanlines
// displaced by the structural luma (an implicit curve family, not warped isophotes); Hybrid = both, mixed before
// the phosphor mask so the raster/dropout/glow stages run exactly once.
enum class Algorithm { Isophote = 0, RasterRelief = 1, Hybrid = 2 };
constexpr int kAlgorithmCount = 3;
inline const char* algorithmName(Algorithm a) {
	return a == Algorithm::Isophote ? "Isophote" : (a == Algorithm::RasterRelief ? "RasterRelief" : "Hybrid");
}

// All "px" values are authored at the 1280x720 reference and scaled by H/720 (see Pipeline::process).
struct Params {
	const char* name = "";
	Algorithm algorithm = Algorithm::Hybrid;
	// ---- source structure: geometry is defined by a blurred, downsampled luminance, never by raw fine detail
	float structBlur = 12.f;                  // gaussian sigma of the structural luma, px
	bool autoRange = true;                    // slow (~1 s) clip-adaptive tonal range from percentiles; off = tonalLo/Hi below
	float tonalLo = 0.f, tonalHi = 1.f;       // manual global tonal range (black/white point) of the structural luma
	float contrast = 1.f, gamma = 1.f;        // about 0.5 / power
	float detail = 0.18f;                     // fine-detail (raw luma - structural luma) modulation of emission, 0..1
	// ---- isophotes
	float levels = 16.f, lineWidth = 2.0f, softness = 0.7f, gradientEmphasis = 0.5f, lumaFloor = 0.5f; // width/softness in px
	// ---- raster relief (displaced scanlines)
	float reliefLines = 40.f;                 // base lines across the image height (vertical: width) at 720p
	float reliefAmp = 100.f;                   // max displacement px (signed: negative inverts)
	float reliefPivot = 0.45f;                 // structural luma that produces zero displacement
	float reliefWidth = 2.2f;                 // px
	float reliefDrift = 5.f;                  // px/s slow scroll of the line family (smooth, never random)
	bool reliefVertical = false;              // baseline orientation
	// ---- hybrid
	float hybridIso = 0.35f;                  // isophote weight (relief gets 1 - this)
	// ---- raster (phosphor mask)
	float cellW = 3.f, cellH = 3.f, dotRadius = 0.5f, stagger = 1.f, jitter = 0.18f;
	float cellSnap = 0.6f;                    // 0 = sample the line per pixel, 1 = light the whole cell if the line crosses it
	float dropout = 0.06f, flickerHz = 1.2f, fill = 0.35f, toneFill = 0.2f;
	// ---- analogue drift
	float warpX = 6.f, warpY = 2.f, warpRate = 0.22f, warpScale = 1.f; // isophote sample-coordinate warp
	float rowAmp = 3.f, rowRate = 0.35f, slip = 2.f;
	// ---- emission
	glm::vec3 tint = {0.93f, 1.0f, 0.04f};
	glm::vec3 background = {0.030f, 0.036f, 0.016f};
	float gain = 1.7f, persistenceTau = 0.08f, bloomThresh = 0.55f, bloomAmount = 0.7f, bloomRadius = 1.4f;
	float grain = 0.025f, vignette = 0.5f;
};

// Clamp every parameter to its supported range (developer GUI, env overrides and presets all pass through this,
// so a bad value can never reach a shader as a division by zero or an unbounded loop).
inline void clampParams(Params& p) {
	auto cl = [](float& v, float lo, float hi) { v = std::min(hi, std::max(lo, v)); };
	cl(p.structBlur, 0.f, 48.f); cl(p.tonalLo, 0.f, 0.9f); cl(p.tonalHi, p.tonalLo + 0.05f, 1.f);
	cl(p.contrast, 0.25f, 4.f); cl(p.gamma, 0.3f, 3.f); cl(p.detail, 0.f, 1.f);
	cl(p.levels, 2.f, 80.f); cl(p.lineWidth, 0.5f, 12.f); cl(p.softness, 0.25f, 3.f);
	cl(p.gradientEmphasis, 0.f, 1.f); cl(p.lumaFloor, 0.f, 1.f);
	cl(p.reliefLines, 4.f, 200.f); cl(p.reliefAmp, -200.f, 200.f); cl(p.reliefPivot, 0.f, 1.f);
	cl(p.reliefWidth, 0.5f, 12.f); cl(p.reliefDrift, -60.f, 60.f);
	cl(p.hybridIso, 0.f, 1.f);
	cl(p.cellW, 2.f, 16.f); cl(p.cellH, 2.f, 12.f); cl(p.dotRadius, 0.2f, 0.7f); cl(p.cellSnap, 0.f, 1.f);
	cl(p.dropout, 0.f, 0.8f); cl(p.flickerHz, 0.f, 8.f); cl(p.fill, 0.f, 1.f); cl(p.toneFill, 0.f, 1.f);
	cl(p.warpX, 0.f, 60.f); cl(p.warpY, 0.f, 20.f); cl(p.rowAmp, 0.f, 60.f); cl(p.slip, 0.f, 100.f);
	cl(p.persistenceTau, 0.f, 0.8f); cl(p.bloomAmount, 0.f, 3.f); cl(p.bloomRadius, 0.5f, 6.f);
	cl(p.gain, 0.2f, 4.f); cl(p.grain, 0.f, 0.3f);
}

constexpr int kPresetCount = 4;
inline std::array<Params, kPresetCount> makePresets() {
	std::array<Params, kPresetCount> p;
	// ReferenceDense: flowing, mostly-continuous luminous topology. Hybrid structure (relief 75 / isophote 25),
	// tight phosphor cells, very light breakup so the geometry stays legible.
	p[0].name = "ReferenceDense";
	// SparseSignal: fewer isophotes, heavy dropout and wide cells - broken signal on a dark ground.
	p[1] = p[0];
	p[1].name = "SparseSignal"; p[1].algorithm = Algorithm::Isophote;
	p[1].levels = 11.f; p[1].lineWidth = 1.6f; p[1].cellW = 5.f; p[1].cellH = 3.f; p[1].dotRadius = 0.46f;
	p[1].dropout = 0.42f; p[1].flickerHz = 1.8f; p[1].fill = 0.2f; p[1].lumaFloor = 0.15f; p[1].cellSnap = 0.3f;
	p[1].background = {0.016f, 0.020f, 0.008f}; p[1].bloomAmount = 0.6f; p[1].grain = 0.04f;
	p[1].warpX = 8.f; p[1].rowAmp = 3.f; p[1].persistenceTau = 0.12f;
	// MeltedRaster: bent scanlines with big analogue row drift and slip.
	p[2] = p[0];
	p[2].name = "MeltedRaster"; p[2].algorithm = Algorithm::RasterRelief;
	p[2].reliefLines = 52.f; p[2].reliefAmp = 70.f; p[2].reliefDrift = 9.f;
	p[2].rowAmp = 16.f; p[2].rowRate = 0.6f; p[2].slip = 26.f; p[2].dropout = 0.18f;
	p[2].bloomAmount = 1.0f; p[2].persistenceTau = 0.12f;
	// Strata: clean, almost topographic isophote bands - many levels, wide soft lines, minimal breakup and slip.
	p[3] = p[0];
	p[3].name = "Strata"; p[3].algorithm = Algorithm::Isophote;
	p[3].levels = 28.f; p[3].lineWidth = 2.6f; p[3].softness = 0.9f; p[3].lumaFloor = 0.3f; p[3].cellSnap = 0.8f;
	p[3].dropout = 0.02f; p[3].fill = 0.45f; p[3].toneFill = 0.1f; p[3].warpX = 3.f; p[3].rowAmp = 1.5f; p[3].slip = 0.5f;
	p[3].persistenceTau = 0.05f; p[3].bloomAmount = 0.5f; p[3].grain = 0.012f;
	for (auto& q : p) clampParams(q);
	return p;
}

// Visitor-facing mode ids / labels (the runtime scene's NextMode axis). Index == preset index.
inline const char* modeId(int i) {
	static const char* ids[kPresetCount] = {"topology", "isolines", "scanlines", "strata"};
	return ids[((i % kPresetCount) + kPresetCount) % kPresetCount];
}
inline const char* modeLabel(int i) {
	static const char* l[kPresetCount] = {"TOPOLOGY", "ISOLINES", "SCANLINES", "STRATA"};
	return l[((i % kPresetCount) + kPresetCount) % kPresetCount];
}

// ---- scene-level selector axes (runtime): two coarse 3-step controls layered on the active mode.
// Step 1 (Medium / Trail) is the identity: the mode's own preset values are untouched.
constexpr int kSelectorSteps = 3;
inline const char* densityLabel(int s) { static const char* l[kSelectorSteps] = {"SPARSE", "MEDIUM", "DENSE"}; return l[((s % kSelectorSteps) + kSelectorSteps) % kSelectorSteps]; }
inline const char* densityId(int s) { static const char* l[kSelectorSteps] = {"sparse", "medium", "dense"}; return l[((s % kSelectorSteps) + kSelectorSteps) % kSelectorSteps]; }
inline const char* decayLabel(int s) { static const char* l[kSelectorSteps] = {"CRISP", "TRAIL", "SMEAR"}; return l[((s % kSelectorSteps) + kSelectorSteps) % kSelectorSteps]; }
inline const char* decayId(int s) { static const char* l[kSelectorSteps] = {"crisp", "trail", "smear"}; return l[((s % kSelectorSteps) + kSelectorSteps) % kSelectorSteps]; }

// DENSITY: scales contour line count only (isophote `levels` and relief `reliefLines`; Hybrid scales both).
inline void applyDensity(Params& p, int step) {
	static const float k[kSelectorSteps] = {0.6f, 1.f, 1.5f};
	const float m = k[((step % kSelectorSteps) + kSelectorSteps) % kSelectorSteps];
	p.levels *= m; p.reliefLines *= m;
}
// DECAY: one move for time smear + breakup (persistence, dropout, row slip/drift, glow).
inline void applyDecay(Params& p, int step) {
	const int s = ((step % kSelectorSteps) + kSelectorSteps) % kSelectorSteps;
	if (s == 1) return;
	const bool smear = s == 2;
	p.persistenceTau *= smear ? 2.f : 0.25f;
	p.dropout = smear ? p.dropout * 1.8f + 0.04f : p.dropout * 0.5f;
	p.slip *= smear ? 2.5f : 0.3f;
	p.rowAmp *= smear ? 2.5f : 0.5f;
	p.bloomAmount *= smear ? 1.4f : 0.8f;
}
// The look the scene renders: mode preset + both selector axes, clamped.
inline Params resolveLook(int mode, int density, int decay) {
	Params p = makePresets()[((mode % kPresetCount) + kPresetCount) % kPresetCount];
	applyDensity(p, density); applyDecay(p, decay);
	clampParams(p);
	return p;
}

inline std::string presetName(int i) { return makePresets()[((i % kPresetCount) + kPresetCount) % kPresetCount].name; }

// ------------------------------------------------------------------ CPU mirrors of the shader math
// GLSL fract(): x - floor(x)  (always in [0,1))
inline float fract1(float x) { return x - std::floor(x); }

// Global luminance quantisation: distance (in band-phase units, 0..0.5) from luma*levels to the nearest integer
// threshold. Depends ONLY on luma and levels - never on position - so contours are true, stable isophotes.
inline float bandDistance(float luma, float levels) {
	const float v = luma * levels;
	const float fr = fract1(v);
	return std::min(fr, 1.f - fr);
}

// cheap deterministic hash (mirrors hash21 in the shaders); result in [0,1)
inline float hash21(float px, float py) {
	float ax = fract1(px * 123.34f), ay = fract1(py * 456.21f);
	const float d = ax * (ax + 45.32f) + ay * (ay + 45.32f);
	ax += d; ay += d;
	return fract1(ax * ay);
}

// Seeded, luma-dependent dropout for one cell on its own slow event clock `key`. true = the phosphor is lit.
inline bool cellAlive(float cellX, float cellY, float key, float seed, float dropout, float luma) {
	const float h = hash21(cellX * 1.31f + key * 1.7f + seed, cellY * 1.31f + key * 3.1f + seed);
	const float t = luma < 0.35f ? 0.f : (luma > 0.95f ? 1.f : (luma - 0.35f) / 0.6f);
	const float thr = dropout * (1.f - 0.8f * t); // highlights keep coverage
	return h >= thr + 0.04f;                      // midpoint of the shader's smoothstep(thr, thr+0.08)
}

// Global tonal range + contrast + gamma applied to the structural luma (mirrors `remap` in the structure shader).
inline float tonalRemap(float l, float lo, float hi, float contrast, float gamma) {
	float x = std::min(1.f, std::max(0.f, (l - lo) / std::max(hi - lo, 1e-3f)));
	x = std::min(1.f, std::max(0.f, (x - 0.5f) * contrast + 0.5f));
	return std::pow(std::max(x, 1e-4f), gamma);
}
// Raster relief: a base scanline at yBase is displaced across the line by amp * (S - pivot). Mirrors the shader.
inline float reliefDisplacement(float structLuma, float pivot, float ampPx) { return ampPx * (structLuma - pivot); }
// Spacing in px of the base scanline family at height H (reference 720p scales linearly).
inline float reliefSpacingPx(float lines, float heightPx) { return heightPx / std::max(lines, 1.f); }
// Distance (px) from a position `across` to the nearest displaced line, for a displacement field that is
// constant across the neighbourhood (analytic check of the implicit-curve distance used by the shader).
inline float reliefLineDistance(float across, float displacementPx, float spacingPx, float phasePx = 0.f) {
	const float yb = across - displacementPx;
	const float u = (yb - phasePx) / spacingPx;
	const float fr = fract1(u);
	return std::min(fr, 1.f - fr) * spacingPx;
}

// Auto tonal range from the 3rd/97th percentile of the structural luma, padded by 10% of the span at each end so
// the brightest masses do not saturate into solid plateaus. The span is at least `minSpan` (a flat or very dark
// clip is lifted but never amplified into noise) and at most 1. Returns lo/hi in [0,1].
inline void autoRangeFromPercentiles(float pLow, float pHigh, float& lo, float& hi, float minSpan = 0.2f) {
	const float core = std::max(pHigh - pLow, minSpan);
	const float span = std::min(core * 1.2f, 1.f);
	const float mid = 0.5f * (pLow + pHigh);
	lo = std::min(std::max(mid - 0.5f * span, 0.f), 1.f - span);
	hi = lo + span;
}
// q in [0,1]; v is sorted ascending, non-empty.
inline float percentileSorted(const std::vector<float>& v, float q) {
	const float f = std::min(std::max(q, 0.f), 1.f) * float(v.size() - 1);
	const size_t i = size_t(f);
	const size_t j = std::min(i + 1, v.size() - 1);
	return v[i] + (v[j] - v[i]) * (f - float(i));
}

// Time-correct persistence decay: depends on elapsed time, not frame count (decay(a)*decay(b) == decay(a+b)).
inline float persistenceDecay(float dt, float tau) { return tau > 0.f ? std::exp(-dt / tau) : 0.f; }

} // namespace phc
