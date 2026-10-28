// System Telemetry v1 — parsers, rings, cadence, CPU math, unavailable/stale.
// Deterministic: fake source + injected monotonic time; no real /proc access.

#include "SystemVitals.h"

#include <memory>

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

using namespace sysvitals;

namespace {
int g_total = 0, g_failures = 0;
void check(bool c, const std::string& what) {
	g_total++;
	if (!c) { g_failures++; std::cerr << "FAIL: " << what << "\n"; }
}
bool near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) <= eps; }

size_t copyOut(const std::string& s, char* buf, size_t cap) {
	if (s.empty() || cap < 2) return 0;
	const size_t n = s.size() < cap - 1 ? s.size() : cap - 1;
	std::memcpy(buf, s.data(), n);
	buf[n] = '\0';
	return n;
}

struct FakeSource : ISystemSource {
	std::string stat, status;
	int zone = -1;               // zone returned by findThermalZone
	std::string zoneType = "gpu-thermal\n";
	std::string zoneTemp = "45000\n";
	int scans = 0, reads = 0;
	size_t readProcStat(char* b, size_t c) override { reads++; return copyOut(stat, b, c); }
	size_t readSelfStatus(char* b, size_t c) override { reads++; return copyOut(status, b, c); }
	int findThermalZone(const char*) override { scans++; return zone; }
	size_t readThermalType(int, char* b, size_t c) override { reads++; return copyOut(zoneType, b, c); }
	size_t readThermalTemp(int, char* b, size_t c) override { reads++; return copyOut(zoneTemp, b, c); }
};

std::string statLine(uint64_t user, uint64_t idle, uint64_t iowait = 0) {
	return "cpu  " + std::to_string(user) + " 0 0 " + std::to_string(idle) + " " + std::to_string(iowait) +
		   " 0 0 0 0 0\ncpu0 1 2 3 4 5 6 7 8 9 10\n";
}

void testParsers() {
	const std::string s = "cpu  100 20 30 400 50 6 7 8 9 10\ncpu0 1 1 1 1 1 1 1 1 1 1\n";
	auto c = parseProcStatCpu(s.data(), s.size());
	check(c && c->idleAll == 450, "idle includes iowait (400+50)");
	check(c && c->total == 100 + 20 + 30 + 400 + 50 + 6 + 7 + 8, "total excludes guest fields");
	const std::string old = "cpu 10 0 5 85 0\n";
	check(parseProcStatCpu(old.data(), old.size()).has_value(), "short (5-field) line accepted");
	check(!parseProcStatCpu("", 0), "empty /proc/stat");
	const std::string percore = "cpu0 1 2 3 4 5\n";
	check(!parseProcStatCpu(percore.data(), percore.size()), "per-core first line rejected");
	const std::string bad = "cpu  12 abc 3 4 5\n";
	check(!parseProcStatCpu(bad.data(), bad.size()), "malformed field");
	const std::string few = "cpu  1 2 3\n";
	check(!parseProcStatCpu(few.data(), few.size()), "too few fields");
	const std::string huge = "cpu  99999999999999999999999 0 0 0 0\n";
	check(!parseProcStatCpu(huge.data(), huge.size()), "overflowing counter");

	const std::string st = "Name:\tx\nVmPeak:\t 9999 kB\nVmRSS:\t  123456 kB\nThreads:\t3\n";
	auto rss = parseVmRssKb(st.data(), st.size());
	check(rss && *rss == 123456, "VmRSS parsed");
	const std::string notRss = "VmHWM:\t5 kB\nxVmRSS:\t7 kB\n";
	check(!parseVmRssKb(notRss.data(), notRss.size()), "VmRSS must match at line start");
	const std::string noKb = "VmRSS:\t12 MB\n";
	check(!parseVmRssKb(noKb.data(), noKb.size()), "wrong unit rejected");
	const std::string junk = "VmRSS:\tabc kB\n";
	check(!parseVmRssKb(junk.data(), junk.size()), "malformed VmRSS");
	check(!parseVmRssKb("", 0), "empty status");

	check(thermalTypeMatches("gpu-thermal\n", 12, "gpu-thermal"), "type match with newline");
	check(!thermalTypeMatches("cpu-thermal\n", 12, "gpu-thermal"), "type mismatch");
	check(!thermalTypeMatches("gpu-thermal2\n", 13, "gpu-thermal"), "type prefix is not a match");
	check(!thermalTypeMatches("", 0, "gpu-thermal"), "empty type");

	auto t = parseThermalMilliC("45123\n", 6);
	check(t && near(*t, 45.123f, 0.001f), "millidegrees parsed");
	t = parseThermalMilliC("0\n", 2);
	check(t && *t == 0.0f, "valid zero degrees is a value, not missing");
	t = parseThermalMilliC("-5000\n", 6);
	check(t && near(*t, -5.0f), "negative temperature");
	check(!parseThermalMilliC("", 0), "empty temp");
	check(!parseThermalMilliC("hot\n", 4), "malformed temp");
	check(!parseThermalMilliC("45000x\n", 7), "trailing junk temp");
	check(!parseThermalMilliC("900000\n", 7), "implausible temp (900 C) rejected");
}

