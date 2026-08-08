#include "HudHistoryStore.h"

#include <algorithm>
#include <cmath>

namespace hudpresent {

HudHistoryStore::Series* HudHistoryStore::findSeries(const std::string& sourceId) {
	for (auto& s : series_) {
		if (s.used && s.sourceId == sourceId) return &s;
	}
	return nullptr;
}

const HudHistoryStore::Series* HudHistoryStore::findSeries(const std::string& sourceId) const {
	for (const auto& s : series_) {
		if (s.used && s.sourceId == sourceId) return &s;
	}
	return nullptr;
}

bool HudHistoryStore::track(const std::string& sourceId, const HudHistoryRequest& request) {
	if (findSeries(sourceId) != nullptr) return false;
	if (seriesCount_ >= kMaxTrackedSignals) return false;

	for (auto& s : series_) {
		if (s.used) continue;
		s = Series{};
		s.used = true;
		s.sourceId = sourceId;
		s.request = request;
		s.capacity = std::clamp(request.sampleCount, size_t(1), kMaxSamplesPerSignal);
		seriesCount_++;
		return true;
	}
	return false; // unreachable given the seriesCount_ guard above
}

void HudHistoryStore::untrack(const std::string& sourceId) {
	for (auto& s : series_) {
		if (s.used && s.sourceId == sourceId) {
			s = Series{};
			seriesCount_--;
			return;
		}
	}
}

bool HudHistoryStore::isTracked(const std::string& sourceId) const {
	return findSeries(sourceId) != nullptr;
}

void HudHistoryStore::update(float dt) {
	for (auto& s : series_) {
		if (!s.used || s.paused) continue;
		s.accumSeconds += dt;
	}
}

void HudHistoryStore::pushSample(const std::string& sourceId, std::optional<float> value) {
	Series* s = findSeries(sourceId);
	if (!s) return;
	if (s->paused) return; // "paused" freezes the trace exactly — no write, no gap, no ring advance

	const float rate = s->request.sampleRateHz > 0.0f ? s->request.sampleRateHz : 1.0f;
	const float interval = 1.0f / rate;
	if (s->accumSeconds < interval) return; // not yet time for this signal's next sample
	s->accumSeconds -= interval;

	Sample sample;
	if (value.has_value()) {
		sample.value = *value;
		sample.gap = false;
		s->hasPeak = true;
		s->peakHold = s->hasPeak ? std::max(s->peakHold, *value) : *value;
	} else {
		sample.gap = true; // never a synthetic 0.0f
	}

	s->ring[s->writeIndex] = sample;
	s->writeIndex = (s->writeIndex + 1) % s->capacity;
	s->count = std::min(s->count + 1, s->capacity);

	s->lastWriteWasEvent = s->pendingEventMarker;
	s->pendingEventMarker = false;
}

void HudHistoryStore::pause(const std::string& sourceId, bool paused) {
	if (Series* s = findSeries(sourceId)) s->paused = paused;
}

bool HudHistoryStore::isPaused(const std::string& sourceId) const {
	const Series* s = findSeries(sourceId);
	return s ? s->paused : false;
}

void HudHistoryStore::resetForSceneEpoch(uint64_t newEpoch) {
	epoch_ = newEpoch;
	for (auto& s : series_) {
		if (!s.used) continue;
		std::string id = s.sourceId;
		HudHistoryRequest request = s.request;
		size_t capacity = s.capacity;
		s = Series{};
		s.used = true;
		s.sourceId = id;
		s.request = request;
		s.capacity = capacity;
	}
}

void HudHistoryStore::markGenerationChange(const std::string& sourceId) {
	if (Series* s = findSeries(sourceId)) s->pendingEventMarker = true;
}

namespace {

// The ring index of the OLDEST logically-retained sample — every other
// logical position is `(oldest + i) % capacity` for i in [0, count).
// Engineering Session 2: this used to build a std::vector<size_t> of
// every index up front (logicalOrder()); replaced with this one-index
// helper so both stats() and samplesOldestToNewest() can loop directly
// with zero heap allocation — see HudHistoryStore.h's SampleView comment.
size_t oldestRingIndex(size_t count, size_t writeIndex, size_t capacity) {
	return (writeIndex + capacity - count) % capacity;
}

} // namespace

HudHistoryStore::Stats HudHistoryStore::stats(const std::string& sourceId) const {
	Stats out;
	const Series* s = findSeries(sourceId);
	if (!s || s->count == 0) return out;

	size_t oldest = oldestRingIndex(s->count, s->writeIndex, s->capacity);

	float sum = 0.0f;
	float minV = 0.0f, maxV = 0.0f;
	bool haveFirst = false;
	float firstNonGap = 0.0f, lastNonGap = 0.0f;
	size_t nonGapCount = 0;

	for (size_t i = 0; i < s->count; ++i) {
		size_t idx = (oldest + i) % s->capacity;
		const Sample& sample = s->ring[idx];
		if (sample.gap) continue;
		if (!haveFirst) {
			minV = maxV = sample.value;
			firstNonGap = sample.value;
			haveFirst = true;
		} else {
			minV = std::min(minV, sample.value);
			maxV = std::max(maxV, sample.value);
		}
		lastNonGap = sample.value;
		sum += sample.value;
		nonGapCount++;
	}

	if (nonGapCount == 0) {
		out.hasData = false;
		out.eventPulse = s->lastWriteWasEvent;
		return out;
	}

	out.hasData = true;
	out.average = sum / static_cast<float>(nonGapCount);
	out.minValue = minV;
	out.maxValue = maxV;
	out.slope = (nonGapCount > 1) ? (lastNonGap - firstNonGap) / static_cast<float>(nonGapCount - 1) : 0.0f;

	// Stability: 1 - normalized standard deviation, clamped to [0,1].
	// Cheap (single extra pass over an already-small, bounded window —
	// at most kMaxSamplesPerSignal=64 entries) and avoids sqrt-of-negative
	// edge cases via the clamp.
	float variance = 0.0f;
	for (size_t i = 0; i < s->count; ++i) {
		size_t idx = (oldest + i) % s->capacity;
		const Sample& sample = s->ring[idx];
		if (sample.gap) continue;
		float d = sample.value - out.average;
		variance += d * d;
	}
	variance /= static_cast<float>(nonGapCount);
	float stdDev = std::sqrt(variance);
	float range = std::max(1e-6f, maxV - minV);
	out.stability = std::clamp(1.0f - (stdDev / range), 0.0f, 1.0f);

	out.peakHold = s->hasPeak ? s->peakHold : maxV;
	out.eventPulse = s->lastWriteWasEvent;
	return out;
}

HudHistoryStore::SampleView HudHistoryStore::samplesOldestToNewest(const std::string& sourceId) const {
	SampleView view; // stack-allocated, zero heap — see SampleView's own comment
	const Series* s = findSeries(sourceId);
	if (!s || s->count == 0) return view;
	size_t oldest = oldestRingIndex(s->count, s->writeIndex, s->capacity);
	for (size_t i = 0; i < s->count; ++i) {
		view.samples[i] = s->ring[(oldest + i) % s->capacity];
	}
	view.count = s->count;
	return view;
}

size_t HudHistoryStore::estimatedBytes() {
	return sizeof(Series) * kMaxTrackedSignals;
}

} // namespace hudpresent
