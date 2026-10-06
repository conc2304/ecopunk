#include "TFVideoAdapterSelfTest.h"
#include "ofFileUtils.h"
#include "ofLog.h"
#include "ofUtils.h"
#include <string>

namespace {

	int g_checks = 0;
	int g_failures = 0;

	void check(bool cond, const std::string & what) {
		g_checks++;
		if (!cond) {
			g_failures++;
			ofLogError("TFVideoAdapterSelfTest") << "FAIL: " << what;
		}
	}

	// Real-frame-arrival phases are bounded by wall-clock budget, not a
	// fixed frame count — this environment's per-frame interval varies
	// (window focus, other startup work still settling), and the point is
	// "give the real decoder a fair, generous amount of real time," not
	// "wait exactly N frames."
	constexpr float kFrameArrivalBudgetSeconds = 10.0f;
	constexpr float kHistoryFillBudgetSeconds = 10.0f;

} // namespace

void TFVideoAdapterSelfTest::begin() {
	g_checks = 0;
	g_failures = 0;
	stage_ = Stage::WaitForFirstFrame;
	stageElapsedSeconds_ = 0.0f;
	sawFrame_ = false;
	sawNonEmptyPixels_ = false;

	// ---- 1. Shared Video ordinary-service proof, real canonical root ----
	VideoPlaybackService::Config config;
	config.mediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	// Long enough that this test's own real-time pumping can never
	// accidentally trigger an automatic hold-timer advance mid-test — this
	// test drives next()/previous() explicitly and must not race the
	// service's own automatic selection.
	config.holdDurationSeconds = 9999.0f;
	config.automaticAdvance = true;

	bool setupOk = service_.setup(config);
	check(setupOk, "VideoPlaybackService::setup() against the real canonical root returns true (structurally successful)");

	VideoPlaybackStatus statusAfterStartup = service_.status();
	check(statusAfterStartup.health == VideoPlaybackHealth::Ready, "canonical root resolves a Ready item on startup (real files found and opened)");
	check(statusAfterStartup.mediaId.has_value(), "startup status carries a mediaId");
	check(statusAfterStartup.selectionOrigin == MediaSelectionOrigin::Startup, "startup selection reports Startup origin");
	if (statusAfterStartup.mediaId.has_value()) firstMediaId_ = *statusAfterStartup.mediaId;

	firstPath_ = service_.currentAbsolutePath();
	check(firstPath_.has_value() && !firstPath_->empty(), "currentAbsolutePath() resolves a non-empty path for the startup selection");
	if (firstPath_.has_value()) {
		check(ofFile(*firstPath_).exists(), "the resolved canonical-root path exists on disk (real file, not synthesized)");
	}

	glm::ivec2 sourceSize = service_.sourceSize();
	check(sourceSize.x > 0 && sourceSize.y > 0, "source dimensions are valid (>0 x >0) once Ready");
	ofLogNotice("TFVideoAdapterSelfTest") << "startup selection: " << (firstPath_.has_value() ? *firstPath_ : "?")
										   << " (" << sourceSize.x << "x" << sourceSize.y << ")";

	// Real frame arrival needs the actual oF run loop pumping between
	// calls (confirmed by this session's own investigation — a
	// synchronous sleep+poll loop from inside setup(), before the run
	// loop starts, never once saw a frame even given an 8s budget) — so
	// from here, tick() takes over once per real ofApp::update().
}

