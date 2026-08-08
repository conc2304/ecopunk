#pragma once

// ============================================================================
// HudHistoryStore.h — fixed-capacity per-source sample history.
//
// Requirements from this task's §10, addressed one-to-one:
//   - key by source ID + history configuration -> track()
//   - fixed-capacity storage / no per-frame allocation -> std::array-backed
//     ring buffers below, sized once at construction, never resized
//   - configurable sample rate -> HudHistoryRequest::sampleRateHz, gated
//     internally by an accumulator in update(dt)
//   - gap markers / paused sampling -> Sample::gap, pause()/resume()
//   - reset on scene epoch -> resetForSceneEpoch()
//   - optionally mark generation changes -> markGenerationChange()
//   - rolling average/min/max/slope/stability/event pulse/peak hold where
//     inexpensive -> stats()
//   - report allocated memory -> estimatedBytes()
//
// Widgets never own history buffers themselves (this task's explicit
// instruction) — only this store does; a Sparkline widget receives
// read-only samples/stats from here via the orchestrator.
// ============================================================================

#include "HudPresentationTypes.h" // reuses the one canonical HudHistoryRequest defined there

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hudpresent {

class HudHistoryStore {
public:
	// HUD-Semantic-Slot-Model-v1.md §20's recommended v1 caps, reused
	// here as this store's fixed capacities.
	static constexpr size_t kMaxTrackedSignals = 8;
	static constexpr size_t kMaxSamplesPerSignal = 64;

	struct Sample {
		float value = 0.0f;
		bool gap = false; // true = "no sample this slot" (missing/paused), never a synthetic 0.0f
	};

	// Engineering Session 2: replaces the old std::vector<Sample>-by-value
	// read API (see git history / Session 1's version of this header for
	// what this superseded) — a fixed-capacity, stack-allocated view.
	// Copying/returning this by value never touches the heap: it is a
	// std::array<Sample, kMaxSamplesPerSignal> plus a count, exactly the
	// same shape (and cost) as copying the store's own internal ring
	// buffer, just truncated to the first `count` logically-ordered
	// entries. Supports range-for and operator[] so call sites that used
	// to iterate a std::vector<Sample> need no logic changes, only a type
	// swap.
	struct SampleView {
		std::array<Sample, kMaxSamplesPerSignal> samples{};
		size_t count = 0;

		size_t size() const { return count; }
		bool empty() const { return count == 0; }
		const Sample& operator[](size_t i) const { return samples[i]; }
		const Sample* begin() const { return samples.data(); }
		const Sample* end() const { return samples.data() + count; }
		const Sample& back() const { return samples[count - 1]; }
	};

	struct Stats {
		bool hasData = false;
		float average = 0.0f;
		float minValue = 0.0f;
		float maxValue = 0.0f;
		float slope = 0.0f;      // (newest - oldest) / (non-gap sample span), normalized signal's per-sample trend
		float stability = 1.0f;  // 1 = perfectly flat, 0 = maximally noisy within this window
		float peakHold = 0.0f;   // highest non-gap value ever seen for this signal since last reset
		bool eventPulse = false; // true this frame if the most recent write was a generation-change marker
	};

	// Registers `sourceId` for tracking with `request`'s cap/rate,
	// clamped into [1, kMaxSamplesPerSignal]. Returns false (does nothing)
	// if kMaxTrackedSignals is already reached or `sourceId` is already
	// tracked (call untrack() first to reconfigure).
	bool track(const std::string& sourceId, const HudHistoryRequest& request);
	void untrack(const std::string& sourceId);
	bool isTracked(const std::string& sourceId) const;

	// Advances every tracked signal's internal sample-rate clock by `dt`
	// seconds. Must be called once per frame, with a real or
	// fixed-dt-simulated frame delta — never a wall-clock read.
	void update(float dt);

	// Pushes one sample for `sourceId` if that signal's sample-rate clock
	// has accumulated enough time since the last write (silently a no-op
	// otherwise, so callers can call this every frame regardless of the
	// signal's own configured rate). `value == std::nullopt` always
	// writes a gap marker — this is the one and only place "missing vs.
	// zero" is enforced on the write side, since it's structurally
	// impossible to accidentally write 0.0f for a missing value through
	// this API.
	void pushSample(const std::string& sourceId, std::optional<float> value);

	void pause(const std::string& sourceId, bool paused);
	bool isPaused(const std::string& sourceId) const;

	// Clears every tracked signal's buffer (all series, not just one) —
	// called once per scene switch. `newEpoch` is stored and returned by
	// currentEpoch() purely for caller/test bookkeeping; this store does
	// not interpret it beyond storing it.
	void resetForSceneEpoch(uint64_t newEpoch);
	uint64_t currentEpoch() const { return epoch_; }

	// Inserts a marker at the next write for `sourceId` without clearing
	// its history — the next pushSample() for this source sets
	// Stats::eventPulse true, and it stays true across any number of
	// stats() reads until the FOLLOWING pushSample() call for this
	// source (a per-write flag, not a per-stats()-call one-shot):
	// eventPulse means "the most recently written sample was a
	// generation-change marker", not "a generation change happened
	// somewhere in this window".
	void markGenerationChange(const std::string& sourceId);

	Stats stats(const std::string& sourceId) const;

	// Read-only access to the raw ring, oldest-to-newest, for widget
	// rendering (Sparkline) and tests — always exactly `sampleCount`
	// entries once the signal has been tracked at least one full window;
	// fewer before that, since this store never backfills unwritten slots
	// with anything (gap or otherwise) before the first real write.
	//
	// Zero heap allocation: returns a fixed-capacity SampleView by value
	// (a stack-copied std::array + count), not a std::vector — see
	// SampleView's own comment. This closes the one allocating path
	// Session 1 flagged as a documented trade-off.
	SampleView samplesOldestToNewest(const std::string& sourceId) const;

	size_t trackedSignalCount() const { return seriesCount_; }

	// A simple, documented estimate: kMaxTrackedSignals *
	// kMaxSamplesPerSignal * sizeof(Sample) + a small fixed per-series
	// overhead — this store never allocates beyond its own fixed
	// std::array members, so this is close to the store's actual
	// worst-case footprint, not a heap-walk measurement.
	static size_t estimatedBytes();

private:
	struct Series {
		bool used = false;
		std::string sourceId;
		HudHistoryRequest request;
		std::array<Sample, kMaxSamplesPerSignal> ring{};
		size_t count = 0;      // how many slots have ever been written (caps at capacity)
		size_t writeIndex = 0; // next slot to write, wraps at capacity
		size_t capacity = kMaxSamplesPerSignal;
		float accumSeconds = 0.0f;
		bool paused = false;
		bool pendingEventMarker = false;
		bool lastWriteWasEvent = false;
		float peakHold = 0.0f;
		bool hasPeak = false;
	};

	Series* findSeries(const std::string& sourceId);
	const Series* findSeries(const std::string& sourceId) const;

	std::array<Series, kMaxTrackedSignals> series_{};
	size_t seriesCount_ = 0;
	uint64_t epoch_ = 0;
};

} // namespace hudpresent