void testCpu() {
	CpuUtilization u;
	check(!u.feed({1000, 2000}), "first sample has no value");
	auto r = u.feed({1100, 2400}); // dTotal 400, dIdle 100 -> 75%
	check(r && near(*r, 75.0f), "delta math 75%");
	r = u.feed({1200, 2500}); // dTotal 100 dIdle 100 -> 0% (valid zero)
	check(r && *r == 0.0f, "fully idle interval is a valid 0%");
	CpuUtilization z;
	z.feed({10, 20});
	check(!z.feed({10, 20}), "zero total delta -> unavailable, not 0");
	CpuUtilization w;
	w.feed({5000, 9000});
	check(!w.feed({100, 200}), "counter reset -> no value");
	r = w.feed({150, 300}); // re-baselined: dTotal 150, dIdle 100... total=150? idleAll=300? use consistent
	check(!r || (*r >= 0.0f && *r <= 100.0f), "post-reset value bounded");
	CpuUtilization inc;
	inc.feed({100, 50});
	check(!inc.feed({110, 55}), "idle delta > total delta rejected");
	// iowait counts as idle
	auto pa = parseProcStatCpu("cpu 0 0 0 0 0\n", 14);
	auto pb = parseProcStatCpu("cpu 50 0 0 25 25\n", 17);
	CpuUtilization io;
	io.feed(*pa);
	r = io.feed(*pb);
	check(r && near(*r, 50.0f), "iowait treated as idle (50% busy)");
}

void testRing() {
	Ring<4> r;
	check(r.size() == 0 && r.newest() == nullptr, "empty ring");
	for (int i = 0; i < 10; ++i) r.push(i, static_cast<float>(i));
	check(r.size() == 4, "ring capped at capacity");
	check(r.at(0).value == 6 && r.at(3).value == 9, "chronological after rollover");
	bool ordered = true;
	for (size_t i = 1; i < r.size(); ++i) ordered = ordered && r.at(i).t > r.at(i - 1).t;
	check(ordered, "timestamps strictly increasing");

	check(Ring<kFpsCapacity>::kCapacity == 120 && kFpsCapacity == static_cast<size_t>(60 / kFpsPeriod), "FPS 60 s @ 2 Hz");
	check(kFrameTimeCapacity == static_cast<size_t>(60 / kFrameTimePeriod), "frame time 60 s @ 2 Hz");
	check(kCpuCapacity == static_cast<size_t>(60 / kCpuPeriod), "CPU 60 s @ 1 Hz");
	check(kGpuCapacity == static_cast<size_t>(300 / kGpuPeriod), "GPU 5 min @ 0.5 Hz");
	check(kRssCapacity == static_cast<size_t>(300 / kRssPeriod), "RSS 5 min @ 1 Hz");
	check(sizeof(SystemVitals) < 16 * 1024, "bounded fixed footprint (" + std::to_string(sizeof(SystemVitals)) + " B)");
}

double g_ms = 0.0;
double fakeMs() { return g_ms += 0.01; }