void TFVideoAdapterSelfTest::tick(float dt) {
	if (stage_ == Stage::Done) return;
	stageElapsedSeconds_ += dt;

	if (stage_ == Stage::WaitForFirstFrame) {
		service_.update(dt);
		if (service_.isFrameNew()) sawFrame_ = true;
		const ofPixels * px = service_.currentPixels();
		if (px != nullptr && px->isAllocated() && px->size() > 0) sawNonEmptyPixels_ = true;

		if ((sawFrame_ && sawNonEmptyPixels_) || stageElapsedSeconds_ >= kFrameArrivalBudgetSeconds) {
			check(sawFrame_, "at least one new frame arrives from the real decoder within a real-time run-loop budget");
			check(sawNonEmptyPixels_, "currentPixels() is non-empty at least once");

			const ofTexture * texture = service_.currentTexture();
			check(texture != nullptr && texture->isAllocated(), "currentTexture() is allocated in this GL-capable harness");

			VideoPlaybackStatus statusAfterFrames = service_.status();
			check(statusAfterFrames.durationSeconds.has_value() && *statusAfterFrames.durationSeconds > 0.0f,
				"durationSeconds is meaningful (present and >0) once frames have arrived");
			check(statusAfterFrames.positionSeconds.has_value(), "positionSeconds is meaningful (present) once frames have arrived");

			stage_ = Stage::AfterFirstFrame;
			stageElapsedSeconds_ = 0.0f;
		}
		return;
	}

	if (stage_ == Stage::AfterFirstFrame) {
		// ---- 2. next() selects another canonical item, identity changes coherently ----
		bool nextOk = service_.next();
		check(nextOk, "next() succeeds when the canonical root has multiple playable items");

		VideoPlaybackStatus statusAfterNext = service_.status();
		check(statusAfterNext.mediaId.has_value() && *statusAfterNext.mediaId != firstMediaId_,
			"next() changes the active media identity to a different mediaId");
		check(statusAfterNext.selectionOrigin == MediaSelectionOrigin::ManualNext, "next() reports ManualNext selection origin");
		if (statusAfterNext.mediaId.has_value()) secondMediaId_ = *statusAfterNext.mediaId;

		secondPath_ = service_.currentAbsolutePath();
		check(secondPath_.has_value() && secondPath_ != firstPath_, "currentAbsolutePath() changes after next() and differs from the first path");

		// ---- 3. Temporal specialized-decoder proof: explicit-load the exact shared-selected file ----
		TimeOffsetVideoBuffer::Settings bufferSettings;
		bufferSettings.numQuantizeBands = 12;
		bufferSettings.numPlayheads = 6;
		bufferSettings.maxHistorySeconds = 3.0f; // short on purpose, so the fill phase below can realistically complete
		adapter_.setup(bufferSettings);

		check(adapter_.reloadCount() == 0, "adapter has never reloaded before its first synchronizeSelectedMedia() call");

		bool firstSyncOk = secondPath_.has_value() && adapter_.synchronizeSelectedMedia(secondMediaId_, *secondPath_);
		check(firstSyncOk, "adapter's first synchronizeSelectedMedia() call, with a real shared-selected file, succeeds");
		check(adapter_.reloadCount() == 1, "exactly one reload happened for the first synchronize call");
		check(secondPath_.has_value() && adapter_.buffer().getCurrentMediaFilename() == *secondPath_,
			"the Temporal dedicated decoder loaded the EXACT absolute path VideoPlaybackService selected — "
			"no directory scan/shuffle picked a different file (see TimeOffsetVideoBuffer::loadExplicit()'s own "
			"source: it never reads mediaFiles/currentFileIndex or calls shuffleMediaFiles())");
		check(adapter_.buffer().getHistoryFrameCount() == 0, "history starts empty immediately after the load/reset");
		check(adapter_.buffer().getNumPlayheads() == 6, "all six playheads remain valid immediately after load");
		check(allocatedPlayheadCount() == 0,
			"TEMP-004 initial empty history: no playhead texture is presentable (all six unallocated) before the first decoded frame");

		// unchanged mediaId must not reload
		bool secondSyncOk = secondPath_.has_value() && adapter_.synchronizeSelectedMedia(secondMediaId_, *secondPath_);
		check(secondSyncOk == firstSyncOk, "re-synchronizing the SAME mediaId returns the same success state as the first call");
		check(adapter_.reloadCount() == 1, "an unchanged mediaId does not trigger a second reload");

		stage_ = Stage::WaitForHistoryFill;
		stageElapsedSeconds_ = 0.0f;
		return;
	}

	if (stage_ == Stage::WaitForHistoryFill) {
		adapter_.update(dt);
		bool historyFilled = adapter_.buffer().getHistoryFrameCount() > 0;

		if (historyFilled || stageElapsedSeconds_ >= kHistoryFillBudgetSeconds) {
			check(adapter_.buffer().getHistoryFrameCount() > 0, "history fills from the newly-loaded media after real decode time");
			bool anyPlayheadTextureAllocated = false;
			for (int i = 0; i < adapter_.buffer().getNumPlayheads(); ++i) {
				if (adapter_.buffer().getPlayheadTexture(i).isAllocated()) anyPlayheadTextureAllocated = true;
			}
			check(anyPlayheadTextureAllocated, "at least one playhead texture becomes allocated after history refill");
			preChangePlayheadHash_ = firstAllocatedPlayheadHash();

			stage_ = Stage::AfterHistoryFill;
			stageElapsedSeconds_ = 0.0f;
		}
		return;
	}

	if (stage_ == Stage::AfterHistoryFill) {
		finish(); // advances to WaitForRefillAfterReactivation
		return;
	}

	if (stage_ == Stage::WaitForRefillAfterReactivation) {
		adapter_.update(dt);
		bool refilled = adapter_.buffer().getHistoryFrameCount() > 0;
		if (refilled || stageElapsedSeconds_ >= kHistoryFillBudgetSeconds) {
			check(refilled, "TEMP-004 history refills from the current canonical media after the reactivation reload");
			check(allocatedPlayheadCount() > 0, "TEMP-004 playhead textures resume (re-allocate) from the refilled current-media history");
			uint64_t refilledHash = firstAllocatedPlayheadHash();
			check(refilledHash != 0 && refilledHash != preChangePlayheadHash_,
				"TEMP-004 refilled playhead content differs from the pre-change (previous media) playhead content");

			// Repetition phase: distinct per-playhead offsets so "existing
			// offsets remain correct" and "all six resume" are meaningful.
			for (int i = 0; i < adapter_.buffer().getNumPlayheads(); ++i) {
				adapter_.buffer().jumpPlayhead(i, static_cast<float>(i) / 5.0f);
			}
			repPlan_ = { RepEvent::MediaChange, RepEvent::Reactivation, RepEvent::Reactivation, RepEvent::MediaChange,
				RepEvent::Reactivation, RepEvent::Reactivation, RepEvent::MediaChange, RepEvent::Reactivation };
			repIndex_ = 0;
			stage_ = Stage::RepetitionTrigger;
			stageElapsedSeconds_ = 0.0f;
		}
		return;
	}

	if (stage_ == Stage::RepetitionTrigger) {
		// Let the current history hold real frames before clearing it, so
		// "pre-clear imagery" exists to (not) leak.
		adapter_.update(dt);
		if (adapter_.buffer().getHistoryFrameCount() >= 5 || stageElapsedSeconds_ >= kHistoryFillBudgetSeconds) {
			triggerRepetitionEvent();
			stage_ = Stage::RepetitionWaitForRefill;
			stageElapsedSeconds_ = 0.0f;
		}
		return;
	}

	if (stage_ == Stage::RepetitionWaitForRefill) {
		adapter_.update(dt);
		repFramesWaited_++;
		int hist = adapter_.buffer().getHistoryFrameCount();
		if (hist == 0) {
			// Empty history: nothing from before the clear may be presentable.
			if (allocatedPlayheadCount() > 0) repStaleFrames_++;
		}
		if (hist > 0 || stageElapsedSeconds_ >= kHistoryFillBudgetSeconds) {
			finishRepetitionEvent();
			stage_ = Stage::RepetitionHold;
			repHoldFrames_ = 0;
			stageElapsedSeconds_ = 0.0f;
		}
		return;
	}

	if (stage_ == Stage::RepetitionHold) {
		adapter_.update(dt);
		if (++repHoldFrames_ >= 10) {
			check(adapter_.reloadCount() == repReloadsAfterTrigger_,
				"TEMP-004 event " + ofToString(repIndex_ + 1) + ": no further decoder reloads after the single follow (no reload loop)");
			repIndex_++;
			if (repIndex_ >= repPlan_.size()) {
				check(reactivationCount_ == 5 && mediaChangeCount_ == 3,
					"TEMP-004 repetition plan executed: 5 reactivations + 3 canonical media changes");
				check(totalStaleAfterReactivation_ == 0, "TEMP-004 stale playhead frames after reactivation = 0 (total)");
				check(totalStaleAfterMediaChange_ == 0, "TEMP-004 stale playhead frames after canonical media change = 0 (total)");
				ofLogNotice("TFVideoAdapterSelfTest") << "TEMP-004 SUMMARY reactivations=" << reactivationCount_
													  << " mediaChanges=" << mediaChangeCount_
													  << " staleAfterReactivation=" << totalStaleAfterReactivation_
													  << " staleAfterMediaChange=" << totalStaleAfterMediaChange_;
				finalize();
				stage_ = Stage::Done;
			} else {
				stage_ = Stage::RepetitionTrigger;
				stageElapsedSeconds_ = 0.0f;
			}
		}
		return;
	}
}

