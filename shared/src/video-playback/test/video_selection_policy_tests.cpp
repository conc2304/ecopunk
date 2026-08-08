// Standalone, dependency-free tests for VideoSelectionPolicy — pure
// playlist/history/hold-timer/no-repeat decision logic, no openFrameworks,
// no decoder, no wall-clock. Same convention as media_catalog_tests.cpp
// (this directory) and sketches/experience_runtime/test/
// lifecycle_state_tests.cpp.
//
// Covers Implement-Shared-Video-Playback-System-Agent-Prompt.md §14.2's
// selection test list (startup selection, automatic next after hold, no
// immediate repeat, manual next, manual previous, backward-then-forward
// history traversal, one-item navigation disabled, failed candidates
// excluded from successful history) and §14.3's hold test list (hold
// starts at Ready, loading time excluded, manual/automatic navigation
// resets hold, progress clamps to [0,1], remaining time never negative,
// disabled automatic advance yields absent hold fields), using an
// injectable deterministic random strategy per §14.2's own instruction.
//
// Build/run: make -C test -f Makefile.tests test

#include "../MediaMetadata.h"
#include "../VideoPlaybackStatus.h"
#include "../VideoSelectionPolicy.h"

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
		m.titleId = "media.title." + std::to_string(i);
		m.fallbackDisplayTitle = "Clip " + std::to_string(i);
		m.enabled = true;
		m.piSafe = true;
		catalog.push_back(m);
	}
	return catalog;
}

// Deterministic "random": always returns 0 (i.e. never swaps during
// Fisher-Yates -- shuffleOrder_ ends up equal to the identity permutation
// of playableIndices_ every time it's (re)built). Makes candidate order
// fully predictable for assertions below.
VideoSelectionPolicy::RandomIndexFn identityRandom() {
	return [](size_t) -> size_t { return 0; };
}

} // namespace

#define VSP_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

static void test_startup_selection() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(3), {}, identityRandom());

	VSP_CHECK(!policy.hasActiveItem());
	auto candidate = policy.requestInitialCandidate();
	VSP_CHECK(candidate.has_value());

	policy.confirmActivationSucceeded(*candidate, MediaSelectionOrigin::Startup);
	VSP_CHECK(policy.hasActiveItem());
	VSP_CHECK(policy.activeCatalogIndex() == *candidate);
	VSP_CHECK(policy.activeSelectionOrigin() == MediaSelectionOrigin::Startup);
}

static void test_no_immediate_repeat_on_automatic_advance() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(4), {}, identityRandom());

	auto first = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);

	for (int i = 0; i < 20; i++) {
		auto next = policy.requestAutomaticAdvanceCandidate();
		VSP_CHECK(next.has_value());
		VSP_CHECK(*next != policy.activeCatalogIndex());  // never repeats the just-active item
		policy.confirmActivationSucceeded(*next, MediaSelectionOrigin::Automatic);
	}
}

static void test_one_item_catalog_repeat_is_unavoidable_but_handled() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(1), {}, identityRandom());

	auto first = policy.requestInitialCandidate();
	VSP_CHECK(first.has_value());
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);

	auto next = policy.requestAutomaticAdvanceCandidate();
	VSP_CHECK(next.has_value());
	VSP_CHECK(*next == *first);  // only one item exists -- an unavoidable repeat, not a crash/nullopt
}

static void test_one_item_catalog_navigation_disabled() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(1), {}, identityRandom());
	auto first = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);

	VSP_CHECK(policy.canSelectPrevious() == false);
	VSP_CHECK(policy.canSelectNext() == false);
	VSP_CHECK(!policy.requestManualPreviousCandidate().has_value());
}

static void test_multi_item_navigation_enabled_after_startup() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(3), {}, identityRandom());
	auto first = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);

	VSP_CHECK(policy.canSelectPrevious() == false);  // nothing before the first item
	VSP_CHECK(policy.canSelectNext() == true);       // other items exist
}

