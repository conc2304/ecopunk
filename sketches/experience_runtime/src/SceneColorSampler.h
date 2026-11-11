// Orbit Console Scene Sampling v0.1 -- runtime-private GL half of the sampler.
// Owns a small halving chain of FBOs (SceneFBO -> ... -> ~160x90 RGBA8), the
// readback (synchronous or PBO+fence asynchronous) and a Series. Called by
// ExperienceRuntime only, right after the SceneFBO pass and before HUD
// composition; the HUD never touches it. Allocation happens when the source
// size changes (scene switch), never per frame.
#pragma once

#include "SceneColorSeries.h"
#include "RawVideoGate.h"
#include "SceneColorDeferredGate.h"
#include "PredominantColor.h"
#include "HalfFloat.h"
#include "PaletteSink.h" // OC-003-F diagnostic (header-only, inert unless a sink is attached)

#include "ofFbo.h"
#include "ofShader.h"
#include "ofBufferObject.h"

#include <algorithm>
#include "ofPixels.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scenecolorsampling {

// Sync     = read the tiny reduction target in the frame it was drawn (CPU waits for the GPU).
// Async    = PBO + fence on the encoded fallback chain (needs GL 3.2).
// Deferred = GPU linear reduction drawn into one of two small targets; read back a couple of frames
//            LATER, when the GPU has already finished it (no stall, portable to GLES2-class drivers).
//            The sample keeps its issue timestamp and generation; a scene change drops it.
enum class ReadbackMode { Sync, Async, Deferred };

// Bounded per-stage timing (ms) with p50/p95 over the most recent kRing samples. Diagnostic only.
struct StageStat {
	static constexpr uint32_t kRing = 256;
	double total = 0.0, max = 0.0;
	uint64_t n = 0;
	std::array<float, kRing> ring{};
	void add(double ms) { total += ms; max = std::max(max, ms); ring[n % kRing] = static_cast<float>(ms); ++n; }
	double mean() const { return n ? total / double(n) : 0.0; }
	double pct(double p) const {
		const uint32_t m = static_cast<uint32_t>(std::min<uint64_t>(n, kRing));
		if (m == 0) return 0.0;
		std::array<float, kRing> tmp = ring;
		std::sort(tmp.begin(), tmp.begin() + m);
		return tmp[std::min<uint32_t>(m - 1, static_cast<uint32_t>(p * m))];
	}
};

// OC-003 EXPERIMENTAL (default Float = the unchanged baseline): how the 160x90 / 16x9 RGBA16F linear target is
// transferred to the CPU. Float = ofFbo::readToPixels (GL_FLOAT, RGBA32F words). Half = glGetTexImage(GL_RGBA,
// GL_HALF_FLOAT) then EXACT widening into the same float buffer, so everything downstream (AVG, histogram, tracker,
// publication) is byte-for-byte the same code. HalfVerify = diagnostic: reads BOTH from the same texture, publishes the
// Float data, and counts any bitwise difference of the widened Half data. An unsupported / failing half transfer is a
// reported failure (gap + counters + log), never a silent switch to Float.
enum class ReadbackFormat { Float, Half, HalfVerify };