void TFVideoAdapterSelfTest::triggerRepetitionEvent() {
	RepEvent ev = repPlan_[repIndex_];
	repPreClearHash_ = firstAllocatedPlayheadHash();
	repPreClearFile_ = adapter_.buffer().getCurrentMediaFilename();
	repOffsets_.clear();
	for (int i = 0; i < adapter_.buffer().getNumPlayheads(); ++i) repOffsets_.push_back(adapter_.buffer().getPlayheadOffset(i));
	int reloadsBefore = adapter_.reloadCount();
	repFramesWaited_ = 0;
	repStaleFrames_ = 0;

	if (ev == RepEvent::MediaChange) {
		// Canonical Shared Video selection change — Temporal only follows.
		VideoPlaybackStatus before = service_.status();
		bool ok = before.canSelectNext ? service_.next() : service_.previous();
		check(ok, "TEMP-004 event " + ofToString(repIndex_ + 1) + ": canonical VideoPlaybackService selection changed");
	} else {
		// TemporalProductionScene::activate() path.
		adapter_.invalidateForReactivation();
	}
	VideoPlaybackStatus status = service_.status();
	repMediaId_ = status.mediaId.value_or("");
	repPath_ = service_.currentAbsolutePath().value_or("");
	bool synced = adapter_.synchronizeSelectedMedia(repMediaId_, repPath_);
	repReloadsAfterTrigger_ = adapter_.reloadCount();

	std::string label = "TEMP-004 event " + ofToString(repIndex_ + 1) + " ("
		+ (ev == RepEvent::MediaChange ? "canonical media change" : "reactivation") + ")";
	check(synced && repReloadsAfterTrigger_ == reloadsBefore + 1, label + ": adapter followed with exactly one reload");
	check(adapter_.buffer().getHistoryFrameCount() == 0, label + ": history cleared (size 0)");
	check(allocatedPlayheadCount() == 0, label + ": immediately after the clear, no pre-clear playhead image is eligible for drawing");
	check(adapter_.buffer().getNumPlayheads() == 6, label + ": six playheads preserved through the clear");
	if (ev == RepEvent::MediaChange) {
		check(!repPreClearFile_.empty() && repPath_ != repPreClearFile_, label + ": canonical media identity differs from the pre-clear media");
	}
}

