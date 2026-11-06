// Orbit Console v0.1 dual-source presentation core: per-source geometry, shared time axis, gaps,
// tuning-derived slot counts, aggregation, radar, generation reset, geometry cache. OF-free.
#include "../hud-compositor/OrbitConsoleModel.h"
#include <cmath>
#include <iostream>
#include <vector>
using namespace hudpresent;
static int T = 0, F = 0;
#define CHECK(c, w) do { T++; if (!(c)) { F++; std::cerr << "FAIL: " << (w) << " (" << __LINE__ << ")\n"; } } while (0)

static OrbitSample S(double t, float r, float g, float b, bool v = true) {
	OrbitSample s; s.t = t; s.r = r; s.g = g; s.b = b; s.luma = 0.2126f * r + 0.7152f * g + 0.0722f * b; s.valid = v; return s;
}
static OrbitSeriesView V(const std::vector<OrbitSample>& v, uint64_t rev, uint64_t gen = 1, OrbitState st = OrbitState::Valid) {
	OrbitSeriesView s; s.state = st; s.generation = gen; s.revision = rev; s.samples = v.data(); s.count = uint32_t(v.size()); return s;
}
static std::vector<OrbitSample> ramp(int n, double t0 = 0.0) {
	std::vector<OrbitSample> v;
	for (int i = 0; i < n; ++i) v.push_back(S(t0 + i * 0.5, 0.1f + 0.005f * i, 0.5f, 0.9f - 0.004f * i));
	return v;
}
static size_t countKind(const SegmentedHistoryGeometry& g, SegKind k) { size_t n = 0; for (size_t i = 0; i < g.count(); ++i) n += g.seg(i).kind == k; return n; }

