#pragma once

#include "ofEvents.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

class ExperienceRuntime;

// RT-004 frame-phase probe — DIAGNOSTIC ONLY (EXPERIENCE_RUNTIME_FRAME_PHASE_PROBE=1).
//
// Splits each openFrameworks frame into exclusive wall-time segments using
// app-owned listeners on ofEvents().update/draw registered BEFORE and AFTER
// the app (no openFrameworks change). In this OF (0.12.1, GLFW window):
//
//   ofMainLoop::loopOnce: window.update() -> window.draw(); then pollEvents()
//   window.update(): notifyUpdate()  [U0 before-app | ofApp::update | U1 after-app]
//                    + window-update tail (show-window once, RPi resize check)
//   window.draw():   renderer.startRender() (clears the default framebuffer)
//                    renderer.setupScreen()
//                    notifyDraw(): fps counter, timerFps.waitNext() (limiter),
//                                  [D0 before-app | ofApp::draw | D1 after-app]
//                    glfwSwapBuffers(); renderer.finishRender()
//
//   update   = U1 - U0       ofApp::update incl. runtime.update
//   preDraw  = D0 - U1       window-update tail + startRender/clear + setupScreen + limiter
//   draw     = D1 - D0       ofApp::draw incl. runtime.draw
//   glFinish = (optional, EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH=1) a timed
//              glFinish() at D1: GPU work still queued for this frame
//   postDraw = next U0 - D1  swapBuffers + finishRender + loop event + pollEvents
//
// preDraw and postDraw each still lump several OF steps; the controls below
// (limiter off, vsync off, glFinish, present-only) separate them by
// experiment rather than by guessing. Each segment also records main-thread
// CPU time (CLOCK_THREAD_CPUTIME_ID) and, on Linux, voluntary/involuntary
// context switches (getrusage RUSAGE_THREAD): wall >> CPU with voluntary
// switches = blocked/sleeping; involuntary switches = preempted.
//
// Every line carries mono=<s>: steady_clock seconds, which on Linux is
// CLOCK_MONOTONIC (aligns with dmesg timestamps, modulo suspend).
//
// Output (EXPERIENCE_RUNTIME_FRAME_PHASE_LOG, default ./rt004-frame-phase.log):
// one CONFIG line, one SLOW line per frame >= EXPERIENCE_RUNTIME_FRAME_PHASE_MS
// (default 200, capped at 5000 lines), one WINDOW summary every 10 s
// (segment p50/p95/max, CPU share, RSS/MemAvailable/CmaFree/SwapFree).
// Nothing is registered when disabled.
//
// Diagnostic controls (only with the probe enabled; never in a normal run):
//   EXPERIENCE_RUNTIME_FRAME_LIMITER=off   ofSetFrameRate(0) after runtime setup
//   EXPERIENCE_RUNTIME_VSYNC=off           ofSetVerticalSync(false) after runtime setup
//   EXPERIENCE_RUNTIME_FRAME_PHASE_GLFINISH=1
//   EXPERIENCE_RUNTIME_FRAME_PHASE_CONTROL=present-only
//        ofApp skips runtime.update()/draw() and every harness: clear + swap only
class Rt004FramePhaseProbe {
public:
	static bool isRequested();
	static bool presentOnlyControl();

	explicit Rt004FramePhaseProbe(ExperienceRuntime& runtime);
	~Rt004FramePhaseProbe();

	// Call once after runtime.setup(): applies the limiter/vsync controls and
	// writes the CONFIG line.
	void afterRuntimeSetup();

private:
	struct Stamp {
		int64_t wallNs = 0;
		int64_t cpuNs = 0;
		long vcsw = -1;
		long ivcsw = -1;
	};
	struct Segment {
		double wallMs = 0, cpuMs = 0;
		long vcsw = -1, ivcsw = -1;
	};
	enum Seg { Update, PreDraw, Draw, Finish, PostDraw, SegCount };

	static Stamp now();
	static Segment between(const Stamp& a, const Stamp& b);

	void onUpdateBefore(ofEventArgs&);
	void onUpdateAfter(ofEventArgs&);
	void onDrawBefore(ofEventArgs&);
	void onDrawAfter(ofEventArgs&);

	void closeFrame(const Stamp& nextU0);
	void writeWindowSummary(double atMs);
	void writeLine(const std::string& line);
	std::string frameContext() const;

	ExperienceRuntime& runtime_;
	std::string logPath_;
	double slowMs_ = 200.0;
	bool glFinish_ = false;
	int64_t startNs_ = 0;

	uint64_t frame_ = 0;
	bool haveFrame_ = false;
	Stamp u0_, u1_, d0_, d1_, f1_;
	bool haveU1_ = false, haveD0_ = false, haveD1_ = false;

	size_t slowLines_ = 0;
	static constexpr size_t kMaxSlowLines = 5000;

	// 10 s window: per-segment samples (wall ms) + totals.
	double windowStartMs_ = 0;
	std::deque<double> windowWall_[SegCount];
	std::deque<double> windowTotal_;
	double windowCpuMs_[SegCount] = {};
	size_t windowSlow_ = 0;
};
