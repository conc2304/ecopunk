#pragma once

// RT-004 stall localization probe — DIAGNOSTIC ONLY, not production behavior.
//
// Enabled only when EXPERIENCE_RUNTIME_STALL_PROBE is set; otherwise every
// RT004_* macro is one branch on a cached bool and records nothing.
//
// What it does when enabled (main thread only — the openFrameworks
// update/draw loop is single-threaded):
//   - RT004_SCOPE("label") brackets a block with steady_clock entry/exit.
//   - rt004::Probe::get().beginFrame() (called at the top of ofApp::update)
//     closes the previous frame. If its interval exceeded the threshold
//     (EXPERIENCE_RUNTIME_STALL_PROBE_MS, default 200), the whole frame's
//     scope tree, the time not covered by any top-level scope, the last
//     few frame summaries, and the current context are appended to the log.
//     Nothing is written for normal frames.
//   - Per-label duration samples (bounded) feed count/max/p95/p99 at exit.
//   - A watchdog thread notices when the main thread has been inside the
//     current frame for > 1 s, records the open scope stack, and (macOS,
//     EXPERIENCE_RUNTIME_STALL_PROBE_SAMPLE=1) runs `sample <pid>` to
//     capture native stacks while the stall is still happening.
//
// Log: EXPERIENCE_RUNTIME_STALL_PROBE_LOG, default ./rt004-stall-probe.log
// (relative to the process working directory).

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#endif

namespace rt004 {

using Clock = std::chrono::steady_clock;

inline int64_t nowNs() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

class Probe {
public:
	static Probe & get() {
		static Probe probe;
		return probe;
	}

	bool enabled() const { return enabled_; }

	// Called at the very top of ofApp::update(). Closes the previous frame.
	void beginFrame() {
		if (!enabled_) return;
		glCheckpoint("(between frames: draw tail, swap, events)");
		int64_t now = nowNs();
		if (frameStartNs_ != 0) {
			closeFrame(now);
		}
		frameIndex_++;
		frameStartNs_ = now;
		frameStartAtomic_.store(now);
		events_.clear();
		watchdogReportedFrame_.store(-1);
	}

	void push(const char * label) {
		int d = depth_.load(std::memory_order_relaxed);
		if (d < kMaxDepth) openStack_[d].store(label, std::memory_order_relaxed);
		depth_.store(d + 1, std::memory_order_release);
	}

	void pop(const char * label, int64_t startNs, int64_t endNs) {
		int d = depth_.load(std::memory_order_relaxed) - 1;
		depth_.store(d < 0 ? 0 : d, std::memory_order_release);
		int64_t dur = endNs - startNs;
		if (events_.size() < kMaxEventsPerFrame) {
			events_.push_back({ label, startNs, dur, d });
		}
		addSample(stats_[label], dur);
		// Per-window copies for Temporal scopes only (A/B attribution).
		if (!window_.empty() && isWindowedLabel(label)) addSample(windowStats_[std::string(label) + "@" + window_], dur);
	}

	static bool isWindowedLabel(const char * label) {
		const std::string l(label);
		return l.rfind("TOVB.", 0) == 0 || l.rfind("TimeOffsetVideoBuffer.", 0) == 0 || l.rfind("Temporal.", 0) == 0;
	}

	// Free-form correlation context (scene, phase, epoch, mediaId, origin).
	void setContext(const char * key, const std::string & value) {
		if (!enabled_) return;
		std::lock_guard<std::mutex> lock(contextMutex_);
		context_[key] = value;
		if (std::string(key) == "window") window_ = value; // main thread only, like pop()
	}

	// Instant event recorded into the current frame (e.g. "media change").
	void note(const char * label, const std::string & detail) {
		if (!enabled_) return;
		std::ostringstream os;
		const int64_t n = nowNs();
		// mono= is steady_clock seconds (CLOCK_MONOTONIC on Linux), the time
		// basis shared with external samplers.
		os << "  NOTE frame=" << frameIndex_ << " mono=" << std::fixed << std::setprecision(3) << n / 1e9 << std::defaultfloat
		   << " t=" << msSinceStart(n) << "ms " << label << ": " << detail << "\n";
		pendingNotes_ += os.str();
		writeRaw(os.str());
	}