void TFVideoAdapterSelfTest::finishRepetitionEvent() {
	RepEvent ev = repPlan_[repIndex_];
	bool isMediaChange = ev == RepEvent::MediaChange;
	if (isMediaChange) {
		mediaChangeCount_++;
		totalStaleAfterMediaChange_ += repStaleFrames_;
	} else {
		reactivationCount_++;
		totalStaleAfterReactivation_ += repStaleFrames_;
	}
	std::string label = "TEMP-004 event " + ofToString(repIndex_ + 1) + " ("
		+ (isMediaChange ? "canonical media change" : "reactivation") + ")";
	int hist = adapter_.buffer().getHistoryFrameCount();
	check(repStaleFrames_ == 0, label + ": 0 stale pre-clear playhead frames while history was empty (stale="
		+ ofToString(repStaleFrames_) + ")");
	check(hist > 0, label + ": history refilled from the current canonical media");
	int allocated = allocatedPlayheadCount(); // queries all six -> each uploads from the refilled history
	check(allocated == adapter_.buffer().getNumPlayheads() && allocated == 6,
		label + ": all six playheads resume after refill (none permanently invalid)");
	bool offsetsPreserved = static_cast<int>(repOffsets_.size()) == adapter_.buffer().getNumPlayheads();
	for (int i = 0; offsetsPreserved && i < adapter_.buffer().getNumPlayheads(); ++i) {
		if (adapter_.buffer().getPlayheadOffset(i) != repOffsets_[i]) offsetsPreserved = false;
	}
	check(offsetsPreserved, label + ": existing playhead offsets unchanged by the clear/refill");
	check(adapter_.buffer().getCurrentMediaFilename() == repPath_, label + ": decoder holds the canonical path");
	if (isMediaChange) {
		uint64_t h = firstAllocatedPlayheadHash();
		check(h != 0 && h != repPreClearHash_, label + ": refilled playhead content is media B, not the pre-change media A");
	}
	ofLogNotice("TFVideoAdapterSelfTest") << "TEMP-004 EVENT " << (repIndex_ + 1) << " type="
										  << (isMediaChange ? "media-change" : "reactivation") << " mediaId=" << repMediaId_
										  << " path=" << repPath_ << " (pre-clear=" << repPreClearFile_ << ")"
										  << " staleFrames=" << repStaleFrames_ << " firstValidHistoryFrame=" << repFramesWaited_
										  << " history=" << hist << "/" << adapter_.buffer().getHistoryCapacityFrames()
										  << " playheads=" << adapter_.buffer().getNumPlayheads() << " allocated=" << allocated
										  << " reloads=" << adapter_.reloadCount();
}

