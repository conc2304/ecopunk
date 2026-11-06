#pragma once

// ============================================================================
// SegmentedHistory.h — ONE reusable segmented-history component (HUD Runtime).
//
// OF-free. Luminance history and colour history are two INSTANCES of this
// component, not two component types. A single SegmentedHistoryGeometry is
// computed once per rebuild from the shared sample sequence and read by both:
// it owns the time window, direction (oldest left -> newest right), the fixed
// segment count/width, the timestamp->slot mapping, invalid-sample gaps and
// the scene-generation reset (the model rebuilds it empty on a new generation).
// An instance adds only its own vertical rectangle and its data projection:
//   Luminance: relative luminance mapped to brightness (grey, display-encoded)
//   Color:     display-encoded average RGB mapped to segment colour
// so corresponding segments have bit-identical x0/x1, timestamp and kind.
//
// Slot count-1 holds the newest sample; a sample d seconds older sits
// about d / binSeconds slots to its left (anchored on the newest; at the default
// density binSeconds == the 0.5 s period, so an exact 2 Hz series fills one slot each). Samples outside
// the window are dropped. A slot with no sample (stall/missing) or an invalid
// sample is NOT painted and never carries a zero value.
// ============================================================================

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace hudpresent {

constexpr size_t kHistoryMaxSegments = 120;

// sRGB OETF (linear 0..1 -> display-encoded 0..1). OrbitSample::luma is RELATIVE LUMINANCE (linear);
// the luminance strip paints its brightness display-encoded so equal luminance steps read evenly.
inline float srgbEncode(float v) {
	v = std::min(1.0f, std::max(0.0f, v));
	return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

enum class SegKind : uint8_t { Missing, Gap, Valid }; // Missing = no sample; Gap = invalid sample

struct HistorySegment {
	float x0 = 0, x1 = 0;   // shared horizontal boundaries, slot-local px
	double t = 0.0;         // timestamp of the sample in this slot (0 if Missing)
	SegKind kind = SegKind::Missing;
	float r = 0, g = 0, b = 0, luma = 0; // meaningful only when kind == Valid
};

// sRGB EOTF (display-encoded 0..1 -> linear): the inverse of srgbEncode, used to average colour in linear light.
inline float srgbDecode(float v) {
	v = std::min(1.0f, std::max(0.0f, v));
	return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

// Slot-count rule shared by every history strip: how many segments of segmentW with gapW between
// them fit in plotW. Bounded to [1, kHistoryMaxSegments]; the sampler cadence and the source
// history capacity are NOT affected, only how many temporal bins are displayed.
inline size_t historySlotCapacity(float plotW, float segmentW, float gapW) {
	if (!(plotW > 0.0f) || !(segmentW > 0.0f)) return 0;
	const float g = std::max(0.0f, gapW);
	const float n = std::floor((plotW + g) / (segmentW + g));
	return static_cast<size_t>(std::min<float>(float(kHistoryMaxSegments), std::max(1.0f, n)));
}

class SegmentedHistoryGeometry {
public:
	void clear() { count_ = 0; }

	// samples: oldest -> newest (n <= any size; only the in-window tail is used).
	// segmentW / gapW (px) set the segment pitch; the slot count is derived from plotW
	// (historySlotCapacity) and the newest slot is anchored to the RIGHT edge of the plot.
	// Each bin spans window / count seconds; samples that fall into one bin are aggregated:
	// colour = mean in LINEAR light (samples carry display-encoded RGB, re-encoded afterwards),
	// luma = mean (already linear), t = newest timestamp in the bin. A bin with at least one
	// valid sample is Valid; a bin holding only invalid samples is a Gap; an empty bin is Missing.
	// anchorT: the time of the newest slot (default: the newest sample). Passing the primary
	// series' newest time puts several series on ONE time axis.
	template <typename Sample>
	void build(const Sample* samples, size_t n, double window, double period, float plotX, float plotW,
		float segmentW, float gapW, double anchorT, bool haveAnchor = true) {
		count_ = 0;
		window_ = window;
		period_ = period;
		if (!samples || n == 0 || window <= 0.0 || period <= 0.0) return;
		count_ = historySlotCapacity(plotW, segmentW, gapW);
		if (count_ == 0) return;
		tNewest_ = haveAnchor ? anchorT : samples[n - 1].t;
		binSeconds_ = window / double(count_);
		const float g = std::max(0.0f, gapW);
		const float used = float(count_) * segmentW + float(count_ - 1) * g;
		const float startX = plotX + std::max(0.0f, plotW - used);
		for (size_t i = 0; i < count_; ++i) {
			seg_[i] = HistorySegment{};
			seg_[i].x0 = startX + (segmentW + g) * float(i);
			seg_[i].x1 = seg_[i].x0 + segmentW;
		}
		struct Acc { double r = 0, g = 0, b = 0, luma = 0, t = 0; int valid = 0, invalid = 0; };
		std::array<Acc, kHistoryMaxSegments> acc{};
		for (size_t k = 0; k < n; ++k) {
			const Sample& s = samples[k];
			const long long sl = slotOf(s.t);
			if (sl < 0) continue; // outside the window
			Acc& a = acc[static_cast<size_t>(sl)];
			a.t = std::max(a.t, s.t);
			if (s.valid) {
				a.r += srgbDecode(s.r); a.g += srgbDecode(s.g); a.b += srgbDecode(s.b); a.luma += s.luma;
				a.valid++;
			} else {
				a.invalid++;
			}
		}
		for (size_t i = 0; i < count_; ++i) {
			const Acc& a = acc[i];
			HistorySegment& g2 = seg_[i];
			if (a.valid > 0) {
				const double inv = 1.0 / a.valid;
				g2.kind = SegKind::Valid; g2.t = a.t;
				g2.r = srgbEncode(float(a.r * inv)); g2.g = srgbEncode(float(a.g * inv)); g2.b = srgbEncode(float(a.b * inv));
				g2.luma = float(a.luma * inv);
			} else if (a.invalid > 0) {
				g2.kind = SegKind::Gap; g2.t = a.t;
			}
		}
	}

	// THE timestamp -> slot definition. Every time-based instrument (all strips
	// AND the RGB chart) uses this; -1 = outside the window / empty. A sample lands in the bin
	// floor((tNewest - t + period/2) / binSeconds) from the right: with binSeconds == period this
	// is "nearest slot" (an exact-period series fills one slot each, jitter under half a period
	// stays put); with wider bins several samples share one slot.
	long long slotOf(double t) const {
		if (count_ == 0) return -1;
		const long long back = static_cast<long long>(std::floor((tNewest_ - t + period_ * 0.5) / binSeconds_));
		if (back < 0 || back >= static_cast<long long>(count_)) return -1;
		return static_cast<long long>(count_) - 1 - back;
	}
	float centerX(size_t i) const { return 0.5f * (seg_[i].x0 + seg_[i].x1); }
	size_t count() const { return count_; }
	double binSeconds() const { return binSeconds_; }
	const HistorySegment& seg(size_t i) const { return seg_[i]; }

private:
	std::array<HistorySegment, kHistoryMaxSegments> seg_{};
	size_t count_ = 0;
	double window_ = 60.0, period_ = 0.5, tNewest_ = 0.0, binSeconds_ = 0.5;
};

enum class HistoryProjection : uint8_t { Luminance, Color };
struct HistoryFill { bool paint = false; float r = 0, g = 0, b = 0; };

struct HistoryRect { float x = 0, y = 0, w = 0, h = 0; };

// One strip: a vertical rectangle + a projection. Horizontal geometry always
// comes from the shared SegmentedHistoryGeometry, never from the instance.
struct SegmentedHistoryInstance {
	HistoryRect rect;
	HistoryProjection projection = HistoryProjection::Luminance;

	HistoryFill fill(const HistorySegment& s) const {
		if (s.kind != SegKind::Valid) return {};
		if (projection == HistoryProjection::Luminance) {
			const float v = srgbEncode(s.luma);
			return {true, v, v, v};
		}
		return {true, std::min(1.0f, std::max(0.0f, s.r)), std::min(1.0f, std::max(0.0f, s.g)), std::min(1.0f, std::max(0.0f, s.b))};
	}
	// Full-height cell for a segment (same x0/x1 as every other instance).
	HistoryRect cell(const HistorySegment& s) const { return {s.x0, rect.y, s.x1 - s.x0, rect.h}; }
};

} // namespace hudpresent