int main() {
	OrbitLayout L;
	OrbitLayout D = L; D.segmentWidthPx = 1.2f; D.segmentGapPx = 0.4f; // dense: one bin per 0.5 s sample
	const float groupW = 409.6f - (4 + 84 + 6 + 66 + 6) - 4; // drawable ribbon width (239.6)
	{ // section layout: every block inside the 72 px section, no overlap, FX/RAW share the same x geometry
		OrbitConsoleModel m; auto v = ramp(10); m.update(V(v, 1), L);
		const auto& s = m.section();
		CHECK(s.swatch.y + s.swatch.h <= s.bars.y && s.bars.y + s.bars.h <= s.lumaRow.y && s.lumaRow.y + s.lumaRow.h <= L.upperH, "left column stacks inside the section");
		CHECK(s.ribbon.y + s.ribbon.h <= s.rgb.y && s.rgb.y + s.rgb.h <= L.upperH, "ribbon above the RGB traces, inside the section");
		CHECK(s.ribbon.x == s.rgb.x && s.ribbon.w == s.rgb.w, "ribbon and RGB traces share x and width");
		CHECK(s.radar.x + s.radar.w <= s.accent.x && s.accent.x + s.accent.w <= s.ribbon.x, "radar, accent stripe, group left-to-right without overlap");
		CHECK(std::fabs(s.ribbon.w - groupW) < 1e-3f && std::fabs(s.ribbon.x + s.ribbon.w - (409.6f - 4.0f)) < 1e-3f, "group fills to the right padding");
		CHECK(s.ribbon.h >= 30.0f && s.rgb.h >= 24.0f, "ribbon and traces stay legible");
		bool axes = true; for (int c = 0; c < 3; ++c) axes &= m.radarAxisEnd(c).x >= s.radar.x && m.radarAxisEnd(c).x <= s.radar.x + s.radar.w && m.radarAxisEnd(c).y >= s.radar.y && m.radarAxisEnd(c).y <= s.radar.y + s.radar.h;
		CHECK(axes, "radar axes inside the radar box");
	}
	{ // chart gap handling; latest valid
		std::vector<OrbitSample> v = {S(0, 1, 0, 0), S(0.5, 0, 1, 0), S(1.0, 0, 0, 0, false), S(1.5, 0, 0, 1), S(2.0, 0.2f, 0.2f, 0.2f)};
		OrbitConsoleModel m; m.update(V(v, 1), D); // dense: one bin per sample, independent of the default tuning
		CHECK(m.fx().rgb[0].segN == 2, "RGB trace breaks at the gap");
		CHECK(m.fx().luma.segN == 2, "luminance overlay breaks at the same gap");
		CHECK(m.hasLatest() && m.latest().r == 0.2f, "latest valid sample");
		CHECK(m.state() == OrbitState::Valid && m.rawState() == OrbitState::Unavailable, "RAW is independent and unavailable without a source");
	}
	{ // tuning-derived slot counts (dense / default / sparse) and bin geometry
		CHECK(historySlotCapacity(groupW, 2.6f, 0.75f) == 71, "default 2.6/0.75 -> 71 bins at 239.6 px");
		CHECK(historySlotCapacity(groupW, 1.2f, 0.4f) == 120, "dense 1.2/0.4 -> capped at 120 (sampler capacity)");
		CHECK(historySlotCapacity(groupW, 6.0f, 2.0f) == 30, "sparse 6/2 -> 30 bins");
		CHECK(historySlotCapacity(3.0f, 6.0f, 2.0f) == 1 && historySlotCapacity(0.0f, 6.0f, 2.0f) == 0, "one slot when it cannot fit, none without width");
		CHECK(historySlotCapacity(groupW, 0.0f, 1.0f) == 0 && historySlotCapacity(groupW, -2.0f, 1.0f) == 0, "zero/negative width never divides");
		for (auto tune : {std::pair<float, float>{1.2f, 0.4f}, {2.6f, 0.75f}, {6.0f, 2.0f}}) {
			OrbitLayout TL = L; TL.segmentWidthPx = tune.first; TL.segmentGapPx = tune.second;
			auto v = ramp(120);
			OrbitConsoleModel m; m.update(V(v, 1), TL);
			const auto& g = m.history();
			CHECK(g.count() == historySlotCapacity(groupW, tune.first, tune.second), "bin count follows width/gap");
			CHECK(std::fabs(g.seg(g.count() - 1).x1 - (m.section().ribbon.x + m.section().ribbon.w)) < 1e-3f, "newest bin anchored on the right edge");
			CHECK(g.seg(0).x0 >= m.section().ribbon.x - 1e-3f, "no drawing left of the ribbon");
			bool pitch = true; for (size_t i = 1; i < g.count(); ++i) pitch &= std::fabs((g.seg(i).x0 - g.seg(i - 1).x0) - (tune.first + tune.second)) < 1e-3f;
			CHECK(pitch, "constant pitch = width + gap");
			CHECK(countKind(g, SegKind::Valid) == g.count(), "a full 60 s history fills every bin (no interpolation, no holes)");
		}
		OrbitLayout bad = L; bad.segmentWidthPx = -3.0f; bad.segmentGapPx = 500.0f;
		auto v = ramp(120); OrbitConsoleModel m; m.update(V(v, 1), bad);
		CHECK(m.history().count() >= 1 && m.history().seg(0).x0 >= m.section().ribbon.x - 1e-3f, "negative width + huge gap are sanitised, no out-of-bounds bins");
	}
	{ // ACCEPTANCE: both sources share one time axis; RAW gaps stay in RAW; FX is never copied into RAW
		std::vector<OrbitSample> fx, rawv;
		for (int i = 0; i < 120; ++i) {
			fx.push_back(S(i * 0.5, 0.2f, 0.4f, 0.6f));
			rawv.push_back(S(i * 0.5, 0.9f, 0.1f, 0.1f, !(i >= 40 && i < 48))); // RAW video drops out for 4 s
		}
		OrbitConsoleModel m; const OrbitSeriesView rv = V(rawv, 1);
		m.update(V(fx, 1), D, &rv);
		const auto& a = m.fx().history; const auto& b = m.raw().history;
		bool same = a.count() == b.count();
		for (size_t i = 0; same && i < a.count(); ++i) same &= a.seg(i).x0 == b.seg(i).x0 && a.seg(i).x1 == b.seg(i).x1;
		CHECK(same, "RAW bins align vertically with FX bins");
		CHECK(countKind(b, SegKind::Gap) > 0 && countKind(a, SegKind::Gap) == 0, "RAW gaps appear only in RAW");
		CHECK(m.raw().rgb[0].segN == 2 && m.fx().rgb[0].segN == 1, "RAW traces break at the RAW gap, FX traces do not");
		bool distinct = true; for (size_t i = 0; i < b.count(); ++i) if (b.seg(i).kind == SegKind::Valid) distinct &= std::fabs(b.seg(i).r - 0.9f) < 1e-3f;
		CHECK(distinct, "RAW ribbon holds RAW colours");
		CHECK(m.fx().latest.r == 0.2f && m.raw().latest.r == 0.9f, "latest values are per source");
		const OrbitPt rc = m.radarCenter();
		CHECK(std::fabs(m.raw().radarPt[0].y - (rc.y + (m.radarAxisEnd(0).y - rc.y) * 0.9f)) < 1e-3f && std::fabs(m.fx().radarPt[0].y - (rc.y + (m.radarAxisEnd(0).y - rc.y) * 0.2f)) < 1e-3f, "radar R vertex scales with the source value on the same [0,1] scale");
		// RAW lagging FX: timestamps, not array positions, decide x
		std::vector<OrbitSample> lag = {S(50.0, 0.9f, 0.1f, 0.1f), S(50.5, 0.9f, 0.1f, 0.1f)}; const OrbitSeriesView lv = V(lag, 1);
		m.update(V(fx, 2), D, &lv);
		const long long slot = m.fx().history.slotOf(50.0);
		CHECK(slot >= 0 && m.raw().history.seg(size_t(slot)).kind == SegKind::Valid && m.raw().history.seg(m.raw().history.count() - 1).kind == SegKind::Missing, "RAW timestamps plot at the FX x for the same time; the newest FX slots stay Missing for RAW");
	}
	{ // aggregation: samples sharing a bin average in LINEAR light, then re-encode
		OrbitLayout TL = L; TL.segmentWidthPx = 6.0f; TL.segmentGapPx = 2.0f;
		std::vector<OrbitSample> a = {S(0.5, 1, 1, 1), S(2.0, 0, 0, 0)}; // both land in the newest 2 s bin
		OrbitConsoleModel m; m.update(V(a, 1), TL);
		bool found = false; for (size_t i = 0; i < m.history().count(); ++i) if (m.history().seg(i).kind == SegKind::Valid && std::fabs(m.history().seg(i).r - srgbEncode(0.5f)) < 1e-3f) found = true;
		CHECK(found, "black+white bin averages to linear 0.5 (encoded ~0.735), not sRGB 0.5");
	}
	{ // valid black stays valid and painted; unsampled slots are Missing
		std::vector<OrbitSample> a = {S(0.0, 0, 0, 0), S(0.5, 0, 0, 0)}; const OrbitSeriesView rv = V(a, 1);
		OrbitConsoleModel m; m.update(V(a, 1), L, &rv);
		const auto& g = m.raw().history;
		CHECK(g.seg(g.count() - 1).kind == SegKind::Valid && m.raw().ribbon.fill(g.seg(g.count() - 1)).paint, "valid black RAW paints (not a gap)");
		CHECK(g.seg(0).kind == SegKind::Missing && !m.raw().ribbon.fill(g.seg(0)).paint, "unsampled RAW slots are Missing and unpainted");
		CHECK(m.raw().rgb[0].n >= 1 && m.raw().rgb[0].v[0].y > m.section().rgb.y + m.section().rgb.h - 3.0f, "valid zero sits on the trace floor");
	}
	{ // no RAW source: unavailable, empty, no radar polygon, never the FX colours; stale/generation independent
		std::vector<OrbitSample> a = {S(0.0, 0.3f, 0.3f, 0.9f), S(0.5, 0.3f, 0.3f, 0.9f)};
		OrbitConsoleModel m; m.update(V(a, 1), L, nullptr);
		CHECK(m.rawState() == OrbitState::Unavailable && m.raw().history.count() == 0 && !m.raw().hasLatest && m.raw().rgb[0].n == 0 && m.raw().luma.n == 0, "no RAW source: unavailable and empty");
		std::vector<OrbitSample> r = {S(0.0, 1, 0, 0)}; OrbitSeriesView rv = V(r, 1, 1, OrbitState::Stale);
		m.update(V(a, 2), L, &rv);
		CHECK(m.rawState() == OrbitState::Stale && m.state() == OrbitState::Valid && m.raw().hasLatest, "RAW stale is independent of the FX state");
		OrbitSeriesView rv2 = V(r, 1, 2, OrbitState::Valid); m.update(V(a, 2), L, &rv2);
		CHECK(m.rawState() == OrbitState::Valid && m.raw().generation == 2, "RAW generation change rebuilds without smearing");
		CHECK(m.update(V(a, 2), L, &rv2) == false, "no rebuild without a change");
	}
	{ // generation reset: both strips restart; stale keeps geometry
		std::vector<OrbitSample> v = {S(0, 1, 0, 0), S(0.5, 0, 1, 0)};
		OrbitConsoleModel m; m.update(V(v, 1, 1, OrbitState::Valid), D);
		const size_t before = countKind(m.history(), SegKind::Valid);
		m.update(V(v, 1, 1, OrbitState::Stale), D);
		CHECK(m.state() == OrbitState::Stale && before == 2 && countKind(m.history(), SegKind::Valid) == 2, "stale keeps geometry, state updates without rebuild");
		std::vector<OrbitSample> w = {S(9, 0, 0, 1)};
		m.update(V(w, 2, 2), D);
		CHECK(countKind(m.history(), SegKind::Valid) == 1 && m.fx().rgb[2].n == 1 && m.fx().luma.n == 1, "new generation: no old samples, traces restart");
	}
	{ // partial history: newest in the last slot, older slots empty
		auto v = ramp(20, 100.0);
		OrbitConsoleModel m; m.update(V(v, 1), L);
		const auto& g = m.history();
		CHECK(g.seg(g.count() - 1).kind == SegKind::Valid && g.seg(0).kind == SegKind::Missing, "newest right, older empty");
		CHECK(m.xOfTime(109.5) == g.centerX(g.count() - 1) && m.xOfTime(0.0) < 0.0f, "xOfTime: newest at the last slot, out-of-window < 0");
		bool mono = true; for (size_t k = 1; k < m.fx().rgb[0].n; ++k) mono &= m.fx().rgb[0].v[k].x > m.fx().rgb[0].v[k - 1].x;
		CHECK(mono, "traces run oldest -> newest, left -> right");
	}
	{ // invalid sample inside a run: ribbon gap + traces break at the same bin
		std::vector<OrbitSample> v = {S(0, 1, 0, 0), S(0.5, 1, 0, 0), S(1.0, 0, 0, 0, false), S(1.5, 1, 0, 0), S(2.0, 1, 0, 0)};
		OrbitConsoleModel m; m.update(V(v, 1), D);
		const size_t n = m.history().count();
		CHECK(m.history().seg(n - 3).kind == SegKind::Gap && !m.fx().ribbon.fill(m.history().seg(n - 3)).paint && m.fx().rgb[0].segN == 2, "invalid sample: ribbon gap (unpainted) + trace break");
	}
	{ // cache: rebuild only on revision/generation/layout/RAW change
		auto v = ramp(30);
		OrbitConsoleModel m;
		CHECK(m.update(V(v, 5, 1), L) && !m.update(V(v, 5, 1), L), "no rebuild on identical input");
		CHECK(m.update(V(v, 6, 1), L), "rebuild on new revision");
		OrbitLayout other = L; other.segmentGapPx = 1.5f;
		CHECK(m.update(V(v, 6, 1), other), "rebuild on tuning change");
		CHECK(m.rebuildCount() == 3, "rebuild count");
	}
	std::cout << (T - F) << "/" << T << " passed\n";
	return F == 0 ? 0 : 1;
}
