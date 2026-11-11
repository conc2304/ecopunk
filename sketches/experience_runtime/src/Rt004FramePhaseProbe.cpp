#include "Rt004FramePhaseProbe.h"

#include "ExperienceRuntime.h"
#include "Rt004StallProbe.h"
#include "TemporalProductionScene.h"
#include "ofAppRunner.h"
#include "ofGLUtils.h"
#include "ofGraphics.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <vector>

#if defined(__linux__)
#include <sys/resource.h>
#endif

namespace {

bool envIs(const char* name, const char* value) {
	const char* v = std::getenv(name);
	return v != nullptr && std::string(v) == value;
}

int64_t steadyNs() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// /proc/meminfo or /proc/self/status value in MiB, -1 if unavailable.
double procMiB(const char* file, const char* key) {
#if defined(__linux__)
	std::ifstream in(file);
	std::string k;
	while (in >> k) {
		if (k == key) { double kb = 0; in >> kb; return kb / 1024.0; }
		in.ignore(4096, '\n');
	}
#else
	(void)file; (void)key;
#endif
	return -1.0;
}

double pct(std::vector<double> v, double p) {
	if (v.empty()) return 0.0;
	std::sort(v.begin(), v.end());
	return v[std::min(v.size() - 1, static_cast<size_t>(p * v.size()))];
}

const char* kSegName[] = { "update", "preDraw", "draw", "glFinish", "postDraw" };

} // namespace

bool Rt004FramePhaseProbe::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE") != nullptr;
}

bool Rt004FramePhaseProbe::presentOnlyControl() {
	static const bool presentOnly = isRequested() && envIs("EXPERIENCE_RUNTIME_FRAME_PHASE_CONTROL", "present-only");
	return presentOnly;
}

