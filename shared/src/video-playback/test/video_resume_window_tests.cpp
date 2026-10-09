// video_resume_window_tests.cpp — VideoSelectionPolicy's dwell / long-clip
// resume-window policy. Dependency-free, same convention as
// video_selection_policy_tests.cpp. ClipSim below plays the role of
// VideoPlaybackService + a looping decoder: it saves the outgoing playhead
// before every selection, confirms, begins playback once the duration is
// known, seeks, and reports a looping playhead every tick.

#include "../MediaMetadata.h"
#include "../VideoPlaybackStatus.h"
#include "../VideoSelectionPolicy.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

std::vector<MediaMetadata> makeCatalog(int count) {
	std::vector<MediaMetadata> catalog;
	for (int i = 0; i < count; i++) {
		MediaMetadata m;
		m.mediaId = "media." + std::to_string(i);
		m.relativePath = "clip" + std::to_string(i) + ".mp4";
		m.enabled = true;
		catalog.push_back(m);
	}
	return catalog;
}

VideoSelectionPolicy::RandomIndexFn identityRandom() {
	return [](size_t) -> size_t { return 0; };
}

bool near(float a, float b, float tol = 0.5f) { return std::fabs(a - b) <= tol; }

using Clip = VideoSelectionPolicy::ClipClass;

struct ClipSim {
	VideoSelectionPolicy& policy;
	std::vector<float> durations; // per catalog index; <= 0 = decoder reports no duration
	int active = -1;
	float pos = 0.0f;
	static constexpr float kDt = 0.05f;

	// Mirrors VideoPlaybackService::attemptSelection() + the resume seek.
	void activate(int index, MediaSelectionOrigin origin, bool seekFails = false) {
		if (policy.hasActiveItem()) policy.notePlaybackExit(pos);
		policy.confirmActivationSucceeded(index, origin);
		active = index;
		pos = 0.0f;
		const float d = durations[static_cast<size_t>(index)];
		if (d > 0.0f) {
			const float start = policy.beginPlayback(d);
			if (start > 0.0f) {
				if (seekFails) policy.notePlaybackStartFailed();
				else pos = start;
			}
		}
	}

	void tick() {
		policy.updateHold(kDt);
		const float d = durations[static_cast<size_t>(active)];
		pos += kDt;
		if (d > 0.0f && pos >= d) pos -= d; // OF_LOOP_NORMAL
		policy.notePlaybackPosition(d > 0.0f ? pos : -1.0f);
	}

	void run(float seconds) {
		for (float t = 0.0f; t < seconds - 1e-4f; t += kDt) tick();
	}

	// Seconds of playback until the policy says the hold is complete.
	float runUntilHold(float cap = 2000.0f) {
		float t = 0.0f;
		while (!policy.isHoldComplete() && t < cap) {
			tick();
			t += kDt;
		}
		return t;
	}
};

VideoSelectionPolicy::Config defaultConfig() {
	VideoSelectionPolicy::Config c;
	c.holdDurationSeconds = 30.0f; // min dwell
	c.maxDwellSeconds = 120.0f;    // threshold derives to 240
	return c;
}

} // namespace

#define VRW_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

static void test_classification() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(1), defaultConfig(), identityRandom());
	VRW_CHECK(policy.classify(0.0f) == Clip::Unknown);
	VRW_CHECK(policy.classify(8.0f) == Clip::Short);
	VRW_CHECK(policy.classify(29.9f) == Clip::Short);
	VRW_CHECK(policy.classify(30.0f) == Clip::Medium);
	VRW_CHECK(policy.classify(240.0f) == Clip::Medium); // threshold = 2 x max dwell, inclusive
	VRW_CHECK(policy.classify(240.5f) == Clip::Long);

	VideoSelectionPolicy::Config custom = defaultConfig();
	custom.longClipThresholdSeconds = 100.0f;
	policy.reset(makeCatalog(1), custom, identityRandom());
	VRW_CHECK(policy.classify(101.0f) == Clip::Long);
}

static void test_short_clip_loops_until_min_dwell() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(2), defaultConfig(), identityRandom());
	ClipSim sim{policy, {8.0f, 60.0f}};
	sim.activate(0, MediaSelectionOrigin::Startup);
	VRW_CHECK(policy.activeClipClass() == Clip::Short);
	VRW_CHECK(policy.activeWindowStartSeconds().value_or(-1.0f) == 0.0f);
	sim.run(29.0f);
	VRW_CHECK(!policy.isHoldComplete()); // looped ~3.6x but min dwell not yet met
	VRW_CHECK(policy.activeCompletedThisVisit());
	VRW_CHECK(near(sim.runUntilHold(), 1.0f));
	VRW_CHECK(near(*policy.holdDurationSeconds(), 30.0f));
}

