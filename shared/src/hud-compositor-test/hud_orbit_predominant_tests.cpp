// OC-003: DOM / VIB through the Orbit presentation core (OF-free): snapshot adapter semantics, independent
// availability/validity per (source, metric), no cross-slot leakage, six-combination binding.
#include "../hud-compositor/OrbitConsoleModel.h"
#include "../hud-compositor/OrbitObservationConfig.h"
#include "../hud-compositor/OrbitSnapshotAdapter.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
using namespace hudpresent;
static int T = 0, F = 0;
#define CHECK(c, w) do { T++; if (!(c)) { F++; std::cerr << "FAIL: " << (w) << " (" << __LINE__ << ")\n"; } } while (0)
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

using Snap = scenecolor::SceneColorSnapshot;
static std::unique_ptr<Snap> mk(uint64_t rev, uint64_t gen, scenecolor::State st, bool provided) {
	auto s = std::make_unique<Snap>();
	s->revision = rev; s->generation = gen; s->state = st; s->predominantProvided = provided;
	return s;
}
static void add(Snap& s, double t, float avg, bool dom, float domV, bool vib, float vibV) {
	auto& x = s.history[s.count++];
	x.t = t; x.valid = true; x.r = x.g = x.b = avg; x.luma = avg;
	x.domValid = dom; x.domR = x.domG = x.domB = domV;
	x.vibValid = vib; x.vibR = vibV; x.vibG = 0.0f; x.vibB = 0.0f;
}
static void addGap(Snap& s, double t) { auto& x = s.history[s.count++]; x.t = t; x.valid = false; }

