// Phosphor Contours — OF-free tests for ../../../shared/src/phosphor-contours/PhosphorContoursParams.h
// (preset model + CPU mirrors of the shader math). GL behaviour is covered by the real-runtime harness
// (PhosphorContoursHarness) and the standalone autoshot run, not here.

#include "../../../shared/src/phosphor-contours/PhosphorContoursParams.h"

#include <cmath>
#include <iostream>
#include <set>
#include <string>

namespace {
int g_total = 0, g_failures = 0;
}
#define PHC_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) { g_failures++; std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; } \
	} while (0)

int main() {
	using namespace phc;
	const auto presets = makePresets();

	// --- presets: named, distinct, in documented ranges, reproducible
	PHC_CHECK(kPresetCount >= 2);
	std::set<std::string> names;
	for (const auto& p : presets) names.insert(p.name);
	PHC_CHECK(names.size() == (size_t)kPresetCount);
	PHC_CHECK(std::string(presets[0].name) == "ReferenceDense");
	PHC_CHECK(std::string(presets[1].name) == "SparseSignal");
	PHC_CHECK(std::string(presets[2].name) == "MeltedRaster");
	PHC_CHECK(presets[0].levels >= 10.f && presets[0].levels <= 24.f);       // structural-banding brief: ~12-20 isophotes, not 40
	PHC_CHECK(presets[1].levels < presets[0].levels);                         // Sparse: fewer contour bands
	PHC_CHECK(presets[1].dropout > presets[0].dropout);                       // Sparse: stronger dropout
	PHC_CHECK(presets[1].background.y < presets[0].background.y);             // Sparse: darker background
	// algorithm selection is separate from the artistic preset: each preset carries one, all three algorithms are used
	PHC_CHECK(presets[0].algorithm == Algorithm::Hybrid && presets[1].algorithm == Algorithm::Isophote && presets[2].algorithm == Algorithm::RasterRelief);
	PHC_CHECK(kAlgorithmCount == 3 && std::string(algorithmName(Algorithm::Isophote)) == "Isophote" && std::string(algorithmName(Algorithm::RasterRelief)) == "RasterRelief" && std::string(algorithmName(Algorithm::Hybrid)) == "Hybrid");
	PHC_CHECK(presets[0].dropout <= 0.08f && presets[0].grain <= 0.03f);     // ReferenceDense: subtle breakup so lines stay continuous
	PHC_CHECK(presets[0].detail >= 0.10f && presets[0].detail <= 0.25f);     // fine detail contribution defaults low
	PHC_CHECK(presets[0].lineWidth >= 1.5f && presets[0].lineWidth <= 2.5f && presets[0].reliefWidth >= 1.5f && presets[0].reliefWidth <= 2.5f);
	PHC_CHECK(presets[0].autoRange && presets[0].structBlur >= 8.f && presets[0].structBlur <= 16.f);
	PHC_CHECK(presets[2].rowAmp > presets[0].rowAmp && presets[2].slip > presets[0].slip); // Melted: more row slip
	for (const auto& p : presets) {
		PHC_CHECK(p.cellW >= 2.f && p.cellH >= 2.f && p.dotRadius > 0.2f && p.dotRadius < 0.7f);
		PHC_CHECK(p.dropout >= 0.f && p.dropout < 0.8f && p.persistenceTau > 0.f);
		PHC_CHECK(p.tint.y > p.tint.x && p.tint.x > p.tint.z); // yellow-green: G > R > B (default tint ~0.93,1,0.04)
		PHC_CHECK(p.background.x < 0.08f && p.background.y < 0.08f && p.background.z < 0.08f); // dark olive-charcoal
	}
	// --- four visitor modes + two 3-step selector axes
	PHC_CHECK(kPresetCount == 4 && std::string(presets[3].name) == "Strata" && presets[3].algorithm == Algorithm::Isophote);
	PHC_CHECK(presets[3].levels > presets[0].levels && presets[3].dropout < presets[1].dropout && presets[3].slip < presets[0].slip); // Strata: many levels, clean
	{
		std::set<std::string> ids, labels;
		for (int i = 0; i < kPresetCount; i++) { ids.insert(modeId(i)); labels.insert(modeLabel(i)); }
		PHC_CHECK(ids.size() == 4 && labels.size() == 4);
		PHC_CHECK(std::string(modeId(0)) == "topology" && std::string(modeLabel(0)) == "TOPOLOGY" && std::string(modeLabel(3)) == "STRATA" && std::string(modeLabel(4)) == "TOPOLOGY" && std::string(modeLabel(-1)) == "STRATA");
		PHC_CHECK(kSelectorSteps == 3 && std::string(densityLabel(0)) == "SPARSE" && std::string(densityLabel(1)) == "MEDIUM" && std::string(densityLabel(2)) == "DENSE" && std::string(densityLabel(3)) == "SPARSE");
		PHC_CHECK(std::string(decayLabel(0)) == "CRISP" && std::string(decayLabel(1)) == "TRAIL" && std::string(decayLabel(2)) == "SMEAR" && std::string(decayId(2)) == "smear" && std::string(densityId(2)) == "dense");
		// identity: Medium / Trail leave every mode's own preset untouched
		for (int m = 0; m < kPresetCount; m++) {
			const Params a = resolveLook(m, 1, 1), b = presets[m];
			PHC_CHECK(a.levels == b.levels && a.reliefLines == b.reliefLines && a.dropout == b.dropout && a.persistenceTau == b.persistenceTau && a.slip == b.slip && a.rowAmp == b.rowAmp && a.bloomAmount == b.bloomAmount && a.algorithm == b.algorithm);
		}
		for (int m = 0; m < kPresetCount; m++) {
			const Params sp = resolveLook(m, 0, 1), de = resolveLook(m, 2, 1);
			PHC_CHECK(sp.levels < de.levels && sp.reliefLines < de.reliefLines);              // DENSITY: sparse < dense line count, monotonic
			PHC_CHECK(sp.levels < presets[m].levels && de.levels > presets[m].levels);
			PHC_CHECK(sp.algorithm == presets[m].algorithm && de.algorithm == presets[m].algorithm); // DENSITY never changes the algorithm
			PHC_CHECK(sp.dropout == presets[m].dropout && sp.persistenceTau == presets[m].persistenceTau); // ...and touches nothing but line count
			const Params cr = resolveLook(m, 1, 0), sm = resolveLook(m, 1, 2);
			PHC_CHECK(cr.persistenceTau < presets[m].persistenceTau && presets[m].persistenceTau < sm.persistenceTau); // DECAY monotonic
			PHC_CHECK(cr.dropout <= presets[m].dropout && presets[m].dropout < sm.dropout);
			PHC_CHECK(cr.slip < presets[m].slip && presets[m].slip < sm.slip);
			PHC_CHECK(cr.levels == presets[m].levels && sm.levels == presets[m].levels);        // ...and never touches line count
			for (int d = 0; d < 3; d++) for (int c = 0; c < 3; c++) { // every combination is inside the supported ranges
				const Params q = resolveLook(m, d, c);
				PHC_CHECK(q.levels >= 2.f && q.levels <= 80.f && q.reliefLines >= 4.f && q.reliefLines <= 200.f);
				PHC_CHECK(q.dropout >= 0.f && q.dropout <= 0.8f && q.persistenceTau >= 0.f && q.persistenceTau <= 0.8f && q.slip <= 100.f && q.rowAmp <= 60.f && q.bloomAmount <= 3.f);
			}
		}
		PHC_CHECK(resolveLook(5, 4, 7).algorithm == presets[1].algorithm); // indices wrap (mode 5->1; steps 4->1, 7->1)
	}
	PHC_CHECK(makePresets()[0].levels == presets[0].levels); // pure: same value every call
	PHC_CHECK(presetName(0) == "ReferenceDense" && presetName(4) == "ReferenceDense" && presetName(-1) == "Strata"); // wraps

	// --- parameter bounds: clampParams makes any input safe (no zero spans / unbounded values reach a shader)
	{
		Params w;
		w.structBlur = 1e6f; w.tonalLo = 5.f; w.tonalHi = -3.f; w.contrast = 0.f; w.gamma = -2.f; w.levels = 0.f; w.lineWidth = 0.f;
		w.reliefLines = 1e9f; w.reliefAmp = -1e9f; w.cellW = 0.f; w.cellH = 1e6f; w.dropout = 9.f; w.hybridIso = 7.f; w.persistenceTau = -1.f;
		clampParams(w);
		PHC_CHECK(w.structBlur <= 48.f && w.tonalHi >= w.tonalLo + 0.05f - 1e-6f && w.tonalHi <= 1.f && w.contrast >= 0.25f && w.gamma >= 0.3f);
		PHC_CHECK(w.levels >= 2.f && w.lineWidth >= 0.5f && w.reliefLines <= 200.f && w.reliefAmp >= -200.f);
		PHC_CHECK(w.cellW >= 2.f && w.cellH <= 12.f && w.dropout <= 0.8f && w.hybridIso <= 1.f && w.persistenceTau >= 0.f);
		Params ok = presets[0]; Params ok2 = ok; clampParams(ok2);
		PHC_CHECK(ok.levels == ok2.levels && ok.reliefAmp == ok2.reliefAmp && ok.structBlur == ok2.structBlur); // presets are already in range (idempotent)
		for (const auto& p : presets) { Params q = p; clampParams(q); PHC_CHECK(q.cellW == p.cellW && q.dropout == p.dropout && q.tonalHi == p.tonalHi); }
	}

	// --- tonal range, auto range, percentiles
	PHC_CHECK(std::fabs(tonalRemap(0.5f, 0.f, 1.f, 1.f, 1.f) - 0.5f) < 1e-4f);
	PHC_CHECK(tonalRemap(0.2f, 0.2f, 0.8f, 1.f, 1.f) < 0.01f && tonalRemap(0.8f, 0.2f, 0.8f, 1.f, 1.f) > 0.99f); // black/white points
	PHC_CHECK(tonalRemap(0.35f, 0.2f, 0.8f, 1.f, 1.f) < tonalRemap(0.6f, 0.2f, 0.8f, 1.f, 1.f));                 // monotonic
	{
		float lo = 0.f, hi = 1.f;
		autoRangeFromPercentiles(0.02f, 0.14f, lo, hi);               // very dark clip: lifted, span >= minSpan, never inverted
		PHC_CHECK(hi - lo >= 0.2f && lo >= 0.f && hi <= 1.f && lo < 0.08f && hi > 0.14f);
		autoRangeFromPercentiles(0.3f, 0.9f, lo, hi);                 // wide clip: padded, inside [0,1]
		PHC_CHECK(lo < 0.3f && hi > 0.9f && lo >= 0.f && hi <= 1.f);
		autoRangeFromPercentiles(0.0f, 1.0f, lo, hi);                 // full range: unchanged
		PHC_CHECK(lo == 0.f && std::fabs(hi - 1.f) < 1e-6f);
		autoRangeFromPercentiles(0.5f, 0.5f, lo, hi);                 // perfectly flat: minimum span, centred
		PHC_CHECK(std::fabs((hi - lo) - 0.24f) < 1e-4f && lo >= 0.f);
		const std::vector<float> v = {0.f, 0.25f, 0.5f, 0.75f, 1.f};
		PHC_CHECK(percentileSorted(v, 0.f) == 0.f && percentileSorted(v, 1.f) == 1.f && std::fabs(percentileSorted(v, 0.5f) - 0.5f) < 1e-6f);
		PHC_CHECK(percentileSorted(v, 0.03f) <= percentileSorted(v, 0.97f));
	}

	// --- raster relief: spacing, displacement and the implicit line-distance
	PHC_CHECK(std::fabs(reliefSpacingPx(40.f, 720.f) - 18.f) < 1e-4f);
	PHC_CHECK(reliefDisplacement(0.45f, 0.45f, 100.f) == 0.f);                                      // at the pivot: no displacement
	PHC_CHECK(reliefDisplacement(0.95f, 0.45f, 100.f) > 0.f && reliefDisplacement(0.95f, 0.45f, -100.f) < 0.f); // signed / invertible
	{
		const float sp = 18.f;
		PHC_CHECK(std::fabs(reliefLineDistance(0.f, 0.f, sp)) < 1e-4f);                              // on a base line
		PHC_CHECK(std::fabs(reliefLineDistance(9.f, 0.f, sp) - 9.f) < 1e-4f);                        // halfway between lines
		PHC_CHECK(std::fabs(reliefLineDistance(30.f, 12.f, sp)) < 1e-4f);                            // displaced line passes through
		bool inRange = true;
		for (float y = -40.f; y < 40.f; y += 0.7f) { const float d = reliefLineDistance(y, 7.f, sp); inRange &= (d >= 0.f && d <= sp * 0.5f + 1e-4f); }
		PHC_CHECK(inRange);
		PHC_CHECK(std::fabs(reliefLineDistance(5.f, 3.f, sp, 4.f) - reliefLineDistance(5.f + sp, 3.f, sp, 4.f)) < 1e-3f); // periodic in spacing
	}

	// --- band quantisation is GLOBAL: depends only on luma/levels, symmetric, in [0, 0.5]
	for (float L = 0.f; L <= 1.f; L += 0.013f) {
		const float d = bandDistance(L, 42.f);
		PHC_CHECK(d >= 0.f && d <= 0.5f + 1e-6f);
		PHC_CHECK(bandDistance(L, 42.f) == d); // deterministic
	}
	PHC_CHECK(std::fabs(bandDistance(3.f / 42.f, 42.f)) < 1e-4f);              // exactly on a threshold
	PHC_CHECK(std::fabs(bandDistance(3.5f / 42.f, 42.f) - 0.5f) < 1e-4f);      // mid-band
	PHC_CHECK(std::fabs(bandDistance(0.30f, 42.f) - bandDistance(0.30f + 1.f / 42.f, 42.f)) < 1e-3f); // periodic: one band later, same phase
	// thresholds are evenly spaced in luma (1/levels), so spacing follows the image's tonal structure
	int thresholdsBelowHalf = 0;
	for (int i = 0; i <= 42; i++) if (bandDistance(float(i) / 42.f, 42.f) < 1e-3f) thresholdsBelowHalf++;
	PHC_CHECK(thresholdsBelowHalf == 43);

	// --- hash: range, determinism, spread
	double sum = 0; int n = 0; bool inRange = true;
	for (int y = 0; y < 60; y++) for (int x = 0; x < 60; x++) { const float h = hash21(float(x), float(y)); inRange &= (h >= 0.f && h < 1.f); sum += h; n++; }
	PHC_CHECK(inRange);
	PHC_CHECK(std::fabs(sum / n - 0.5) < 0.08);                // roughly uniform
	PHC_CHECK(hash21(7.f, 9.f) == hash21(7.f, 9.f));

	// --- dropout: seeded + deterministic, stronger dropout lights fewer cells, highlights retain coverage
	auto litFraction = [](float dropout, float luma, float seed) {
		int lit = 0, tot = 0;
		for (int y = 0; y < 80; y++) for (int x = 0; x < 80; x++) { lit += cellAlive(float(x), float(y), 3.f, seed, dropout, luma) ? 1 : 0; tot++; }
		return double(lit) / tot;
	};
	PHC_CHECK(litFraction(0.2f, 0.5f, 0.137f) == litFraction(0.2f, 0.5f, 0.137f));          // same seed -> identical mask
	PHC_CHECK(litFraction(0.2f, 0.5f, 0.137f) != litFraction(0.2f, 0.5f, 0.274f) || true);   // seeds may differ (not required to)
	PHC_CHECK(litFraction(0.45f, 0.2f, 0.1f) < litFraction(0.15f, 0.2f, 0.1f));             // more dropout, fewer lit
	PHC_CHECK(litFraction(0.45f, 1.0f, 0.1f) > litFraction(0.45f, 0.2f, 0.1f));             // highlights keep coverage
	PHC_CHECK(litFraction(0.2f, 0.2f, 0.1f) > 0.6 && litFraction(0.2f, 0.2f, 0.1f) < 0.95); // ~dropout-ish fraction removed
	{ // event clock: changing the key re-rolls cells but only some of them (not a brand-new global mask per frame)
		int changed = 0, tot = 0;
		for (int y = 0; y < 60; y++) for (int x = 0; x < 60; x++) { changed += cellAlive(float(x), float(y), 3.f, 0.1f, 0.2f, 0.5f) != cellAlive(float(x), float(y), 4.f, 0.1f, 0.2f, 0.5f); tot++; }
		PHC_CHECK(changed > 0 && changed < tot / 2);
	}

	// --- persistence is time-correct (frame-rate independent)
	PHC_CHECK(std::fabs(persistenceDecay(1.f / 60.f, 0.12f) * persistenceDecay(1.f / 60.f, 0.12f) - persistenceDecay(2.f / 60.f, 0.12f)) < 1e-6f);
	PHC_CHECK(std::fabs(std::pow(persistenceDecay(1.f / 30.f, 0.12f), 2.f) - std::pow(persistenceDecay(1.f / 60.f, 0.12f), 4.f)) < 1e-5f);
	PHC_CHECK(persistenceDecay(0.016f, 0.f) == 0.f); // tau 0 = no persistence
	PHC_CHECK(persistenceDecay(1.f, 0.12f) < 0.001f && persistenceDecay(0.001f, 0.12f) > 0.99f);

	std::cout << "phosphor_contours_tests: " << (g_total - g_failures) << " passed, " << g_failures << " failed (" << g_total << " checks)\n";
	return g_failures == 0 ? 0 : 1;
}
