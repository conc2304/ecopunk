#pragma once

// ============================================================================
// OrbitConsoleModel.h — Orbit Console v0.1 dual-source presentation core (HUD Runtime).
//
// Renderer-private and OF-free (standard library only). Turns TWO independent sample series
// into cached slot-local geometry for the Orbit instruments:
//   FX OUTPUT  = the final scene SceneFBO after effects, before the HUD (runtime.scene_color)
//   RAW VIDEO  = decoded source video BEFORE scene shaders/effects (no runtime source yet:
//                an absent/empty view is "unavailable" and is NEVER filled from the FX series)
// Each source has its own identity (generation, revision, state) and its own geometry:
//   swatch + RGB bars + relative-luminance value (left column),
//   one large segmented colour ribbon (actual average RGB per bin) with the relative-luminance
//   polyline overlaid, and a compact R/G/B time series directly below it (right group),
// and both sources share ONE three-axis R/G/B radar (values normalised to [0,1], same scale).
//
// Time axis: oldest on the LEFT, newest on the RIGHT, one projection for every chart of both
// sources (SegmentedHistoryGeometry): the newest slot is anchored to the right edge, a slot holds
// the bin floor((tAnchor - t + period/2) / binSeconds) and the visible slot count is derived from
// the drawable width and the segment width/gap (kOrbitHistoryTuning). Samples that share a bin
// are averaged (linear light), nothing is interpolated, an empty bin is Missing, an all-invalid
// bin is a Gap. Both sources anchor on the FX series' newest timestamp (RAW anchors on its own
// newest only when FX has no samples), so equal timestamps sit at equal x. update() rebuilds only
// when a source's (revision, generation), the presence of RAW, or the layout changes.
// ============================================================================

#include "SegmentedHistory.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace hudpresent {

struct OrbitSample {
	double t = 0.0;
	float r = 0, g = 0, b = 0, luma = 0;
	bool valid = false;
};

enum class OrbitState : uint8_t { Unavailable,
	Valid,
	Stale };

struct OrbitSeriesView {
	OrbitState state = OrbitState::Unavailable;
	uint64_t generation = 0;
	uint64_t revision = 0;
	const OrbitSample * samples = nullptr; // oldest -> newest
	uint32_t count = 0;
};

// ---- Orbit history tuning (code-only; no user setting, no persistence) --------------------------
// Segment width and the gap between segments, in slot-local px, for EVERY history ribbon and
// trace of both sources. The number of displayed temporal bins is derived from the drawable width:
// floor((width + gap) / (segmentWidthPx + gap)), bounded to [1, kHistoryMaxSegments]; zero/negative
// widths fall back to the defaults and the gap is clamped to [0, kOrbitMaxGapPx]. Changing these
// changes the bin count (each bin = window / count and aggregates the samples inside it), never the
// 2 Hz sampler cadence or the 120-sample history capacity. Defaults give ~71 bins at the canonical
// 239.6 px group width. Verification-only override: EXPERIENCE_RUNTIME_ORBIT_SEG_WIDTH_PX / _GAP_PX.
struct OrbitHistoryTuning {
	float segmentWidthPx;
	float segmentGapPx;
};
constexpr OrbitHistoryTuning kOrbitHistoryTuning = { 16.2f, 2.75f };
// constexpr OrbitHistoryTuning kOrbitHistoryTuning = {2.6f, 0.75f};
constexpr float kOrbitMaxGapPx = 8.0f;

struct OrbitLayout {
	// Two coordinated sections (universal.activity_trace above = FX, universal.secondary_trace
	// below = RAW), each in its own slot-local pixel space.
	float w = 409.6f, upperH = 72.0f, lowerH = 72.0f;
	double windowSeconds = 60.0;
	float segmentWidthPx = kOrbitHistoryTuning.segmentWidthPx;
	float segmentGapPx = kOrbitHistoryTuning.segmentGapPx;
	bool operator==(const OrbitLayout & o) const {
		return w == o.w && upperH == o.upperH && lowerH == o.lowerH && windowSeconds == o.windowSeconds
			&& segmentWidthPx == o.segmentWidthPx && segmentGapPx == o.segmentGapPx;
	}
};

constexpr size_t kOrbitMaxSamples = 120;
constexpr double kOrbitPeriodSeconds = 0.5;

struct OrbitRect {
	float x = 0, y = 0, w = 0, h = 0;
};
struct OrbitPt {
	float x = 0, y = 0;
};