static void test_medium_clip_plays_once() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(2), defaultConfig(), identityRandom());
	ClipSim sim{policy, {45.0f, 200.0f}};
	sim.activate(0, MediaSelectionOrigin::Startup);
	VRW_CHECK(policy.activeClipClass() == Clip::Medium);
	VRW_CHECK(near(*policy.holdDurationSeconds(), 45.0f));
	sim.run(40.0f);
	VRW_CHECK(!policy.isHoldComplete());
	VRW_CHECK(near(40.0f + sim.runUntilHold(), 45.0f)); // eligible right after the first completion

	// A 200 s medium clip is NOT cut at the 120 s max dwell: one full playback.
	sim.activate(1, MediaSelectionOrigin::Automatic);
	VRW_CHECK(policy.activeClipClass() == Clip::Medium);
	VRW_CHECK(policy.activeWindowStartSeconds().value_or(-1.0f) == 0.0f);
	VRW_CHECK(near(sim.runUntilHold(), 200.0f));
}

static void test_long_clip_progressive_windows() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(2), defaultConfig(), identityRandom());
	ClipSim sim{policy, {720.0f, 8.0f}};

	const float expectedStarts[] = {0.0f, 120.0f, 240.0f, 360.0f, 480.0f};
	for (float expectedStart : expectedStarts) {
		sim.activate(0, MediaSelectionOrigin::Automatic);
		VRW_CHECK(policy.activeClipClass() == Clip::Long);
		VRW_CHECK(near(*policy.activeWindowStartSeconds(), expectedStart));
		VRW_CHECK(near(sim.runUntilHold(), 120.0f)); // bounded window
		sim.activate(1, MediaSelectionOrigin::Automatic); // another clip in between
		VRW_CHECK(near(policy.resumeSecondsFor(0), expectedStart + 120.0f));
		sim.runUntilHold();
	}
}

static void test_long_clip_wraps_at_end() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(2), defaultConfig(), identityRandom());
	ClipSim sim{policy, {300.0f, 8.0f}};

	// Visits: 0-120, 120-240, then 240 -> end (60 s left, >= min dwell).
	for (int i = 0; i < 2; ++i) {
		sim.activate(0, MediaSelectionOrigin::Automatic);
		sim.runUntilHold();
		sim.activate(1, MediaSelectionOrigin::Automatic);
		sim.runUntilHold();
	}
	sim.activate(0, MediaSelectionOrigin::Automatic);
	VRW_CHECK(near(*policy.activeWindowStartSeconds(), 240.0f));
	VRW_CHECK(near(sim.runUntilHold(), 60.0f)); // reached the end -> leaves once min dwell met
	VRW_CHECK(policy.activeCompletedThisVisit());
	sim.activate(1, MediaSelectionOrigin::Automatic);
	VRW_CHECK(policy.resumeSecondsFor(0) == 0.0f); // wrapped — not overwritten by post-loop playhead
	sim.runUntilHold();
	sim.activate(0, MediaSelectionOrigin::Automatic);
	VRW_CHECK(*policy.activeWindowStartSeconds() == 0.0f);

	// Near-end exit: fewer than min-dwell seconds would remain -> wrap to 0.
	sim.run(10.0f);
	sim.pos = 285.0f;
	sim.activate(1, MediaSelectionOrigin::ManualNext);
	VRW_CHECK(policy.resumeSecondsFor(0) == 0.0f);
}

static void test_manual_next_and_previous_preserve_progress() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(3), defaultConfig(), identityRandom());
	ClipSim sim{policy, {720.0f, 600.0f, 8.0f}};

	sim.activate(0, MediaSelectionOrigin::Startup);
	sim.run(100.0f); // video A at ~1:40
	sim.activate(1, MediaSelectionOrigin::ManualNext);
	VRW_CHECK(near(policy.resumeSecondsFor(0), 100.0f)); // saved before leaving
	sim.run(50.0f);

	sim.activate(0, MediaSelectionOrigin::ManualPrevious);
	VRW_CHECK(policy.activeCatalogIndex() == 0);
	VRW_CHECK(near(*policy.activeWindowStartSeconds(), 100.0f)); // resumes, not restarts
	VRW_CHECK(near(policy.resumeSecondsFor(1), 50.0f));            // forward media's progress kept
	sim.run(20.0f);

	sim.activate(1, MediaSelectionOrigin::ManualNext); // replays forward history
	VRW_CHECK(policy.activeCatalogIndex() == 1);
	VRW_CHECK(near(*policy.activeWindowStartSeconds(), 50.0f));
	VRW_CHECK(near(policy.resumeSecondsFor(0), 120.0f));
}

// Scene switches never call into the policy — the only writer of resume state
// is notePlaybackExit(). Time passing, position updates and hold queries
// while the same media stays selected must not touch saved positions.
static void test_resume_only_changes_on_exit() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(2), defaultConfig(), identityRandom());
	ClipSim sim{policy, {720.0f, 8.0f}};
	sim.activate(0, MediaSelectionOrigin::Startup);
	sim.run(90.0f);
	sim.activate(1, MediaSelectionOrigin::Automatic);
	const float saved = policy.resumeSecondsFor(0);
	sim.run(25.0f); // e.g. Blob -> Temporal -> Blob while clip 1 stays selected
	(void)policy.isHoldComplete();
	(void)policy.holdProgress();
	VRW_CHECK(policy.resumeSecondsFor(0) == saved);
	VRW_CHECK(near(saved, 90.0f));
}

