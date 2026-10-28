#pragma once

#include "SystemVitalsSnapshot.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

// SystemVitals — private, read-only system telemetry histories owned by
// RuntimeTelemetryCollector (System Telemetry v1). OF-free so the parsers,
// rings and cadence logic are testable with a bare compiler.
//
// NOT part of RuntimeTelemetry / HudFrameData / SceneContract.h. Nothing
// here is a shared contract; the HUD cannot consume it until a Tier C
// proposal is approved (see the System Telemetry v1 report).
//
// Five metrics, each a fixed-capacity timestamped ring (monotonic seconds):
//   FPS        ~2 Hz, 60 s   existing collector fps, sampled
//   FrameTime  ~2 Hz, 60 s   MAX single-frame ms observed in each 500 ms bucket
//   CpuPercent ~1 Hz, 60 s   host-wide busy %, see CpuUtilization below
//   GpuTempC   ~0.5 Hz, 5 min  thermal zone whose `type` is "gpu-thermal"
//   RssMiB     ~1 Hz, 5 min  this process's VmRSS
//
// Only samples that were actually read are stored; an unreadable source
// never writes a 0 (gaps are visible as timestamp jumps).
namespace sysvitals {

template <size_t N>
class Ring {
public:
	static constexpr size_t kCapacity = N;
	// valid == false records a GAP (no usable reading at time t).
	void push(double t, float v, bool valid = true) {
		data_[head_] = Sample{t, v, valid};
		head_ = (head_ + 1) % N;
		if (count_ < N) count_++;
		revision_++;
		if (valid) { lastValid_ = Sample{t, v, true}; hasValid_ = true; }
	}
	size_t size() const { return count_; }
	// i = 0 is the oldest retained entry, size()-1 the newest.
	const Sample& at(size_t i) const { return data_[(head_ + N - count_ + i) % N]; }
	const Sample* newest() const { return count_ ? &at(count_ - 1) : nullptr; }
	// Newest entry that is not a gap (survives the ring wrapping over it).
	const Sample* newestValid() const { return hasValid_ ? &lastValid_ : nullptr; }
	uint64_t revision() const { return revision_; }
	void clear() { head_ = 0; count_ = 0; hasValid_ = false; }

private:
	std::array<Sample, N> data_{};
	size_t head_ = 0;
	size_t count_ = 0;
	Sample lastValid_{};
	bool hasValid_ = false;
	uint64_t revision_ = 0;
};

// While no GPU thermal zone is found, rescan at most this often.
constexpr double kThermalRediscoverSeconds = 30.0;

// ---- Parsers (pure; no I/O) ------------------------------------------------

struct CpuCounters {
	uint64_t idleAll = 0; // idle + iowait
	uint64_t total = 0;   // user+nice+system+idle+iowait+irq+softirq+steal
};

// Parses the aggregate "cpu " line of /proc/stat. guest/guest_nice are not
// added to total (the kernel already counts them inside user/nice).
// nullopt for missing/malformed input or a per-core "cpuN" first line.
std::optional<CpuCounters> parseProcStatCpu(const char* text, size_t len);

// VmRSS from /proc/self/status, in kB. nullopt if absent or malformed.
std::optional<uint64_t> parseVmRssKb(const char* text, size_t len);

// Contents of a thermal zone's `type` file (trailing whitespace ignored).
bool thermalTypeMatches(const char* text, size_t len, const char* wanted);

// Contents of a thermal zone's `temp` file, millidegrees C -> degrees C.
// nullopt for empty/malformed/implausible values (outside -50..150 C).
std::optional<float> parseThermalMilliC(const char* text, size_t len);

// Host CPU busy percent from successive aggregate counters:
//   (deltaTotal - deltaIdleAll) / deltaTotal * 100
// idle INCLUDES iowait (a core waiting on I/O is not "busy").
// First sample, counter reset/wrap (any counter decreased), and a zero
// total delta all yield nullopt (re-baselined) rather than a fake 0.
class CpuUtilization {
public:
	std::optional<float> feed(const CpuCounters& now);
	void reset() { havePrev_ = false; }
	bool hasBaseline() const { return havePrev_; }

private:
	CpuCounters prev_{};
	bool havePrev_ = false;
};

// ---- Source seam -----------------------------------------------------------

// Every method fills `buf` (NUL-terminated) and returns the byte count, or
// 0 if unreadable. Implementations must not allocate or spawn processes.
class ISystemSource {
public:
	virtual ~ISystemSource() = default;
	virtual size_t readProcStat(char* buf, size_t cap) = 0;
	virtual size_t readSelfStatus(char* buf, size_t cap) = 0;
	// Returns the zone index whose type == wanted, or -1. May scan the
	// filesystem (rate-limited by the caller).
	virtual int findThermalZone(const char* wantedType) = 0;
	virtual size_t readThermalType(int zone, char* buf, size_t cap) = 0;
	virtual size_t readThermalTemp(int zone, char* buf, size_t cap) = 0;
};

// Linux: /proc and /sys via fopen/fread. Elsewhere: every read fails, so
// Linux-only metrics stay Unavailable.
ISystemSource* defaultSystemSource();

// DEVELOPMENT / VERIFICATION ONLY. A deterministic, clock-driven fake source
// (plausible CPU %, RSS and gpu-thermal values) so the HUD's CPU/GPU/RSS traces
// can be exercised on desktops that have no /proc or /sys. `dropouts` makes the
// GPU sensor vanish periodically, to show gap handling. Selected only when
// EXPERIENCE_RUNTIME_VITALS_SYNTHETIC is set; never used otherwise, and not a
// Rock Pi measurement.
ISystemSource* syntheticSystemSource(bool dropouts);

// ---- Collector core --------------------------------------------------------

class SystemVitals {
public:
	explicit SystemVitals(ISystemSource* source = nullptr);

