#pragma once

// HudVitalsDraw.h — OF-dependent draw cache for one system-vitals sparkline
// binding. Owned by HudWireframeRenderer (one per binding), filled in its
// update path, read (never modified) by SparklineWidget's "vitals" variant.
// The ofPolylines are rebuilt only when HudVitalsTrace reports a geometry
// rebuild (new series revision or a changed slot size) — draw() only strokes
// them.

#include "HudVitalsTrace.h"

#include "ofPolyline.h"

#include <vector>

namespace hudpresent {

// Layout shared by the renderer (cache sizing) and the widget (drawing), in
// canonical slot-local pixels. The text column (label + value) sits left of
// the trace plot.
constexpr float kVitalsPad = 4.0f;
constexpr float kVitalsTextColumn = 160.0f; // 20 native 8-px glyph cells

struct HudVitalsDrawData {
	HudVitalsTrace trace;
	sysvitals::Metric metric = sysvitals::Metric::Fps; // which dial this binding draws
	std::vector<ofPolyline> lines;       // one per multi-point segment, slot-local px
	std::vector<HudVitalsTrace::Vertex> dots; // single-point segments
	float slotWidth = 0.0f;
	float slotHeight = 0.0f;
	float plotX = 0.0f, plotY = 0.0f, plotW = 0.0f, plotH = 0.0f;
	uint64_t linesBuiltAtRebuild = 0;    // trace.rebuildCount() the polylines match
	uint64_t polylineRebuilds = 0;       // instrumentation
};

} // namespace hudpresent