struct Stats {
	uint64_t issued = 0;
	uint64_t completed = 0;
	uint64_t droppedGeneration = 0; // async results discarded: scene changed in flight
	uint64_t droppedBusy = 0;       // due, but a readback was still in flight
	uint64_t gaps = 0;
	uint64_t unsupportedFormat = 0; // due, but the SceneFBO format/encoding is not the validated 8-bit display-encoded RGBA
	double issueMsTotal = 0.0, issueMsMax = 0.0;     // chain draw + (sync) readback, CPU wall time
	double collectMsTotal = 0.0, collectMsMax = 0.0; // async map+average
	StageStat submit;  // (1) reduction draw submission (CPU side of the two passes)
	StageStat wait;    // (2)+(3a) readback call: GPU completion wait + transfer of the 16x9 target
	StageStat convert; // (3b) CPU average of the read pixels
	StageStat push;    // (4) series push (history construction)
	StageStat total;   // whole sample, CPU wall time
	// OC-003 (Deferred + fence only): GPU completion latency (issue -> fence observed signalled, resolved to the
	// poll granularity of one frame) is kept apart from the CPU blocking time in `wait`.
	StageStat latency;
	uint64_t notReadyPolls = 0;    // polls that found the fence unsignalled and returned without blocking
	// A7: there is no blocking fallback. A request the fence never signals (timeout) or whose wait failed is
	// DISCARDED and recorded as a gap; repeated failures disable scene-colour telemetry (never scene rendering).
	uint64_t discardedTimeout = 0;      // fence still unsignalled after kFenceTimeoutMs: request discarded, gap recorded
	uint64_t discardedWaitFailed = 0;   // glClientWaitSync failed (GL_WAIT_FAILED / unknown status): request discarded, gap recorded
	uint64_t invalidSamples = 0;        // read finished but the data was empty / non-finite: gap recorded
	uint64_t telemetryDisabledSkips = 0; // due, but telemetry was disabled by the failure threshold
	// OC-003 sampling-frame attribution (observation only; read by Rt004FramePhaseProbe, never by sampler logic): outcome of
	// every collection attempt. collectOk = a VALID sample was read and accepted by the series (published);
	// collectFail = the attempt ended without a published valid measurement (invalid data, fence timeout, wait failed,
	// generation changed in flight, or rejected by the series). Issue outcomes already exist: `issued` (success) and
	// `telemetryDisabledSkips` + `unsupportedFormat` (the issue was refused).
	uint64_t collectOk = 0, collectFail = 0;
	// OC-003 acquisition accounting: every GPU->CPU readback this sampler issues for a sample, and its payload.
	// DOM/VIB add NONE: with predominant sampling on, the single readback is the 160x90 target instead of the 16x9 one.
	uint64_t gpuReadbacks = 0;
	uint64_t gpuReadbackBytes = 0;
	StageStat predominantCpu; // CPU time of the DOM/VIB histogram + tracker (and the 160x90 average) per sample
	uint64_t domValid = 0, vibValid = 0, vibInvalid = 0; // valid samples only
	// OC-003 experimental half-float readback accounting (all zero in Float mode).
	StageStat widen;                 // CPU time of the exact half -> float widening (Half and HalfVerify)
	uint64_t halfReadbacks = 0;      // successful glGetTexImage(GL_HALF_FLOAT) transfers
	uint64_t halfFailures = 0;       // half transfer unsupported / GL error: sample recorded as a gap (Half) or skipped (HalfVerify)
	uint64_t glErrors = 0;           // GL errors raised by the half transfer itself
	uint64_t verifySamples = 0;      // HalfVerify: samples compared
	uint64_t verifyMismatchSamples = 0; // ... of which at least one float word differed bitwise (NaN == NaN)
	uint64_t verifyMismatchWords = 0;
	long long verifyFirstIndex = -1; // first differing word (float index) and its values, for a minimal reproduction
	float verifyFirstBase = 0.0f, verifyFirstHalf = 0.0f;
};

class SceneColorSampler {
public:
	SceneColorSampler();
	~SceneColorSampler();
	SceneColorSampler(const SceneColorSampler&) = delete;
	SceneColorSampler& operator=(const SceneColorSampler&) = delete;

	// OC-003A diagnostic (EXPERIENCE_RUNTIME_SCENE_COLOR_PREDOM=<dir>, off by default; SceneColorSamplerDiag.cpp).
	// Independently of the ratified series it reduces the same SceneFBO, reads BOTH the 160x90 and the 16x9
	// linear-light targets and logs average / dominant / vibrant colours (+ smoothing variants and stage costs)
	// to <dir>/predominant.csv, plus periodic SceneFBO frames. Never touches the series or the published snapshot.
	void diagFrame(const ofTexture& sceneTexture, double now, const std::string& sceneId, uint64_t generation);
	void diagStaticFrame(double now, const std::string& sceneId, uint64_t generation); // retained transition frame: logs a gap row

