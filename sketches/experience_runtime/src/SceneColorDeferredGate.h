// A7 -- runtime-private, OpenFrameworks-free decision core for the Scene Color sampler's deferred readback.
// SceneColorSampler.cpp does the GL calls (fence create / glClientWaitSync(timeout 0) / delete / readToPixels)
// and feeds the observed fence status in here; this class decides what happens next, so every safety invariant
// is unit-testable without a GL context.
//
// Invariants (A7 Phase 1):
//   - at most ONE request is pending (bounded GPU resources: one fence, two reduction targets);
//   - a fence that is not ready never blocks: the poll returns None and the request stays pending;
//   - a fence that stays not ready past its timeout is DISCARDED (never read with a blocking call);
//   - a failed wait (GL_WAIT_FAILED / unknown status) invalidates the request immediately;
//   - a request from a previous scene generation is discarded without touching the fence result;
//   - discarded / invalid requests are explicit failures: the caller records a gap, never a zero or a copy
//     of a previous sample;
//   - kFailureThreshold consecutive failures disable scene-colour TELEMETRY only (the caller keeps rendering);
//     after kRecoveryCooldownMs one probe request is allowed (half-open): success re-enables, failure re-arms
//     the cooldown. reenable() is the explicit manual path.
#pragma once

#include <cstdint>

namespace scenecolorsampling {

enum class FenceStatus { Signaled, NotReady, WaitFailed };

enum class PollAction {
	None,               // nothing pending, or pending and not ready yet: do nothing this frame
	Collect,            // the target is finished: read it (non-blocking in practice) and publish
	DiscardWaitFailed,  // glClientWaitSync failed: release resources, record a gap
	DiscardTimeout,     // not ready within the timeout: release resources, record a gap (NO blocking read)
	DiscardGeneration,  // scene generation changed while in flight: release resources, publish nothing
};

struct DeferredGateStats {
	uint64_t issued = 0;
	uint64_t collected = 0;          // Collect actions returned
	uint64_t completedValid = 0;     // reportResult(true)
	uint64_t completedInvalid = 0;   // reportResult(false): read finished but the data was unusable
	uint64_t discardedWaitFailed = 0;
	uint64_t discardedTimeout = 0;
	uint64_t discardedGeneration = 0;
	uint64_t notReadyPolls = 0;
	uint64_t rejectedBusy = 0;       // issue refused: a request is already pending
	uint64_t rejectedDisabled = 0;   // issue refused: telemetry disabled by the failure threshold
	uint64_t disabledEvents = 0;     // times the threshold tripped
	uint64_t recoveries = 0;         // times a probe success re-enabled telemetry
};

class DeferredReadbackGate {
public:
	static constexpr int kFailureThreshold = 3;               // consecutive failures that disable telemetry
	static constexpr double kRecoveryCooldownMs = 30000.0;    // disabled this long, then one probe request is allowed

	// May a new request be issued now? Counts the refusal reason. A disabled gate admits exactly one probe
	// request once the cooldown has elapsed.
	bool tryIssue(double nowMs) {
		if (pending_) { ++stats_.rejectedBusy; return false; }
		if (disabled_) {
			if (nowMs < disabledSinceMs_ + kRecoveryCooldownMs) { ++stats_.rejectedDisabled; return false; }
			probing_ = true;
		}
		return true;
	}

	// Registers the request the caller has just submitted. Precondition: tryIssue() returned true.
	// timeoutMs <= 0: no timeout (the legacy frame-count strategy, which has no fence to time out).
	void issued(int slot, double issueMs, double sampleT, uint64_t generation, double timeoutMs) {
		pending_ = true;
		slot_ = slot;
		issueMs_ = issueMs;
		sampleT_ = sampleT;
		generation_ = generation;
		timeoutMs_ = timeoutMs;
		++stats_.issued;
	}

	// Called once per frame while pending. `status` is the observed fence status (the caller maps GL results;
	// for the frame-count strategy it passes Signaled once enough frames have passed, else NotReady).
	// Anything other than None/Collect has released the request; the caller must release its GL resources.
	PollAction poll(FenceStatus status, double nowMs, uint64_t currentGeneration) {
		if (!pending_) return PollAction::None;
		if (generation_ != currentGeneration) {
			pending_ = false;
			++stats_.discardedGeneration; // not a device failure: does not count toward the threshold
			clearProbe();
			return PollAction::DiscardGeneration;
		}
		switch (status) {
		case FenceStatus::Signaled:
			pending_ = false;
			++stats_.collected;
			return PollAction::Collect;
		case FenceStatus::WaitFailed:
			pending_ = false;
			++stats_.discardedWaitFailed;
			registerFailure(nowMs);
			return PollAction::DiscardWaitFailed;
		case FenceStatus::NotReady:
		default:
			if (timeoutMs_ > 0.0 && nowMs - issueMs_ >= timeoutMs_) {
				pending_ = false;
				++stats_.discardedTimeout;
				registerFailure(nowMs);
				return PollAction::DiscardTimeout;
			}
			++stats_.notReadyPolls;
			return PollAction::None;
		}
	}

	// The caller finished a Collect: was the data usable (finite, non-empty)?
	void reportResult(bool valid, double nowMs) {
		if (valid) {
			++stats_.completedValid;
			consecutiveFailures_ = 0;
			if (disabled_) { disabled_ = false; ++stats_.recoveries; }
			probing_ = false;
		} else {
			++stats_.completedInvalid;
			registerFailure(nowMs);
		}
	}

	// A scene change (or sampler shutdown) abandons whatever is in flight. Returns true if something was pending.
	bool cancelPending() {
		if (!pending_) return false;
		pending_ = false;
		++stats_.discardedGeneration;
		clearProbe();
		return true;
	}

	// Explicit manual re-enable (e.g. an operator action); clears the failure streak.
	void reenable() {
		if (disabled_) ++stats_.recoveries;
		disabled_ = false;
		probing_ = false;
		consecutiveFailures_ = 0;
	}

	bool pending() const { return pending_; }
	int slot() const { return slot_; }
	double sampleT() const { return sampleT_; }
	uint64_t generation() const { return generation_; }
	double issueMs() const { return issueMs_; }
	bool disabled() const { return disabled_; }
	int consecutiveFailures() const { return consecutiveFailures_; }
	const DeferredGateStats& stats() const { return stats_; }
	void resetStats() { stats_ = DeferredGateStats{}; }

private:
	void registerFailure(double nowMs) {
		++consecutiveFailures_;
		const bool wasProbe = probing_;
		probing_ = false;
		if (wasProbe || (!disabled_ && consecutiveFailures_ >= kFailureThreshold)) {
			if (!disabled_) ++stats_.disabledEvents;
			disabled_ = true;
			disabledSinceMs_ = nowMs; // (re)arm the cooldown
		}
	}
	void clearProbe() { probing_ = false; }

	DeferredGateStats stats_;
	bool pending_ = false;
	bool disabled_ = false;
	bool probing_ = false;
	int consecutiveFailures_ = 0;
	int slot_ = 0;
	double issueMs_ = 0.0, sampleT_ = 0.0, timeoutMs_ = 0.0, disabledSinceMs_ = 0.0;
	uint64_t generation_ = 0;
};

} // namespace scenecolorsampling