static void test_reselection_resumes_and_history_unchanged() {
	// Same shuffle/selection sequence with or without the dwell calls.
	VideoSelectionPolicy plain;
	VideoSelectionPolicy withResume;
	plain.reset(makeCatalog(4), defaultConfig(), identityRandom());
	withResume.reset(makeCatalog(4), defaultConfig(), identityRandom());
	ClipSim sim{withResume, {720.0f, 50.0f, 8.0f, 400.0f}};

	auto first = plain.requestInitialCandidate();
	auto firstR = withResume.requestInitialCandidate();
	VRW_CHECK(first == firstR);
	plain.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);
	sim.activate(*firstR, MediaSelectionOrigin::Startup);
	for (int i = 0; i < 6; ++i) {
		sim.runUntilHold();
		auto a = plain.requestAutomaticAdvanceCandidate();
		auto b = withResume.requestAutomaticAdvanceCandidate();
		VRW_CHECK(a == b);
		plain.confirmActivationSucceeded(*a, MediaSelectionOrigin::Automatic);
		sim.activate(*b, MediaSelectionOrigin::Automatic);
		VRW_CHECK(plain.canSelectPrevious() == withResume.canSelectPrevious());
		if (withResume.activeClipClass() == Clip::Long && withResume.resumeSecondsFor(*b) > 0.0f) {
			VRW_CHECK(near(*withResume.activeWindowStartSeconds(), withResume.resumeSecondsFor(*b)));
		}
	}
}

static void test_unknown_duration_falls_back() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(2), defaultConfig(), identityRandom());
	ClipSim sim{policy, {720.0f, 0.0f}};
	sim.activate(0, MediaSelectionOrigin::Startup);
	sim.run(70.0f);
	sim.activate(1, MediaSelectionOrigin::Automatic); // decoder never reports a duration
	VRW_CHECK(!policy.playbackStarted());
	VRW_CHECK(policy.beginPlayback(0.0f) == 0.0f);  // no blind seek
	VRW_CHECK(!policy.playbackStarted());
	VRW_CHECK(near(sim.runUntilHold(), 30.0f)); // plain hold
	sim.activate(0, MediaSelectionOrigin::Automatic);
	VRW_CHECK(policy.resumeSecondsFor(1) == 0.0f);
	VRW_CHECK(near(*policy.activeWindowStartSeconds(), 70.0f)); // clip 0's progress untouched

	// Duration arriving late is applied once; a second call never re-seeks.
	VideoSelectionPolicy late;
	late.reset(makeCatalog(1), defaultConfig(), identityRandom());
	late.confirmActivationSucceeded(0, MediaSelectionOrigin::Startup);
	VRW_CHECK(late.beginPlayback(-1.0f) == 0.0f);
	VRW_CHECK(late.beginPlayback(720.0f) == 0.0f);
	VRW_CHECK(late.playbackStarted());
	VRW_CHECK(late.beginPlayback(999.0f) == 0.0f && late.activeClipClass() == Clip::Long);
}

static void test_seek_failure_falls_back_to_zero() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(2), defaultConfig(), identityRandom());
	ClipSim sim{policy, {720.0f, 8.0f}};
	sim.activate(0, MediaSelectionOrigin::Startup);
	sim.runUntilHold();
	sim.activate(1, MediaSelectionOrigin::Automatic);
	sim.runUntilHold();
	sim.activate(0, MediaSelectionOrigin::Automatic, /*seekFails=*/true);
	VRW_CHECK(policy.activeClipClass() == Clip::Long);
	VRW_CHECK(*policy.activeWindowStartSeconds() == 0.0f); // played from 0 instead
	VRW_CHECK(near(sim.runUntilHold(), 120.0f));            // and continued normally
	sim.activate(1, MediaSelectionOrigin::Automatic);
	VRW_CHECK(near(policy.resumeSecondsFor(0), 120.0f));    // progress = where it really was
}

int main() {
	test_classification();
	test_short_clip_loops_until_min_dwell();
	test_medium_clip_plays_once();
	test_long_clip_progressive_windows();
	test_long_clip_wraps_at_end();
	test_manual_next_and_previous_preserve_progress();
	test_resume_only_changes_on_exit();
	test_reselection_resumes_and_history_unchanged();
	test_unknown_duration_falls_back();
	test_seek_failure_falls_back_to_zero();

	if (g_failures == 0) {
		std::cout << g_total << "/" << g_total << " checks passed\n";
		return 0;
	}
	std::cout << g_failures << " of " << g_total << " checks FAILED\n";
	return 1;
}
