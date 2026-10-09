// video_advance_mode_tests.cpp — VideoSelectionPolicy's runtime cadence switch
// (setAutomaticAdvanceEnabled): timer-driven automatic advance vs a caller that
// drives media changes with next() at its own boundaries (Blueprint's
// composition cycle). Dependency-free, same convention as the other suites.

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

VideoSelectionPolicy::Config defaultConfig() {
	VideoSelectionPolicy::Config c;
	c.holdDurationSeconds = 30.0f;
	c.maxDwellSeconds = 120.0f;
	return c;
}

bool near(float a, float b, float tol = 0.5f) { return std::fabs(a - b) <= tol; }

// Mirrors VideoPlaybackService: exit save, confirm, begin playback, seek.
void activate(VideoSelectionPolicy& p, int index, MediaSelectionOrigin origin, float duration, float& pos) {
	if (p.hasActiveItem()) p.notePlaybackExit(pos);
	p.confirmActivationSucceeded(index, origin);
	pos = p.beginPlayback(duration);
}

void advanceTime(VideoSelectionPolicy& p, float seconds, float& pos, float duration) {
	const float dt = 0.05f;
	for (float t = 0.0f; t < seconds - 1e-4f; t += dt) {
		p.updateHold(dt);
		pos += dt;
		if (pos >= duration) pos -= duration;
		p.notePlaybackPosition(pos);
	}
}

} // namespace

#define VAM_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

static void test_default_enabled_unchanged() {
	VideoSelectionPolicy p;
	p.reset(makeCatalog(3), defaultConfig(), identityRandom());
	VAM_CHECK(p.automaticAdvanceEnabled());
	float pos = 0.0f;
	activate(p, 0, MediaSelectionOrigin::Startup, 8.0f, pos);
	advanceTime(p, 29.0f, pos, 8.0f);
	VAM_CHECK(!p.isHoldComplete());
	advanceTime(p, 1.5f, pos, 8.0f);
	VAM_CHECK(p.isHoldComplete()); // short clip: min dwell, as before
}

static void test_disabled_timer_never_advances() {
	VideoSelectionPolicy p;
	p.reset(makeCatalog(3), defaultConfig(), identityRandom());
	float pos = 0.0f;
	activate(p, 0, MediaSelectionOrigin::Startup, 8.0f, pos);
	p.setAutomaticAdvanceEnabled(false);
	advanceTime(p, 600.0f, pos, 8.0f);
	VAM_CHECK(!p.isHoldComplete());
	VAM_CHECK(!p.holdElapsedSeconds().has_value()); // no hold reported while boundary-driven
	VAM_CHECK(p.activeCatalogIndex() == 0);
}

static void test_next_and_previous_while_disabled() {
	VideoSelectionPolicy p;
	p.reset(makeCatalog(4), defaultConfig(), identityRandom());
	float pos = 0.0f;
	auto first = p.requestInitialCandidate();
	activate(p, *first, MediaSelectionOrigin::Startup, 45.0f, pos);
	p.setAutomaticAdvanceEnabled(false);

	// Boundary advance (VideoPlaybackService::advanceAtBoundary()): the
	// automatic candidate, confirmed as Automatic — not gated by the timer.
	auto next = p.requestAutomaticAdvanceCandidate();
	VAM_CHECK(next.has_value() && *next != *first);
	activate(p, *next, MediaSelectionOrigin::Automatic, 45.0f, pos);
	VAM_CHECK(p.activeCatalogIndex() == *next);
	VAM_CHECK(p.activeSelectionOrigin() == MediaSelectionOrigin::Automatic);
	VAM_CHECK(p.canSelectPrevious());

	// A user's Next while boundary-driven is immediate and ordinary.
	auto manual = p.requestManualNextCandidate();
	VAM_CHECK(manual.has_value() && *manual != *next);
	activate(p, *manual, MediaSelectionOrigin::ManualNext, 45.0f, pos);
	VAM_CHECK(p.activeCatalogIndex() == *manual);
	auto back = p.requestManualPreviousCandidate();
	VAM_CHECK(back.has_value() && *back == *next);
	activate(p, *back, MediaSelectionOrigin::ManualPrevious, 45.0f, pos);

	auto prev = p.requestManualPreviousCandidate();
	VAM_CHECK(prev.has_value() && *prev == *first);
	activate(p, *prev, MediaSelectionOrigin::ManualPrevious, 45.0f, pos);
	VAM_CHECK(p.activeCatalogIndex() == *first);
	VAM_CHECK(p.canSelectNext());
}

static void test_resume_windows_while_disabled() {
	VideoSelectionPolicy p;
	p.reset(makeCatalog(2), defaultConfig(), identityRandom());
	const float durations[] = {720.0f, 20.0f};
	float pos = 0.0f;
	activate(p, 0, MediaSelectionOrigin::Startup, durations[0], pos);
	p.setAutomaticAdvanceEnabled(false);
	advanceTime(p, 150.0f, pos, durations[0]); // a Blueprint cycle longer than max dwell
	VAM_CHECK(!p.isHoldComplete());
	activate(p, 1, MediaSelectionOrigin::Automatic, durations[1], pos); // cycle boundary next()
	VAM_CHECK(near(p.resumeSecondsFor(0), 150.0f)); // saved once, at the real exit playhead
	advanceTime(p, 100.0f, pos, durations[1]);
	activate(p, 0, MediaSelectionOrigin::Automatic, durations[0], pos);
	VAM_CHECK(near(*p.activeWindowStartSeconds(), 150.0f)); // resumes where it left off
	VAM_CHECK(near(pos, 150.0f));
}

static void test_restore_starts_fresh_window() {
	VideoSelectionPolicy p;
	p.reset(makeCatalog(3), defaultConfig(), identityRandom());
	float pos = 0.0f;
	activate(p, 0, MediaSelectionOrigin::Startup, 8.0f, pos);
	advanceTime(p, 25.0f, pos, 8.0f);             // 25 s of a 30 s dwell already used
	p.setAutomaticAdvanceEnabled(false);          // Blueprint active ...
	advanceTime(p, 120.0f, pos, 8.0f);            // ... for two minutes
	p.setAutomaticAdvanceEnabled(true);           // back to Blob
	VAM_CHECK(!p.isHoldComplete());               // no overdue immediate advance
	VAM_CHECK(near(*p.holdElapsedSeconds(), 0.0f));
	advanceTime(p, 29.0f, pos, 8.0f);
	VAM_CHECK(!p.isHoldComplete());
	advanceTime(p, 1.5f, pos, 8.0f);
	VAM_CHECK(p.isHoldComplete());                // a full fresh dwell window

	// Enabling while already enabled does not restart the window.
	VideoSelectionPolicy q;
	q.reset(makeCatalog(2), defaultConfig(), identityRandom());
	float qpos = 0.0f;
	activate(q, 0, MediaSelectionOrigin::Startup, 8.0f, qpos);
	advanceTime(q, 20.0f, qpos, 8.0f);
	q.setAutomaticAdvanceEnabled(true);
	VAM_CHECK(near(*q.holdElapsedSeconds(), 20.0f));
}

int main() {
	test_default_enabled_unchanged();
	test_disabled_timer_never_advances();
	test_next_and_previous_while_disabled();
	test_resume_windows_while_disabled();
	test_restore_starts_fresh_window();

	if (g_failures == 0) {
		std::cout << g_total << "/" << g_total << " checks passed\n";
		return 0;
	}
	std::cout << g_failures << " of " << g_total << " checks FAILED\n";
	return 1;
}