	// Call once per frame from update() (never draw()). `nowSeconds` is a
	// monotonic clock. `fps` is the collector's existing scalar;
	// `frameTimeMs` is this frame's duration (ofGetLastFrameTime()*1000).
	// Each metric publishes at most one sample per call, so a long frame
	// gap yields a single sample, never a catch-up burst.
	void update(double nowSeconds, float fps, float frameTimeMs);

	const Ring<kFpsCapacity>& fps() const { return fps_; }
	const Ring<kFrameTimeCapacity>& frameTime() const { return frameTime_; }
	const Ring<kCpuCapacity>& cpu() const { return cpu_; }
	const Ring<kGpuCapacity>& gpuTemp() const { return gpu_; }
	const Ring<kRssCapacity>& rss() const { return rss_; }

	// Builds (or reuses) the immutable snapshot for consumers. Cheap when
	// nothing changed: returns the previous pointer after five comparisons.
	// A new snapshot is produced only when some metric gained a sample/gap
	// or changed state (<= ~4 Hz), reusing a pooled object nobody holds, and
	// copying only the histories whose revision changed. Call from update()
	// context only.
	const std::shared_ptr<const SystemVitalsSnapshot>& publish(double nowSeconds);
	const std::shared_ptr<const SystemVitalsSnapshot>& published() const { return published_; }

	struct PublishStats {
		uint64_t calls = 0;
		uint64_t publications = 0;     // new snapshot revisions
		uint64_t historiesCopied = 0;  // series histories copied (of 5 per publication at most)
		uint64_t bytesCopied = 0;      // Sample bytes copied into snapshots
		uint64_t poolAllocations = 0;  // heap allocations of snapshot objects (pool growth)
	};
	const PublishStats& publishStats() const { return publishStats_; }

	State state(Metric m, double nowSeconds) const;
	// Latest valid value (also while Stale); nullopt only when Unavailable.
	std::optional<float> latest(Metric m) const;

	// Sampler cost accounting for the file-read + parse portion of update().
	struct Cost {
		uint64_t samplerRuns = 0;  // update() calls that read at least one source
		double totalMs = 0.0;
		double maxMs = 0.0;
	};
	const Cost& cost() const { return cost_; }
	void setCostClock(double (*nowMs)()) { costClock_ = nowMs; } // tests; default steady_clock

private:
	void sampleCpu(double now);
	void sampleRss(double now);
	void sampleGpu(double now);

	ISystemSource* source_;

	Ring<kFpsCapacity> fps_;
	Ring<kFrameTimeCapacity> frameTime_;
	Ring<kCpuCapacity> cpu_;
	Ring<kGpuCapacity> gpu_;
	Ring<kRssCapacity> rss_;

	bool started_ = false;
	double nextFps_ = 0.0, nextCpu_ = 0.0, nextGpu_ = 0.0, nextRss_ = 0.0;
	float bucketMaxMs_ = 0.0f;
	bool bucketHasFrame_ = false;

	CpuUtilization cpuCalc_;
	int gpuZone_ = -1;
	double nextThermalScan_ = 0.0;

	static constexpr size_t kPoolSize = 4;
	struct PoolEntry {
		std::shared_ptr<SystemVitalsSnapshot> snap;
		std::array<uint64_t, kMetricCount> histRev{}; // ring revision each history was copied at
		bool filled = false;
	};
	template <typename R, typename S>
	void fillSeries(Metric m, const R& ring, State st, uint64_t metricRev, PoolEntry& entry, S& out);
	uint64_t ringRevision(Metric m) const;
	uint64_t metricRevision(Metric m) const { return ringRevision(m) + stateChanges_[static_cast<size_t>(m)]; }
	State stateOfMetric(Metric m, double now) const;

	std::array<PoolEntry, kPoolSize> pool_{};
	std::shared_ptr<const SystemVitalsSnapshot> published_;
	std::array<uint64_t, kMetricCount> stateChanges_{};     // Valid/Stale/Unavailable transitions seen
	std::array<uint64_t, kMetricCount> publishedMetricRev_{};
	std::array<State, kMetricCount> lastState_{};
	uint64_t snapshotRevision_ = 0;
	PublishStats publishStats_;

	Cost cost_;
	double (*costClock_)() = nullptr;
};

} // namespace sysvitals
