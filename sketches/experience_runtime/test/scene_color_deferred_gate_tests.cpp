// A7 -- DeferredReadbackGate (SceneColorDeferredGate.h, OF-free): the decision core of the Scene Color sampler's
// deferred readback. A tiny fake sampler below applies the gate's actions to a real Series exactly as
// SceneColorSampler::poll does (Collect -> push valid/invalid, Discard* -> push an explicit gap, generation -> nothing),
// so publication semantics (valid-black vs gap vs stale) are checked against the real Series/SnapshotPublisher.
#include "SceneColorDeferredGate.h"
#include "SceneColorSeries.h"

#include <iostream>
#include <string>

using namespace scenecolorsampling;

namespace {
int g_total = 0, g_failures = 0;
void check(bool cond, const std::string& what) {
	g_total++;
	if (!cond) { g_failures++; std::cerr << "FAIL: " << what << "\n"; }
}

constexpr double kTimeout = 250.0;

// Mirrors SceneColorSampler's use of the gate, minus GL. `releases` counts fence releases (resource cleanup).
struct FakeSampler {
	DeferredReadbackGate gate;
	Series series;
	int releases = 0, issuedFences = 0, blockingReads = 0, skippedDisabled = 0;
	double nowMs = 0.0;

	void begin(uint64_t gen) {
		if (gate.cancelPending()) ++releases; // beginGeneration releases the fence
		series.beginGeneration(gen, nowMs / 1000.0);
	}
	bool issue(float v = 0.5f) {
		if (gate.pending()) return false;
		if (!gate.tryIssue(nowMs)) { ++skippedDisabled; return false; }
		++issuedFences;
		gate.issued(issuedFences % 2, nowMs, nowMs / 1000.0, series.generation(), kTimeout);
		valueToRead = v;
		return true;
	}
	// `readOk`: whether the finished read yields usable data (false = empty / non-finite).
	PollAction poll(FenceStatus st, bool readOk = true) {
		const double t = gate.sampleT();
		const uint64_t g = gate.generation();
		const PollAction a = gate.poll(st, nowMs, series.generation());
		Sample s; s.t = t; s.generation = g;
		switch (a) {
		case PollAction::None: break;
		case PollAction::Collect:
			++releases;
			if (readOk) { s.r = s.g = s.b = valueToRead; s.luma = luma709(s.r, s.g, s.b); s.valid = true; }
			series.push(s);
			gate.reportResult(readOk, nowMs);
			break;
		case PollAction::DiscardWaitFailed:
		case PollAction::DiscardTimeout:
			++releases;
			series.push(s); // explicit gap
			break;
		case PollAction::DiscardGeneration:
			++releases;
			break;
		}
		return a;
	}
	float valueToRead = 0.5f;
};
} // namespace