static void test_manual_previous_replays_prior_history_entry() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(3), {}, identityRandom());

	auto a = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*a, MediaSelectionOrigin::Startup);

	auto b = policy.requestManualNextCandidate();
	VSP_CHECK(b.has_value());
	VSP_CHECK(*b != *a);
	policy.confirmActivationSucceeded(*b, MediaSelectionOrigin::ManualNext);
	VSP_CHECK(policy.activeCatalogIndex() == *b);
	VSP_CHECK(policy.canSelectPrevious());

	auto backTo = policy.requestManualPreviousCandidate();
	VSP_CHECK(backTo.has_value());
	VSP_CHECK(*backTo == *a);  // previous replays the exact prior entry, not a new random pick
	policy.confirmActivationSucceeded(*backTo, MediaSelectionOrigin::ManualPrevious);
	VSP_CHECK(policy.activeCatalogIndex() == *a);
	VSP_CHECK(policy.activeSelectionOrigin() == MediaSelectionOrigin::ManualPrevious);
	VSP_CHECK(!policy.canSelectPrevious());  // back at the very start of history
	VSP_CHECK(policy.canSelectNext());       // b is still ahead, replayable
}

static void test_backward_then_forward_history_traversal() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(3), {}, identityRandom());

	auto a = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*a, MediaSelectionOrigin::Startup);
	auto b = policy.requestManualNextCandidate();
	policy.confirmActivationSucceeded(*b, MediaSelectionOrigin::ManualNext);

	// Step back to a.
	auto backToA = policy.requestManualPreviousCandidate();
	VSP_CHECK(backToA.has_value() && *backToA == *a);
	policy.confirmActivationSucceeded(*backToA, MediaSelectionOrigin::ManualPrevious);

	// Step forward again -- must replay b, not pick a third random item.
	auto forwardToB = policy.requestManualNextCandidate();
	VSP_CHECK(forwardToB.has_value());
	VSP_CHECK(*forwardToB == *b);
	policy.confirmActivationSucceeded(*forwardToB, MediaSelectionOrigin::ManualNext);
	VSP_CHECK(policy.activeCatalogIndex() == *b);
	VSP_CHECK(policy.activeSelectionOrigin() == MediaSelectionOrigin::ManualNext);
}

static void test_fresh_forward_step_after_going_back_clears_stale_redo() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(4), {}, identityRandom());

	auto a = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*a, MediaSelectionOrigin::Startup);
	auto b = policy.requestManualNextCandidate();
	policy.confirmActivationSucceeded(*b, MediaSelectionOrigin::ManualNext);

	auto backToA = policy.requestManualPreviousCandidate();
	policy.confirmActivationSucceeded(*backToA, MediaSelectionOrigin::ManualPrevious);

	// Instead of stepping forward (which would replay b), let AUTOMATIC
	// advance fire from a -- this must discard the stale "b" redo entry
	// and append a brand-new forward entry instead.
	auto freshFromA = policy.requestAutomaticAdvanceCandidate();
	VSP_CHECK(freshFromA.has_value());
	policy.confirmActivationSucceeded(*freshFromA, MediaSelectionOrigin::Automatic);

	// Now stepping back must land on `a` again (the redo slot that used to
	// hold `b` is gone), not skip past it.
	auto backAgain = policy.requestManualPreviousCandidate();
	VSP_CHECK(backAgain.has_value());
	VSP_CHECK(*backAgain == *a);
}