int TFVideoAdapterSelfTest::allocatedPlayheadCount() {
	int count = 0;
	for (int i = 0; i < adapter_.buffer().getNumPlayheads(); ++i) {
		if (adapter_.buffer().getPlayheadTexture(i).isAllocated()) count++;
	}
	return count;
}

uint64_t TFVideoAdapterSelfTest::firstAllocatedPlayheadHash() {
	for (int i = 0; i < adapter_.buffer().getNumPlayheads(); ++i) {
		const ofTexture& tex = adapter_.buffer().getPlayheadTexture(i);
		if (!tex.isAllocated()) continue;
		ofPixels px;
		tex.readToPixels(px);
		uint64_t h = 1469598103934665603ULL; // FNV-1a
		const unsigned char* d = px.getData();
		for (size_t k = 0, n = px.size(); k < n; ++k) {
			h ^= d[k];
			h *= 1099511628211ULL;
		}
		return h;
	}
	return 0;
}

void TFVideoAdapterSelfTest::finish() {
	// ---- 4. previous() drives the adapter — no local Previous/Next state ----
	bool prevOk = service_.previous();
	check(prevOk, "previous() succeeds after a prior next()");

	VideoPlaybackStatus statusAfterPrevious = service_.status();
	check(statusAfterPrevious.mediaId.has_value() && *statusAfterPrevious.mediaId == firstMediaId_,
		"previous() returns the shared service to the original media identity");
	check(statusAfterPrevious.selectionOrigin == MediaSelectionOrigin::ManualPrevious, "previous() reports ManualPrevious selection origin");

	std::optional<std::string> thirdPath = service_.currentAbsolutePath();
	check(thirdPath.has_value(), "currentAbsolutePath() resolves after previous()");

	int reloadCountBeforeThirdSync = adapter_.reloadCount();
	bool thirdSyncOk = thirdPath.has_value() && statusAfterPrevious.mediaId.has_value()
		&& adapter_.synchronizeSelectedMedia(*statusAfterPrevious.mediaId, *thirdPath);
	check(thirdSyncOk, "adapter follows previous()'s resulting identity change");
	check(adapter_.reloadCount() == reloadCountBeforeThirdSync + 1, "the previous()-driven identity change triggers exactly one more reload");
	check(thirdPath.has_value() && adapter_.buffer().getCurrentMediaFilename() == *thirdPath,
		"the adapter's decoder now holds the EXACT path previous() selected");
	check(adapter_.buffer().getHistoryFrameCount() == 0,
		"history is reset again on the previous()-driven change — IDENTICAL semantics to the next()-driven change above, "
		"no selection-origin-specific branching (this session's prompt §4.3)");

	// ---- TEMP-004: stale playhead imagery is never presentable after a reset ----
	check(preChangePlayheadHash_ != 0, "TEMP-004 precondition: playhead imagery from the previous media existed before the change");
	check(allocatedPlayheadCount() == 0,
		"TEMP-004 canonical media change: previous-media playhead imagery is not presentable while the new history is empty "
		"(all six playhead textures unallocated)");

	// Reactivation path: TemporalProductionScene::activate() calls
	// invalidateForReactivation() and re-synchronizes the SAME canonical
	// selection, forcing a reload + history reset.
	int reloadsBeforeReactivation = adapter_.reloadCount();
	adapter_.invalidateForReactivation();
	bool reactivationSyncOk = thirdPath.has_value() && statusAfterPrevious.mediaId.has_value()
		&& adapter_.synchronizeSelectedMedia(*statusAfterPrevious.mediaId, *thirdPath);
	check(reactivationSyncOk && adapter_.reloadCount() == reloadsBeforeReactivation + 1,
		"TEMP-004 reactivation reload of the unchanged canonical selection reloads exactly once");
	check(adapter_.buffer().getHistoryFrameCount() == 0 && allocatedPlayheadCount() == 0,
		"TEMP-004 reactivation: no frame from before the reload is presentable while history is empty");

	stage_ = Stage::WaitForRefillAfterReactivation;
	stageElapsedSeconds_ = 0.0f;
}