int main() {
	// -- fence signaled: collect, valid sample published
	{
		FakeSampler f; f.begin(1);
		check(f.issue(0.25f), "issue accepted when idle");
		f.nowMs = 33.0;
		check(f.poll(FenceStatus::Signaled) == PollAction::Collect, "signaled fence -> Collect");
		check(f.series.size() == 1 && f.series.at(0).valid && f.series.at(0).r == 0.25f, "valid sample published with the read value");
		check(!f.gate.pending() && f.releases == 1, "fence released after collect");
		check(f.gate.stats().completedValid == 1 && f.gate.consecutiveFailures() == 0, "success counted, streak clear");
	}
	// -- fence unsignaled: never blocks, stays pending, nothing published, no blocking read
	{
		FakeSampler f; f.begin(1); f.issue();
		for (int i = 1; i <= 5; ++i) { f.nowMs = 30.0 * i; check(f.poll(FenceStatus::NotReady) == PollAction::None, "not-ready poll returns None"); }
		check(f.gate.pending() && f.series.size() == 0 && f.releases == 0, "still pending, nothing published, nothing released");
		check(f.gate.stats().notReadyPolls == 5, "not-ready polls counted");
		check(f.blockingReads == 0, "no blocking read on the not-ready path");
	}
	// -- timeout: discarded, NOT collected (no forced blocking read), gap recorded
	{
		FakeSampler f; f.begin(1); f.issue();
		f.nowMs = kTimeout - 1; check(f.poll(FenceStatus::NotReady) == PollAction::None, "just under the timeout: still waiting");
		f.nowMs = kTimeout;     check(f.poll(FenceStatus::NotReady) == PollAction::DiscardTimeout, "at the timeout: DiscardTimeout (not Collect)");
		check(!f.gate.pending() && f.releases == 1, "timed-out request released");
		check(f.series.size() == 1 && !f.series.at(0).valid, "timeout publishes an explicit gap");
		check(f.gate.stats().discardedTimeout == 1 && f.gate.stats().collected == 0, "timeout counted; nothing collected");
		check(f.series.state(1.0) == State::Unavailable, "a gap alone never makes the series Valid");
	}
	// -- GL_WAIT_FAILED: immediate invalidation + release + gap
	{
		FakeSampler f; f.begin(1); f.issue();
		f.nowMs = 5.0;
		check(f.poll(FenceStatus::WaitFailed) == PollAction::DiscardWaitFailed, "WAIT_FAILED -> DiscardWaitFailed (no waiting for the timeout)");
		check(!f.gate.pending() && f.releases == 1 && f.series.size() == 1 && !f.series.at(0).valid, "released, gap published");
		check(f.gate.stats().discardedWaitFailed == 1 && f.gate.consecutiveFailures() == 1, "counted as a failure");
	}
	// -- pending capacity: exactly one request, a second issue is refused and counted
	{
		FakeSampler f; f.begin(1);
		check(f.issue(), "first issue ok");
		check(!f.issue() && !f.issue(), "further issues refused while pending");
		check(f.gate.stats().issued == 1 && f.issuedFences == 1, "one fence ever outstanding");
		check(!f.gate.tryIssue(f.nowMs) && f.gate.stats().rejectedBusy == 1, "tryIssue itself refuses and counts a busy request");
		f.poll(FenceStatus::Signaled);
		check(f.issue(), "capacity returns after completion");
	}
	// -- resource cleanup: every issued request is released exactly once on every exit path
	{
		FakeSampler f; f.begin(1);
		f.issue(); f.poll(FenceStatus::Signaled);
		f.issue(); f.poll(FenceStatus::WaitFailed);
		f.issue(); f.nowMs += kTimeout + 1; f.poll(FenceStatus::NotReady);
		f.issue(); f.begin(2); // abandoned by a scene change
		check(f.issuedFences == 4 && f.releases == 4 && !f.gate.pending(), "4 requests, 4 releases, none pending (collect/wait-failed/timeout/generation)");
		check(f.gate.cancelPending() == false, "shutdown with nothing pending is a no-op");
		f.issue();
		check(f.gate.cancelPending() && !f.gate.pending(), "shutdown abandons a pending request");
	}
	// -- scene-generation replacement: stale result never reaches the new generation
	{
		FakeSampler f; f.begin(1); f.issue(0.9f);
		f.begin(2); // scene switch while in flight
		check(!f.gate.pending() && f.series.generation() == 2 && f.series.size() == 0, "pending cleared, new generation empty");
		check(f.poll(FenceStatus::Signaled) == PollAction::None, "late 'signaled' with nothing pending does nothing");
		check(f.series.size() == 0, "nothing from generation 1 appears in generation 2");
		// Belt and braces: a request still marked pending with an old generation is discarded before any read.
		DeferredReadbackGate g;
		g.tryIssue(0.0); g.issued(0, 0.0, 0.0, 1, kTimeout);
		check(g.poll(FenceStatus::Signaled, 10.0, 2) == PollAction::DiscardGeneration, "generation mismatch -> DiscardGeneration even if signaled");
		check(g.consecutiveFailures() == 0, "a scene change is not a device failure");
		Series s; s.beginGeneration(2, 0.0);
		Sample old; old.generation = 1; old.valid = true; old.r = 1.f;
		check(!s.push(old), "Series itself also rejects the previous generation");
	}
	// -- invalid sample publication: stale / missing / valid-black stay distinguishable
	{
		FakeSampler f; f.begin(1);
		f.issue(0.0f); f.nowMs = 10; f.poll(FenceStatus::Signaled); // valid black
		check(f.series.at(0).valid && f.series.at(0).r == 0.0f && f.series.state(0.5) == State::Valid, "valid black is a VALID zero");
		f.issue(); f.nowMs = 20; f.poll(FenceStatus::WaitFailed);   // missing
		check(!f.series.at(1).valid, "failure is a gap (valid=false)");
		check(f.series.at(1).r == 0.0f && f.series.newestValid() == &f.series.at(0), "gap is not a zero and does not hide the last valid sample");
		f.issue(0.7f); f.nowMs = 30; f.poll(FenceStatus::Signaled, /*readOk*/ false); // finished but unusable
		check(!f.series.at(2).valid && f.series.newestValid() == &f.series.at(0), "unusable read -> gap, previous result NOT copied forward");
		check(f.gate.stats().completedInvalid == 1, "invalid completion counted");
		check(f.series.state(0.0 + 0.5) == State::Valid && f.series.state(0.0 + 100.0) == State::Stale, "stale is decided by age of the last VALID sample");
		SnapshotPublisher pub;
		auto snap = pub.publish(f.series, 0.5, "scene");
		check(snap && snap->count == 3 && snap->history[0].valid && !snap->history[1].valid && !snap->history[2].valid, "snapshot keeps valid/gap flags per sample");
	}
	// -- repeated failure threshold -> telemetry disabled, never throws/blocks, scene unaffected (gate just refuses issue)
	{
		FakeSampler f; f.begin(1);
		for (int i = 0; i < DeferredReadbackGate::kFailureThreshold - 1; ++i) { f.nowMs += 100; f.issue(); f.poll(FenceStatus::WaitFailed); }
		check(!f.gate.disabled() && f.gate.consecutiveFailures() == DeferredReadbackGate::kFailureThreshold - 1, "below the threshold: still enabled");
		f.nowMs += 100; f.issue(); f.poll(FenceStatus::WaitFailed);
		check(f.gate.disabled() && f.gate.stats().disabledEvents == 1, "threshold reached: telemetry disabled (once)");
		const int fences = f.issuedFences;
		for (int i = 0; i < 20; ++i) { f.nowMs += 500; check(!f.issue(), "disabled: issue refused"); }
		check(f.issuedFences == fences && f.skippedDisabled == 20 && f.gate.stats().rejectedDisabled == 20, "no fences created while disabled; skips counted");
		check(f.series.state(f.nowMs / 1000.0) != State::Valid, "disabled telemetry never reports Valid (goes Stale/Unavailable)");
	}
	// -- a success in between resets the streak
	{
		FakeSampler f; f.begin(1);
		f.issue(); f.poll(FenceStatus::WaitFailed);
		f.issue(); f.poll(FenceStatus::WaitFailed);
		f.issue(); f.poll(FenceStatus::Signaled);
		check(f.gate.consecutiveFailures() == 0 && !f.gate.disabled(), "success clears the failure streak");
		f.issue(); f.poll(FenceStatus::WaitFailed);
		check(!f.gate.disabled(), "streak restarted from zero: not disabled");
	}
	// -- recovery: cooldown, single probe, success re-enables, failure re-arms
	{
		FakeSampler f; f.begin(1);
		for (int i = 0; i < DeferredReadbackGate::kFailureThreshold; ++i) { f.issue(); f.poll(FenceStatus::WaitFailed); }
		check(f.gate.disabled(), "disabled");
		f.nowMs += DeferredReadbackGate::kRecoveryCooldownMs - 1;
		check(!f.issue(), "before the cooldown ends: no probe");
		f.nowMs += 2;
		check(f.issue(), "after the cooldown: exactly one probe request is admitted");
		check(!f.issue(), "probe pending: nothing else admitted");
		f.poll(FenceStatus::WaitFailed); // probe fails
		check(f.gate.disabled() && f.gate.stats().disabledEvents == 1, "failed probe keeps it disabled (not a new disable event)");
		f.nowMs += 1000;
		check(!f.issue(), "failed probe re-armed the cooldown");
		f.nowMs += DeferredReadbackGate::kRecoveryCooldownMs;
		check(f.issue(), "second probe admitted after the new cooldown");
		f.nowMs += 20; f.poll(FenceStatus::Signaled);
		check(!f.gate.disabled() && f.gate.stats().recoveries == 1 && f.gate.consecutiveFailures() == 0, "successful probe re-enables telemetry");
		check(f.series.newestValid() != nullptr && f.series.state(f.nowMs / 1000.0) == State::Valid, "series is Valid again after recovery");
		// manual path
		for (int i = 0; i < DeferredReadbackGate::kFailureThreshold; ++i) { f.nowMs += 10; f.issue(); f.poll(FenceStatus::WaitFailed); }
		check(f.gate.disabled(), "disabled again");
		f.gate.reenable();
		check(!f.gate.disabled() && f.gate.consecutiveFailures() == 0 && f.issue(), "explicit reenable() restores issuing immediately");
	}
	// -- a generation drop during a recovery probe does not wedge the gate
	{
		FakeSampler f; f.begin(1);
		for (int i = 0; i < DeferredReadbackGate::kFailureThreshold; ++i) { f.issue(); f.poll(FenceStatus::WaitFailed); }
		f.nowMs += DeferredReadbackGate::kRecoveryCooldownMs + 1;
		check(f.issue(), "probe admitted");
		f.begin(2);
		check(f.gate.disabled() && !f.gate.pending(), "probe abandoned by a scene change, still disabled");
		check(f.issue(), "cooldown already elapsed: next probe admitted (not wedged)");
	}
	// -- legacy frame-count strategy has no timeout (timeoutMs <= 0)
	{
		DeferredReadbackGate g; g.tryIssue(0.0); g.issued(0, 0.0, 0.0, 1, 0.0);
		check(g.poll(FenceStatus::NotReady, 1.0e9, 1) == PollAction::None && g.pending(), "no fence -> no timeout discard");
		check(g.poll(FenceStatus::Signaled, 1.0e9, 1) == PollAction::Collect, "frame strategy collects when its frame count says so");
	}

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