enum class OrbitSourceId : uint8_t { Fx = 0,
	Raw = 1 };

// Section-local rectangles; identical for both sources (FX in the upper section, RAW in the lower).
struct OrbitSectionLayout {
	OrbitRect swatch, bars, lumaRow; // left column
	OrbitRect radar; // radar box (upper section) / legend box (lower section)
	OrbitRect ribbon, rgb; // right group: colour ribbon (+ luminance overlay) and R/G/B traces
	OrbitRect accent; // thin source-accent stripe left of the group
};

// One polyline set: vertex array + gap-free segment ranges (single-vertex segments are dots).
struct OrbitTrace {
	static constexpr size_t kMaxSegs = kOrbitMaxSamples;
	struct Seg {
		uint16_t first = 0, count = 0;
	};
	std::array<OrbitPt, kOrbitMaxSamples> v {};
	std::array<Seg, kMaxSegs> seg {};
	size_t n = 0, segN = 0;
	void clear() {
		n = 0;
		segN = 0;
	}
};

struct OrbitSourceModel {
	OrbitState state = OrbitState::Unavailable;
	bool hasLatest = false;
	OrbitSample latest {}; // newest VALID sample (also when Stale)
	SegmentedHistoryGeometry history; // the ONE bin geometry of this source's ribbon and traces
	SegmentedHistoryInstance ribbon { {}, HistoryProjection::Color };
	OrbitTrace rgb[3]; // R, G, B traces (display-encoded 0..1)
	OrbitTrace luma; // relative-luminance polyline over the ribbon (linear 0..1)
	OrbitPt radarPt[3] {}; // radar vertices (R, G, B) in section-local px; valid iff hasLatest
	uint64_t generation = 0, revision = 0;
};

class OrbitConsoleModel {
public:
	static constexpr size_t kMaxSegs = OrbitTrace::kMaxSegs;
	using Seg = OrbitTrace::Seg;

	// Returns true iff geometry was rebuilt. raw: optional RAW VIDEO series (decoded video colour
	// BEFORE scene effects), owned and sampled by the media/runtime owner. nullptr / empty =
	// RAW VIDEO unavailable: nothing is drawn for it and it never borrows the FX colours.
	bool update(const OrbitSeriesView & fx, const OrbitLayout & layout, const OrbitSeriesView * raw = nullptr);

	const OrbitSourceModel & source(OrbitSourceId id) const { return src_[size_t(id)]; }
	const OrbitSourceModel & fx() const { return src_[0]; }
	const OrbitSourceModel & raw() const { return src_[1]; }
	const OrbitSectionLayout & section() const { return sec_; }
	OrbitPt radarCenter() const { return radarCenter_; }
	float radarRadius() const { return radarR_; }
	OrbitPt radarAxisEnd(int ch) const { return axis_[ch]; } // unit-radius vertex of axis ch (R, G, B)

	// Convenience for the FX source (historic accessors).
	OrbitState state() const { return src_[0].state; }
	bool hasLatest() const { return src_[0].hasLatest; }
	OrbitSample latest() const { return src_[0].latest; }
	const SegmentedHistoryGeometry & history() const { return src_[0].history; }
	size_t chartSegCount(int ch) const { return src_[0].rgb[ch].segN; }
	uint64_t generation() const { return src_[0].generation; }
	OrbitState rawState() const { return src_[1].state; }

	uint64_t rebuildCount() const { return rebuildCount_; }
	// Section-local x (shared by every chart of every source) a sample time plots at: the centre of
	// its slot; <0 if outside the window.
	float xOfTime(double t) const {
		const long long s = src_[0].history.slotOf(t);
		return s < 0 ? -1.0f : src_[0].history.centerX(size_t(s));
	}

private:
	void rebuild(const OrbitSeriesView & fx, const OrbitLayout & layout, const OrbitSeriesView * raw);
	void buildSource(OrbitSourceModel & m, const OrbitSeriesView & s, const OrbitLayout & L, bool haveAnchor, double anchorT);

	OrbitSectionLayout sec_;
	OrbitPt radarCenter_ {};
	float radarR_ = 0;
	OrbitPt axis_[3] {};
	std::array<OrbitSourceModel, 2> src_ {};
	bool built_ = false, builtHadRaw_ = false;
	uint64_t builtFxRevision_ = 0, builtFxGeneration_ = 0, builtRawRevision_ = 0, builtRawGeneration_ = 0, rebuildCount_ = 0;
	OrbitLayout builtLayout_ {};
};

} // namespace hudpresent