	// OC-003B: what this instance measures. SceneFbo (default, the FX sampler) keeps the strict
	// 8-bit RGBA validation; DecodedVideo also accepts opaque RGB/RGB8 decoder textures.
	// OC-003 (opt-in, default off): also measure DOMINANT and VIBRANT colour from the 160x90 linear-light target. With
	// this on the sampler reads back that target INSTEAD of the 16x9 one (one readback per sample either way) and
	// derives AVG, DOM and VIB from the same pixels. Only on the GPU linear path (not the GLES/encoded fallback).
	// Set before the first sample; temporal state (hysteresis + EMA) restarts at every generation and gap.
	// OC-003-F diagnostic (default: none attached, zero cost): hand the 160x90 grid this sampler ALREADY read back to a palette sink.
	// Adds no readback and changes no sample; the sink's time is accumulated and taken once per frame (takePaletteUs) so it is
	// reported separately from acquisition cost.
	void setPaletteSink(palette::Sink* sink, int stream) { paletteSink_ = sink; paletteStream_ = stream; }
	double takePaletteUs() { const double v = framePaletteUs_; framePaletteUs_ = 0.0; return v; }
	void setPredominantEnabled(bool on) { predominantWanted_ = on; }
	bool predominantWanted() const { return predominantWanted_; }
	// True iff samples from this instance carry DOM/VIB measurements (wanted AND the linear path is in use).
	bool predominantActive() const { return predominantWanted_ && linearOk_; }
	// OC-003 experimental readback format (default Float). Set before the first sample.
	void setReadbackFormat(ReadbackFormat f) { readbackFormat_ = f; }
	ReadbackFormat readbackFormat() const { return readbackFormat_; }
	const char* readbackName() const { return readbackFormat_ == ReadbackFormat::Float ? "float" : readbackFormat_ == ReadbackFormat::Half ? "half" : "half-verify"; }
	// One log fragment: format, actual bytes per readback, effective GL transfer format/type, counters.
	std::string readbackSummary() const;
	void setSourceKind(rawvideo::SourceKind k) { sourceKind_ = k; formatChecked_ = false; }
	rawvideo::SourceKind sourceKind() const { return sourceKind_; }
	void setMode(ReadbackMode m) { mode_ = m; }
	ReadbackMode mode() const { return mode_; }
	// The mode actually used: Async needs GL 3.2 sync objects (desktop); GLES2 / older GL fall back to Sync.
	// True when the GPU linear-light reduction is in use (exact up to filtering); false = the 8-bit
	// encoded halving chain + CPU linearisation fallback (GLES2 / no half-float render targets).
	bool gpuLinearPath() const { return linearOk_; }
	ReadbackMode effectiveMode() const { return mode_ == ReadbackMode::Deferred ? (linearOk_ ? ReadbackMode::Deferred : ReadbackMode::Sync) : (asyncUsable_ ? mode_ : ReadbackMode::Sync); }

	// New scene generation: clears history, drops any in-flight readback.
	void beginGeneration(uint64_t generation, double now);

	// Live-frame hook (SceneFBO just rendered). Cheap no-op unless due.
	void onLiveFrame(const ofTexture& sceneTexture, double now);
	// Transition static frame: the SceneFBO holds a retained frame, so no
	// new measurement of "the scene" exists. Records a gap when a sample
	// would have been due.
	void onStaticFrame(double now);
	// Per-frame: completes a finished async readback. No-op in Sync mode.
	void poll(double now);

