#pragma once

// ============================================================================
// SystemVitalsSnapshot.h — System Telemetry v1 runtime-to-HUD snapshot.
//
// Architecture-ratified (Tier C, narrow amendments): an additive, optional,
// read-only, bounded, immutable snapshot carried by HudFrameData::systemVitals
// as std::shared_ptr<const SystemVitalsSnapshot>. A null pointer means "no
// snapshot yet" (never a snapshot of zeros).
//
// Ownership: the runtime-private sysvitals::SystemVitals collector builds
// snapshots (SystemVitals::publish) from its own rings; consumers only ever
// hold const pointers. A published snapshot is never mutated: the collector
// recycles an object only when no consumer still references it.
//
// OF-free: standard library only.
//
// Units / meaning (all histories are oldest -> newest, monotonic seconds):
//   fps            Hz        existing collector fps, sampled ~2 Hz,  60 s
//   frameTimeMax   ms        WORST single frame in each 500 ms bucket, 60 s
//   cpuBusy        percent   system-wide busy 0..100 (idle + iowait count as
//                            idle), ~1 Hz, 60 s. NOT process CPU.
//   gpuTemp        deg C     thermal zone whose type is "gpu-thermal", ~0.5 Hz, 5 min
//   processRss     MiB       this process's VmRSS, ~1 Hz, 5 min. NOT total RAM.
//
// A history entry with valid == false is a GAP (the source was unreadable or
// produced no usable value at that time); its value is meaningless and is
// never a synthetic 0. A valid zero is {valid = true, value = 0}.
// ============================================================================

#include <array>
#include <cstddef>
#include <cstdint>

namespace sysvitals {

enum class Metric : uint8_t { Fps, FrameTimeMs, CpuPercent, GpuTempC, RssMiB, Count };
constexpr size_t kMetricCount = static_cast<size_t>(Metric::Count);

// Unavailable: no valid sample has ever been read (or the platform has no
//              such source). Never a valid zero.
// Valid:       the newest valid sample is younger than the stale limit.
// Stale:       a valid sample exists but newer ones have stopped arriving.
enum class State : uint8_t { Unavailable, Valid, Stale };

struct Sample {
	double t = 0.0;     // monotonic seconds
	float value = 0.0f;
	bool valid = true;  // false = gap marker
};

// Cadences (seconds) and capacities.
constexpr double kFpsPeriod = 0.5;
constexpr double kFrameTimePeriod = 0.5;
constexpr double kCpuPeriod = 1.0;
constexpr double kGpuPeriod = 2.0;
constexpr double kRssPeriod = 1.0;

constexpr size_t kFpsCapacity = 120;       // 60 s @ 2 Hz
constexpr size_t kFrameTimeCapacity = 120; // 60 s @ 2 Hz
constexpr size_t kCpuCapacity = 60;        // 60 s @ 1 Hz
constexpr size_t kGpuCapacity = 150;       // 5 min @ 0.5 Hz
constexpr size_t kRssCapacity = 300;       // 5 min @ 1 Hz

// A newest-valid sample older than this many periods marks the metric Stale.
constexpr double kStalePeriods = 3.0;

template <size_t N>
struct SeriesSnapshot {
	static constexpr size_t kCapacity = N;
	State state = State::Unavailable;
	bool hasLatest = false;       // false only while Unavailable
	float latest = 0.0f;          // newest VALID value (also while Stale)
	double latestTimeSeconds = 0.0;
	uint64_t revision = 0;        // monotonic; bumps on a new sample/gap or a state change
	uint32_t count = 0;           // entries used in `history`
	std::array<Sample, N> history{}; // chronological, oldest first, [0, count)

	const Sample& at(size_t i) const { return history[i]; }
};

struct SystemVitalsSnapshot {
	uint64_t revision = 0;           // monotonic; bumps once per publication
	double capturedAtSeconds = 0.0;  // monotonic clock the states were evaluated at

	SeriesSnapshot<kFpsCapacity> fps;
	SeriesSnapshot<kFrameTimeCapacity> frameTimeMax;
	SeriesSnapshot<kCpuCapacity> cpuBusy;
	SeriesSnapshot<kGpuCapacity> gpuTemp;
	SeriesSnapshot<kRssCapacity> processRss;
};

} // namespace sysvitals
