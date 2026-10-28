#include "SystemVitals.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sysvitals {

namespace {

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// Parses an unsigned decimal at p (bounded by end). Advances p. false on no
// digits or overflow.
bool parseU64(const char*& p, const char* end, uint64_t& out) {
	if (p >= end || *p < '0' || *p > '9') return false;
	uint64_t v = 0;
	while (p < end && *p >= '0' && *p <= '9') {
		const uint64_t d = static_cast<uint64_t>(*p - '0');
		if (v > (UINT64_MAX - d) / 10) return false;
		v = v * 10 + d;
		++p;
	}
	out = v;
	return true;
}

void skipBlanks(const char*& p, const char* end) {
	while (p < end && (*p == ' ' || *p == '\t')) ++p;
}

double steadyMs() {
	using namespace std::chrono;
	return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

std::optional<CpuCounters> parseProcStatCpu(const char* text, size_t len) {
	if (!text || len < 5) return std::nullopt;
	const char* p = text;
	const char* end = text + len;
	if (std::memcmp(p, "cpu", 3) != 0 || p[3] != ' ') return std::nullopt; // aggregate line only
	p += 3;
	uint64_t f[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // user nice system idle iowait irq softirq steal
	int n = 0;
	while (n < 8) {
		skipBlanks(p, end);
		if (p >= end || *p == '\n') break;
		if (!parseU64(p, end, f[n])) return std::nullopt;
		++n;
	}
	if (n < 5) return std::nullopt; // need at least through iowait
	CpuCounters c;
	c.idleAll = f[3] + f[4];
	for (int i = 0; i < 8; ++i) c.total += f[i];
	return c;
}

std::optional<uint64_t> parseVmRssKb(const char* text, size_t len) {
	if (!text) return std::nullopt;
	static const char kKey[] = "VmRSS:";
	const size_t klen = sizeof(kKey) - 1;
	const char* end = text + len;
	for (const char* line = text; line < end;) {
		if (static_cast<size_t>(end - line) > klen && std::memcmp(line, kKey, klen) == 0) {
			const char* p = line + klen;
			skipBlanks(p, end);
			uint64_t kb = 0;
			if (!parseU64(p, end, kb)) return std::nullopt;
			skipBlanks(p, end);
			if (p < end && *p != '\n') {
				if (static_cast<size_t>(end - p) < 2 || p[0] != 'k' || p[1] != 'B') return std::nullopt;
			}
			return kb;
		}
		const char* nl = static_cast<const char*>(std::memchr(line, '\n', static_cast<size_t>(end - line)));
		if (!nl) break;
		line = nl + 1;
	}
	return std::nullopt;
}

bool thermalTypeMatches(const char* text, size_t len, const char* wanted) {
	if (!text || !wanted) return false;
	while (len > 0 && isSpace(text[len - 1])) --len;
	const size_t wlen = std::strlen(wanted);
	return len == wlen && std::memcmp(text, wanted, wlen) == 0;
}

std::optional<float> parseThermalMilliC(const char* text, size_t len) {
	if (!text) return std::nullopt;
	const char* p = text;
	const char* end = text + len;
	while (p < end && isSpace(*p)) ++p;
	bool neg = false;
	if (p < end && *p == '-') { neg = true; ++p; }
	uint64_t v = 0;
	if (!parseU64(p, end, v)) return std::nullopt;
	while (p < end && isSpace(*p)) ++p;
	if (p != end) return std::nullopt; // trailing junk
	const double c = (neg ? -1.0 : 1.0) * static_cast<double>(v) / 1000.0;
	if (c < -50.0 || c > 150.0) return std::nullopt;
	return static_cast<float>(c);
}

std::optional<float> CpuUtilization::feed(const CpuCounters& now) {
	if (!havePrev_) {
		prev_ = now;
		havePrev_ = true;
		return std::nullopt;
	}
	const CpuCounters prev = prev_;
	prev_ = now; // always re-baseline
	if (now.total < prev.total || now.idleAll < prev.idleAll) return std::nullopt; // reset / wrap
	const uint64_t dTotal = now.total - prev.total;
	const uint64_t dIdle = now.idleAll - prev.idleAll;
	if (dTotal == 0 || dIdle > dTotal) return std::nullopt;
	const double pct = static_cast<double>(dTotal - dIdle) / static_cast<double>(dTotal) * 100.0;
	return static_cast<float>(pct);
}

// ---- Default source --------------------------------------------------------

namespace {

#if defined(__linux__)
size_t readFile(const char* path, char* buf, size_t cap) {
	if (!buf || cap < 2) return 0;
	buf[0] = '\0';
	std::FILE* f = std::fopen(path, "r");
	if (!f) return 0;
	const size_t n = std::fread(buf, 1, cap - 1, f);
	std::fclose(f);
	buf[n] = '\0';
	return n;
}

class LinuxSource : public ISystemSource {
public:
	size_t readProcStat(char* buf, size_t cap) override { return readFile("/proc/stat", buf, cap < 512 ? cap : 512); }
	size_t readSelfStatus(char* buf, size_t cap) override { return readFile("/proc/self/status", buf, cap); }
	int findThermalZone(const char* wantedType) override {
		char path[64];
		char tbuf[64];
		for (int i = 0; i < 32; ++i) {
			std::snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/type", i);
			const size_t n = readFile(path, tbuf, sizeof(tbuf));
			if (n && thermalTypeMatches(tbuf, n, wantedType)) return i;
		}
		return -1;
	}
	size_t readThermalType(int zone, char* buf, size_t cap) override {
		char path[64];
		std::snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/type", zone);
		return readFile(path, buf, cap);
	}
	size_t readThermalTemp(int zone, char* buf, size_t cap) override {
		char path[64];
		std::snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", zone);
		return readFile(path, buf, cap);
	}
};
#else
class LinuxSource : public ISystemSource { // non-Linux: nothing to read
public:
	size_t readProcStat(char*, size_t) override { return 0; }
	size_t readSelfStatus(char*, size_t) override { return 0; }
	int findThermalZone(const char*) override { return -1; }
	size_t readThermalType(int, char*, size_t) override { return 0; }
	size_t readThermalTemp(int, char*, size_t) override { return 0; }
};
#endif

} // namespace

ISystemSource* defaultSystemSource() {
	static LinuxSource source;
	return &source;
}

namespace {

class SyntheticSource : public ISystemSource {
public:
	explicit SyntheticSource(bool dropouts) : dropouts_(dropouts) {}
	size_t readProcStat(char* buf, size_t cap) override {
		const double t = seconds();
		const double dt = t - lastT_;
		lastT_ = t;
		const double busyFrac = 0.35 + 0.25 * std::sin(t / 7.0);
		const double ticks = dt * 400.0; // 4 "cores" x 100 Hz
		busy_ += ticks * busyFrac;
		idle_ += ticks * (1.0 - busyFrac);
		return std::snprintf(buf, cap, "cpu  %llu 0 0 %llu 0 0 0 0 0 0\n", static_cast<unsigned long long>(busy_), static_cast<unsigned long long>(idle_)) > 0
			? std::strlen(buf) : 0;
	}
	size_t readSelfStatus(char* buf, size_t cap) override {
		const double t = seconds();
		const double kb = 180000.0 + 20000.0 * std::sin(t / 40.0) + 30.0 * t;
		return std::snprintf(buf, cap, "Name:\tx\nVmRSS:\t%llu kB\n", static_cast<unsigned long long>(kb)) > 0 ? std::strlen(buf) : 0;
	}
	int findThermalZone(const char*) override { return gpuUp() ? 0 : -1; }
	size_t readThermalType(int, char* buf, size_t cap) override { return gpuUp() ? std::snprintf(buf, cap, "gpu-thermal\n") : 0; }
	size_t readThermalTemp(int, char* buf, size_t cap) override {
		if (!gpuUp()) return 0;
		const double c = 52.0 + 6.0 * std::sin(seconds() / 60.0);
		return std::snprintf(buf, cap, "%d\n", static_cast<int>(c * 1000.0)) > 0 ? std::strlen(buf) : 0;
	}

private:
	static double rawSeconds() {
		using namespace std::chrono;
		return duration<double>(steady_clock::now().time_since_epoch()).count();
	}
	double seconds() const { return rawSeconds() - start_; } // since this source was created
	bool gpuUp() const {
		if (!dropouts_) return true;
		const double m = std::fmod(seconds(), 40.0);
		return !(m >= 20.0 && m < 32.0);
	}
	bool dropouts_;
	double start_ = rawSeconds();
	double lastT_ = 0.0;
	double busy_ = 0.0, idle_ = 0.0;
};

} // namespace

ISystemSource* syntheticSystemSource(bool dropouts) {
	static SyntheticSource plain(false);
	static SyntheticSource dropping(true);
	return dropouts ? &dropping : &plain;
}

// ---- SystemVitals ----------------------------------------------------------

SystemVitals::SystemVitals(ISystemSource* source) : source_(source ? source : defaultSystemSource()), costClock_(&steadyMs) {}

void SystemVitals::update(double now, float fps, float frameTimeMs) {
	if (!started_) {
		started_ = true;
		nextFps_ = now + kFpsPeriod;
		nextCpu_ = now; // first read baselines the CPU counters
		nextGpu_ = now;
		nextRss_ = now;
	}

	// Frame-time max accumulates every frame; nothing else happens per frame.
	if (frameTimeMs > bucketMaxMs_ || !bucketHasFrame_) bucketMaxMs_ = frameTimeMs;
	bucketHasFrame_ = true;

	if (now >= nextFps_) {
		fps_.push(now, fps);
		frameTime_.push(now, bucketMaxMs_);
		bucketMaxMs_ = 0.0f;
		bucketHasFrame_ = false;
		nextFps_ = now + kFpsPeriod; // no catch-up after a long gap
	}

	const bool cpuDue = now >= nextCpu_;
	const bool rssDue = now >= nextRss_;
	const bool gpuDue = now >= nextGpu_;
	if (!cpuDue && !rssDue && !gpuDue) return;

	const double t0 = costClock_();
	if (cpuDue) { sampleCpu(now); nextCpu_ = now + kCpuPeriod; }
	if (rssDue) { sampleRss(now); nextRss_ = now + kRssPeriod; }
	if (gpuDue) { sampleGpu(now); nextGpu_ = now + kGpuPeriod; }
	const double ms = costClock_() - t0;
	cost_.samplerRuns++;
	cost_.totalMs += ms;
	if (ms > cost_.maxMs) cost_.maxMs = ms;
}

void SystemVitals::sampleCpu(double now) {
	char buf[512];
	const size_t n = source_->readProcStat(buf, sizeof(buf));
	if (!n) { cpuCalc_.reset(); cpu_.push(now, 0.0f, false); return; } // unreadable: gap; next success re-baselines
	const auto counters = parseProcStatCpu(buf, n);
	if (!counters) { cpuCalc_.reset(); cpu_.push(now, 0.0f, false); return; }
	if (!cpuCalc_.hasBaseline()) { cpuCalc_.feed(*counters); return; } // first sample: baseline only, not a gap
	if (const auto pct = cpuCalc_.feed(*counters)) cpu_.push(now, *pct);
	else cpu_.push(now, 0.0f, false); // counter reset / zero total delta
}

void SystemVitals::sampleRss(double now) {
	char buf[4096];
	const size_t n = source_->readSelfStatus(buf, sizeof(buf));
	const auto kb = n ? parseVmRssKb(buf, n) : std::nullopt;
	if (kb) rss_.push(now, static_cast<float>(static_cast<double>(*kb) / 1024.0));
	else rss_.push(now, 0.0f, false);
}

void SystemVitals::sampleGpu(double now) {
	static const char kType[] = "gpu-thermal";
	char buf[64];
	if (gpuZone_ >= 0) {
		// Re-verify the zone's identity each sample: zone numbering can
		// change if devices come and go.
		const size_t tn = source_->readThermalType(gpuZone_, buf, sizeof(buf));
		if (!tn || !thermalTypeMatches(buf, tn, kType)) {
			gpuZone_ = -1;
			nextThermalScan_ = 0.0;
		}
	}
	if (gpuZone_ < 0 && now >= nextThermalScan_) {
		nextThermalScan_ = now + kThermalRediscoverSeconds;
		gpuZone_ = source_->findThermalZone(kType);
	}
	if (gpuZone_ < 0) { gpu_.push(now, 0.0f, false); return; }
	const size_t n = source_->readThermalTemp(gpuZone_, buf, sizeof(buf));
	if (!n) { gpuZone_ = -1; nextThermalScan_ = 0.0; gpu_.push(now, 0.0f, false); return; }
	if (const auto c = parseThermalMilliC(buf, n)) gpu_.push(now, *c);
	else gpu_.push(now, 0.0f, false);
}

namespace {
template <typename R>
State stateOf(const R& ring, double period, double now) {
	const Sample* s = ring.newestValid();
	if (!s) return State::Unavailable;
	return (now - s->t) > period * kStalePeriods ? State::Stale : State::Valid;
}
template <typename R>
std::optional<float> latestOf(const R& ring) {
	const Sample* s = ring.newestValid();
	return s ? std::optional<float>(s->value) : std::nullopt;
}
} // namespace

State SystemVitals::stateOfMetric(Metric m, double now) const {
	switch (m) {
		case Metric::Fps: return stateOf(fps_, kFpsPeriod, now);
		case Metric::FrameTimeMs: return stateOf(frameTime_, kFrameTimePeriod, now);
		case Metric::CpuPercent: return stateOf(cpu_, kCpuPeriod, now);
		case Metric::GpuTempC: return stateOf(gpu_, kGpuPeriod, now);
		case Metric::RssMiB: return stateOf(rss_, kRssPeriod, now);
		default: return State::Unavailable;
	}
}

State SystemVitals::state(Metric m, double now) const { return stateOfMetric(m, now); }

uint64_t SystemVitals::ringRevision(Metric m) const {
	switch (m) {
		case Metric::Fps: return fps_.revision();
		case Metric::FrameTimeMs: return frameTime_.revision();
		case Metric::CpuPercent: return cpu_.revision();
		case Metric::GpuTempC: return gpu_.revision();
		case Metric::RssMiB: return rss_.revision();
		default: return 0;
	}
}

std::optional<float> SystemVitals::latest(Metric m) const {
	switch (m) {
		case Metric::Fps: return latestOf(fps_);
		case Metric::FrameTimeMs: return latestOf(frameTime_);
		case Metric::CpuPercent: return latestOf(cpu_);
		case Metric::GpuTempC: return latestOf(gpu_);
		case Metric::RssMiB: return latestOf(rss_);
		default: return std::nullopt;
	}
}

template <typename R, typename S>
void SystemVitals::fillSeries(Metric m, const R& ring, State st, uint64_t metricRev, PoolEntry& e, S& out) {
	const size_t mi = static_cast<size_t>(m);
	out.state = st;
	out.revision = metricRev;
	if (const Sample* v = ring.newestValid()) {
		out.hasLatest = true;
		out.latest = v->value;
		out.latestTimeSeconds = v->t;
	} else {
		out.hasLatest = false;
		out.latest = 0.0f;
		out.latestTimeSeconds = 0.0;
	}
	// History is copied only if this pooled object does not already hold
	// the ring's current contents.
	if (!e.filled || e.histRev[mi] != ring.revision()) {
		out.count = static_cast<uint32_t>(ring.size());
		for (size_t i = 0; i < ring.size(); ++i) out.history[i] = ring.at(i);
		e.histRev[mi] = ring.revision();
		publishStats_.historiesCopied++;
		publishStats_.bytesCopied += ring.size() * sizeof(Sample);
	}
}

const std::shared_ptr<const SystemVitalsSnapshot>& SystemVitals::publish(double now) {
	publishStats_.calls++;
	bool changed = !published_;
	State st[kMetricCount];
	for (size_t i = 0; i < kMetricCount; ++i) {
		const Metric m = static_cast<Metric>(i);
		st[i] = stateOfMetric(m, now);
		if (st[i] != lastState_[i]) { stateChanges_[i]++; lastState_[i] = st[i]; }
		if (metricRevision(m) != publishedMetricRev_[i]) changed = true;
	}
	if (!changed) return published_;

	// Pick a pooled object that no consumer (and not published_) references.
	PoolEntry* entry = nullptr;
	for (auto& e : pool_) {
		if (e.snap && e.snap.use_count() == 1) { entry = &e; break; }
	}
	PoolEntry spill; // used only if every pooled object is still held elsewhere
	if (!entry) {
		for (auto& e : pool_) {
			if (!e.snap) { e.snap = std::make_shared<SystemVitalsSnapshot>(); publishStats_.poolAllocations++; entry = &e; break; }
		}
	}
	if (!entry) {
		spill.snap = std::make_shared<SystemVitalsSnapshot>();
		publishStats_.poolAllocations++;
		entry = &spill;
	}
	SystemVitalsSnapshot& s = *entry->snap;
	s.revision = ++snapshotRevision_;
	s.capturedAtSeconds = now;
	fillSeries(Metric::Fps, fps_, st[0], metricRevision(Metric::Fps), *entry, s.fps);
	fillSeries(Metric::FrameTimeMs, frameTime_, st[1], metricRevision(Metric::FrameTimeMs), *entry, s.frameTimeMax);
	fillSeries(Metric::CpuPercent, cpu_, st[2], metricRevision(Metric::CpuPercent), *entry, s.cpuBusy);
	fillSeries(Metric::GpuTempC, gpu_, st[3], metricRevision(Metric::GpuTempC), *entry, s.gpuTemp);
	fillSeries(Metric::RssMiB, rss_, st[4], metricRevision(Metric::RssMiB), *entry, s.processRss);
	entry->filled = true;
	for (size_t i = 0; i < kMetricCount; ++i) publishedMetricRev_[i] = metricRevision(static_cast<Metric>(i));
	published_ = entry->snap; // shared_ptr<const> view of the pooled object
	publishStats_.publications++;
	return published_;
}

} // namespace sysvitals