	const Series& series() const { return series_; }
	const Stats& stats() const { return stats_; }
	void resetStats() { stats_ = Stats{}; gate_.resetStats(); }
	// A7: repeated deferred-readback failures disable scene-colour telemetry (the series simply goes Stale; the
	// scene keeps rendering). It re-probes by itself after DeferredReadbackGate::kRecoveryCooldownMs; this is
	// the explicit manual path.
	bool telemetryDisabled() const { return gate_.disabled(); }
	void reenableTelemetry() { gate_.reenable(); }
	const DeferredGateStats& gateStats() const { return gate_.stats(); }
	// Releases GL resources (fences) while the context is alive; call from the owner's exit().
	void shutdown();
	// OC-003: how a Deferred readback decides it may read. "fence" = non-blocking glClientWaitSync poll,
	// "frames:N" = N frames of deferral with no completion check (the previous behaviour is N = 2).
	const char* deferStrategy() const { return defer_ == Defer::Fence ? "fence" : "frames"; }
	int deferFrames() const { return deferFrames_; }

private:
	void ensureChain(int w, int h);
	bool validateSource(const ofTexture& tex);
	bool setupLinearPath();                                   // half-float FBOs + shaders; false = fallback
	void submitLinear(const ofTexture& scene, int slot, bool coarse = true); // reduction into linA_ (+ linBs_[slot] when `coarse`)
	bool collectLinear(int slot, double t, uint64_t gen);       // read + average + push
	bool readHalfTarget(const ofTexture& tex, bool publish);    // OC-003 experimental: GL_HALF_FLOAT transfer (+ widening into pxBuf_ when `publish`)
	bool sampleLinearGpu(const ofTexture& scene, double now); // exact linear-light reduction, sync float readback // transfer characteristics / format check, logged once
	void finishSample(const uint8_t* px, double t, uint64_t gen);
	void releaseFence();
	void recordGap(double t, uint64_t gen); // explicit missing sample (valid=false): never a zero, never a copy

	Series series_;
	Stats stats_;
	ReadbackMode mode_ = ReadbackMode::Sync;
	std::array<ofFbo, 12> chain_;
	int chainStages_ = 0;
	int srcW_ = 0, srcH_ = 0;
	ofBufferObject pbo_;
	void* fence_ = nullptr;   // GLsync
	double inflightT_ = 0.0;
	uint64_t inflightGen_ = 0;
	bool capabilitiesChecked_ = false;
	bool asyncUsable_ = false;
	bool formatOk_ = true;
	int validatedW_ = 0, validatedH_ = 0, validatedFormat_ = 0;
	bool formatChecked_ = false;
	rawvideo::SourceKind sourceKind_ = rawvideo::SourceKind::SceneFbo;
	ofShader reduceRect_, reduce2D_;   // pass shaders for a rectangle / 2D source texture
	ofFbo linA_;                       // RGBA16F ~160x90 (linear light)
	std::array<ofFbo, 2> linBs_;       // RGBA16F 16x9 targets (two: a deferred read never races the next draw)
	ofFloatPixels pxBuf_;              // reused readback buffer (no per-sample allocation)
	ReadbackFormat readbackFormat_ = ReadbackFormat::Float;
	std::vector<uint16_t> halfBuf_;    // experimental half readback storage (reused)
	std::vector<float> verifyBuf_;     // HalfVerify: widened copy for the bitwise comparison (reused)
	std::string halfInfo_;             // effective GL transfer description, filled at the first half transfer
	int halfLogged_ = 0;
	DeferredReadbackGate gate_;        // A7: single pending request + failure threshold (OF-free, unit tested)
	int pendingFrames_ = 0, nextSlot_ = 0;
	static constexpr int kDeferFrames = 2;
	static constexpr double kFenceTimeoutMs = 250.0; // a fence this old is DISCARDED (counted), never read with a blocking call
	enum class Defer { Frames, Fence };
	Defer defer_ = Defer::Frames;
	int deferFrames_ = kDeferFrames;
	bool deferFlush_ = false;          // EXPERIENCE_RUNTIME_SCENE_COLOR_DEFER_FLUSH=1: glFlush right after the fence
	void* linFence_ = nullptr;         // GLsync guarding the pending deferred read
	void chooseDeferStrategy();
	void releaseLinFence();
	struct PredomDiag;                 // OC-003A diagnostic state (SceneColorSamplerDiag.cpp)
	std::unique_ptr<PredomDiag> diag_;
	palette::Sink* paletteSink_ = nullptr; // OC-003-F, not owned
	int paletteStream_ = 0;
	double framePaletteUs_ = 0.0;
	bool linearTried_ = false, linearOk_ = false;
	bool predominantWanted_ = false;
	predominant::Tracker tracker_;     // OC-003 temporal state of this source (reset per generation / gap)
	int probeW_ = 0, probeH_ = 0;      // final chain stage = readback size (no forced square, no upsample)
	int probeMax_ = kDefaultProbeMax;  // chain stops once both sides are <= this (env EXPERIENCE_RUNTIME_SCENE_COLOR_PROBE)
};

} // namespace scenecolorsampling
