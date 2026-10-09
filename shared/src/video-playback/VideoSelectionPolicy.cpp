#include "VideoSelectionPolicy.h"

#include <algorithm>
#include <memory>
#include <random>

namespace {

// Fisher-Yates, same idiom already used throughout this repo (e.g.
// shared/src/TimeOffsetVideoBuffer.cpp::shuffleMediaFiles(),
// sketches/quadrant-crosshair/src/VideoSystem.cpp::buildPlaylist()) —
// matched here deliberately rather than reinvented, per
// docs/video-playback-ownership-probe-report.md §D's "Playlist shuffle"
// row ("Supported by current code").
void fisherYatesShuffle(std::vector<int>& values, const VideoSelectionPolicy::RandomIndexFn& randomFn) {
	for (int i = static_cast<int>(values.size()) - 1; i > 0; i--) {
		size_t j = randomFn(static_cast<size_t>(i) + 1);
		std::swap(values[i], values[static_cast<int>(j)]);
	}
}

} // namespace

void VideoSelectionPolicy::reset(std::vector<MediaMetadata> catalog, Config config, RandomIndexFn randomFn) {
	catalog_ = std::move(catalog);
	config_ = config;

	if (randomFn) {
		randomFn_ = std::move(randomFn);
	} else {
		// Real (non-test) default: a per-instance RNG, not a shared global
		// — reset() may be called more than once (e.g. VideoPlaybackService
		// re-setup after shutdown), and each instance's shuffle order
		// should not depend on any other policy instance's call history.
		auto rng = std::make_shared<std::mt19937>(std::random_device{}());
		randomFn_ = [rng](size_t exclusiveUpperBound) -> size_t {
			if (exclusiveUpperBound == 0) return 0;
			std::uniform_int_distribution<size_t> dist(0, exclusiveUpperBound - 1);
			return dist(*rng);
		};
	}

	playableIndices_.clear();
	for (int i = 0; i < static_cast<int>(catalog_.size()); i++) {
		if (catalog_[static_cast<size_t>(i)].enabled) playableIndices_.push_back(i);
	}

	shuffleOrder_ = playableIndices_;
	fisherYatesShuffle(shuffleOrder_, randomFn_);
	shufflePos_ = 0;

	history_.clear();
	cursor_ = -1;
	activeIndex_.reset();
	activeOrigin_ = MediaSelectionOrigin::Startup;
	holdElapsedSeconds_ = 0.0f;

	resumeSeconds_.assign(catalog_.size(), 0.0f);
	resetVisit();
}

bool VideoSelectionPolicy::isPlayable(int catalogIndex) const {
	if (catalogIndex < 0 || catalogIndex >= static_cast<int>(catalog_.size())) return false;
	return catalog_[static_cast<size_t>(catalogIndex)].enabled;
}

void VideoSelectionPolicy::reshuffle() {
	int justActive = activeIndex_.value_or(-1);
	shuffleOrder_ = playableIndices_;
	fisherYatesShuffle(shuffleOrder_, randomFn_);
	// Anti-repeat-at-seam: don't let the item that was just active land
	// first in the freshly reshuffled order — same convention as
	// TimeOffsetVideoBuffer::advanceToNextMedia()'s lap-boundary swap.
	if (shuffleOrder_.size() > 1 && shuffleOrder_[0] == justActive) {
		std::swap(shuffleOrder_[0], shuffleOrder_[1]);
	}
	shufflePos_ = 0;
}

std::optional<int> VideoSelectionPolicy::nextFreshPick(int excludeIndex) {
	if (playableIndices_.empty()) return std::nullopt;
	if (playableIndices_.size() == 1) return playableIndices_[0];  // unavoidable repeat, see header comment

	if (shufflePos_ >= shuffleOrder_.size()) reshuffle();
	int candidate = shuffleOrder_[shufflePos_];
	shufflePos_++;
	if (shufflePos_ >= shuffleOrder_.size()) reshuffle();

	if (candidate == excludeIndex) {
		// Pull the next one instead of returning an immediate repeat.
		// `candidate` itself is not lost — it stays in shuffleOrder_ at its
		// (already-passed) position and will simply come up again on a
		// future lap, same as any other item.
		if (shufflePos_ >= shuffleOrder_.size()) reshuffle();
		int candidate2 = shuffleOrder_[shufflePos_];
		shufflePos_++;
		if (shufflePos_ >= shuffleOrder_.size()) reshuffle();
		return candidate2;
	}
	return candidate;
}

std::optional<int> VideoSelectionPolicy::requestInitialCandidate() {
	return nextFreshPick(-1);
}

std::optional<int> VideoSelectionPolicy::requestManualPreviousCandidate() const {
	if (cursor_ <= 0) return std::nullopt;
	return history_[static_cast<size_t>(cursor_ - 1)];
}

std::optional<int> VideoSelectionPolicy::requestManualNextCandidate() {
	if (cursor_ + 1 < static_cast<int>(history_.size())) {
		return history_[static_cast<size_t>(cursor_ + 1)];  // replay — no shuffle advance
	}
	return nextFreshPick(activeIndex_.value_or(-1));
}