void testCadenceAndBuckets() {
	FakeSource src;
	src.stat = statLine(0, 0);
	src.status = "VmRSS:\t 204800 kB\n";
	src.zone = 3;
	SystemVitals v(&src);
	v.setCostClock(&fakeMs);

	// 60 fps for 10 s, with one 90 ms spike at t=2.2 s.
	double t = 100.0;
	for (int i = 0; i < 600; ++i, t += 1.0 / 60.0) {
		const float ft = (i == 132) ? 90.0f : 16.6f;
		src.stat = statLine(static_cast<uint64_t>(i) * 10, static_cast<uint64_t>(i) * 30);
		v.update(t, 60.0f, ft);
	}
	check(v.fps().size() >= 19 && v.fps().size() <= 21, "fps ~2 Hz over 10 s (" + std::to_string(v.fps().size()) + ")");
	check(v.cpu().size() >= 9 && v.cpu().size() <= 10, "cpu ~1 Hz, first read baseline only, no gaps (" + std::to_string(v.cpu().size()) + ")");
	check(v.rss().size() >= 10 && v.rss().size() <= 11, "rss ~1 Hz");
	check(v.gpuTemp().size() >= 5 && v.gpuTemp().size() <= 6, "gpu ~0.5 Hz (" + std::to_string(v.gpuTemp().size()) + ")");
	float maxFt = 0;
	for (size_t i = 0; i < v.frameTime().size(); ++i) maxFt = std::fmax(maxFt, v.frameTime().at(i).value);
	check(near(maxFt, 90.0f), "bucket keeps the MAX frame time (spike survives)");
	int spikeBuckets = 0;
	for (size_t i = 0; i < v.frameTime().size(); ++i) if (v.frameTime().at(i).value > 50.0f) spikeBuckets++;
	check(spikeBuckets == 1, "spike appears in exactly one bucket");
	check(v.state(Metric::RssMiB, t) == State::Valid, "rss valid");
	check(near(*v.latest(Metric::RssMiB), 200.0f), "rss MiB conversion");
	check(near(*v.latest(Metric::GpuTempC), 45.0f), "gpu deg C");
	check(near(*v.latest(Metric::CpuPercent), 25.0f), "cpu 10/(10+30)=25%");
	check(v.cost().samplerRuns > 0 && v.cost().totalMs > 0, "sampler cost accounted");

	// Long gap: one sample per metric, no catch-up burst.
	const size_t f0 = v.fps().size(), c0 = v.cpu().size(), r0 = v.rss().size();
	v.update(t + 30.0, 60.0f, 16.6f);
	check(v.fps().size() == f0 + 1 && v.cpu().size() <= c0 + 1 && v.rss().size() == r0 + 1, "30 s gap yields single sample per metric");
	check(v.state(Metric::RssMiB, t + 30.0 + 10.0) == State::Stale, "stale after source silence");
}

void testRollover() {
	FakeSource src;
	src.stat = statLine(0, 0);
	src.status = "VmRSS:\t 1024 kB\n";
	SystemVitals v(&src);
	double t = 0;
	for (int i = 0; i < 20000; ++i, t += 0.05) {
		src.stat = statLine(static_cast<uint64_t>(i) * 5, static_cast<uint64_t>(i) * 5);
		v.update(t, 20.0f, 50.0f);
	}
	check(v.fps().size() == kFpsCapacity && v.frameTime().size() == kFrameTimeCapacity, "fps/frame rings full, not larger");
	check(v.cpu().size() == kCpuCapacity && v.rss().size() == kRssCapacity, "cpu/rss rings full");
	bool ordered = true;
	for (size_t i = 1; i < v.rss().size(); ++i) ordered = ordered && v.rss().at(i).t > v.rss().at(i - 1).t;
	check(ordered, "rss chronology after wrap");
	check(near(v.rss().newest()->t, t - 0.05, 1.1), "newest retained sample is recent");
	check(!v.gpuTemp().newestValid() && v.state(Metric::GpuTempC, t) == State::Unavailable, "no gpu zone -> Unavailable");
	check(src.scans > 0 && src.scans < 20000 * 0.05 / kThermalRediscoverSeconds + 3, "thermal rescan rate-limited (" + std::to_string(src.scans) + ")");
}