Rt004FramePhaseProbe::Rt004FramePhaseProbe(ExperienceRuntime& runtime) : runtime_(runtime) {
	const char* path = std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_LOG");
	logPath_ = path ? path : "rt004-frame-phase.log";
	if (const char* ms = std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_MS")) slowMs_ = std::max(1.0, std::atof(ms));
	glFinish_ = std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH") != nullptr;
	if (const char* w = std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_WINDOW_MS")) windowMs_ = std::max(250.0, std::atof(w));
	startNs_ = steadyNs();
	if (const char* w = std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_WARMUP_S")) warmupMs_ = std::max(0.0, std::atof(w)) * 1000.0;
	if (const char* s = std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_SETTLE_MS")) settleMs_ = std::max(0.0, std::atof(s));
	if (const char* p = std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES")) {
		frameCsv_ = std::fopen(p, "w");
		if (frameCsv_) {
			static char buf[1 << 16];
			std::setvbuf(frameCsv_, buf, _IOFBF, sizeof(buf));
			std::fprintf(frameCsv_, "frame,mono,total_ms");
			for (const char* s : kSegName) std::fprintf(frameCsv_, ",%s_wall,%s_cpu,%s_vcsw,%s_ivcsw", s, s, s, s);
			std::fprintf(frameCsv_, ",scene,phase,media,ex_startup,ex_media,ex_scene,ex_transition,exclude"
					",ev,ev_fail,fx_issue,fx_collect,raw_issue,raw_collect,fx_ifail,fx_cfail,raw_ifail,raw_cfail"
						// OC-003-F (appended; all earlier columns unchanged): CPU RAW samples/gaps this frame, their wall microseconds (access+reduce+analyze),
						// palette-sink microseconds (analysis + file I/O; 0 unless EXPERIENCE_RUNTIME_PALETTE_DIAG), decoder delivered a new frame
						",cpu_raw,cpu_raw_gap,cpu_raw_us,pal_us,vid_new\n");
		}
	}
	ofAddListener(ofEvents().update, this, &Rt004FramePhaseProbe::onUpdateBefore, OF_EVENT_ORDER_BEFORE_APP);
	ofAddListener(ofEvents().update, this, &Rt004FramePhaseProbe::onUpdateAfter, OF_EVENT_ORDER_AFTER_APP);
	ofAddListener(ofEvents().draw, this, &Rt004FramePhaseProbe::onDrawBefore, OF_EVENT_ORDER_BEFORE_APP);
	ofAddListener(ofEvents().draw, this, &Rt004FramePhaseProbe::onDrawAfter, OF_EVENT_ORDER_AFTER_APP);
}

Rt004FramePhaseProbe::~Rt004FramePhaseProbe() {
	ofRemoveListener(ofEvents().update, this, &Rt004FramePhaseProbe::onUpdateBefore, OF_EVENT_ORDER_BEFORE_APP);
	ofRemoveListener(ofEvents().update, this, &Rt004FramePhaseProbe::onUpdateAfter, OF_EVENT_ORDER_AFTER_APP);
	ofRemoveListener(ofEvents().draw, this, &Rt004FramePhaseProbe::onDrawBefore, OF_EVENT_ORDER_BEFORE_APP);
	ofRemoveListener(ofEvents().draw, this, &Rt004FramePhaseProbe::onDrawAfter, OF_EVENT_ORDER_AFTER_APP);
	writeWindowSummary((steadyNs() - startNs_) / 1e6);
	if (frameCsv_) std::fclose(frameCsv_);
}

void Rt004FramePhaseProbe::afterRuntimeSetup() {
	const bool limiterOff = envIs("EXPERIENCE_RUNTIME_FRAME_LIMITER", "off");
	const bool vsyncOff = envIs("EXPERIENCE_RUNTIME_VSYNC", "off");
	if (limiterOff) ofSetFrameRate(0);
	if (vsyncOff) ofSetVerticalSync(false);
	const char* control = std::getenv("EXPERIENCE_RUNTIME_FRAME_PHASE_CONTROL");
	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* version = glGetString(GL_VERSION);
	std::ostringstream os;
	os << "CONFIG t=0 mono=" << std::fixed << steadyNs() / 1e9 << " of=" << ofGetVersionInfo().substr(0, ofGetVersionInfo().find('\n')) << " glRenderer=\""
	   << (renderer ? reinterpret_cast<const char*>(renderer) : "?") << "\" glVersion=\"" << (version ? reinterpret_cast<const char*>(version) : "?")
	   << "\" targetFps=" << ofGetTargetFrameRate() << " limiter=" << (limiterOff ? "off" : "on") << " vsync=" << (vsyncOff ? "off" : "on")
	   << " glFinish=" << (glFinish_ ? "on" : "off") << " control=" << (control ? control : "none") << " slowMs=" << slowMs_
	   << " windowMs=" << windowMs_ << " stallProbe=" << (rt004::Probe::get().enabled() ? "on" : "off") << " glTrace=" << (rt004::Probe::get().glTraceActive() ? "on" : "off")
	   << " window=" << ofGetWidth() << "x" << ofGetHeight();
	writeLine(os.str());
	writeLine("SEGMENTS update=U1-U0 preDraw=D0-U1(window-update tail+startRender clear+setupScreen+limiter) draw=D1-D0 "
		"glFinish=F1-D1 postDraw=nextU0-F1(swapBuffers+finishRender+loopEvent+pollEvents); per segment wall/cpu ms, vcsw/ivcsw");
}

Rt004FramePhaseProbe::Stamp Rt004FramePhaseProbe::now() {
	Stamp s;
	s.wallNs = steadyNs();
	timespec ts{};
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) s.cpuNs = static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
#if defined(__linux__) && defined(RUSAGE_THREAD)
	rusage ru{};
	if (getrusage(RUSAGE_THREAD, &ru) == 0) { s.vcsw = ru.ru_nvcsw; s.ivcsw = ru.ru_nivcsw; }
#endif
	return s;
}

Rt004FramePhaseProbe::Segment Rt004FramePhaseProbe::between(const Stamp& a, const Stamp& b) {
	Segment s;
	s.wallMs = (b.wallNs - a.wallNs) / 1e6;
	s.cpuMs = (b.cpuNs - a.cpuNs) / 1e6;
	if (a.vcsw >= 0 && b.vcsw >= 0) { s.vcsw = b.vcsw - a.vcsw; s.ivcsw = b.ivcsw - a.ivcsw; }
	return s;
}

Rt004FramePhaseProbe::EvSnap Rt004FramePhaseProbe::readEvents(bool raw) const {
	// Read-only view of the sampler's own monotonic counters (additive Stats fields; the sampler never reads them).
	const scenecolorsampling::Stats& st = (raw ? runtime_.videoColorForTesting() : runtime_.sceneColorForTesting()).stats();
	EvSnap e;
	e.issueOk = st.issued;                                          // submission performed
	e.issueFail = st.telemetryDisabledSkips + st.unsupportedFormat; // due, but the issue was refused
	e.collectOk = st.collectOk;                                     // valid sample read AND accepted by the series
	e.collectFail = st.collectFail;                                 // invalid / timeout / wait failed / generation drop / series rejected
	return e;
}

Rt004FramePhaseProbe::EvDelta Rt004FramePhaseProbe::evDelta(EvSnap& prev, const EvSnap& cur) {
	auto d = [](uint64_t a, uint64_t b) { return static_cast<uint32_t>(b >= a ? b - a : b); }; // a reset counter restarts from 0
	EvDelta r;
	r.issueOk = d(prev.issueOk, cur.issueOk);
	r.issueFail = d(prev.issueFail, cur.issueFail);
	r.collectOk = d(prev.collectOk, cur.collectOk);
	r.collectFail = d(prev.collectFail, cur.collectFail);
	prev = cur;
	return r;
}

void Rt004FramePhaseProbe::onUpdateBefore(ofEventArgs&) {
	const Stamp s = now();
	if (frameCsv_ && !haveEvBase_) { // baseline = counters at the start of the first observed frame
		for (int i = 0; i < 2; i++) evPrev_[i] = evAbs_[i] = evBase_[i] = readEvents(i == 1);
		haveEvBase_ = true;
	}
	// Anything GL raised since the app's draw returned: swapBuffers,
	// finishRender, loop event, pollEvents (only with the GL trace on).
	rt004::Probe::get().glCheckpoint("(postDraw: swapBuffers, finishRender, loopEvent, pollEvents)");
	if (haveFrame_) closeFrame(s);
	checkTransitions(s);
	u0_ = s;
	haveFrame_ = true;
	haveU1_ = haveD0_ = haveD1_ = false;
}

void Rt004FramePhaseProbe::onUpdateAfter(ofEventArgs&) {
	u1_ = now();
	haveU1_ = true;
}

void Rt004FramePhaseProbe::onDrawBefore(ofEventArgs&) {
	d0_ = now();
	haveD0_ = true;
	rt004::Probe::get().glCheckpoint("(preDraw: window-update tail, startRender clear, setupScreen, limiter)");
}

void Rt004FramePhaseProbe::onDrawAfter(ofEventArgs&) {
	d1_ = now();
	haveD1_ = true;
	if (glFinish_) glFinish(); // diagnostic: drain this frame's GPU work into a measured segment
	f1_ = now();
}

std::string Rt004FramePhaseProbe::frameContext() const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	std::ostringstream os;
	os << "owner=" << (f.sceneManager.activeSceneId.empty() ? "-" : f.sceneManager.activeSceneId)
	   << " phase=" << static_cast<int>(f.sceneManager.transitionPhase)
	   << " media=" << (f.video && f.video->mediaId ? *f.video->mediaId : std::string("-"));
	return os.str();
}

void Rt004FramePhaseProbe::closeFrame(const Stamp& nextU0) {
	// Events of the frame that is closing: counter deltas since the previous close (before the partial-frame early return,
	// so no event is ever attributed to the wrong frame; partial-frame events are kept as `unattributed`).
	EvDelta evFx, evRaw;
	if (frameCsv_ && haveEvBase_) {
		evAbs_[0] = readEvents(false);
		evAbs_[1] = readEvents(true);
		evFx = evDelta(evPrev_[0], evAbs_[0]);
		evRaw = evDelta(evPrev_[1], evAbs_[1]);
	}
	if (!haveU1_ || !haveD0_ || !haveD1_) { // partial frame (startup/exit)
		evUnattributed_[0].issueOk += evFx.issueOk; evUnattributed_[0].issueFail += evFx.issueFail;
		evUnattributed_[0].collectOk += evFx.collectOk; evUnattributed_[0].collectFail += evFx.collectFail;
		evUnattributed_[1].issueOk += evRaw.issueOk; evUnattributed_[1].issueFail += evRaw.issueFail;
		evUnattributed_[1].collectOk += evRaw.collectOk; evUnattributed_[1].collectFail += evRaw.collectFail;
		frame_++;
		return;
	}
	Segment seg[SegCount] = { between(u0_, u1_), between(u1_, d0_), between(d0_, d1_), between(d1_, f1_), between(f1_, nextU0) };
	const double totalMs = (nextU0.wallNs - u0_.wallNs) / 1e6;
	const double atMs = (u0_.wallNs - startNs_) / 1e6;

	if (windowTotal_.empty()) windowStartMs_ = atMs;
	for (int i = 0; i < SegCount; i++) { windowWall_[i].push_back(seg[i].wallMs); windowCpuMs_[i] += seg[i].cpuMs; }
	windowTotal_.push_back(totalMs);

	if (frameCsv_) {
		writeFrameRow(frame_, nextU0, totalMs, seg, &evFx, &evRaw);
		evCsv_[0].issueOk += evFx.issueOk; evCsv_[0].issueFail += evFx.issueFail; evCsv_[0].collectOk += evFx.collectOk; evCsv_[0].collectFail += evFx.collectFail;
		evCsv_[1].issueOk += evRaw.issueOk; evCsv_[1].issueFail += evRaw.issueFail; evCsv_[1].collectOk += evRaw.collectOk; evCsv_[1].collectFail += evRaw.collectFail;
		lastCsvFrame_ = frame_;
	}

	if (totalMs >= slowMs_) {
		windowSlow_++;
		if (slowLines_ < kMaxSlowLines) {
			slowLines_++;
			int largest = 0;
			for (int i = 1; i < SegCount; i++) if (seg[i].wallMs > seg[largest].wallMs) largest = i;
			std::ostringstream os;
			os.setf(std::ios::fixed);
			os.precision(2);
			os << "SLOW frame=" << frame_ << " mono=" << std::setprecision(3) << u0_.wallNs / 1e9 << std::setprecision(2) << " t=" << atMs << "ms total=" << totalMs << "ms largest=" << kSegName[largest] << " " << frameContext();
			for (int i = 0; i < SegCount; i++) {
				os << " | " << kSegName[i] << " wall=" << seg[i].wallMs << " cpu=" << seg[i].cpuMs;
				if (seg[i].vcsw >= 0) os << " vcsw=" << seg[i].vcsw << " ivcsw=" << seg[i].ivcsw;
			}
			os << " | U0=" << atMs << " U1=" << (u1_.wallNs - startNs_) / 1e6 << " D0=" << (d0_.wallNs - startNs_) / 1e6
			   << " D1=" << (d1_.wallNs - startNs_) / 1e6 << " nextU0=" << (nextU0.wallNs - startNs_) / 1e6;
			writeLine(os.str());
		}
	}
	if (atMs - windowStartMs_ >= windowMs_) writeWindowSummary(atMs);
	frame_++;
}

void Rt004FramePhaseProbe::writeFrameRow(uint64_t frame, const Stamp& nextU0, double totalMs, const Segment* seg, const EvDelta* fx, const EvDelta* raw) {
	const HudFrameData& f = runtime_.currentHudFrameData();
	const std::string& scene = f.sceneManager.activeSceneId;
	const std::string media = (f.video && f.video->mediaId) ? *f.video->mediaId : std::string();
	// Change detection is per-frame here (independent of checkTransitions' EVENT bookkeeping).
	if (scene != csvScene_) { csvScene_ = scene; lastSceneChangeNs_ = u0_.wallNs; }
	if (media != csvMedia_) { csvMedia_ = media; lastMediaChangeNs_ = u0_.wallNs; }
	(void)nextU0;
	const double settleNs = settleMs_ * 1e6;
	const bool exStartup = (u0_.wallNs - startNs_) < warmupMs_ * 1e6;
	const bool exMedia = lastMediaChangeNs_ != 0 && (u0_.wallNs - lastMediaChangeNs_) < settleNs;
	const bool exScene = lastSceneChangeNs_ != 0 && (u0_.wallNs - lastSceneChangeNs_) < settleNs;
	const bool exTrans = f.sceneManager.transitionPhase != SceneTransitionPhase::Idle;
	std::fprintf(frameCsv_, "%llu,%.6f,%.4f", static_cast<unsigned long long>(frame), u0_.wallNs / 1e9, totalMs);
	for (int i = 0; i < SegCount; i++) std::fprintf(frameCsv_, ",%.4f,%.4f,%ld,%ld", seg[i].wallMs, seg[i].cpuMs, seg[i].vcsw, seg[i].ivcsw);
	std::fprintf(frameCsv_, ",%s,%d,%s,%d,%d,%d,%d,%d", scene.empty() ? "-" : scene.c_str(), static_cast<int>(f.sceneManager.transitionPhase),
		media.empty() ? "-" : media.c_str(), exStartup, exMedia, exScene, exTrans, (exStartup || exMedia || exScene || exTrans) ? 1 : 0);
	// Event columns (appended; all earlier columns unchanged). Bits: 1 fxIssue, 2 fxCollect, 4 rawIssue, 8 rawCollect.
	const int ev = (fx->issueOk ? 1 : 0) | (fx->collectOk ? 2 : 0) | (raw->issueOk ? 4 : 0) | (raw->collectOk ? 8 : 0);
	const int evFail = (fx->issueFail ? 1 : 0) | (fx->collectFail ? 2 : 0) | (raw->issueFail ? 4 : 0) | (raw->collectFail ? 8 : 0);
	const ExperienceRuntime::FrameAcquisition& acq = runtime_.lastFrameAcquisition();
	std::fprintf(frameCsv_, ",%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%.1f,%.1f,%d\n", ev, evFail, fx->issueOk, fx->collectOk, raw->issueOk, raw->collectOk,
		fx->issueFail, fx->collectFail, raw->issueFail, raw->collectFail, acq.cpuRaw, acq.cpuRawGap, acq.cpuRawUs, acq.palUs, acq.videoNew ? 1 : 0);
	if (nextU0.wallNs - lastCsvFlushNs_ > 1000000000LL) { std::fflush(frameCsv_); lastCsvFlushNs_ = nextU0.wallNs; }
}

std::string Rt004FramePhaseProbe::eventTotals() const {
	// Cumulative sampler-event accounting for offline reconciliation (scripts/oc003/reconcile_events.py):
	//   csv          sum of the events written to CSV rows up to csvFrame
	//   unattributed events of partial frames (startup/exit) that have no CSV row
	//   abs          the sampler counters themselves at the latest frame close (absolute, since process start)
	//   base         the sampler counters at the first observed frame (abs - base == csv + unattributed when csvFrame is the latest)
	std::ostringstream os;
	os << "EVENTS mono=" << std::fixed << std::setprecision(3) << steadyNs() / 1e9 << " csvFrame=" << lastCsvFrame_;
	auto group = [&](const char* tag, const EvSnap* v) {
		os << " | " << tag << ":";
		for (int i = 0; i < 2; i++) {
			const char* p = i ? "raw" : "fx";
			os << " " << p << ".issue=" << v[i].issueOk << " " << p << ".ifail=" << v[i].issueFail << " " << p << ".collect=" << v[i].collectOk << " " << p << ".cfail=" << v[i].collectFail;
		}
	};
	group("csv", evCsv_);
	group("unattributed", evUnattributed_);
	group("abs", evAbs_);
	group("base", evBase_);
	return os.str();
}

void Rt004FramePhaseProbe::writeWindowSummary(double atMs) {
	if (frameCsv_ && haveEvBase_) writeLine(eventTotals());
	if (windowTotal_.empty()) return;
	std::ostringstream os;
	os.setf(std::ios::fixed);
	os.precision(2);
	std::vector<double> total(windowTotal_.begin(), windowTotal_.end());
	double totalSum = 0.0;
	for (double v : total) totalSum += v;
	os << "WINDOW mono=" << std::setprecision(3) << (startNs_ / 1e9 + atMs / 1000.0) << std::setprecision(2) << " t=" << windowStartMs_ << "-" << atMs << "ms frames=" << total.size() << " slow=" << windowSlow_
	   << " frame p50=" << pct(total, 0.50) << " p95=" << pct(total, 0.95) << " max=" << pct(total, 1.0) << " " << frameContext();
	for (int i = 0; i < SegCount; i++) {
		std::vector<double> v(windowWall_[i].begin(), windowWall_[i].end());
		double sum = 0.0;
		for (double x : v) sum += x;
		os << " | " << kSegName[i] << " p50=" << pct(v, 0.50) << " p95=" << pct(v, 0.95) << " max=" << pct(v, 1.0)
		   << " share=" << (totalSum > 0 ? 100.0 * sum / totalSum : 0.0) << "% cpu/wall=" << (sum > 0 ? 100.0 * windowCpuMs_[i] / sum : 0.0) << "%";
	}
	os << " | " << temporalResidency();
	os << " | rssMiB=" << procMiB("/proc/self/status", "VmRSS:") << " rssAnonMiB=" << procMiB("/proc/self/status", "RssAnon:")
	   << " rssFileMiB=" << procMiB("/proc/self/status", "RssFile:") << " rssShmemMiB=" << procMiB("/proc/self/status", "RssShmem:")
	   << " memAvailMiB=" << procMiB("/proc/meminfo", "MemAvailable:")
	   << " cmaFreeMiB=" << procMiB("/proc/meminfo", "CmaFree:") << " swapFreeMiB=" << procMiB("/proc/meminfo", "SwapFree:");
	writeLine(os.str());
	for (int i = 0; i < SegCount; i++) { windowWall_[i].clear(); windowCpuMs_[i] = 0.0; }
	windowTotal_.clear();
	windowSlow_ = 0;
}

std::string Rt004FramePhaseProbe::temporalResidency() const {
	// Measured: history frame count / capacity and live history pixel bytes.
	// Estimated: playhead texture bytes from allocated dimensions x format.
	const TimeOffsetVideoBuffer& b = runtime_.temporalSceneForTesting().videoAdapterForTesting().buffer();
	std::ostringstream os;
	os.setf(std::ios::fixed);
	os.precision(2);
	os << "historyFrames=" << b.getHistoryFrameCount() << "/" << b.getHistoryCapacityFrames()
	   << " historyPixMiB=" << b.getHistoryPixelBytes() / (1024.0 * 1024.0)
	   << " playheadTex=" << b.getAllocatedPlayheadTextureCount() << "/" << b.getNumPlayheads()
	   << " playheadEstMiB=" << b.getPlayheadTextureBytesEstimate(false) / (1024.0 * 1024.0)
	   << " playheadEstRgbaMiB=" << b.getPlayheadTextureBytesEstimate(true) / (1024.0 * 1024.0);
	return os.str();
}

void Rt004FramePhaseProbe::checkTransitions(const Stamp& at) {
	// Transition-only EVENT lines (no per-frame I/O): scene owner, canonical
	// media, Temporal history reset (count drops) and history full.
	const HudFrameData& f = runtime_.currentHudFrameData();
	const std::string& owner = f.sceneManager.activeSceneId;
	const std::string media = (f.video && f.video->mediaId) ? *f.video->mediaId : std::string();
	const int history = runtime_.temporalSceneForTesting().videoAdapterForTesting().buffer().getHistoryFrameCount();
	const int capacity = runtime_.temporalSceneForTesting().videoAdapterForTesting().buffer().getHistoryCapacityFrames();
	std::ostringstream os;
	os.setf(std::ios::fixed);
	const double mono = at.wallNs / 1e9;
	const double t = (at.wallNs - startNs_) / 1e6;
	auto event = [&](const std::string& kind, const std::string& detail) {
		os.str("");
		os << "EVENT mono=" << std::setprecision(3) << mono << " t=" << std::setprecision(2) << t << "ms frame=" << frame_
		   << " kind=" << kind << " " << detail << " | " << temporalResidency();
		writeLine(os.str());
	};
	if (owner != lastOwner_) { event("scene-owner", "from=" + (lastOwner_.empty() ? std::string("-") : lastOwner_) + " to=" + (owner.empty() ? std::string("-") : owner)); lastOwner_ = owner; }
	if (media != lastMedia_) { event("canonical-media", "from=" + (lastMedia_.empty() ? std::string("-") : lastMedia_) + " to=" + (media.empty() ? std::string("-") : media)); lastMedia_ = media; }
	if (history < lastHistory_) event("history-reset", "from=" + std::to_string(lastHistory_) + " to=" + std::to_string(history));
	if (history >= capacity && lastHistory_ < capacity && capacity > 0) event("history-full", "frames=" + std::to_string(history));
	lastHistory_ = history;

	// Effect snapshot changes (first snapshot included). Hash without
	// allocating; build the text only when it changes. "none" = no
	// HudFrameData.effects; "slots=0" = Raw / no active effect.
	size_t h = 0x9e3779b9u;
	if (f.effects) {
		h ^= std::hash<int>()(static_cast<int>(f.effects->health)) + 0x9e3779b9u + (h << 6) + (h >> 2);
		for (const auto& slot : f.effects->slots) {
			h ^= std::hash<std::string>()(slot.slotId) + 0x9e3779b9u + (h << 6) + (h >> 2);
			h ^= std::hash<std::string>()(slot.effectId) + 0x9e3779b9u + (h << 6) + (h >> 2);
		}
	} else {
		h = 1;
	}
	if (h != lastEffectsHash_) {
		lastEffectsHash_ = h;
		std::string detail = "none";
		if (f.effects) {
			detail = "health=" + std::to_string(static_cast<int>(f.effects->health)) + " slots=" + std::to_string(f.effects->slots.size());
			for (const auto& slot : f.effects->slots) detail += " [" + slot.slotId + ":" + slot.effectId + "|" + slot.displayName + "]";
		}
		event("effects", detail);
	}
}

void Rt004FramePhaseProbe::writeLine(const std::string& line) {
	if (FILE* f = std::fopen(logPath_.c_str(), "a")) {
		std::fprintf(f, "%s\n", line.c_str());
		std::fclose(f);
	}
}
