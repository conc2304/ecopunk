// OC-005 Stage 1: configurable Orbit observations — configuration model, editor state machine
// (preview / commit / cancel / navigation), editor geometry, and the data-truth rules the draw path
// applies (valid black vs missing, DOM/VIB unavailable, FX never substituted for RAW). OF-free.
#include "../hud-compositor/OrbitConsoleModel.h"
#include "../hud-compositor/OrbitObservationConfig.h"
#include <iostream>
#include <vector>
using namespace hudpresent;
static int T = 0, F = 0;
#define CHECK(c, w) do { T++; if (!(c)) { F++; std::cerr << "FAIL: " << (w) << " (" << __LINE__ << ")\n"; } } while (0)

using R = OrbitConfigEditor::Result;

static bool overlap(const OrbitEditRect& a, const OrbitEditRect& b) {
	return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
static bool inside(const OrbitEditRect& r, float w, float h) { return r.x >= 0 && r.y >= 0 && r.x + r.w <= w && r.y + r.h <= h && r.w > 0 && r.h > 0; }

static OrbitObservationConfig O(OrbitSource s, OrbitMetric m) { OrbitObservationConfig c; c.source = s; c.metric = m; return c; }
// What the draw path shows for an observation, per the rules in SparklineWidget::drawOrbit.
enum class Shown { Measurement, NoData, NotAvailable };
static Shown shownFor(const OrbitObservationConfig& c, const OrbitConsoleModel& m) {
	if (!orbitMetricAvailable(c.metric)) return Shown::NotAvailable;
	const OrbitSourceModel& s = m.source(c.source == OrbitSource::Fx ? OrbitSourceId::Fx : OrbitSourceId::Raw);
	return s.hasLatest ? Shown::Measurement : Shown::NoData;
}
static OrbitSample S(double t, float r, float g, float b, bool v = true) {
	OrbitSample s; s.t = t; s.r = r; s.g = g; s.b = b; s.luma = 0.2126f * r + 0.7152f * g + 0.0722f * b; s.valid = v; return s;
}
static OrbitSeriesView V(const std::vector<OrbitSample>& v, uint64_t rev, OrbitState st = OrbitState::Valid) {
	OrbitSeriesView s; s.state = st; s.generation = 1; s.revision = rev; s.samples = v.data(); s.count = uint32_t(v.size()); return s;
}

int main() {
	{ // defaults reproduce the previous fixed console
		OrbitConsoleConfig c;
		CHECK(c.a.source == OrbitSource::Fx && c.a.metric == OrbitMetric::Avg, "A defaults to FX / AVG");
		CHECK(c.b.source == OrbitSource::Raw && c.b.metric == OrbitMetric::Avg, "B defaults to RAW / AVG");
		OrbitConfigEditor e;
		CHECK(!e.isOpen() && e.config() == c && e.revision() == 0, "editor starts closed on the defaults");
	}
	{ // labels
		CHECK(std::string(orbitSourceText(OrbitSource::Fx)) == "FX" && std::string(orbitSourceText(OrbitSource::Raw)) == "RAW", "source labels");
		CHECK(std::string(orbitMetricText(OrbitMetric::Avg)) == "AVG" && std::string(orbitMetricText(OrbitMetric::Dom)) == "DOM" && std::string(orbitMetricText(OrbitMetric::Vib)) == "VIB", "metric labels");
		CHECK(orbitMetricAvailable(OrbitMetric::Avg) && !orbitMetricAvailable(OrbitMetric::Dom) && !orbitMetricAvailable(OrbitMetric::Vib), "only AVG is a published measurement");
	}
	{ // all 36 combinations of (A, B) are representable, selectable through the editor, and independent
		int combos = 0;
		for (int as = 0; as < 2; ++as) for (int am = 0; am < 3; ++am) for (int bs = 0; bs < 2; ++bs) for (int bm = 0; bm < 3; ++bm) {
			OrbitConfigEditor e;
			auto pick = [&](OrbitObs o, OrbitSource s, OrbitMetric m) {
				e.open(o);
				const OrbitEditItem src = s == OrbitSource::Fx ? OrbitEditItem::SrcFx : OrbitEditItem::SrcRaw;
				const OrbitEditItem met = OrbitEditItem(int(OrbitEditItem::MetAvg) + int(m));
				while (e.cursor() != src) e.step(1);
				e.activate();
				while (e.cursor() != met) e.step(1);
				e.activate();
				while (e.cursor() != OrbitEditItem::Done) e.step(1);
				return e.activate();
			};
			CHECK(pick(OrbitObs::A, OrbitSource(as), OrbitMetric(am)) == R::Committed, "A commit");
			CHECK(pick(OrbitObs::B, OrbitSource(bs), OrbitMetric(bm)) == R::Committed, "B commit");
			const OrbitConsoleConfig& c = e.config();
			CHECK(c.a.source == OrbitSource(as) && c.a.metric == OrbitMetric(am), "A holds its own selection");
			CHECK(c.b.source == OrbitSource(bs) && c.b.metric == OrbitMetric(bm), "B holds its own selection");
			++combos;
		}
		CHECK(combos == 36, "36 combinations enumerated (including the 6 identical A/B pairs among them)");
	}
	{ // identical A/B selections are legal and independent afterwards
		OrbitConfigEditor e;
		e.open(OrbitObs::B);                       // B: RAW/AVG -> FX/AVG (identical to A)
		while (e.cursor() != OrbitEditItem::SrcFx) e.step(1);
		CHECK(e.activate() == R::Previewed, "select FX previews");
		while (e.cursor() != OrbitEditItem::Done) e.step(1);
		e.activate();
		CHECK(e.config().a == e.config().b, "identical A/B allowed");
		e.open(OrbitObs::A);
		while (e.cursor() != OrbitEditItem::MetVib) e.step(1);
		e.activate();
		CHECK(e.config().a.metric == OrbitMetric::Vib && e.config().b.metric == OrbitMetric::Avg, "editing A leaves B alone");
	}
	{ // editor occlusion: the edited observation stays visible, the OTHER region hosts the editor (radar lives in region A)
		OrbitConfigEditor e;
		e.open(OrbitObs::B);
		CHECK(e.overlayRegion() == OrbitObs::A, "editing B puts the editor in region A (the radar region: temporary, accepted occlusion)");
		e.cancel();
		e.open(OrbitObs::A);
		CHECK(e.overlayRegion() == OrbitObs::B, "editing A keeps region A (and its radar) visible");
	}
	{ // preview applies immediately; Done commits; the other observation never changes
		OrbitConfigEditor e;
		e.open(OrbitObs::A);
		CHECK(e.isOpen() && e.target() == OrbitObs::A && e.overlayRegion() == OrbitObs::B, "editing A -> overlay in region B");
		CHECK(e.cursor() == OrbitEditItem::SrcFx, "cursor starts on the current source");
		const uint64_t r0 = e.revision();
		while (e.cursor() != OrbitEditItem::SrcRaw) e.step(1);
		CHECK(e.config().a.source == OrbitSource::Fx, "moving the cursor alone changes nothing");
		CHECK(e.activate() == R::Previewed && e.config().a.source == OrbitSource::Raw && e.revision() == r0 + 1, "select previews immediately");
		CHECK(e.activate() == R::Previewed && e.revision() == r0 + 1, "re-selecting the same option does not bump the revision");
		while (e.cursor() != OrbitEditItem::Done) e.step(1);
		CHECK(e.activate() == R::Committed && !e.isOpen(), "Done commits and closes");
		CHECK(e.config().a.source == OrbitSource::Raw && e.config().b == OrbitConsoleConfig{}.b, "commit keeps the selection; B untouched");
	}
	{ // cancel restores the prior configuration exactly
		OrbitConfigEditor e;
		e.open(OrbitObs::B);
		while (e.cursor() != OrbitEditItem::SrcFx) e.step(1);
		e.activate();
		while (e.cursor() != OrbitEditItem::MetDom) e.step(1);
		e.activate();
		CHECK(e.config().b.source == OrbitSource::Fx && e.config().b.metric == OrbitMetric::Dom, "preview visible before cancel");
		const uint64_t r = e.revision();
		CHECK(e.cancel() == R::Cancelled && !e.isOpen(), "cancel closes");
		CHECK(e.config() == OrbitConsoleConfig{} && e.revision() == r + 1, "cancel restores the prior configuration");
		CHECK(e.cancel() == R::None && e.activate() == R::None, "no-ops when closed");
		e.step(1);
		CHECK(!e.isOpen(), "step when closed is inert");
	}
	{ // cancel after an earlier commit restores the COMMITTED value, not the defaults
		OrbitConfigEditor e;
		e.open(OrbitObs::A); while (e.cursor() != OrbitEditItem::MetVib) e.step(1); e.activate();
		while (e.cursor() != OrbitEditItem::Done) e.step(1); e.activate();
		e.open(OrbitObs::A); while (e.cursor() != OrbitEditItem::MetAvg) e.step(1); e.activate();
		CHECK(e.config().a.metric == OrbitMetric::Avg, "second edit previews");
		e.cancel();
		CHECK(e.config().a.metric == OrbitMetric::Vib, "cancel restores the committed VIB");
	}
	{ // open is not re-entrant; wrapping both ways
		OrbitConfigEditor e;
		e.open(OrbitObs::A); e.open(OrbitObs::B);
		CHECK(e.target() == OrbitObs::A, "a second open while editing is ignored");
		e.step(-1);
		CHECK(e.cursor() == OrbitEditItem::Done, "step -1 from the first item wraps to DONE");
		e.step(1);
		CHECK(e.cursor() == OrbitEditItem::SrcFx, "step +1 from DONE wraps to FX");
		e.step(7);
		CHECK(e.cursor() == OrbitEditItem::SrcRaw, "multi-detent jog wraps");
		e.step(0);
		CHECK(e.cursor() == OrbitEditItem::SrcRaw, "zero step is inert");
	}
	{ // editor geometry at the canonical 409.6 x 72 region: inside the region, no overlap, legible
		const float W = 409.6f, H = 72.0f;
		const OrbitEditorLayout L = orbitEditorLayout(W, H);
		for (int i = 0; i < kOrbitEditItemCount; ++i) CHECK(inside(L.item[size_t(i)], W, H), "item inside the region");
		for (int i = 0; i < kOrbitEditItemCount; ++i) for (int j = i + 1; j < kOrbitEditItemCount; ++j) CHECK(!overlap(L.item[size_t(i)], L.item[size_t(j)]), "items do not overlap");
		CHECK(inside(L.title, W, H) && inside(L.sourceHeader, W, H) && inside(L.metricHeader, W, H), "headers inside the region");
		CHECK(!overlap(L.title, L.sourceHeader) && !overlap(L.sourceHeader, L.metricHeader), "headers do not overlap");
		CHECK(!overlap(L.metricHeader, L.item[size_t(OrbitEditItem::Done)]), "DONE does not cover the METRIC header");
		bool headerClear = true;
		for (int i = 0; i < kOrbitEditItemCount; ++i) {
			if (OrbitEditItem(i) == OrbitEditItem::Done) continue;
			headerClear &= !overlap(L.item[size_t(i)], L.title) && !overlap(L.item[size_t(i)], L.sourceHeader) && !overlap(L.item[size_t(i)], L.metricHeader);
		}
		CHECK(headerClear, "option cells clear of every header");
		CHECK(L.item[size_t(OrbitEditItem::SrcFx)].x < L.item[size_t(OrbitEditItem::MetAvg)].x, "source group left of metric group");
		CHECK(L.item[size_t(OrbitEditItem::SrcFx)].h >= 24.0f, "option cells are large enough to read");
		// text fits: bitmap font is 8 px per character
		CHECK(L.item[size_t(OrbitEditItem::MetDom)].w >= 8.0f * 7, "'NO DATA' caption fits the DOM/VIB cells");
		CHECK(L.sourceHeader.w >= 8.0f * 6 && L.title.w >= 8.0f * 6, "SOURCE / EDIT X labels fit");
	}
	{ // data truth: valid black is a measurement; missing is not; DOM/VIB never show AVG; FX never stands in for RAW
		std::vector<OrbitSample> black = { S(0, 0, 0, 0), S(0.5, 0, 0, 0) };
		std::vector<OrbitSample> fx = { S(0, 0.2f, 0.4f, 0.6f), S(0.5, 0.2f, 0.4f, 0.6f) };
		OrbitConsoleModel m; OrbitLayout L;
		auto vfx = V(fx, 1);
		m.update(vfx, L, nullptr); // RAW missing
		CHECK(shownFor(O(OrbitSource::Fx, OrbitMetric::Avg), m) == Shown::Measurement, "FX/AVG shows the measurement");
		CHECK(shownFor(O(OrbitSource::Raw, OrbitMetric::Avg), m) == Shown::NoData, "unavailable RAW is NO DATA, never the FX series");
		CHECK(!m.raw().hasLatest, "RAW holds no FX-derived latest");
		for (OrbitSource s : { OrbitSource::Fx, OrbitSource::Raw }) for (OrbitMetric mt : { OrbitMetric::Dom, OrbitMetric::Vib })
			CHECK(shownFor(O(s, mt), m) == Shown::NotAvailable, "DOM/VIB is an explicit unavailable state for both sources");
		auto vb = V(black, 2);
		OrbitConsoleModel mb; mb.update(vfx, L, &vb);
		CHECK(shownFor(O(OrbitSource::Raw, OrbitMetric::Avg), mb) == Shown::Measurement, "valid black RAW is a measurement");
		CHECK(mb.raw().hasLatest && mb.raw().latest.r == 0.0f && mb.raw().latest.g == 0.0f && mb.raw().latest.b == 0.0f, "valid black reads 0,0,0 (not missing)");
		CHECK(mb.fx().latest.b == 0.6f, "FX untouched by RAW");
		std::vector<OrbitSample> invalid = { S(0, 0, 0, 0, false) };
		auto vi = V(invalid, 3);
		OrbitConsoleModel mi; mi.update(vfx, L, &vi);
		CHECK(shownFor(O(OrbitSource::Raw, OrbitMetric::Avg), mi) == Shown::NoData, "an all-invalid series is missing, not black");
	}
	{ // 36 combinations x availability: the shown state follows (metric, source data) only
		std::vector<OrbitSample> fx = { S(0, 0.2f, 0.4f, 0.6f) };
		OrbitConsoleModel m; OrbitLayout L; auto vfx = V(fx, 1); m.update(vfx, L, nullptr);
		int measurement = 0, nodata = 0, na = 0;
		for (int s = 0; s < 2; ++s) for (int mt = 0; mt < 3; ++mt) {
			switch (shownFor(O(OrbitSource(s), OrbitMetric(mt)), m)) { case Shown::Measurement: ++measurement; break; case Shown::NoData: ++nodata; break; default: ++na; }
		}
		CHECK(measurement == 1 && nodata == 1 && na == 4, "of 6 selections with only FX available: 1 measurement, RAW/AVG no data, 4 DOM/VIB unavailable");
	}
	{ // OC-005 Stage 2: persisted tokens round-trip for all 6 selections; unknown tokens are refused
		for (int so = 0; so < 2; ++so) for (int mt = 0; mt < 3; ++mt) {
			OrbitObservationConfig in = O(OrbitSource(so), OrbitMetric(mt)), out;
			CHECK(orbitFromTokens(orbitSourceToken(in.source), orbitMetricToken(in.metric), out) && out == in, "token round-trip");
		}
		OrbitObservationConfig keep = O(OrbitSource::Raw, OrbitMetric::Vib), x = keep;
		CHECK(!orbitFromTokens("banana", "avg", x) && x == keep, "unknown source refused, output untouched");
		CHECK(!orbitFromTokens("fx", "median", x) && x == keep, "unknown metric refused, output untouched");
		CHECK(!orbitFromTokens("FX", "avg", x) && !orbitFromTokens("", "", x), "tokens are exact, lower-case, non-empty");
	}
	{ // setCommitted: installs the persisted selection; never overwrites an open preview
		OrbitConfigEditor e;
		OrbitConsoleConfig c; c.a = O(OrbitSource::Raw, OrbitMetric::Vib); c.b = O(OrbitSource::Fx, OrbitMetric::Dom);
		CHECK(e.setCommitted(c) && e.config() == c && e.revision() == 1, "startup restore installs both observations");
		CHECK(e.setCommitted(c) && e.revision() == 1, "same value: no revision bump");
		e.open(OrbitObs::A);
		while (e.cursor() != OrbitEditItem::MetAvg) e.step(1);
		e.activate(); // preview A = RAW/AVG
		OrbitConsoleConfig other; // defaults
		CHECK(!e.setCommitted(other) && e.config().a.metric == OrbitMetric::Avg && e.config().b == c.b, "an open preview is never overwritten from outside");
		e.cancel();
		CHECK(e.config() == c, "cancel returns to the restored committed selection");
	}
	std::cout << (T - F) << "/" << T << " checks passed\n";
	return F ? 1 : 0;
}