std::optional<int> VideoSelectionPolicy::requestAutomaticAdvanceCandidate() {
	// Automatic advance always moves the frontier forward — it never
	// replays history, even if the user had previously navigated
	// backward. See this class's header comment.
	return nextFreshPick(activeIndex_.value_or(-1));
}

std::optional<int> VideoSelectionPolicy::requestRecoveryCandidate(const std::vector<int>& alreadyTried) {
	if (playableIndices_.size() <= alreadyTried.size()) return std::nullopt;  // every playable item exhausted
	// Bounded by playableIndices_.size() attempts — "never loop forever
	// through invalid media in one update" (§12). Pool sizes here are
	// dozens of clips at most (Pi-conscious per §13), so a linear
	// membership check per attempt is intentionally simple, not a
	// per-frame cost (this only runs during a failed-load recovery pass).
	for (size_t attempt = 0; attempt < playableIndices_.size(); attempt++) {
		std::optional<int> candidate = nextFreshPick(activeIndex_.value_or(-1));
		if (!candidate) return std::nullopt;
		if (std::find(alreadyTried.begin(), alreadyTried.end(), *candidate) == alreadyTried.end()) {
			return candidate;
		}
	}
	return std::nullopt;
}

void VideoSelectionPolicy::confirmActivationSucceeded(int catalogIndex, MediaSelectionOrigin origin) {
	if (origin == MediaSelectionOrigin::ManualPrevious) {
		// Precondition: caller only confirms with this origin after a
		// successful requestManualPreviousCandidate() result — replay,
		// cursor moves back onto the existing history entry.
		cursor_ = cursor_ - 1;
	} else if (origin == MediaSelectionOrigin::ManualNext
		&& cursor_ + 1 < static_cast<int>(history_.size())
		&& history_[static_cast<size_t>(cursor_ + 1)] == catalogIndex) {
		// Replaying an existing forward history entry.
		cursor_ = cursor_ + 1;
	} else {
		// Fresh append: Startup, Automatic, Recovery, or a ManualNext that
		// picked a brand-new item (no forward history to replay). Discard
		// any stale "redo" entries beyond the current cursor first.
		history_.resize(static_cast<size_t>(cursor_ + 1));
		history_.push_back(catalogIndex);
		cursor_ = static_cast<int>(history_.size()) - 1;
	}

	activeIndex_ = catalogIndex;
	activeOrigin_ = origin;
	holdElapsedSeconds_ = 0.0f;  // every successful activation resets the hold clock
	resetVisit();                // classification waits for beginPlayback()
}

void VideoSelectionPolicy::resetVisit() {
	activeClass_ = ClipClass::Unknown;
	activeDurationSeconds_ = 0.0f;
	windowStartSeconds_ = 0.0f;
	lastPositionSeconds_ = -1.0f;
	completedThisVisit_ = false;
}

float VideoSelectionPolicy::longClipThreshold() const {
	return config_.longClipThresholdSeconds > 0.0f ? config_.longClipThresholdSeconds : 2.0f * config_.maxDwellSeconds;
}

VideoSelectionPolicy::ClipClass VideoSelectionPolicy::classify(float durationSeconds) const {
	if (durationSeconds <= 0.0f) return ClipClass::Unknown;
	if (durationSeconds < config_.holdDurationSeconds) return ClipClass::Short;
	if (durationSeconds <= longClipThreshold()) return ClipClass::Medium;
	return ClipClass::Long;
}

float VideoSelectionPolicy::beginPlayback(float durationSeconds) {
	if (!activeIndex_.has_value() || playbackStarted()) return windowStartSeconds_;
	const ClipClass cls = classify(durationSeconds);
	if (cls == ClipClass::Unknown) return 0.0f; // caller retries once a duration is known

	activeClass_ = cls;
	activeDurationSeconds_ = durationSeconds;
	windowStartSeconds_ = 0.0f;
	if (cls == ClipClass::Long) {
		float resume = resumeSeconds_[static_cast<size_t>(*activeIndex_)];
		// Same wrap rule as notePlaybackExit(): never start inside the tail.
		if (resume < 0.0f || resume >= durationSeconds || durationSeconds - resume < config_.holdDurationSeconds) {
			resume = 0.0f;
		}
		windowStartSeconds_ = resume;
	}
	return windowStartSeconds_;
}

void VideoSelectionPolicy::notePlaybackStartFailed() {
	windowStartSeconds_ = 0.0f;
}

void VideoSelectionPolicy::notePlaybackPosition(float positionSeconds) {
	if (!playbackStarted() || positionSeconds < 0.0f) return;
	// A natural end shows up as the looping decoder jumping from the last
	// ~1.5 s of the clip back toward its start. Requiring the previous sample
	// to be near the end keeps a resume seek (or an async seek settling) from
	// ever reading as a completion.
	constexpr float kEndZoneSeconds = 1.5f;
	if (lastPositionSeconds_ >= activeDurationSeconds_ - kEndZoneSeconds && positionSeconds < lastPositionSeconds_ - 1.0f) {
		completedThisVisit_ = true;
	}
	lastPositionSeconds_ = positionSeconds;
}

