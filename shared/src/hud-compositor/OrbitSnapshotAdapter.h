#pragma once

// ============================================================================
// OrbitSnapshotAdapter.h - OC-003: maps the ratified scenecolor::SceneColorSnapshot to the Orbit
// presentation series of ONE metric (AVG / DOM / VIB). OF-free, allocation-free.
//
// Colour is display-encoded (sRGB OETF of the snapshot's linear light) for swatch/ribbon/bars/radar; the
// luminance value stays the linear relative luminance Y of the metric's own colour. A sample whose metric
// is not valid is an explicit gap (valid == false): nothing is copied from AVG, the other metric or an
// earlier sample.
//
// Per-metric state (so DOM/VIB can be unavailable independently of AVG and of each other):
//   snapshot Unavailable                          -> Unavailable
//   no sample in the history has this metric valid -> Unavailable
//   snapshot Stale                                 -> Stale
//   snapshot Valid, NEWEST sample has the metric   -> Valid
//   snapshot Valid, newest sample lacks the metric -> Unavailable ("no such colour right now"; the
//                                                    earlier bins stay visible as history, the live
//                                                    readout never shows an old colour)
// AVG keeps the snapshot's own state unchanged.
// ============================================================================

#include "../hud-runtime/SceneColorSnapshot.h"
#include "OrbitConsoleModel.h"
#include "OrbitObservationConfig.h"

#include <algorithm>

namespace hudpresent {

inline bool orbitSampleHas(const scenecolor::Sample& s, OrbitMetric m) {
	return m == OrbitMetric::Avg ? s.valid : m == OrbitMetric::Dom ? (s.valid && s.domValid) : (s.valid && s.vibValid);
}

// Fills out[0..n) (n returned, <= kOrbitMaxSamples) with the series of metric m.
inline uint32_t orbitFillSamples(const scenecolor::SceneColorSnapshot& snap, OrbitMetric m, OrbitSample* out) {
	const uint32_t n = std::min<uint32_t>(snap.count, static_cast<uint32_t>(kOrbitMaxSamples));
	for (uint32_t i = 0; i < n; ++i) {
		const scenecolor::Sample& in = snap.history[i];
		OrbitSample& o = out[i];
		o = OrbitSample {};
		o.t = in.t;
		o.valid = orbitSampleHas(in, m);
		if (!o.valid) continue;
		const float lr = m == OrbitMetric::Avg ? in.r : m == OrbitMetric::Dom ? in.domR : in.vibR;
		const float lg = m == OrbitMetric::Avg ? in.g : m == OrbitMetric::Dom ? in.domG : in.vibG;
		const float lb = m == OrbitMetric::Avg ? in.b : m == OrbitMetric::Dom ? in.domB : in.vibB;
		o.r = srgbEncode(lr);
		o.g = srgbEncode(lg);
		o.b = srgbEncode(lb);
		o.luma = m == OrbitMetric::Avg ? in.luma : 0.2126f * lr + 0.7152f * lg + 0.0722f * lb;
	}
	return n;
}

inline OrbitState orbitMetricState(const scenecolor::SceneColorSnapshot& snap, OrbitMetric m) {
	if (snap.state == scenecolor::State::Unavailable) return OrbitState::Unavailable;
	if (m == OrbitMetric::Avg) return snap.state == scenecolor::State::Valid ? OrbitState::Valid : OrbitState::Stale;
	const uint32_t n = std::min<uint32_t>(snap.count, static_cast<uint32_t>(kOrbitMaxSamples));
	bool any = false;
	for (uint32_t i = 0; i < n && !any; ++i) any = orbitSampleHas(snap.history[i], m);
	if (!any) return OrbitState::Unavailable;
	if (snap.state == scenecolor::State::Stale) return OrbitState::Stale;
	return (n > 0 && orbitSampleHas(snap.history[n - 1], m)) ? OrbitState::Valid : OrbitState::Unavailable;
}

} // namespace hudpresent
