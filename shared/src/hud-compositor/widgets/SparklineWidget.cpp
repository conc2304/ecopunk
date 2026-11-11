#include "SparklineWidget.h"

#include "HudWidgetDrawUtils.h"
#include "../HudVitalsDraw.h"
#include "../HudVitalsDial.h"
#include "../OrbitDraw.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include <algorithm>

namespace hudpresent {

// System Performance "vitals" variant: one semicircular arc dial per metric
// (label top, arc middle, value + unit bottom). Fill length = the normalised
// measurement; fill colour = metric health (HudVitalsDial.h). Reads only the
// renderer-cached HudVitalsTrace state/text — no history reads or allocation
// beyond a reused arc mesh.
void SparklineWidget::drawVitals(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const HudVitalsDrawData& d = *input.vitals;
	const auto state = d.trace.state();
	const bool valid = state == sysvitals::State::Valid && d.trace.hasLatest();
	const bool stale = state == sysvitals::State::Stale && d.trace.hasLatest();
	const VitalsDialSpec* dial = &vitalsDialSpec(d.metric);

	ofPushStyle();
	ofEnableAlphaBlending();

	const float cx = bounds.x + bounds.width * 0.5f;
	// Label (top, centred, caption colour).
	ofSetColor(colors.caption);
	if (!input.captionText.empty()) {
		const auto& caption = textCache_.truncateToWidth(input.captionText, bounds.width - 2.0f * kVitalsPad);
		drawText(caption.text, cx - caption.widthLocal * 0.5f, bounds.y + 6.0f + kBitmapCapHeight);
	}

	// Arc (top half, 180 -> 360 degrees).
	const float r = std::max(8.0f, std::min(bounds.width * 0.5f - 8.0f, bounds.height * 0.34f));
	const float thick = std::max(3.0f, r * 0.2f);
	const float cy = bounds.y + bounds.height * 0.60f;
	constexpr int kSeg = 36;
	static ofMesh mesh;
	auto strip = [&](float frac, const ofColor& c) {
		const int n = std::max(1, static_cast<int>(std::ceil(kSeg * frac)));
		mesh.clear();
		mesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
		for (int i = 0; i <= n; ++i) {
			const float a = PI + PI * frac * (static_cast<float>(i) / static_cast<float>(n));
			const float ca = std::cos(a), sa = std::sin(a);
			mesh.addVertex({cx + ca * r, cy + sa * r, 0.0f});
			mesh.addVertex({cx + ca * (r - thick), cy + sa * (r - thick), 0.0f});
			mesh.addColor(c);
			mesh.addColor(c);
		}
		mesh.draw();
	};
	strip(1.0f, scaledAlpha(colors.muted, 0.6f)); // track
	ofColor valueColor = colors.muted;
	if ((valid || stale) && dial) {
		const float v = d.trace.latestValue();
		const float frac = vitalsDialFraction(*dial, v);
		static const ofColor kGood(0x9B, 0xCD, 0xAA), kOk(0xE5, 0xB9, 0x6D), kBad(0xDF, 0x77, 0x6B);
		const VitalsHealth h = vitalsDialHealth(*dial, v);
		ofColor c = h == VitalsHealth::Good ? kGood : h == VitalsHealth::Ok ? kOk : kBad;
		if (stale) c = colors.muted; // stale never looks live
		if (frac > 0.0f) strip(frac, c);
		valueColor = stale ? colors.muted : colors.primary;
	}

	// Value + unit (bottom, centred); honest placeholders when there is no live reading.
	const char* text = d.trace.valueText();
	if (state == sysvitals::State::Unavailable) text = "NO DATA";
	const float tw = 8.0f * static_cast<float>(std::strlen(text));
	ofSetColor(valueColor);
	drawText(text, cx - tw * 0.5f, bounds.y + bounds.height - 10.0f);
	if (stale) {
		ofSetColor(colors.warning);
		drawText("STALE", cx - 20.0f, cy - r * 0.3f);
	}
	ofPopStyle();
}


namespace {
ofColor measured(float r, float g, float b, float a = 1.0f) {
	auto c8 = [](float v) { return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
	return ofColor(c8(r), c8(g), c8(b), c8(a));
}
} // namespace

// Orbit Console v0.1. "orbit.upper" (universal.activity_trace): chromatic orbit,
// average swatch, RGB bars with numbers, RGB time-series chart. "orbit.lower"
// (universal.secondary_trace): relative-luminance history with the colour ribbon
// directly beneath. Strokes pre-built geometry from OrbitDrawData (rebuilt in
// the renderer's update path by series revision/generation/layout) — no history
// reads, no geometry building, no GPU readback here. Unavailable draws an
// honest "NO DATA"; Stale dims every reading and says so.
void SparklineWidget::drawOrbit(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	const OrbitDrawData& d = *input.orbit;
	const OrbitConsoleModel& m = d.model;
	const bool upper = input.variantId == "orbit.upper"; // OC-005: upper region = observation A, lower = observation B
	const OrbitObs obs = upper ? OrbitObs::A : OrbitObs::B;
	const float sectionH = upper ? d.upperH : d.lowerH;
	if (std::fabs(bounds.width - d.upperW) > 0.5f || std::fabs(bounds.height - sectionH) > 0.5f) return; // layout not yet built for this size

	// The other observation's region hosts the editor overlay while one observation is edited.
	if (d.ui.isOpen() && d.ui.overlayRegion() == obs) { drawOrbitEditor(bounds, colors, d, d.ui.target()); return; }

	const OrbitObservationConfig cfg = d.ui.config().at(obs);
	const OrbitSectionLayout& Ls = m.section();
	const size_t slot = orbitSlotOf(cfg.source, cfg.metric); // OC-003: one model + draw slot per (source, metric)
	const OrbitSourceModel& S = m.slot(slot);
	const OrbitSourceDraw& D = d.src[slot];
	// AVG is always published; DOM/VIB only when the runtime measures them for this source (opt-in). Otherwise an
	// explicit NOT AVAILABLE state: never AVG, never the other source, never another metric.
	const bool metricLive = orbitMetricAlwaysPublished(cfg.metric) || S.provided;
	const bool valid = metricLive && S.state == OrbitState::Valid;
	const bool stale = metricLive && S.state == OrbitState::Stale;
	const bool unavailable = !metricLive || S.state == OrbitState::Unavailable || !S.hasLatest;
	const ofColor accent = upper ? colors.dataA : colors.dataB; // observation A / B series colours
	char titleBuf[16];
	std::snprintf(titleBuf, sizeof(titleBuf), "%s %s/%s", orbitObsText(obs), orbitSourceText(cfg.source), orbitMetricText(cfg.metric));
	const char* title = titleBuf;
	char noDataBuf[24];
	const char* noData = noDataBuf;
	if (!metricLive) std::snprintf(noDataBuf, sizeof(noDataBuf), "%s NOT AVAILABLE", orbitMetricText(cfg.metric));
	else if (cfg.metric != OrbitMetric::Avg) std::snprintf(noDataBuf, sizeof(noDataBuf), "NO %s COLOR", orbitMetricText(cfg.metric)); // measured, but no such colour right now
	else std::snprintf(noDataBuf, sizeof(noDataBuf), "%s", cfg.source == OrbitSource::Fx ? "NO DATA" : "NO RAW VIDEO");

	ofPushStyle();
	ofEnableAlphaBlending();
	ofPushMatrix();
	ofTranslate(bounds.x, bounds.y);
	ofNoFill();
	ofSetLineWidth(1.0f);
	const float k = (valid || !metricLive || !S.hasLatest) ? 1.0f : 0.45f; // stale readings never look live
	const OrbitSample L = metricLive ? S.latest : OrbitSample {};
	char buf[32];
	const ofColor markCol[3] = {colors.warning, colors.dataA, colors.dataB};

	// -- Average swatch, labelled with the source (the label is the truthful source distinction).
	{
		const OrbitRect sw = Ls.swatch;
		if (!unavailable) {
			ofFill();
			ofSetColor(measured(L.r, L.g, L.b, k));
			ofDrawRectangle(sw.x, sw.y, sw.w, sw.h);
			ofSetColor(L.luma > 0.5f ? ofColor(10, 12, 10) : ofColor(235, 240, 230));
			drawText(title, sw.x + 4.0f, sw.y + 11.0f);
		} else {
			ofSetColor(colors.muted);
			drawText(title, sw.x + 4.0f, sw.y + 11.0f);
		}
		ofNoFill();
		ofSetColor(scaledAlpha(accent, 0.9f * k));
		ofDrawRectangle(sw.x, sw.y, sw.w, sw.h);
	}

	// -- RGB bars with numeric readings (display-encoded, same [0,1] scale as the radar).
	{
		const OrbitRect br = Ls.bars;
		const char* names[3] = {"R", "G", "B"};
		const float vals[3] = {L.r, L.g, L.b};
		const float barX = br.x + 10.0f, barW = 34.0f;
		for (int i = 0; i < 3; ++i) {
			const float ry = br.y + 13.0f * i;
			ofNoFill();
			ofSetColor(scaledAlpha(colors.caption, k));
			drawText(names[i], br.x, ry + 10.5f);
			ofSetColor(scaledAlpha(colors.muted, 0.9f));
			ofDrawRectangle(barX, ry + 2.5f, barW, 7.0f);
			if (!unavailable) {
				ofFill();
				ofSetColor(markCol[i].r, markCol[i].g, markCol[i].b, static_cast<int>(255 * k));
				ofDrawRectangle(barX, ry + 2.5f, barW * std::clamp(vals[i], 0.0f, 1.0f), 7.0f);
				std::snprintf(buf, sizeof(buf), "%.2f", vals[i]);
				ofSetColor(valid ? colors.primary : colors.muted);
			} else {
				std::snprintf(buf, sizeof(buf), "--");
				ofSetColor(colors.muted);
			}
			drawText(buf, barX + barW + 4.0f, ry + 10.5f);
		}
		// Relative luminance (linear), the numeric twin of the overlaid luminance line.
		ofNoFill();
		if (!unavailable) { std::snprintf(buf, sizeof(buf), "Y %.2f", L.luma); ofSetColor(valid ? colors.primary : colors.muted); }
		else { std::snprintf(buf, sizeof(buf), "Y --"); ofSetColor(colors.muted); }
		drawText(buf, Ls.lumaRow.x, Ls.lumaRow.y + 10.5f);
	}

	// -- Radar (upper section): ONE three-axis R/G/B radar, FX and RAW overlaid on the same [0,1]
	// scale. Axes are channel values, not chromaticity coordinates. Lower section: its legend.
	if (upper) {
		const OrbitPt c = m.radarCenter();
		ofNoFill();
		for (float ring : {0.5f, 1.0f}) {
			ofSetColor(scaledAlpha(colors.muted, ring < 1.0f ? 0.45f : 0.9f));
			ofDrawTriangle(c.x + (m.radarAxisEnd(0).x - c.x) * ring, c.y + (m.radarAxisEnd(0).y - c.y) * ring,
				c.x + (m.radarAxisEnd(1).x - c.x) * ring, c.y + (m.radarAxisEnd(1).y - c.y) * ring,
				c.x + (m.radarAxisEnd(2).x - c.x) * ring, c.y + (m.radarAxisEnd(2).y - c.y) * ring);
		}
		ofSetColor(scaledAlpha(colors.muted, 0.6f));
		for (int a = 0; a < 3; ++a) ofDrawLine(c.x, c.y, m.radarAxisEnd(a).x, m.radarAxisEnd(a).y);
		ofSetColor(scaledAlpha(colors.caption, 0.9f));
		drawText("R", m.radarAxisEnd(0).x + 4.0f, m.radarAxisEnd(0).y + 5.0f);
		drawText("G", m.radarAxisEnd(1).x - 10.0f, m.radarAxisEnd(1).y + 11.0f);
		drawText("B", m.radarAxisEnd(2).x + 3.0f, m.radarAxisEnd(2).y + 11.0f);
		// OC-005: the radar compares observation A and B, each with ITS selected source's RGB . The two polygons are independent measurements, not frame-synchronised and not
		// necessarily the same algorithm. An unavailable observation (no data, DOM/VIB) draws no polygon.
		for (int oi = 1; oi >= 0; --oi) { // B first so A reads on top
			const OrbitObservationConfig oc = d.ui.config().at(OrbitObs(oi));
			const OrbitSourceModel& P = m.slot(orbitSlotOf(oc.source, oc.metric));
			if (!orbitMetricAlwaysPublished(oc.metric) && !P.provided) continue;
			if (!P.hasLatest || P.state == OrbitState::Unavailable) continue; // no fabricated / held polygon for an unavailable observation
			const ofColor ac = oi == 0 ? colors.dataA : colors.dataB;
			const float pk = P.state == OrbitState::Valid ? 1.0f : 0.45f;
			ofFill();
			ofSetColor(ac.r, ac.g, ac.b, static_cast<int>(48 * pk));
			ofDrawTriangle(P.radarPt[0].x, P.radarPt[0].y, P.radarPt[1].x, P.radarPt[1].y, P.radarPt[2].x, P.radarPt[2].y);
			ofNoFill();
			ofSetLineWidth(1.4f);
			ofSetColor(ac.r, ac.g, ac.b, static_cast<int>(255 * pk));
			ofDrawTriangle(P.radarPt[0].x, P.radarPt[0].y, P.radarPt[1].x, P.radarPt[1].y, P.radarPt[2].x, P.radarPt[2].y);
			ofSetLineWidth(1.0f);
		}
	} else {
		// OC-005: the radar legend names the comparison A/B and each side's configured source/metric.
		const float lx = Ls.radar.x + 2.0f;
		const OrbitConsoleConfig& cc = d.ui.config();
		char sub[16];
		ofFill();
		ofSetColor(colors.dataA); ofDrawRectangle(lx, 8.0f, 10.0f, 3.0f);
		ofSetColor(colors.dataB); ofDrawRectangle(lx, 33.0f, 10.0f, 3.0f);
		ofSetColor(colors.caption);
		drawText("A", lx + 14.0f, 12.0f);
		drawText("B", lx + 14.0f, 37.0f);
		ofSetColor(colors.muted);
		std::snprintf(sub, sizeof(sub), "%s/%s", orbitSourceText(cc.a.source), orbitMetricText(cc.a.metric));
		drawText(sub, lx, 22.0f);
		std::snprintf(sub, sizeof(sub), "%s/%s", orbitSourceText(cc.b.source), orbitMetricText(cc.b.metric));
		drawText(sub, lx, 47.0f);
		drawText("AXES 0-1", lx, 62.0f);
	}

	// -- Source accent stripe.
	ofFill();
	ofSetColor(scaledAlpha(accent, 0.9f * k));
	ofDrawRectangle(Ls.accent.x, Ls.accent.y, Ls.accent.w, Ls.accent.h);

	// -- Colour ribbon: each bin is filled with the ACTUAL average RGB (never the accent), with the
	// relative-luminance polyline overlaid on a dark halo for contrast on bright and dark bins.
	const OrbitRect rb = Ls.ribbon, rg = Ls.rgb;
	ofFill();
	ofSetColor(255, 255, 255, 255); // vertex colours carry the measurement
	if (metricLive && D.ribbonMesh.getNumVertices() > 0) D.ribbonMesh.draw(); // never draw an empty mesh: stale GL vertex state paints grey slabs
	if (stale) { ofSetColor(0, 0, 0, 150); ofDrawRectangle(rb.x, rb.y, rb.w, rb.h); }
	ofNoFill();
	ofSetLineWidth(3.4f);
	ofSetColor(0, 0, 0, static_cast<int>(210 * k));
	if (metricLive) for (const auto& line : D.lumaLines) line.draw();
	ofSetLineWidth(1.3f);
	ofSetColor(255, 255, 255, static_cast<int>(255 * k));
	if (metricLive) for (const auto& line : D.lumaLines) line.draw();
	ofFill();
	if (metricLive) for (const auto& dot : D.lumaDots) {
		ofSetColor(0, 0, 0, static_cast<int>(210 * k)); ofDrawCircle(dot.x, dot.y, 2.2f);
		ofSetColor(255, 255, 255, static_cast<int>(255 * k)); ofDrawCircle(dot.x, dot.y, 1.2f);
	}
	ofNoFill();
	ofSetLineWidth(1.0f);
	ofSetColor(scaledAlpha(colors.muted, 0.9f));
	ofDrawRectangle(rb.x, rb.y, rb.w, rb.h);
	if (unavailable) { ofSetColor(colors.muted); drawText(noData, rb.x + 6.0f, rb.y + rb.h * 0.5f + 4.5f); }
	else if (stale) { ofSetColor(colors.warning); drawText(cfg.source == OrbitSource::Fx ? "STALE" : "RAW STALE", rb.x + 6.0f, rb.y + rb.h * 0.5f + 4.5f); }

	// -- Compact R/G/B time series directly below: same x as the ribbon, same bins and gaps.
	ofNoFill();
	ofSetColor(scaledAlpha(colors.muted, 0.9f));
	ofDrawRectangle(rg.x, rg.y, rg.w, rg.h);
	ofSetColor(scaledAlpha(colors.muted, 0.3f));
	ofDrawLine(rg.x, rg.y + rg.h * 0.5f, rg.x + rg.w, rg.y + rg.h * 0.5f);
	ofSetLineWidth(1.2f);
	for (int c = 0; c < 3; ++c) {
		ofSetColor(markCol[c].r, markCol[c].g, markCol[c].b, static_cast<int>(230 * k));
		if (metricLive) for (const auto& line : D.rgbLines[c]) line.draw();
	}
	ofFill();
	for (int c = 0; c < 3; ++c) {
		ofSetColor(markCol[c].r, markCol[c].g, markCol[c].b, static_cast<int>(230 * k));
		if (metricLive) for (const auto& dot : D.rgbDots[c]) ofDrawCircle(dot.x, dot.y, 1.0f);
	}

	// -- OC-005 focus / edit indication: a frame around the whole observation region + a short tag. The
	// observation stays fully visible; the frame is HUD focus, not data.
	{
		const bool focused = upper ? d.focusA : d.focusB;
		const bool editing = d.ui.isOpen() && d.ui.target() == obs;
		if (focused || editing) {
			ofNoFill();
			ofSetLineWidth(editing ? 2.0f : 1.6f);
			ofColor frameCol = editing ? colors.warning : colors.accent;
			if (!editing) frameCol.a = static_cast<unsigned char>(float(frameCol.a) * d.focusPulse + 0.5f); // subtle focus pulse
			ofSetColor(frameCol);
			ofDrawRectangle(0.5f, 0.5f, bounds.width - 1.0f, bounds.height - 1.0f);
			ofSetLineWidth(1.0f);
			const char* tag = editing ? "EDITING" : "ENTER=EDIT";
			ofFill();
			ofSetColor(0, 0, 0, 170);
			ofDrawRectangle(bounds.width - 8.0f * float(std::strlen(tag)) - 8.0f, 0.0f, 8.0f * float(std::strlen(tag)) + 8.0f, 13.0f);
			ofSetColor(editing ? colors.warning : colors.accent);
			drawText(tag, bounds.width - 8.0f * float(std::strlen(tag)) - 4.0f, 10.5f);
		}
	}

	ofPopMatrix();
	ofPopStyle();
}

// OC-005 editor overlay: replaces the WHOLE region of the observation that is NOT being edited (so it can
// never overlap the clock, navigation controls or any other HUD region). Left: source FX / RAW; right: metric
// AVG / DOM / VIB; DONE closes and commits. The cursor is a bright frame; the selected option is filled.
void SparklineWidget::drawOrbitEditor(const ofRectangle& bounds, const HudWidgetColors& colors, const OrbitDrawData& d, OrbitObs edited) const {
	const OrbitConfigEditor& ui = d.ui;
	const OrbitEditorLayout lay = orbitEditorLayout(bounds.width, bounds.height);
	ofPushStyle();
	ofEnableAlphaBlending();
	ofPushMatrix();
	ofTranslate(bounds.x, bounds.y);
	ofFill();
	ofSetColor(0, 0, 0, 205);
	ofDrawRectangle(0.0f, 0.0f, bounds.width, bounds.height);
	ofNoFill();
	ofSetLineWidth(1.0f);
	ofSetColor(colors.warning);
	ofDrawRectangle(0.5f, 0.5f, bounds.width - 1.0f, bounds.height - 1.0f);

	char buf[16];
	std::snprintf(buf, sizeof(buf), "EDIT %s", orbitObsText(edited));
	ofSetColor(colors.warning);
	drawText(buf, lay.title.x, lay.title.y + 10.5f);
	ofSetColor(colors.caption);
	drawText("SOURCE", lay.sourceHeader.x, lay.sourceHeader.y + 10.5f);
	drawText("METRIC", lay.metricHeader.x, lay.metricHeader.y + 10.5f);

	static const char* const kLabel[kOrbitEditItemCount] = {"FX", "RAW", "AVG", "DOM", "VIB", "DONE"};
	for (int i = 0; i < kOrbitEditItemCount; ++i) {
		const OrbitEditItem it = OrbitEditItem(i);
		const OrbitEditRect& r = lay.item[size_t(i)];
		const bool sel = ui.selected(it);
		const bool cur = ui.cursor() == it;
		const bool metricNa = (it == OrbitEditItem::MetDom || it == OrbitEditItem::MetVib);
		if (sel) {
			ofFill();
			ofSetColor(colors.accent.r, colors.accent.g, colors.accent.b, 70);
			ofDrawRectangle(r.x, r.y, r.w, r.h);
		}
		ofNoFill();
		ofSetLineWidth(cur ? 2.4f : 1.0f);
		ofSetColor(cur ? colors.accent : colors.muted);
		ofDrawRectangle(r.x, r.y, r.w, r.h);
		ofSetLineWidth(1.0f);
		const float tw = 8.0f * float(std::strlen(kLabel[i]));
		ofSetColor(sel || cur ? colors.primary : colors.caption);
		const bool done = it == OrbitEditItem::Done;
		drawText(kLabel[i], r.x + (r.w - tw) * 0.5f, r.y + (done ? 10.5f : r.h * 0.5f - (metricNa ? 1.0f : -4.0f)));
		if (metricNa) { // explicit: no production data behind DOM / VIB
			const char* na = "NO DATA";
			ofSetColor(colors.muted);
			drawText(na, r.x + (r.w - 8.0f * float(std::strlen(na))) * 0.5f, r.y + r.h * 0.5f + 12.0f);
		}
	}
	ofPopMatrix();
	ofPopStyle();
}

void SparklineWidget::draw(const ofRectangle& bounds, const HudWidgetColors& colors, const HudWidgetInput& input) const {
	if (input.orbit) {
		// Diagnostic only (EXPERIENCE_RUNTIME_ORBIT_DRAW=0, default on): skip the Orbit draw so HUD draw
		// cost can be compared with/without it. Model/snapshot work and every other widget are unchanged.
		static const bool orbitDrawOn = [] { const char* e = std::getenv("EXPERIENCE_RUNTIME_ORBIT_DRAW"); return !(e && e[0] == '0'); }();
		if (orbitDrawOn) drawOrbit(bounds, colors, input);
		return;
	}
	if (input.vitals) { drawVitals(bounds, colors, input); return; }
	if (input.historySamples.empty()) return; // nothing tracked yet — not an error, just no data

	ofPushStyle();
	ofEnableAlphaBlending();
	ofNoFill();

	if (!input.captionText.empty()) {
		ofFill();
		ofSetColor(scaledAlpha(captionColor(colors, input.findRole("value")), 1.0f));
		// Fixed native pixel-space budget (see HudWidgetDrawUtils.h's v1
		// typography strategy comment) — not scale-adjusted.
		const auto& caption = textCache_.truncateToWidth(input.captionText, bounds.width);
		drawText(caption.text, bounds.x, bounds.y + wu(bounds, 10.0f));
		ofNoFill();
	}

	float minV = input.historyStats.hasData ? input.historyStats.minValue : 0.0f;
	float maxV = input.historyStats.hasData ? input.historyStats.maxValue : 1.0f;
	float range = std::max(1e-6f, maxV - minV);

	float plotTop = bounds.y + wu(bounds, 14.0f);
	float plotHeight = std::max(1.0f, bounds.height - wu(bounds, 18.0f));

	ofPolyline line;
	bool building = false;
	size_t n = input.historySamples.size();
	for (size_t i = 0; i < n; ++i) {
		const auto& sample = input.historySamples[i];
		float u = (n > 1) ? static_cast<float>(i) / static_cast<float>(n - 1) : 0.0f;
		float x = bounds.x + u * bounds.width;
		if (sample.gap) {
			// A gap breaks the polyline into a separate segment rather
			// than interpolating across it or drawing a fabricated 0 —
			// "never insert zero into a history for a missing source"
			// applies to what's DRAWN here too, not just what's stored.
			if (building && line.size() > 1) {
				ofSetColor(scaledAlpha(colors.secondary, 0.85f));
				ofSetLineWidth(std::max(1.0f, wu(bounds, 1.2f)));
				line.draw();
			}
			line.clear();
			building = false;
			continue;
		}
		float normalized = (sample.value - minV) / range;
		float y = plotTop + (1.0f - std::clamp(normalized, 0.0f, 1.0f)) * plotHeight;
		line.addVertex(x, y);
		building = true;
	}
	if (building && line.size() > 1) {
		ofSetColor(scaledAlpha(colors.secondary, 0.85f));
		ofSetLineWidth(std::max(1.0f, wu(bounds, 1.2f)));
		line.draw();
	}

	if (input.historyStats.hasData && input.historyStats.eventPulse) {
		ofFill();
		ofSetColor(scaledAlpha(colors.accent, 0.9f));
		float x = bounds.x + bounds.width;
		float y = plotTop + (1.0f - std::clamp((input.historySamples.back().value - minV) / range, 0.0f, 1.0f)) * plotHeight;
		ofDrawCircle(x, y, wu(bounds, 2.5f));
	}

	ofPopStyle();
}

} // namespace hudpresent