	// GL error attribution (EXPERIENCE_RUNTIME_GL_ERROR_TRACE=1, probe on).
	// The header stays GL-free: the app installs a glGetError() wrapper
	// once a context exists. Every scope then drains the GL error flags on
	// entry (charged to the enclosing scope's own unscoped code) and on exit
	// (charged to that scope, whose nested scopes already took theirs), so
	// each error lands on the innermost instrumented code that raised it.
	// Diagnostic only — glGetError per scope edge.
	using GlErrorFn = unsigned (*)();
	void setGlErrorHook(GlErrorFn fn) { glErrorFn_ = (enabled_ && glTrace_) ? fn : nullptr; }
	bool glTraceActive() const { return glErrorFn_ != nullptr; }
	void glCheckpoint(const char * attributeTo) {
		if (glErrorFn_ == nullptr) return;
		for (int guard = 0; guard < 8; guard++) { // GL may queue several flags
			const unsigned err = glErrorFn_();
			if (err == 0) return;
			std::ostringstream key;
			key << attributeTo << ",0x" << std::hex << err;
			uint64_t & n = glErrors_[key.str()];
			if (n++ == 0) {
				std::ostringstream os;
				os << "  GLERROR first frame=" << frameIndex_ << " t=" << msSinceStart(nowNs()) << "ms in " << attributeTo
				   << " code=0x" << std::hex << err << "\n";
				writeRaw(os.str());
			}
		}
	}
	const char * openScopeLabel() const {
		int d = depth_.load(std::memory_order_relaxed);
		if (d <= 0) return "(outside scopes)";
		const char * l = openStack_[std::min(d, kMaxDepth) - 1].load(std::memory_order_relaxed);
		return l ? l : "?";
	}

	// Event counter (allocations, uploads, ...) reported in the summary.
	void count(const char * label, uint64_t n = 1) {
		if (!enabled_) return;
		counters_[label] += n;
		if (!window_.empty()) windowCounters_[std::string(label) + "@" + window_] += n;
	}

	uint64_t frameIndex() const { return frameIndex_; }

	~Probe() {
		if (!enabled_) return;
		stopWatchdog_.store(true);
		if (watchdog_.joinable()) watchdog_.join();
		writeSummary();
	}