void VideoSelectionPolicy::notePlaybackExit(std::optional<float> positionSeconds) {
	if (!activeIndex_.has_value()) return;
	float& saved = resumeSeconds_[static_cast<size_t>(*activeIndex_)];
	if (activeClass_ != ClipClass::Long) {
		// Short/medium clips always start at 0. Unknown: never classified
		// this visit (no duration) -> leave any earlier value untouched.
		if (activeClass_ != ClipClass::Unknown) saved = 0.0f;
		return;
	}
	if (completedThisVisit_) {
		saved = 0.0f; // reached the end this visit: next visit wraps to the opening
		return;
	}
	if (!positionSeconds.has_value() || *positionSeconds < 0.0f) return;
	float resume = *positionSeconds;
	if (resume >= activeDurationSeconds_ || activeDurationSeconds_ - resume < config_.holdDurationSeconds) {
		resume = 0.0f; // too little left for a meaningful window
	}
	saved = resume;
}

std::optional<float> VideoSelectionPolicy::activeWindowStartSeconds() const {
	if (!playbackStarted()) return std::nullopt;
	return windowStartSeconds_;
}

float VideoSelectionPolicy::resumeSecondsFor(int catalogIndex) const {
	if (catalogIndex < 0 || catalogIndex >= static_cast<int>(resumeSeconds_.size())) return 0.0f;
	return resumeSeconds_[static_cast<size_t>(catalogIndex)];
}

float VideoSelectionPolicy::holdTargetSeconds() const {
	const float minDwell = config_.holdDurationSeconds;
	switch (activeClass_) {
		case ClipClass::Unknown:
		case ClipClass::Short:
			return minDwell;
		case ClipClass::Medium:
			return std::max(minDwell, activeDurationSeconds_);
		case ClipClass::Long:
			return std::max(minDwell, std::min(config_.maxDwellSeconds, activeDurationSeconds_ - windowStartSeconds_));
	}
	return minDwell;
}

void VideoSelectionPolicy::setAutomaticAdvanceEnabled(bool enabled) {
	if (enabled && !config_.automaticAdvanceEnabled) holdElapsedSeconds_ = 0.0f;
	config_.automaticAdvanceEnabled = enabled;
}

void VideoSelectionPolicy::updateHold(float dt) {
	if (!config_.automaticAdvanceEnabled) return;
	if (!activeIndex_.has_value()) return;
	if (dt > 0.0f) holdElapsedSeconds_ += dt;
}

bool VideoSelectionPolicy::isHoldComplete() const {
	if (!config_.automaticAdvanceEnabled) return false;
	if (!activeIndex_.has_value()) return false;
	const float minDwell = config_.holdDurationSeconds;
	switch (activeClass_) {
		case ClipClass::Unknown:
		case ClipClass::Short:
			return holdElapsedSeconds_ >= minDwell;
		case ClipClass::Medium: {
			// One full playback, then min dwell. The small slack is a safety
			// net in case a wrap sample is ever missed.
			constexpr float kCompletionSlackSeconds = 2.0f;
			if (completedThisVisit_ && holdElapsedSeconds_ >= minDwell) return true;
			return holdElapsedSeconds_ >= std::max(minDwell, activeDurationSeconds_) + kCompletionSlackSeconds;
		}
		case ClipClass::Long:
			if (completedThisVisit_ && holdElapsedSeconds_ >= minDwell) return true;
			return holdElapsedSeconds_ >= config_.maxDwellSeconds;
	}
	return holdElapsedSeconds_ >= minDwell;
}

bool VideoSelectionPolicy::canSelectPrevious() const {
	return cursor_ > 0;
}

bool VideoSelectionPolicy::canSelectNext() const {
	bool hasForwardHistory = (cursor_ + 1) < static_cast<int>(history_.size());
	bool hasAlternateItem = playableIndices_.size() > 1;
	return hasForwardHistory || hasAlternateItem;
}

std::optional<float> VideoSelectionPolicy::holdProgress() const {
	if (!config_.automaticAdvanceEnabled || !activeIndex_.has_value()) return std::nullopt;
	const float target = holdTargetSeconds();
	if (target <= 0.0f) return 1.0f;
	float raw = holdElapsedSeconds_ / target;
	return std::clamp(raw, 0.0f, 1.0f);
}

std::optional<float> VideoSelectionPolicy::holdElapsedSeconds() const {
	if (!config_.automaticAdvanceEnabled || !activeIndex_.has_value()) return std::nullopt;
	return holdElapsedSeconds_;
}

std::optional<float> VideoSelectionPolicy::holdDurationSeconds() const {
	if (!config_.automaticAdvanceEnabled || !activeIndex_.has_value()) return std::nullopt;
	return holdTargetSeconds();
}

std::optional<float> VideoSelectionPolicy::holdRemainingSeconds() const {
	if (!config_.automaticAdvanceEnabled || !activeIndex_.has_value()) return std::nullopt;
	return std::max(0.0f, holdTargetSeconds() - holdElapsedSeconds_);
}