void testUnavailableAndRecovery() {
	FakeSource src; // nothing readable
	SystemVitals v(&src);
	double t = 0;
	for (int i = 0; i < 300; ++i, t += 0.1) v.update(t, 30.0f, 33.0f);
	check(v.state(Metric::CpuPercent, t) == State::Unavailable && !v.latest(Metric::CpuPercent), "cpu unavailable");
	check(v.state(Metric::RssMiB, t) == State::Unavailable && !v.latest(Metric::RssMiB), "rss unavailable");
	check(v.state(Metric::Fps, t) == State::Valid, "fps independent of /proc");

	// Malformed sources never create samples.
	src.stat = "garbage\n";
	src.status = "VmRSS:\tnope kB\n";
	for (int i = 0; i < 100; ++i, t += 0.1) v.update(t, 30.0f, 33.0f);
	check(!v.cpu().newestValid() && !v.rss().newestValid(), "malformed -> no VALID samples");
	check(v.cpu().size() > 0 && !v.cpu().newest()->valid && !v.rss().newest()->valid, "malformed -> gap markers, not zeros");

	// Source appears.
	src.stat = statLine(0, 0);
	src.status = "VmRSS:\t 2048 kB\n";
	src.zone = 1;
	for (int i = 0; i < 400; ++i, t += 0.1) { src.stat = statLine(i * 10, i * 10); v.update(t, 30.0f, 33.0f); }
	check(v.cpu().newestValid() && v.rss().newestValid(), "recovers when source appears");
	check(v.gpuTemp().newestValid(), "gpu zone discovered after the 30 s rescan window");

	// Thermal device change: zone now reports a different type.
	const double gt = v.gpuTemp().newestValid()->t;
	src.zoneType = "soc-thermal\n";
	src.zone = -1;
	for (int i = 0; i < 100; ++i, t += 0.1) v.update(t, 30.0f, 33.0f);
	check(v.gpuTemp().newestValid()->t == gt, "no valid samples from a zone that is no longer gpu-thermal");
	check(!v.gpuTemp().newest()->valid, "device loss recorded as a gap");
	check(v.latest(Metric::GpuTempC).has_value(), "last good gpu value retained (reported Stale)");
	check(v.state(Metric::GpuTempC, t + 100) == State::Stale, "gpu Stale, not Unavailable, after device loss");

	// Valid zero is distinct from unavailable.
	FakeSource z;
	z.zone = 0;
	z.zoneTemp = "0\n";
	SystemVitals vz(&z);
	for (int i = 0, tt = 0; i < 50; ++i, tt += 1) vz.update(tt, 30.0f, 33.0f);
	check(vz.latest(Metric::GpuTempC) && *vz.latest(Metric::GpuTempC) == 0.0f, "0 C is a valid sample");
	check(vz.gpuTemp().newest()->valid, "0 C entry is not a gap");
}