	void writeSummary() {
		if (!enabled_ || summaryWritten_) return;
		summaryWritten_ = true;
		std::ostringstream os;
		os << "\n=== RT-004 SUMMARY frames=" << frameIndex_ << " longFrames=" << longFrames_
		   << " thresholdMs=" << thresholdMs_ << " ===\n";
		os << "label,count,totalMs,meanMs,p95Ms,p99Ms,maxMs\n";
		std::vector<std::pair<const char *, Stat *>> rows;
		for (auto & kv : stats_) rows.push_back({ kv.first, &kv.second });
		std::sort(rows.begin(), rows.end(), [](auto & a, auto & b) { return a.second->maxNs > b.second->maxNs; });
		for (auto & r : rows) {
			Stat & s = *r.second;
			std::vector<int64_t> v = s.samples;
			std::sort(v.begin(), v.end());
			auto pct = [&](double p) { return v.empty() ? 0.0 : v[std::min(v.size() - 1, static_cast<size_t>(p * v.size()))] / 1e6; };
			os << r.first << "," << s.count << "," << s.totalNs / 1e6 << "," << (s.count ? s.totalNs / 1e6 / s.count : 0.0)
			   << "," << pct(0.95) << "," << pct(0.99) << "," << s.maxNs / 1e6 << "\n";
		}
		for (auto & kv : counters_) os << "counter," << kv.first << "," << kv.second << "\n";
		for (auto & kv : windowStats_) {
			std::vector<int64_t> v = kv.second.samples;
			std::sort(v.begin(), v.end());
			auto pct = [&](double p) { return v.empty() ? 0.0 : v[std::min(v.size() - 1, static_cast<size_t>(p * v.size()))] / 1e6; };
			const Stat & st = kv.second;
			os << "window," << kv.first << "," << st.count << "," << st.totalNs / 1e6 << "," << (st.count ? st.totalNs / 1e6 / st.count : 0.0)
			   << "," << pct(0.95) << "," << pct(0.99) << "," << st.maxNs / 1e6 << "\n";
		}
		for (auto & kv : windowCounters_) os << "windowCounter," << kv.first << "," << kv.second << "\n";
		for (auto & kv : glErrors_) os << "glError," << kv.first << "," << kv.second << "\n";
		for (auto & kv : intervalsByOwner_) {
			std::vector<float> v(kv.second.begin(), kv.second.end());
			std::sort(v.begin(), v.end());
			auto q = [&](double p) { return v.empty() ? 0.0f : v[std::min(v.size() - 1, static_cast<size_t>(p * v.size()))]; };
			os << "frameIntervalMs[owner=" << kv.first << "] n=" << v.size() << " p50=" << q(0.50) << " p95=" << q(0.95)
			   << " p99=" << q(0.99) << " max=" << (v.empty() ? 0.0f : v.back()) << "\n";
		}
		os << "frameIntervalMs p50=" << intervalPct(0.50) << " p95=" << intervalPct(0.95) << " p99=" << intervalPct(0.99)
		   << " max=" << maxIntervalMs_ << "\n";
		writeRaw(os.str());
	}

	// Measures the enabled-probe cost of one empty scope (push/pop/stat).
	double measureScopeOverheadNs(int iterations = 200000) {
		static const char * kLabel = "rt004.overheadCalibration";
		int64_t t0 = nowNs();
		for (int i = 0; i < iterations; i++) {
			int64_t s = nowNs();
			push(kLabel);
			pop(kLabel, s, nowNs());
		}
		double perScope = static_cast<double>(nowNs() - t0) / iterations;
		stats_.erase(kLabel);
		events_.clear();
		return perScope;
	}

private:
	struct Event {
		const char * label;
		int64_t startNs;
		int64_t durNs;
		int depth;
	};
	struct Stat {
		uint64_t count = 0;
		int64_t totalNs = 0;
		int64_t maxNs = 0;
		std::vector<int64_t> samples;
	};
	static void addSample(Stat & s, int64_t dur) {
		s.count++;
		s.totalNs += dur;
		s.maxNs = std::max(s.maxNs, dur);
		if (s.samples.size() < kMaxSamplesPerLabel) {
			s.samples.push_back(dur);
		} else {
			s.samples[s.count % kMaxSamplesPerLabel] = dur;
		}
	}
	struct FrameSummary {
		uint64_t index;
		double intervalMs;
		double coveredMs;
		std::string context;
	};

	static constexpr int kMaxDepth = 32;
	static constexpr size_t kMaxEventsPerFrame = 4096;
	static constexpr size_t kMaxSamplesPerLabel = 20000;
	static constexpr size_t kRecentFrames = 8;

	Probe() {
		enabled_ = std::getenv("EXPERIENCE_RUNTIME_STALL_PROBE") != nullptr;
		if (!enabled_) return;
		if (const char * ms = std::getenv("EXPERIENCE_RUNTIME_STALL_PROBE_MS")) thresholdMs_ = std::atof(ms);
		const char * path = std::getenv("EXPERIENCE_RUNTIME_STALL_PROBE_LOG");
		logPath_ = path ? path : "rt004-stall-probe.log";
		sampleStacks_ = std::getenv("EXPERIENCE_RUNTIME_STALL_PROBE_SAMPLE") != nullptr;
		glTrace_ = std::getenv("EXPERIENCE_RUNTIME_GL_ERROR_TRACE") != nullptr;
		processStartNs_ = nowNs();
		std::ostringstream os;
		os << "=== RT-004 stall probe enabled: thresholdMs=" << thresholdMs_ << " sampleStacks=" << sampleStacks_ << " ===\n";
		writeRaw(os.str());
		watchdog_ = std::thread([this] { watchdogLoop(); });
	}