static void test_failed_candidate_not_recorded_in_history() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(3), {}, identityRandom());

	auto first = policy.requestInitialCandidate();
	VSP_CHECK(first.has_value());
	// Simulate a failed load: never call confirmActivationSucceeded for
	// `first`. The caller (VideoPlaybackService) would instead ask for a
	// recovery candidate.
	std::vector<int> tried = {*first};
	auto recovery = policy.requestRecoveryCandidate(tried);
	VSP_CHECK(recovery.has_value());
	VSP_CHECK(*recovery != *first);

	policy.confirmActivationSucceeded(*recovery, MediaSelectionOrigin::Recovery);
	VSP_CHECK(policy.hasActiveItem());
	VSP_CHECK(policy.activeCatalogIndex() == *recovery);
	VSP_CHECK(policy.activeSelectionOrigin() == MediaSelectionOrigin::Recovery);
	// `first` (the failed candidate) must never have become the active
	// item, and canSelectPrevious() must reflect a history of exactly one
	// entry (the recovered one) -- not two.
	VSP_CHECK(!policy.canSelectPrevious());
}

static void test_recovery_candidate_exhausted_returns_nullopt() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(2), {}, identityRandom());
	auto first = policy.requestInitialCandidate();
	std::vector<int> tried = {0, 1};  // every playable item already tried
	auto recovery = policy.requestRecoveryCandidate(tried);
	VSP_CHECK(!recovery.has_value());
	(void)first;
}

static void test_hold_starts_at_zero_after_confirm() {
	VideoSelectionPolicy policy;
	VideoSelectionPolicy::Config cfg;
	cfg.automaticAdvanceEnabled = true;
	cfg.holdDurationSeconds = 10.0f;
	policy.reset(makeCatalog(2), cfg, identityRandom());

	auto first = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);
	VSP_CHECK(policy.holdElapsedSeconds().has_value());
	VSP_CHECK(*policy.holdElapsedSeconds() == 0.0f);
	VSP_CHECK(*policy.holdProgress() == 0.0f);
}

static void test_hold_progress_and_completion() {
	VideoSelectionPolicy policy;
	VideoSelectionPolicy::Config cfg;
	cfg.automaticAdvanceEnabled = true;
	cfg.holdDurationSeconds = 10.0f;
	policy.reset(makeCatalog(2), cfg, identityRandom());

	auto first = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);

	policy.updateHold(4.0f);
	VSP_CHECK(*policy.holdElapsedSeconds() == 4.0f);
	VSP_CHECK(*policy.holdProgress() == 0.4f);
	VSP_CHECK(*policy.holdRemainingSeconds() == 6.0f);
	VSP_CHECK(!policy.isHoldComplete());

	policy.updateHold(6.0f);
	VSP_CHECK(*policy.holdElapsedSeconds() == 10.0f);
	VSP_CHECK(policy.isHoldComplete());
	VSP_CHECK(*policy.holdRemainingSeconds() == 0.0f);
}

static void test_hold_progress_clamps_and_remaining_never_negative() {
	VideoSelectionPolicy policy;
	VideoSelectionPolicy::Config cfg;
	cfg.automaticAdvanceEnabled = true;
	cfg.holdDurationSeconds = 5.0f;
	policy.reset(makeCatalog(2), cfg, identityRandom());

	auto first = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);

	// Overshoot well past the hold duration in one update -- as would
	// happen if the caller was slow to react to isHoldComplete().
	policy.updateHold(500.0f);
	VSP_CHECK(*policy.holdProgress() == 1.0f);        // clamped, not 100.0
	VSP_CHECK(*policy.holdRemainingSeconds() == 0.0f); // never negative
}

static void test_manual_navigation_resets_hold() {
	VideoSelectionPolicy policy;
	VideoSelectionPolicy::Config cfg;
	cfg.automaticAdvanceEnabled = true;
	cfg.holdDurationSeconds = 10.0f;
	policy.reset(makeCatalog(3), cfg, identityRandom());

	auto first = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);
	policy.updateHold(7.0f);
	VSP_CHECK(*policy.holdElapsedSeconds() == 7.0f);

	auto second = policy.requestManualNextCandidate();
	policy.confirmActivationSucceeded(*second, MediaSelectionOrigin::ManualNext);
	VSP_CHECK(*policy.holdElapsedSeconds() == 0.0f);  // reset by the successful manual activation
}