// ---- Snapshot publication ---------------------------------------------------
void testSnapshot() {
	FakeSource src;
	src.stat = statLine(0, 0);
	src.status = "VmRSS:\t 102400 kB\n";
	src.zone = 0;
	SystemVitals v(&src);
	check(v.published() == nullptr, "no snapshot before first publish");

	double t = 10.0;
	uint64_t lastRev = 0;
	bool monotonic = true;
	std::shared_ptr<const SystemVitalsSnapshot> first;
	std::shared_ptr<const SystemVitalsSnapshot> prev;
	int changes = 0, frames = 0;
	for (int i = 0; i < 1200; ++i, t += 1.0 / 60.0) {
		src.stat = statLine(static_cast<uint64_t>(i) * 10, static_cast<uint64_t>(i) * 30);
		v.update(t, 60.0f, i == 700 ? 80.0f : 16.6f);
		const auto& s = v.publish(t);
		frames++;
		if (s != prev) changes++;
		if (s->revision < lastRev) monotonic = false;
		lastRev = s->revision;
		if (!first) first = s;
		prev = s;
	}
	const auto last = v.published();
	check(monotonic && last->revision > first->revision, "snapshot revision monotonic");
	check(first.use_count() >= 2, "consumer keeps snapshot alive");
	check(changes > 20 && changes < frames / 5, "published far less often than per frame (" + std::to_string(changes) + "/" + std::to_string(frames) + ")");
	check(v.publishStats().publications == static_cast<uint64_t>(changes), "stats count publications");

	// Immutability: `first` was held the whole run; it must never change.
	check(first->fps.count <= 1 && first->revision == 1, "held snapshot unchanged (rev " + std::to_string(first->revision) + ")");
	check(last->fps.count == kFpsCapacity || last->fps.count > 30, "latest snapshot carries history");

	// Snapshot contents match collector, chronological, bounded.
	bool chron = true;
	for (uint32_t i = 1; i < last->processRss.count; ++i) chron = chron && last->processRss.at(i).t > last->processRss.at(i - 1).t;
	check(chron, "snapshot history chronological");
	check(last->processRss.count == v.rss().size() && last->processRss.count <= kRssCapacity, "snapshot rss bounded + matches ring");
	check(last->processRss.state == State::Valid && near(last->processRss.latest, 100.0f), "rss latest 100 MiB");
	check(last->cpuBusy.state == State::Valid && near(last->cpuBusy.latest, 25.0f), "cpu latest 25%");
	check(last->gpuTemp.state == State::Valid && near(last->gpuTemp.latest, 45.0f), "gpu latest 45 C");
	float worst = 0; for (uint32_t i = 0; i < last->frameTimeMax.count; ++i) worst = std::fmax(worst, last->frameTimeMax.at(i).value);
	check(near(worst, 80.0f), "worst frame survives into snapshot history");

	// No change -> same pointer, no new publication.
	const auto before = v.publishStats().publications;
	const auto& again = v.publish(t);
	check(again == last && v.publishStats().publications == before, "unchanged -> reused pointer");

	// Staleness is a published state change even with no new samples.
	src.stat = ""; src.status = ""; // sources die
	src.zone = -1; src.zoneType = "";
	double t2 = t;
	for (int i = 0; i < 600; ++i, t2 += 1.0 / 60.0) { v.update(t2, 60.0f, 16.6f); v.publish(t2); }
	const auto st = v.published();
	check(st->processRss.state == State::Stale && st->processRss.hasLatest, "rss Stale while last value retained");
	check(!st->processRss.history[st->processRss.count - 1].valid, "unavailable interval recorded as gap");
	check(st->fps.state == State::Valid, "fps unaffected");
	check(last->processRss.state == State::Valid, "old snapshot still reports what it saw (immutable)");

	// Pool: bounded allocations even after thousands of publications.
	check(v.publishStats().poolAllocations <= 5, "snapshot pool allocations bounded (" + std::to_string(v.publishStats().poolAllocations) + ")");
	check(v.publishStats().bytesCopied / v.publishStats().publications < sizeof(SystemVitalsSnapshot), "avg bytes copied per publication < full snapshot");
}

void testDefaultSourceSafe() {
	SystemVitals v; // real default source: Linux reads /proc, others unavailable
	double t = 0;
	for (int i = 0; i < 400; ++i, t += 0.05) v.update(t, 60.0f, 16.6f);
#if defined(__linux__)
	check(v.rss().size() > 0 && *v.latest(Metric::RssMiB) > 0.0f, "linux: real VmRSS read");
	check(v.cpu().newestValid(), "linux: real /proc/stat read");
#else
	check(v.state(Metric::RssMiB, t) == State::Unavailable, "non-linux: rss Unavailable");
	check(v.state(Metric::CpuPercent, t) == State::Unavailable, "non-linux: cpu Unavailable");
	check(v.state(Metric::GpuTempC, t) == State::Unavailable, "non-linux: gpu Unavailable");
#endif
	check(v.fps().size() > 0, "fps/frame time work on every platform");
}
} // namespace

int main() {
	testParsers();
	testCpu();
	testRing();
	testCadenceAndBuckets();
	testRollover();
	testUnavailableAndRecovery();
	testSnapshot();
	testDefaultSourceSafe();
	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures ? 1 : 0;
}
