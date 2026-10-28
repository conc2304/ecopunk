#pragma once

// ============================================================================
// HudVitalsTrace.h — System Telemetry v1 trace geometry + text, cached.
//
// Renderer-private (HUD Runtime). OF-free (standard library + the OF-free
// shared/src/hud-runtime/SystemVitalsSnapshot.h) so geometry, gap handling,
// axis scaling and cache invalidation are testable with a bare compiler.
//
// Consumes ONLY the immutable HudFrameData::systemVitals snapshot. It is NOT
// built on HudHistoryStore (scene-epoch, 64 samples, no timestamps) and never
// touches /proc or /sys.
//
// Geometry is built in slot-local pixel coordinates (origin = the slot's top
// left) from the series' own timestamps:
//   x = (t - (tNewest - window)) / window * plotWidth
// so irregular intervals and sampling gaps are visible as spacing. The right
// edge is the series' newest entry (a gap marker counts), which keeps the
// geometry a pure function of the series revision + layout — the cache key.
// Entries older than the window are skipped. The line BREAKS at every
// invalid (gap) entry and across any timestamp jump larger than
// kBreakPeriods nominal periods, so nothing is interpolated across missing
// data and no gap is drawn as a 0.
//
// update() is called from the renderer's update path, never from draw().
// Everything is fixed-capacity; update() performs no heap allocation.
// ============================================================================

#include "SystemVitalsSnapshot.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace hudpresent {

// Non-owning view of one snapshot series (valid only while the snapshot is
// alive; the trace copies what it needs).
struct VitalsSeriesView {
	sysvitals::State state = sysvitals::State::Unavailable;
	bool hasLatest = false;
	float latest = 0.0f;
	uint64_t revision = 0;
	const sysvitals::Sample* history = nullptr;
	uint32_t count = 0;
};

template <size_t N>
VitalsSeriesView vitalsViewOf(const sysvitals::SeriesSnapshot<N>& s) {
	VitalsSeriesView v;
	v.state = s.state;
	v.hasLatest = s.hasLatest;
	v.latest = s.latest;
	v.revision = s.revision;
	v.history = s.history.data();
	v.count = s.count;
	return v;
}

// A discontinuity longer than this many nominal periods breaks the line.
constexpr double kVitalsBreakPeriods = 4.0;

struct VitalsMetricSpec {
	const char* sourceId;       // runtime.vitals.<m> — the binding's "value" source
	sysvitals::Metric metric;
	const char* shortLabel;     // fallback caption (the binding normally supplies a vocabulary label)
	const char* unit;           // appended to the value text ("Hz", "ms", "%", "C", "MiB")
	int decimals;
	double windowSeconds;       // 60 or 300
	double nominalPeriod;       // seconds, for break detection
	bool fixedMin; float minValue;
	bool fixedMax; float maxValue;
	float minSpan;              // auto-range floor
	float floorMax;             // auto max is at least this (0 = none)
	float referenceValue;       // horizontal reference line; <= 0 = none
};

// nullptr for a source id that is not one of the five runtime.vitals.* scalars.
const VitalsMetricSpec* vitalsSpecForSource(const std::string& sourceId);
const VitalsMetricSpec& vitalsSpecFor(sysvitals::Metric metric);

constexpr size_t kVitalsMaxPoints = 300; // largest history capacity (RSS)

class HudVitalsTrace {
public:
	struct Vertex { float x = 0.0f, y = 0.0f; };
	struct Segment { uint16_t first = 0, count = 0; }; // indices into vertices(); count >= 1
	static constexpr size_t kMaxSegments = kVitalsMaxPoints; // worst case: every point its own segment

	// Rebuilds geometry only if the series revision, spec, or plot size
	// changed since the last build. Always refreshes the cheap state/text
	// fields. Returns true iff geometry was rebuilt.
	bool update(const VitalsMetricSpec& spec, const VitalsSeriesView& series, float plotWidth, float plotHeight);

	sysvitals::State state() const { return state_; }
	bool hasLatest() const { return hasLatest_; }
	// Pre-formatted "59.8 Hz" etc.; "--" when Unavailable. Valid zeros print as zeros.
	const char* valueText() const { return valueText_; }

	size_t vertexCount() const { return vertexCount_; }
	size_t segmentCount() const { return segmentCount_; }
	const Vertex& vertex(size_t i) const { return vertices_[i]; }
	const Segment& segment(size_t i) const { return segments_[i]; }
	// Slot-local y of the reference line, valid when hasReference().
	bool hasReference() const { return hasReference_; }
	float referenceY() const { return referenceY_; }
	// Slot-local position of the newest VALID point (Valid state only).
	bool hasLatestPoint() const { return hasLatestPoint_; }
	Vertex latestPoint() const { return latestPoint_; }
	float axisMin() const { return axisMin_; }
	float axisMax() const { return axisMax_; }

	uint64_t rebuildCount() const { return rebuildCount_; }
	void invalidate() { built_ = false; }

private:
	void rebuild(const VitalsMetricSpec& spec, const VitalsSeriesView& series, float plotWidth, float plotHeight);

	std::array<Vertex, kVitalsMaxPoints> vertices_{};
	std::array<Segment, kMaxSegments> segments_{};
	size_t vertexCount_ = 0;
	size_t segmentCount_ = 0;
	bool hasReference_ = false;
	float referenceY_ = 0.0f;
	bool hasLatestPoint_ = false;
	Vertex latestPoint_{};
	float axisMin_ = 0.0f, axisMax_ = 1.0f;

	sysvitals::State state_ = sysvitals::State::Unavailable;
	bool hasLatest_ = false;
	char valueText_[24] = "--";

	// Cache key.
	bool built_ = false;
	const VitalsMetricSpec* builtSpec_ = nullptr;
	uint64_t builtRevision_ = 0;
	float builtW_ = -1.0f, builtH_ = -1.0f;
	uint64_t rebuildCount_ = 0;
};

} // namespace hudpresent