static void test_automatic_advance_resets_hold_for_new_item() {
	VideoSelectionPolicy policy;
	VideoSelectionPolicy::Config cfg;
	cfg.automaticAdvanceEnabled = true;
	cfg.holdDurationSeconds = 5.0f;
	policy.reset(makeCatalog(3), cfg, identityRandom());

	auto first = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);
	policy.updateHold(5.0f);
	VSP_CHECK(policy.isHoldComplete());

	auto next = policy.requestAutomaticAdvanceCandidate();
	policy.confirmActivationSucceeded(*next, MediaSelectionOrigin::Automatic);
	VSP_CHECK(*policy.holdElapsedSeconds() == 0.0f);
	VSP_CHECK(!policy.isHoldComplete());
}

static void test_disabled_automatic_advance_yields_absent_hold_fields() {
	VideoSelectionPolicy policy;
	VideoSelectionPolicy::Config cfg;
	cfg.automaticAdvanceEnabled = false;
	cfg.holdDurationSeconds = 10.0f;
	policy.reset(makeCatalog(2), cfg, identityRandom());

	auto first = policy.requestInitialCandidate();
	policy.confirmActivationSucceeded(*first, MediaSelectionOrigin::Startup);
	policy.updateHold(100.0f);  // must be a no-op

	VSP_CHECK(!policy.holdProgress().has_value());
	VSP_CHECK(!policy.holdElapsedSeconds().has_value());
	VSP_CHECK(!policy.holdDurationSeconds().has_value());
	VSP_CHECK(!policy.holdRemainingSeconds().has_value());
	VSP_CHECK(!policy.isHoldComplete());
}

static void test_hold_absent_before_any_activation() {
	VideoSelectionPolicy policy;
	policy.reset(makeCatalog(2), {}, identityRandom());
	VSP_CHECK(!policy.holdProgress().has_value());
	VSP_CHECK(!policy.isHoldComplete());
}

static void test_disabled_catalog_entries_excluded_from_playable_pool() {
	std::vector<MediaMetadata> catalog = makeCatalog(3);
	catalog[1].enabled = false;

	VideoSelectionPolicy policy;
	policy.reset(catalog, {}, identityRandom());
	VSP_CHECK(policy.playableCount() == 2);
	VSP_CHECK(!policy.isPlayable(1));
	VSP_CHECK(policy.isPlayable(0));
	VSP_CHECK(policy.isPlayable(2));

	auto first = policy.requestInitialCandidate();
	VSP_CHECK(first.has_value());
	VSP_CHECK(*first != 1);  // the disabled entry must never be selected
}

static void test_empty_catalog_startup_candidate_is_absent() {
	VideoSelectionPolicy policy;
	policy.reset({}, {}, identityRandom());
	VSP_CHECK(!policy.requestInitialCandidate().has_value());
	VSP_CHECK(!policy.hasActiveItem());
	VSP_CHECK(!policy.canSelectPrevious());
	VSP_CHECK(!policy.canSelectNext());
}

int main() {
	test_startup_selection();
	test_no_immediate_repeat_on_automatic_advance();
	test_one_item_catalog_repeat_is_unavoidable_but_handled();
	test_one_item_catalog_navigation_disabled();
	test_multi_item_navigation_enabled_after_startup();
	test_manual_previous_replays_prior_history_entry();
	test_backward_then_forward_history_traversal();
	test_fresh_forward_step_after_going_back_clears_stale_redo();
	test_failed_candidate_not_recorded_in_history();
	test_recovery_candidate_exhausted_returns_nullopt();
	test_hold_starts_at_zero_after_confirm();
	test_hold_progress_and_completion();
	test_hold_progress_clamps_and_remaining_never_negative();
	test_manual_navigation_resets_hold();
	test_automatic_advance_resets_hold_for_new_item();
	test_disabled_automatic_advance_yields_absent_hold_fields();
	test_hold_absent_before_any_activation();
	test_disabled_catalog_entries_excluded_from_playable_pool();
	test_empty_catalog_startup_candidate_is_absent();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