int main() {
	{ // adapter: per-metric validity, encoding, luminance
		auto s = mk(1, 1, scenecolor::State::Valid, true);
		add(*s, 0.0, 0.2f, true, 0.1f, false, 0.0f);
		add(*s, 0.5, 0.2f, false, 0.0f, true, 0.5f);
		OrbitSample o[kOrbitMaxSamples];
		CHECK(orbitFillSamples(*s, OrbitMetric::Dom, o) == 2 && o[0].valid && !o[1].valid, "DOM valid only where domValid");
		CHECK(orbitFillSamples(*s, OrbitMetric::Vib, o) == 2 && !o[0].valid && o[1].valid, "VIB valid only where vibValid");
		CHECK(near(o[1].r, srgbEncode(0.5f)) && o[1].g == 0.0f, "VIB colour is the sRGB-encoded VIB rgb, not AVG");
		CHECK(near(o[1].luma, 0.2126f * 0.5f), "VIB luminance is that of the VIB colour (linear)");
		orbitFillSamples(*s, OrbitMetric::Dom, o);
		CHECK(near(o[0].r, srgbEncode(0.1f)) && near(o[0].luma, 0.1f), "DOM colour/luma from DOM rgb, not AVG 0.2");
		CHECK(orbitFillSamples(*s, OrbitMetric::Avg, o) == 2 && o[0].valid && o[1].valid && near(o[0].r, srgbEncode(0.2f)), "AVG unchanged and independent of DOM/VIB validity");
	}
	{ // adapter: states
		auto s = mk(1, 1, scenecolor::State::Valid, true);
		add(*s, 0.0, 0.2f, true, 0.1f, true, 0.4f);
		add(*s, 0.5, 0.2f, true, 0.1f, false, 0.0f); // VIB lost on the newest frame
		CHECK(orbitMetricState(*s, OrbitMetric::Avg) == OrbitState::Valid, "AVG Valid");
		CHECK(orbitMetricState(*s, OrbitMetric::Dom) == OrbitState::Valid, "DOM Valid");
		CHECK(orbitMetricState(*s, OrbitMetric::Vib) == OrbitState::Unavailable, "VIB Unavailable now (no hold of the earlier colour) while AVG and DOM stay Valid");
		add(*s, 1.0, 0.2f, true, 0.1f, true, 0.4f);
		CHECK(orbitMetricState(*s, OrbitMetric::Vib) == OrbitState::Valid, "VIB Valid again");
		auto n = mk(2, 1, scenecolor::State::Valid, true);
		add(*n, 0.0, 0.2f, true, 0.1f, false, 0.0f);
		CHECK(orbitMetricState(*n, OrbitMetric::Vib) == OrbitState::Unavailable, "VIB never valid in this generation: Unavailable");
		auto st = mk(3, 1, scenecolor::State::Stale, true);
		add(*st, 0.0, 0.2f, true, 0.1f, true, 0.4f);
		CHECK(orbitMetricState(*st, OrbitMetric::Dom) == OrbitState::Stale && orbitMetricState(*st, OrbitMetric::Avg) == OrbitState::Stale, "stale snapshot => stale metrics");
		auto un = mk(4, 2, scenecolor::State::Unavailable, true);
		CHECK(orbitMetricState(*un, OrbitMetric::Dom) == OrbitState::Unavailable, "unavailable snapshot");
		auto g = mk(5, 1, scenecolor::State::Valid, true);
		add(*g, 0.0, 0.2f, true, 0.1f, true, 0.4f); addGap(*g, 0.5);
		CHECK(orbitMetricState(*g, OrbitMetric::Dom) == OrbitState::Unavailable, "a gap frame has no DOM");
	}
	{ // six combinations through the model: independent slots, no leakage, availability
		auto fx = mk(10, 1, scenecolor::State::Valid, true);
		auto raw = mk(20, 7, scenecolor::State::Valid, true);
		for (int i = 0; i < 6; ++i) {
			add(*fx, i * 0.5, 0.20f, true, 0.30f, i % 2 == 0, 0.60f);   // FX: DOM always, VIB on even frames
			add(*raw, i * 0.5, 0.25f, i < 3, 0.40f, false, 0.0f);       // RAW: DOM first half, VIB never
		}
		std::vector<OrbitSample> bFx(kOrbitMaxSamples), bRaw(kOrbitMaxSamples), bP[4];
		for (auto& v : bP) v.resize(kOrbitMaxSamples);
		OrbitSeriesView vFx, vRaw, vp[4];
		auto view = [&](const Snap& s, OrbitMetric m, std::vector<OrbitSample>& buf, OrbitSeriesView& v, bool pred) {
			v.count = orbitFillSamples(s, m, buf.data());
			v.samples = buf.data(); v.revision = s.revision; v.generation = s.generation; v.state = orbitMetricState(s, m); v.provided = pred && s.predominantProvided;
		};
		view(*fx, OrbitMetric::Avg, bFx, vFx, false);
		view(*raw, OrbitMetric::Avg, bRaw, vRaw, false);
		view(*fx, OrbitMetric::Dom, bP[0], vp[0], true);
		view(*raw, OrbitMetric::Dom, bP[1], vp[1], true);
		view(*fx, OrbitMetric::Vib, bP[2], vp[2], true);
		view(*raw, OrbitMetric::Vib, bP[3], vp[3], true);
		OrbitConsoleModel m;
		OrbitLayout L;
		CHECK(m.update(vFx, L, &vRaw, vp), "first update builds");
		const OrbitSourceModel &fa = m.slot(0), &ra = m.slot(1), &fd = m.slot(2), &rd = m.slot(3), &fv = m.slot(4), &rv = m.slot(5);
		CHECK(&fa == &m.fx() && &ra == &m.raw(), "slots 0/1 are the historic FX/RAW AVG models");
		CHECK(fa.hasLatest && ra.hasLatest && fd.hasLatest && rd.hasLatest && fv.hasLatest, "FX/AVG RAW/AVG FX/DOM RAW/DOM FX/VIB have data");
		CHECK(rv.provided && !rv.hasLatest && rv.state == OrbitState::Unavailable, "RAW/VIB is measured but has no colour: unavailable, not AVG");
		CHECK(near(fd.latest.r, srgbEncode(0.30f)) && near(fa.latest.r, srgbEncode(0.20f)), "FX DOM and AVG are different values (no substitution)");
		CHECK(near(rd.latest.r, srgbEncode(0.40f)) && near(ra.latest.r, srgbEncode(0.25f)), "RAW DOM and AVG are different values and differ from FX");
		CHECK(near(fv.latest.r, srgbEncode(0.60f)), "FX VIB colour");
		CHECK(fv.generation == 1 && rd.generation == 7, "source identities (generation) preserved per slot");
		CHECK(fd.state == OrbitState::Valid && fv.state == OrbitState::Unavailable, "FX/DOM Valid while FX/VIB Unavailable on an odd newest frame (VIB invalid, AVG still valid)");
		CHECK(fa.state == OrbitState::Valid, "FX/AVG remains Valid");
		CHECK(rd.state == OrbitState::Unavailable, "RAW/DOM lost on its newest frame");
		CHECK(!m.update(vFx, L, &vRaw, vp), "nothing changed => no rebuild");
		// opt-out: pred views not provided => NOT AVAILABLE for all four, AVG unaffected
		OrbitSeriesView none[4];
		CHECK(m.update(vFx, L, &vRaw, none), "turning DOM/VIB off rebuilds");
		CHECK(!m.slot(2).provided && !m.slot(3).provided && !m.slot(4).provided && !m.slot(5).provided && !m.slot(2).hasLatest, "all four DOM/VIB slots NOT AVAILABLE");
		CHECK(m.fx().hasLatest && m.raw().hasLatest, "AVG slots unaffected");
		// RAW absent while FX valid
		m.update(vFx, L, nullptr, vp);
		CHECK(m.fx().hasLatest && !m.raw().hasLatest && m.slot(2).hasLatest, "RAW/AVG unavailable while FX stays valid");
		OrbitSeriesView vp2[4] = {vp[0], OrbitSeriesView {}, vp[2], OrbitSeriesView {}};
		m.update(vFx, L, nullptr, vp2);
		CHECK(!m.slot(3).provided && !m.slot(5).provided && m.slot(2).provided && m.slot(4).provided, "RAW/DOM and RAW/VIB NOT AVAILABLE with FX/DOM and FX/VIB intact");
		// generation change: slots replace content; no previous-generation DOM leaks
		auto fx2 = mk(11, 2, scenecolor::State::Valid, true);
		add(*fx2, 100.0, 0.1f, false, 0.0f, false, 0.0f);
		std::vector<OrbitSample> b2(kOrbitMaxSamples), bp2(kOrbitMaxSamples);
		OrbitSeriesView v2, pv[4];
		view(*fx2, OrbitMetric::Avg, b2, v2, false);
		view(*fx2, OrbitMetric::Dom, bp2, pv[0], true);
		view(*fx2, OrbitMetric::Vib, bp2, pv[2], true);
		m.update(v2, L, nullptr, pv);
		CHECK(!m.slot(2).hasLatest && !m.slot(4).hasLatest && m.slot(2).generation == 2 && m.fx().hasLatest, "new generation: no DOM/VIB from the previous generation, AVG valid");
	}
	{ // availability binding rule used by orbitBoundSource: AVG always; DOM/VIB iff provided
		OrbitSourceModel avgLike; avgLike.provided = true;
		CHECK(orbitMetricAlwaysPublished(OrbitMetric::Avg) && !orbitMetricAlwaysPublished(OrbitMetric::Dom) && !orbitMetricAlwaysPublished(OrbitMetric::Vib), "binding rule");
	}
	std::cout << (T - F) << "/" << T << " checks passed\n";
	return F == 0 ? 0 : 1;
}
