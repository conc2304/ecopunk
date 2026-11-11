#pragma once

// OrbitDraw.h — OF-dependent draw cache for Orbit Console v0.1. Owned by
// HudWireframeRenderer (one instance, shared by the two orbit bindings in
// universal.activity_trace / universal.secondary_trace), filled in its update
// path, read (never modified) by SparklineWidget's "orbit.upper" and
// "orbit.lower" variants. Polylines are rebuilt only when OrbitConsoleModel
// reports a geometry rebuild.

#include "OrbitConsoleModel.h"
#include "OrbitObservationConfig.h"

#include "ofMesh.h"
#include "ofPolyline.h"

#include <vector>

namespace hudpresent {

// Per-source draw geometry (FX = index 0, RAW = index 1). Built only when the model rebuilds.
struct OrbitSourceDraw {
	ofMesh ribbonMesh;                     // per-vertex colours: the actual average RGB of each bin
	std::vector<ofPolyline> rgbLines[3];   // R, G, B traces; one polyline per gap-free segment
	std::vector<OrbitPt> rgbDots[3];       // single-bin segments
	std::vector<ofPolyline> lumaLines;     // relative-luminance polyline over the ribbon
	std::vector<OrbitPt> lumaDots;
};

struct OrbitDrawData {
	OrbitConsoleModel model;
	OrbitSourceDraw src[kOrbitSlotCount]; // slot order: see orbitSlotOf()
	// OC-005: HUD-owned observation configuration + editor state (presentation state, never telemetry).
	// Observation A draws in the upper region, B in the lower; the draw path reads it only.
	OrbitConfigEditor ui;
	float focusPulse = 1.0f; // 0.7..1.0 alpha factor for the focused (not editing) frame; set by the renderer each update
	bool focusA = false, focusB = false; // HUD focus on an observation (set by the renderer's setFocusedControl)
	float upperW = 0, upperH = 0, lowerH = 0;
	uint64_t linesBuiltAtRebuild = 0;
	uint64_t polylineRebuilds = 0;         // instrumentation
};

// Which source model an observation binds to for its selected metric. nullptr when that metric has no
// production data (DOM/VIB): such an observation is an explicit unavailable state, never AVG, never the other
// source. Shared by the draw path and the verification harness so both resolve identically.
inline OrbitSourceId orbitSourceIdOf(OrbitSource s) { return s == OrbitSource::Fx ? OrbitSourceId::Fx : OrbitSourceId::Raw; }
// OC-003: model slot of a (source, metric): 0/1 AVG, 2/3 DOM, 4/5 VIB (FX first in each pair).
inline size_t orbitSlotOf(OrbitSource s, OrbitMetric m) { return m == OrbitMetric::Avg ? size_t(s) : size_t(2) * size_t(m) + size_t(s); }
// The model for an observation's (source, metric), or nullptr when the runtime does not measure it (NOT AVAILABLE).
inline const OrbitSourceModel* orbitBoundSource(const OrbitDrawData& d, OrbitObs o) {
	const OrbitObservationConfig& c = d.ui.config().at(o);
	const OrbitSourceModel& m = d.model.slot(orbitSlotOf(c.source, c.metric));
	return (orbitMetricAlwaysPublished(c.metric) || m.provided) ? &m : nullptr;
}

} // namespace hudpresent
