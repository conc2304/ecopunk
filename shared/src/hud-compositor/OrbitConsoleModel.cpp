#include "OrbitConsoleModel.h"

#include <algorithm>

namespace hudpresent {

namespace {
constexpr float kPi = 3.14159265358979f;

// Appends a point to a trace, opening a new gap-free segment when needed.
void tracePush(OrbitTrace& t, bool& open, float x, float y) {
	if (!open) {
		if (t.segN >= OrbitTrace::kMaxSegs) return;
		t.seg[t.segN++] = {uint16_t(t.n), 0};
		open = true;
	}
	if (t.n >= t.v.size()) return;
	t.v[t.n++] = {x, y};
	t.seg[t.segN - 1].count++;
}
} // namespace

bool OrbitConsoleModel::update(const OrbitSeriesView& fx, const OrbitLayout& layout, const OrbitSeriesView* raw, const OrbitSeriesView* pred) {
	// Cheap state is refreshed every call; geometry only on a new key.
	src_[0].state = fx.state;
	const bool haveRaw = raw && raw->samples && raw->count > 0;
	src_[1].state = haveRaw ? raw->state : OrbitState::Unavailable;
	const uint64_t rawRev = haveRaw ? raw->revision : 0, rawGen = haveRaw ? raw->generation : 0;
	// OC-003: DOM/VIB slots. State and availability refresh every call; geometry only on a new key.
	std::array<PredKey, kOrbitPredSlotCount> pk {};
	for (size_t i = 0; i < kOrbitPredSlotCount; ++i) {
		const OrbitSeriesView* v = pred ? &pred[i] : nullptr;
		const bool prov = v && v->provided;
		const bool has = prov && v->samples && v->count > 0;
		pk[i] = {prov, has, has ? v->revision : 0, has ? v->generation : 0};
		OrbitSourceModel& m = src_[2 + i];
		m.provided = prov;
		m.state = has ? v->state : OrbitState::Unavailable;
	}
	const bool key = built_ && pk == builtPred_ && fx.revision == builtFxRevision_ && fx.generation == builtFxGeneration_ && layout == builtLayout_
		&& haveRaw == builtHadRaw_ && rawRev == builtRawRevision_ && rawGen == builtRawGeneration_;
	if (key) return false;
	builtHadRaw_ = haveRaw;
	builtRawRevision_ = rawRev;
	builtRawGeneration_ = rawGen;
	builtPred_ = pk;
	rebuild(fx, layout, haveRaw ? raw : nullptr, pred);
	return true;
}

void OrbitConsoleModel::rebuild(const OrbitSeriesView& fx, const OrbitLayout& L0, const OrbitSeriesView* raw, const OrbitSeriesView* pred) {
	rebuildCount_++;
	built_ = true;
	builtFxRevision_ = fx.revision;
	builtFxGeneration_ = fx.generation;
	builtLayout_ = L0;

	// Sanitise the tuning: non-positive width -> defaults, gap clamped to [0, kOrbitMaxGapPx].
	OrbitLayout L = L0;
	if (!(L.segmentWidthPx > 0.0f)) L.segmentWidthPx = kOrbitHistoryTuning.segmentWidthPx;
	L.segmentGapPx = std::min(kOrbitMaxGapPx, std::max(0.0f, L.segmentGapPx));

	// Section layout (identical for both sources): left column 84 px, radar/legend box 66 px,
	// right group = the rest. Heights target a 72 px section.
	const float pad = 4.0f, colW = 84.0f, radarW = 66.0f, mid = 6.0f;
	sec_.swatch = {pad, 3.0f, colW, 13.0f};
	sec_.bars = {pad, 19.0f, colW, 39.0f};     // three 13 px rows
	sec_.lumaRow = {pad, 58.0f, colW, 13.0f};
	sec_.radar = {pad + colW + mid, 3.0f, radarW, 66.0f};
	const float gx = sec_.radar.x + radarW + mid;
	const float gw = std::max(1.0f, L.w - gx - pad);
	sec_.ribbon = {gx, 2.0f, gw, 38.0f};
	sec_.rgb = {gx, 42.0f, gw, 28.0f};
	sec_.accent = {gx - 3.0f, 2.0f, 2.0f, 68.0f};

	radarCenter_ = {sec_.radar.x + radarW * 0.5f, sec_.radar.y + 33.0f};
	radarR_ = 22.0f;
	for (int c = 0; c < 3; ++c) { // R at the top, G lower-left, B lower-right
		const float a = (90.0f + 120.0f * c) * kPi / 180.0f;
		axis_[c] = {radarCenter_.x + radarR_ * std::cos(a), radarCenter_.y - radarR_ * std::sin(a)};
	}

	for (OrbitSourceModel& m : src_) {
		m.history.clear();
		m.hasLatest = false;
		for (auto& t : m.rgb) t.clear();
		m.luma.clear();
		m.ribbon.rect = {sec_.ribbon.x, sec_.ribbon.y, sec_.ribbon.w, sec_.ribbon.h};
	}
	src_[0].state = fx.state;
	src_[0].generation = fx.generation;
	src_[0].revision = fx.revision;
	src_[1].state = raw ? raw->state : OrbitState::Unavailable;
	src_[1].generation = raw ? raw->generation : 0;
	src_[1].revision = raw ? raw->revision : 0;

	// FX anchors the shared time axis; RAW follows it (own newest only when FX has no samples).
	const uint32_t fn = std::min<uint32_t>(fx.count, kOrbitMaxSamples);
	const bool fxHas = fx.samples && fn > 0;
	const double anchor = fxHas ? fx.samples[fn - 1].t : 0.0;
	if (fxHas) buildSource(src_[0], fx, L, false, 0.0);
	if (raw) buildSource(src_[1], *raw, L, fxHas, anchor);
	// DOM/VIB slots follow the same shared time axis (FX anchor; own newest only when FX has no samples).
	for (size_t i = 0; i < kOrbitPredSlotCount; ++i) {
		OrbitSourceModel& m = src_[2 + i];
		const OrbitSeriesView* v = pred ? &pred[i] : nullptr;
		m.provided = v && v->provided;
		m.state = (m.provided && v->samples && v->count > 0) ? v->state : OrbitState::Unavailable;
		m.generation = (m.provided && v->samples) ? v->generation : 0;
		m.revision = (m.provided && v->samples) ? v->revision : 0;
		if (m.provided) buildSource(m, *v, L, fxHas, anchor);
	}
}

void OrbitConsoleModel::buildSource(OrbitSourceModel& m, const OrbitSeriesView& s, const OrbitLayout& L, bool haveAnchor, double anchorT) {
	const uint32_t n = std::min<uint32_t>(s.count, kOrbitMaxSamples);
	if (!s.samples || n == 0) return;

	// Latest valid measurement: shared by swatch, bars, luminance readout and radar.
	for (uint32_t i = n; i > 0; --i) {
		if (s.samples[i - 1].valid) { m.latest = s.samples[i - 1]; m.hasLatest = true; break; }
	}
	if (m.hasLatest) {
		const float vals[3] = {m.latest.r, m.latest.g, m.latest.b};
		for (int c = 0; c < 3; ++c) {
			const float v = std::min(1.0f, std::max(0.0f, vals[c])); // same [0,1] scale on every axis
			m.radarPt[c] = {radarCenter_.x + (axis_[c].x - radarCenter_.x) * v, radarCenter_.y + (axis_[c].y - radarCenter_.y) * v};
		}
	}

	// ONE bin geometry per source feeds the ribbon, the luminance overlay and the R/G/B traces.
	m.history.build(s.samples, n, L.windowSeconds, kOrbitPeriodSeconds, sec_.ribbon.x, sec_.ribbon.w, L.segmentWidthPx, L.segmentGapPx,
		anchorT, haveAnchor);

	// Traces plot each Valid bin's aggregated value at the CENTRE of its slot; a Gap or Missing bin
	// breaks the line (a stalled/missed sample leaves an empty slot, so lines never bridge it).
	bool open[3] = {false, false, false}, lumaOpen = false;
	for (size_t i = 0; i < m.history.count(); ++i) {
		const HistorySegment& sg = m.history.seg(i);
		if (sg.kind != SegKind::Valid) {
			open[0] = open[1] = open[2] = lumaOpen = false;
			continue;
		}
		const float x = m.history.centerX(i);
		const float vals[3] = {sg.r, sg.g, sg.b};
		for (int c = 0; c < 3; ++c) {
			const float y = sec_.rgb.y + sec_.rgb.h - 1.0f - std::min(1.0f, std::max(0.0f, vals[c])) * (sec_.rgb.h - 2.0f);
			tracePush(m.rgb[c], open[c], x, y);
		}
		const float yl = sec_.ribbon.y + sec_.ribbon.h - 1.0f - std::min(1.0f, std::max(0.0f, sg.luma)) * (sec_.ribbon.h - 2.0f);
		tracePush(m.luma, lumaOpen, x, yl);
	}
}

} // namespace hudpresent