	double msSinceStart(int64_t ns) const { return (ns - processStartNs_) / 1e6; }

	std::string contextString() {
		std::lock_guard<std::mutex> lock(contextMutex_);
		std::ostringstream os;
		for (auto & kv : context_) os << kv.first << "=" << kv.second << " ";
		return os.str();
	}

	double intervalPct(double p) const {
		if (intervals_.empty()) return 0.0;
		std::vector<float> v(intervals_.begin(), intervals_.end());
		std::sort(v.begin(), v.end());
		return v[std::min(v.size() - 1, static_cast<size_t>(p * v.size()))];
	}

	void closeFrame(int64_t now) {
		double intervalMs = (now - frameStartNs_) / 1e6;
		double coveredMs = 0.0;
		for (const Event & e : events_) {
			if (e.depth == 0) coveredMs += e.durNs / 1e6;
		}
		maxIntervalMs_ = std::max(maxIntervalMs_, intervalMs);
		if (intervals_.size() >= kMaxSamplesPerLabel) intervals_.pop_front();
		intervals_.push_back(static_cast<float>(intervalMs));

		std::string ctx = contextString();
		{
			std::string owner;
			{
				std::lock_guard<std::mutex> lock(contextMutex_);
				auto it = context_.find("owner");
				owner = it == context_.end() ? "?" : it->second;
				// Optional measurement window set by a driver script
				// (e.g. warmup/steady/media-change), so steady-state frame
				// intervals are reported apart from transition frames.
				auto w = context_.find("window");
				if (w != context_.end()) owner += "/" + w->second;
			}
			auto & dq = intervalsByOwner_[owner];
			if (dq.size() >= kMaxSamplesPerLabel) dq.pop_front();
			dq.push_back(static_cast<float>(intervalMs));
		}
		if (intervalMs >= thresholdMs_) {
			longFrames_++;
			std::ostringstream os;
			os << "\n--- LONG FRAME #" << longFrames_ << " frame=" << frameIndex_ << " mono=" << std::fixed << std::setprecision(3)
			   << frameStartNs_ / 1e9 << std::defaultfloat << " start=" << msSinceStart(frameStartNs_)
			   << "ms interval=" << intervalMs << "ms covered=" << coveredMs << "ms uncovered=" << (intervalMs - coveredMs)
			   << "ms\n  context: " << ctx << "\n  previous frames:\n";
			for (const FrameSummary & f : recent_) {
				os << "    frame=" << f.index << " interval=" << f.intervalMs << "ms covered=" << f.coveredMs << "ms " << f.context << "\n";
			}
			os << "  scope tree (start offset ms, duration ms):\n";
			for (const Event & e : sortedByStart()) {
				if (e.durNs < 100000 && e.depth > 1) continue; // < 0.1 ms leaf noise
				os << "    " << std::string(e.depth * 2, ' ') << e.label << " @" << (e.startNs - frameStartNs_) / 1e6
				   << " dur=" << e.durNs / 1e6 << "\n";
			}
			writeRaw(os.str());
		}
		recent_.push_back({ frameIndex_, intervalMs, coveredMs, ctx });
		if (recent_.size() > kRecentFrames) recent_.pop_front();
		pendingNotes_.clear();
	}

	std::vector<Event> sortedByStart() const {
		std::vector<Event> v = events_;
		std::stable_sort(v.begin(), v.end(), [](const Event & a, const Event & b) { return a.startNs < b.startNs; });
		return v;
	}