void TFVideoAdapterSelfTest::finalize() {
	// ---- 5. Failed Temporal load does not alter shared selection ----
	std::string activeMediaIdBeforeFailedLoad = service_.status().mediaId.value_or("");
	bool failedLoadOk = adapter_.synchronizeSelectedMedia(
		"media.auto.selftest_bogus_id_never_in_catalog", "/this/path/does/not/exist/on/disk/selftest.mp4");
	check(!failedLoadOk, "synchronizeSelectedMedia() with a nonexistent path reports failure honestly (no local fallback selection)");
	check(!adapter_.lastLoadSucceeded(), "lastLoadSucceeded() reflects the failure");
	check(service_.status().mediaId.has_value() && *service_.status().mediaId == activeMediaIdBeforeFailedLoad,
		"a failed Temporal decoder load does NOT alter VideoPlaybackService's own active selection — "
		"the shared service remains authoritative regardless of the specialized decoder's own local failure");
	check(adapter_.buffer().getHistoryFrameCount() == 0, "history remains empty/unavailable after a failed load — not populated from any fallback");
	check(allocatedPlayheadCount() == 0,
		"TEMP-004 failed load: no playhead imagery from the previously loaded media remains presentable");

	bool allPassed = (g_failures == 0);
	ofLogNotice("TFVideoAdapterSelfTest")
		<< (allPassed ? "PASS" : "FAIL") << ": " << (g_checks - g_failures) << "/" << g_checks << " checks passed";
}

namespace {
	TFVideoAdapterSelfTest g_selfTest;
}

void beginTemporalVideoAdapterSelfTest() {
	g_selfTest.begin();
}

void tickTemporalVideoAdapterSelfTest(float dt) {
	if (!g_selfTest.isDone()) {
		g_selfTest.tick(dt);
	}
}
