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

			stage_ = Stage::AfterHistoryFill;
			stageElapsedSeconds_ = 0.0f;
		}
		return;
	}

	if (stage_ == Stage::AfterHistoryFill) {
		finish();
		stage_ = Stage::Done;
		return;
	}
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