	void watchdogLoop() {
		while (!stopWatchdog_.load()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			int64_t start = frameStartAtomic_.load();
			if (start == 0) continue;
			int64_t frame = static_cast<int64_t>(frameIndex_);
			double inFrameMs = (nowNs() - start) / 1e6;
			if (inFrameMs < 1000.0 || watchdogReportedFrame_.load() == frame) continue;
			watchdogReportedFrame_.store(frame);
			std::ostringstream os;
			os << "\n!!! WATCHDOG frame=" << frame << " main thread " << inFrameMs << "ms into frame; open scopes: ";
			int d = std::min(depth_.load(std::memory_order_acquire), kMaxDepth);
			if (d == 0) os << "(none — outside instrumented code)";
			for (int i = 0; i < d; i++) {
				const char * l = openStack_[i].load(std::memory_order_relaxed);
				os << (i ? " > " : "") << (l ? l : "?");
			}
			os << "\n";
			writeRaw(os.str());
#if defined(__APPLE__)
			if (sampleStacks_) {
				std::ostringstream cmd;
				std::string out = logPath_ + ".sample-frame" + std::to_string(frame) + ".txt";
				cmd << "/usr/bin/sample " << getpid() << " 1 1 -mayDie -file '" << out << "' >/dev/null 2>&1";
				int rc = std::system(cmd.str().c_str());
				writeRaw("!!! WATCHDOG native stack sample -> " + out + " rc=" + std::to_string(rc) + "\n");
			}
#endif
		}
	}

	void writeRaw(const std::string & text) {
		std::lock_guard<std::mutex> lock(fileMutex_);
		if (FILE * f = std::fopen(logPath_.c_str(), "a")) {
			std::fwrite(text.data(), 1, text.size(), f);
			std::fclose(f);
		}
	}

	bool enabled_ = false;
	bool summaryWritten_ = false;
	bool sampleStacks_ = false;
	double thresholdMs_ = 200.0;
	std::string logPath_;
	int64_t processStartNs_ = 0;
	int64_t frameStartNs_ = 0;
	uint64_t frameIndex_ = 0;
	uint64_t longFrames_ = 0;
	double maxIntervalMs_ = 0.0;
	std::vector<Event> events_;
	std::unordered_map<const char *, Stat> stats_;
	std::unordered_map<const char *, uint64_t> counters_;
	std::string window_;
	std::map<std::string, Stat> windowStats_;
	std::map<std::string, uint64_t> windowCounters_;
	bool glTrace_ = false;
	GlErrorFn glErrorFn_ = nullptr;
	std::map<std::string, uint64_t> glErrors_;
	std::deque<float> intervals_;
	std::unordered_map<std::string, std::deque<float>> intervalsByOwner_;
	std::deque<FrameSummary> recent_;
	std::string pendingNotes_;

	std::array<std::atomic<const char *>, kMaxDepth> openStack_ {};
	std::atomic<int> depth_ { 0 };
	std::atomic<int64_t> frameStartAtomic_ { 0 };
	std::atomic<int64_t> watchdogReportedFrame_ { -1 };
	std::atomic<bool> stopWatchdog_ { false };
	std::thread watchdog_;

	std::mutex contextMutex_;
	std::unordered_map<std::string, std::string> context_;
	std::mutex fileMutex_;
};

class Scope {
public:
	explicit Scope(const char * label) {
		if (!Probe::get().enabled()) return;
		label_ = label;
		Probe & p = Probe::get();
		if (p.glTraceActive()) p.glCheckpoint(p.openScopeLabel());
		p.push(label);
		start_ = nowNs();
	}
	~Scope() {
		if (label_ == nullptr) return;
		const int64_t end = nowNs();
		Probe & p = Probe::get();
		if (p.glTraceActive()) p.glCheckpoint(label_);
		p.pop(label_, start_, end);
	}
	Scope(const Scope &) = delete;
	Scope & operator=(const Scope &) = delete;

private:
	const char * label_ = nullptr;
	int64_t start_ = 0;
};

} // namespace rt004

#define RT004_CONCAT_INNER(a, b) a##b
#define RT004_CONCAT(a, b) RT004_CONCAT_INNER(a, b)
#define RT004_SCOPE(label) ::rt004::Scope RT004_CONCAT(rt004Scope_, __LINE__)(label)
