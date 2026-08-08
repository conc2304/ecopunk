#pragma once

#include "MediaMetadata.h"
#include "VideoPlaybackStatus.h"

#include <functional>
#include <optional>
#include <vector>

// ============================================================================
// VideoSelectionPolicy.h — the pure playlist/history/hold-timer/no-repeat
// decision logic behind VideoPlaybackService, deliberately factored out so
// it has ZERO dependency on openFrameworks, a real decoder, or wall-clock
// time. See test/video_selection_policy_tests.cpp for the standalone test
// suite this makes possible, following the same "dependency-free
// standalone test" convention as
// sketches/blob-region-prototype/test/videoregion_math_tests.cpp.
//
// This class answers "which catalog item should be tried next" and "what
// happened after that attempt was confirmed" — it never touches a decoder,
// a file, or a texture. VideoPlaybackService.cpp owns the decoder I/O and
// the retry-on-failure loop; this class only tracks the CONSEQUENCES of a
// successful activation.
//
// -- Two-phase protocol -----------------------------------------------------
//
// Every selection goes through two calls:
//
//   1. request*() — read-only. Returns which catalog index to try next, or
//      nullopt if that operation is not currently available (e.g.
//      requestManualPreviousCandidate() with no earlier history). Does NOT
//      mutate history, the hold clock, or the active item.
//
//   2. confirmActivationSucceeded(index, origin) — the ONLY mutating call.
//      The caller (VideoPlaybackService) invokes this once its decoder has
//      actually confirmed the item loaded successfully. A failed load
//      attempt should simply not be confirmed — the caller instead calls
//      requestRecoveryCandidate() to try another item. This is what
//      guarantees "a failed candidate should not become the active history
//      entry" (Shared-Video-Playback-HUD-Semantic-Slot-Review.md §4) and
//      "the hold clock starts on Ready, not on load start" (§3) — nothing
//      is committed until confirmation.
//
// -- Session history model ---------------------------------------------------
//
// history_ is an append-only (except for one deliberate truncation case,
// below) sequence of catalog indices, in the order they were successfully
// activated. cursor_ always points at the currently-active entry within
// history_.
//
//   - Startup/Automatic/Recovery/a "fresh" ManualNext (no forward history
//     to replay) all APPEND: any stale forward ("redo") entries beyond
//     cursor_ are discarded first, then the new index is appended and
//     cursor_ moves to point at it. This mirrors ordinary
//     browser-history/undo-redo semantics: taking a new forward step after
//     having stepped back clears the old "future."
//   - ManualPrevious REPLAYS: cursor_ decrements to the existing history
//     entry immediately behind it. Nothing is appended.
//   - ManualNext REPLAYS the existing entry immediately ahead of cursor_ if
//     one exists (the user is walking forward through history they
//     previously walked back through); otherwise it behaves exactly like
//     the "fresh append" case above.
//
// See Shared-Video-Playback-HUD-Semantic-Slot-Review.md §4's "Proposed
// navigation semantics" and Implement-Shared-Video-Playback-System-Agent-
// Prompt.md §4.2/§9 — both describe this behavior narratively; this class
// implements it exactly.
// ============================================================================

class VideoSelectionPolicy {
public:
	struct Config {
		bool automaticAdvanceEnabled = true;
		float holdDurationSeconds = 30.0f;
	};

	// Injectable so tests can force a specific shuffle order — "use an
	// injectable deterministic random seed or selection strategy for
	// tests" (implementation prompt §14.2). Must return a value in
	// [0, exclusiveUpperBound).
	using RandomIndexFn = std::function<size_t(size_t exclusiveUpperBound)>;

	// catalog: the FULL catalog (VideoSelectionPolicy filters to
	// entry.enabled itself — "playable-item filtering" per §2.1 — piSafe is
	// carried as metadata only; this v1 implementation does not
	// dynamically gate on it, see this increment's implementation report
	// for why that's flagged as an open question rather than decided
	// here).
	void reset(std::vector<MediaMetadata> catalog, Config config, RandomIndexFn randomFn = nullptr);

	// ---- Candidate requests — read-only, never mutate state below ----
	std::optional<int> requestInitialCandidate();
	std::optional<int> requestManualPreviousCandidate() const;
	std::optional<int> requestManualNextCandidate();
	std::optional<int> requestAutomaticAdvanceCandidate();
	// alreadyTried: catalog indices already attempted this recovery pass
	// (including the original first attempt) — skipped when picking a
	// fallback. Returns nullopt once every playable item has been tried
	// ("never loop forever through invalid media in one update", §12).
	std::optional<int> requestRecoveryCandidate(const std::vector<int>& alreadyTried);

	// ---- The only mutating call ----
	void confirmActivationSucceeded(int catalogIndex, MediaSelectionOrigin origin);

	// ---- Hold timer ----
	// No-op if automatic advance is disabled or nothing is active yet.
	void updateHold(float dt);
	bool isHoldComplete() const;

	// ---- Status ----
	bool hasActiveItem() const { return activeIndex_.has_value(); }
	int activeCatalogIndex() const { return activeIndex_.value_or(-1); }
	MediaSelectionOrigin activeSelectionOrigin() const { return activeOrigin_; }

	bool canSelectPrevious() const;
	bool canSelectNext() const;

	std::optional<float> holdProgress() const;
	std::optional<float> holdElapsedSeconds() const;
	std::optional<float> holdDurationSeconds() const;
	std::optional<float> holdRemainingSeconds() const;

	size_t playableCount() const { return playableIndices_.size(); }
	bool isPlayable(int catalogIndex) const;

private:
	std::optional<int> nextFreshPick(int excludeIndex);
	void reshuffle();

	std::vector<MediaMetadata> catalog_;
	Config config_{};
	RandomIndexFn randomFn_;

	std::vector<int> playableIndices_;  // catalog indices where enabled == true
	std::vector<int> shuffleOrder_;     // permutation of playableIndices_
	size_t shufflePos_ = 0;

	std::vector<int> history_;  // successfully-activated catalog indices, chronological
	int cursor_ = -1;           // index into history_; -1 = nothing activated yet

	std::optional<int> activeIndex_;
	MediaSelectionOrigin activeOrigin_ = MediaSelectionOrigin::Startup;

	float holdElapsedSeconds_ = 0.0f;
};
