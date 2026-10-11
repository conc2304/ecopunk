#include "Rt004FramePhaseProbe.h"

#include "ExperienceRuntime.h"
#include "Rt004StallProbe.h"
#include "ofAppRunner.h"
#include "ofGLUtils.h"
#include "ofGraphics.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
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
	startNs_ = steadyNs();
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
	   << " stallProbe=" << (rt004::Probe::get().enabled() ? "on" : "off") << " glTrace=" << (rt004::Probe::get().glTraceActive() ? "on" : "off")
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

void Rt004FramePhaseProbe::onUpdateBefore(ofEventArgs&) {
	const Stamp s = now();
	// Anything GL raised since the app's draw returned: swapBuffers,
	// finishRender, loop event, pollEvents (only with the GL trace on).
	rt004::Probe::get().glCheckpoint("(postDraw: swapBuffers, finishRender, loopEvent, pollEvents)");
	if (haveFrame_) closeFrame(s);
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
	if (!haveU1_ || !haveD0_ || !haveD1_) { frame_++; return; } // partial frame (startup/exit)
	Segment seg[SegCount] = { between(u0_, u1_), between(u1_, d0_), between(d0_, d1_), between(d1_, f1_), between(f1_, nextU0) };
	const double totalMs = (nextU0.wallNs - u0_.wallNs) / 1e6;
	const double atMs = (u0_.wallNs - startNs_) / 1e6;

	if (windowTotal_.empty()) windowStartMs_ = atMs;
	for (int i = 0; i < SegCount; i++) { windowWall_[i].push_back(seg[i].wallMs); windowCpuMs_[i] += seg[i].cpuMs; }
	windowTotal_.push_back(totalMs);

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
	if (atMs - windowStartMs_ >= 10000.0) writeWindowSummary(atMs);
	frame_++;
}

void Rt004FramePhaseProbe::writeWindowSummary(double atMs) {
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
	os << " | rssMiB=" << procMiB("/proc/self/status", "VmRSS:") << " memAvailMiB=" << procMiB("/proc/meminfo", "MemAvailable:")
	   << " cmaFreeMiB=" << procMiB("/proc/meminfo", "CmaFree:") << " swapFreeMiB=" << procMiB("/proc/meminfo", "SwapFree:");
	writeLine(os.str());
	for (int i = 0; i < SegCount; i++) { windowWall_[i].clear(); windowCpuMs_[i] = 0.0; }
	windowTotal_.clear();
	windowSlow_ = 0;
}

void Rt004FramePhaseProbe::writeLine(const std::string& line) {
	if (FILE* f = std::fopen(logPath_.c_str(), "a")) {
		std::fprintf(f, "%s\n", line.c_str());
		std::fclose(f);
	}
}
