#pragma once

#include "ofEvents.h"

#include <cstdint>
#include <cstdio>
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
// RT-004 memory attribution additions: each WINDOW also carries Temporal
// residency (historyFrames/capacity and live history pixel bytes = measured;
// playhead texture bytes = estimate from dimensions x format, plus an
// RGB-padded-to-RGBA figure) and RssAnon/RssFile/RssShmem; EVENT lines are
// written only on transitions (scene owner, canonical media, history reset,
// history full). EXPERIENCE_RUNTIME_FRAME_PHASE_WINDOW_MS sets the summary
// period (default 10000, min 250).
//
// OC-003 per-frame stream (EXPERIENCE_RUNTIME_FRAME_PHASE_FRAMES=<csv path>, needs the probe on): one CSV row per
// frame, STREAMED through a buffered FILE* (flushed about once a second; nothing is retained in memory). Columns:
// frame,mono,total_ms, <seg>_wall/_cpu/_vcsw/_ivcsw for update/preDraw/draw/glFinish/postDraw, scene,phase,media,
// ex_startup,ex_media,ex_scene,ex_transition,exclude. Exclusion markers (offline script drops exclude=1 rows):
//   ex_startup    frame starts < EXPERIENCE_RUNTIME_FRAME_PHASE_WARMUP_S (default 30) after probe creation
//   ex_media      within EXPERIENCE_RUNTIME_FRAME_PHASE_SETTLE_MS (default 3000) after a canonical-media change
//   ex_scene      within the settle time after a scene-owner change
//   ex_transition scene transition phase != Idle
// Sampling-event columns (appended after `exclude`; only with the CSV on): ev, ev_fail (bitmasks 1 fxIssue, 2 fxCollect, 4 rawIssue,
// 8 rawCollect), fx_issue fx_collect raw_issue raw_collect, fx_ifail fx_cfail raw_ifail raw_cfail. See the EvSnap block below and
// docs/reviews/OC-003-sampling-frame-attribution-procedure.md. The probe only READS the samplers' counters.
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
	struct EvSnap {
		uint64_t issueOk = 0, issueFail = 0, collectOk = 0, collectFail = 0;
	};
	struct EvDelta {
		uint32_t issueOk = 0, issueFail = 0, collectOk = 0, collectFail = 0;
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
	void writeFrameRow(uint64_t frame, const Stamp& nextU0, double totalMs, const Segment* seg, const EvDelta* fx, const EvDelta* raw);
	std::string eventTotals() const;
	std::string temporalResidency() const;
	void checkTransitions(const Stamp& at);
	std::string frameContext() const;

	ExperienceRuntime& runtime_;
	std::string logPath_;
	double slowMs_ = 200.0;
	double windowMs_ = 10000.0; // EXPERIENCE_RUNTIME_FRAME_PHASE_WINDOW_MS (min 250)
	std::string lastOwner_, lastMedia_;
	int lastHistory_ = 0;
	size_t lastEffectsHash_ = 0; // 0 = nothing seen yet (first snapshot always logged)
	bool glFinish_ = false;
	int64_t startNs_ = 0;

	uint64_t frame_ = 0;
	bool haveFrame_ = false;
	Stamp u0_, u1_, d0_, d1_, f1_;
	bool haveU1_ = false, haveD0_ = false, haveD1_ = false;

	// OC-003 per-frame CSV stream (null = off).
	FILE* frameCsv_ = nullptr;
	double warmupMs_ = 30000.0, settleMs_ = 3000.0;
	int64_t lastMediaChangeNs_ = 0, lastSceneChangeNs_ = 0, lastCsvFlushNs_ = 0;
	std::string csvScene_, csvMedia_;

	// OC-003 sampling-frame attribution (only with the per-frame CSV on; observation of the samplers' own counters, nothing
	// is called into the sampler lifecycle). Per frame, event = delta of a monotonic counter between two consecutive
	// frame closes, so an event is attributed to the frame (update+draw) that performed it and several events in one frame
	// stay individually counted. Event schema (CSV columns appended after `exclude`):
	//   ev        bitmask of SUCCESSFUL events: 1 fxIssue, 2 fxCollect, 4 rawIssue, 8 rawCollect  (0 = no-event frame)
	//   ev_fail   bitmask of FAILED attempts, same bit order (issue refused / collection without a published valid sample)
	//   fx_issue fx_collect raw_issue raw_collect   successful event counts in the frame
	//   fx_ifail fx_cfail raw_ifail raw_cfail       failed attempt counts in the frame
	EvSnap readEvents(bool raw) const;
	EvDelta evDelta(EvSnap& prev, const EvSnap& cur);
	bool haveEvBase_ = false;
	EvSnap evPrev_[2], evAbs_[2], evBase_[2]; // [0]=FX [1]=RAW: previous snapshot, latest absolute, absolute at the first snapshot
	EvSnap evCsv_[2];                          // sum of events written to CSV rows
	EvSnap evUnattributed_[2];                 // events of partial frames (startup/exit) that have no CSV row
	uint64_t lastCsvFrame_ = 0;

	size_t slowLines_ = 0;
	static constexpr size_t kMaxSlowLines = 5000;

	// 10 s window: per-segment samples (wall ms) + totals.
	double windowStartMs_ = 0;
	std::deque<double> windowWall_[SegCount];
	std::deque<double> windowTotal_;
	double windowCpuMs_[SegCount] = {};
	size_t windowSlow_ = 0;
};
