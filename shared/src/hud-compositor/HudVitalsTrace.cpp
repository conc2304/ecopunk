#include "HudVitalsTrace.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace hudpresent {

using sysvitals::Metric;
using sysvitals::State;

namespace {

// {sourceId, metric, label, unit, decimals, window, period,
//  fixedMin, min, fixedMax, max, minSpan, floorMax, reference}
const VitalsMetricSpec kSpecs[] = {
	{"runtime.vitals.fps", Metric::Fps, "FPS", "Hz", 1, 60.0, sysvitals::kFpsPeriod, true, 0.0f, false, 0.0f, 1.0f, 60.0f, 0.0f},
	// 33.3 ms = the 30 FPS target line. A reference, not a performance guarantee.
	{"runtime.vitals.frame_time_max", Metric::FrameTimeMs, "FRAME MAX", "ms", 1, 60.0, sysvitals::kFrameTimePeriod, true, 0.0f, false, 0.0f, 1.0f, 40.0f, 33.3f},
	{"runtime.vitals.cpu", Metric::CpuPercent, "CPU", "%", 0, 60.0, sysvitals::kCpuPeriod, true, 0.0f, true, 100.0f, 1.0f, 0.0f, 0.0f},
	{"runtime.vitals.gpu_temp", Metric::GpuTempC, "GPU TEMP", "C", 1, 300.0, sysvitals::kGpuPeriod, false, 0.0f, false, 0.0f, 10.0f, 0.0f, 0.0f},
	{"runtime.vitals.rss", Metric::RssMiB, "RSS", "MiB", 0, 300.0, sysvitals::kRssPeriod, false, 0.0f, false, 0.0f, 16.0f, 0.0f, 0.0f},
};

} // namespace

const VitalsMetricSpec* vitalsSpecForSource(const std::string& sourceId) {
	for (const auto& s : kSpecs) {
		if (sourceId == s.sourceId) return &s;
	}
	return nullptr;
}

const VitalsMetricSpec& vitalsSpecFor(Metric metric) {
	for (const auto& s : kSpecs) {
		if (s.metric == metric) return s;
	}
	return kSpecs[0];
}

bool HudVitalsTrace::update(const VitalsMetricSpec& spec, const VitalsSeriesView& series, float plotWidth, float plotHeight) {
	const bool same = built_ && builtSpec_ == &spec && builtRevision_ == series.revision &&
		builtW_ == plotWidth && builtH_ == plotHeight && state_ == series.state;
	if (same) return false;
	rebuild(spec, series, plotWidth, plotHeight);
	return true;
}

void HudVitalsTrace::rebuild(const VitalsMetricSpec& spec, const VitalsSeriesView& series, float plotWidth, float plotHeight) {
	built_ = true;
	builtSpec_ = &spec;
	builtRevision_ = series.revision;
	builtW_ = plotWidth;
	builtH_ = plotHeight;
	rebuildCount_++;

	state_ = series.state;
	hasLatest_ = series.hasLatest;
	latest_ = series.hasLatest ? series.latest : 0.0f;
	if (series.state == State::Unavailable || !series.hasLatest) {
		std::snprintf(valueText_, sizeof(valueText_), "--");
	} else {
		std::snprintf(valueText_, sizeof(valueText_), "%.*f %s", spec.decimals, static_cast<double>(series.latest), spec.unit);
	}

	vertexCount_ = 0;
	segmentCount_ = 0;
	hasReference_ = false;
	hasLatestPoint_ = false;

	const uint32_t count = std::min<uint32_t>(series.count, static_cast<uint32_t>(kVitalsMaxPoints));
	if (!series.history || count == 0 || plotWidth <= 0.0f || plotHeight <= 0.0f) {
		axisMin_ = spec.fixedMin ? spec.minValue : 0.0f;
		axisMax_ = spec.fixedMax ? spec.maxValue : 1.0f;
		return;
	}

	const double tNewest = series.history[count - 1].t;
	const double tLeft = tNewest - spec.windowSeconds;

	// Axis from the valid points inside the window.
	bool any = false;
	float lo = 0.0f, hi = 0.0f;
	for (uint32_t i = 0; i < count; ++i) {
		const auto& s = series.history[i];
		if (!s.valid || s.t < tLeft) continue;
		if (!any) { lo = hi = s.value; any = true; }
		else { lo = std::min(lo, s.value); hi = std::max(hi, s.value); }
	}
	float amin = spec.fixedMin ? spec.minValue : (any ? lo : 0.0f);
	float amax = spec.fixedMax ? spec.maxValue : (any ? hi : amin + spec.minSpan);
	if (!spec.fixedMax && spec.floorMax > 0.0f) amax = std::max(amax, spec.floorMax);
	if (amax - amin < spec.minSpan) {
		if (spec.fixedMin) amax = amin + spec.minSpan;
		else if (spec.fixedMax) amin = amax - spec.minSpan;
		else { const float mid = 0.5f * (amin + amax); amin = mid - 0.5f * spec.minSpan; amax = mid + 0.5f * spec.minSpan; }
	}
	axisMin_ = amin;
	axisMax_ = amax;
	const float range = amax - amin;

	auto yOf = [&](float v) {
		const float norm = std::clamp((v - amin) / range, 0.0f, 1.0f);
		return plotHeight * (1.0f - norm);
	};

	if (spec.referenceValue > 0.0f && spec.referenceValue >= amin && spec.referenceValue <= amax) {
		hasReference_ = true;
		referenceY_ = yOf(spec.referenceValue);
	}

	const double breakGap = kVitalsBreakPeriods * spec.nominalPeriod;
	bool open = false;
	double prevT = 0.0;
	auto closeSegment = [&]() { open = false; };
	for (uint32_t i = 0; i < count; ++i) {
		const auto& s = series.history[i];
		if (s.t < tLeft) continue;
		if (!s.valid) { closeSegment(); continue; } // gap marker: break, never a 0
		if (open && (s.t - prevT) > breakGap) closeSegment(); // sampling gap with no marker
		if (!open) {
			if (segmentCount_ >= kMaxSegments) break;
			segments_[segmentCount_] = Segment{static_cast<uint16_t>(vertexCount_), 0};
			segmentCount_++;
			open = true;
		}
		vertices_[vertexCount_] = Vertex{static_cast<float>((s.t - tLeft) / spec.windowSeconds) * plotWidth, yOf(s.value)};
		vertexCount_++;
		segments_[segmentCount_ - 1].count++;
		prevT = s.t;
	}
	if (series.state == State::Valid && vertexCount_ > 0) {
		hasLatestPoint_ = true;
		latestPoint_ = vertices_[vertexCount_ - 1];
	}
}

} // namespace hudpresent
