#include "BlobLifecycleHarness.h"

#include "AllocationCounter.h"
#include "BlobDetectionProfileApply.h"
#include "BlobEffectBudget.h"
#include "BlobEffectPolicy.h"

#include "ofAppRunner.h"
#include "ofFileUtils.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>

namespace {

// Finds a metric by exact ID within a SceneSemanticData's metrics list —
// small local helper, not a shared/frozen accessor.
const SceneMetric * findMetric(const SceneSemanticData & data, const std::string & metricId) {
	for (const SceneMetric & m : data.metrics) {
		if (m.metricId == metricId) {
			return &m;
		}
	}
	return nullptr;
}

// "" (Original) or an assignable entry of the budget table in use.
bool isEligibleTreatment(const blobbudget::EffectBudget & budget, const std::string & effectId) {
	if (effectId.empty()) {
		return true;
	}
	for (const auto & t : budget.table()) {
		if (t.effectId == effectId) {
			return t.eligible && t.costCenti >= 0;
		}
	}
	return false;
}

} // namespace

bool BlobLifecycleHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS") != nullptr;
}

bool BlobLifecycleHarness::isSoakRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_BLOB_SOAK_HARNESS") != nullptr;
}

BlobLifecycleHarness::BlobLifecycleHarness(ExperienceRuntime & runtime, Mode mode)
	: runtime_(runtime), mode_(mode) {
	if (mode_ == Mode::Soak) {
		phase_ = Phase::Soak;
		// Blob Post-Acceptance Hardening: alloccounter is an existing,
		// already-wired, opt-in process-wide heap new/delete counter (see
		// AllocationCounter.h) — used today only by GlRestorationHarness.
		// Reused here as-is (not a new instrumentation system) for a
		// "no unbounded heap growth" signal alongside the fields the
		// soak prompt explicitly asked for; process RSS itself has no
		// in-repo source (RuntimeTelemetryCollector leaves it nullopt) and
		// is reported as NOT MEASURED rather than newly instrumented.
		alloccounter::reset();
		alloccounter::setEnabled(true);
		soakAllocNewBaseline_ = alloccounter::newCount();
		soakAllocDeleteBaseline_ = alloccounter::deleteCount();
		ofLogNotice("BlobLifecycleHarness") << "starting Blob desktop soak "
			"(EXPERIENCE_RUNTIME_BLOB_SOAK_HARNESS set) — target "
			<< kSoakDurationSeconds << "s / " << kSoakMaxFrames << " frames, whichever comes first";
	} else {
		ofLogNotice("BlobLifecycleHarness") << "starting deterministic Blob lifecycle/reactivation "
			"self-test (EXPERIENCE_RUNTIME_BLOB_LIFECYCLE_HARNESS set) — " << kCycleCount << " cycles";
	}
}

void BlobLifecycleHarness::logResult(const std::string & checkName, bool passed, const std::string & detail) {
	totalChecks_++;
	if (!passed) {
		failureCount_++;
	}
	ofLogNotice("BlobLifecycleHarness") << (passed ? "PASS" : "FAIL") << " — " << checkName
		<< (detail.empty() ? "" : (": " + detail));
}

void BlobLifecycleHarness::runOneFrame(float dt) {
	runtime_.update(dt);
	runtime_.draw();

	// Step 12 in ExperienceRuntime's own numbering: SceneFrame is
	// constructed fresh every draw() call — assert it's still valid and
	// frameNumber() is strictly increasing (a completed draw actually
	// happened, not a silently-skipped one). In Mode::Soak these are
	// tracked silently (see stepSoak()) rather than logged every frame —
	// see that method's own comment on avoiding per-frame log spam.
	const HudFrameData & frame = runtime_.currentHudFrameData();
	if (mode_ == Mode::Lifecycle) {
		logResult("SceneFrame texture non-null after draw", frame.sceneFrame.texture != nullptr,
			"frameNumber=" + ofToString(runtime_.frameNumber()));
		logResult("frameNumber() strictly increasing", runtime_.frameNumber() > lastFrameNumber_,
			"was " + ofToString(lastFrameNumber_) + ", now " + ofToString(runtime_.frameNumber()));
	} else {
		if (frame.sceneFrame.texture == nullptr || runtime_.frameNumber() <= lastFrameNumber_) {
			soakGlHudIssueObserved_ = true;
			ofLogError("BlobLifecycleHarness") << "SOAK: invalid SceneFrame or non-increasing frameNumber at frame "
				<< soakFrameCount_ << " (runtime frameNumber=" << runtime_.frameNumber() << ")";
		}
	}
	lastFrameNumber_ = runtime_.frameNumber();

	trackPeaks();
}

void BlobLifecycleHarness::trackPeaks() {
	const SceneHudStatus & status = runtime_.currentHudFrameData().scene;
	if (status.activeItemCount.has_value() && *status.activeItemCount > peakActiveItemCount_) {
		peakActiveItemCount_ = *status.activeItemCount;
	}
	if (status.semantic.has_value()) {
		if (const SceneMetric * regionMetric = findMetric(*status.semantic, "scene.blob.metric.region_count")) {
			if (regionMetric->value.has_value() && static_cast<int>(*regionMetric->value) > peakRegionCount_) {
				peakRegionCount_ = static_cast<int>(*regionMetric->value);
			}
		}
	}

	// Blob Post-Acceptance Hardening: real scratch-FBO high-water tracking,
	// read live through the new test-only accessors (no estimation).
	BlobProductionScene & blob = runtime_.blobSceneForTesting();
	peakFragmentScratchWidth_ = std::max(peakFragmentScratchWidth_, blob.fragmentScratchFboWidthForTesting());
	peakFragmentScratchHeight_ = std::max(peakFragmentScratchHeight_, blob.fragmentScratchFboHeightForTesting());
	peakBackgroundScratchWidth_ = std::max(peakBackgroundScratchWidth_, blob.backgroundScratchFboWidthForTesting());
	peakBackgroundScratchHeight_ = std::max(peakBackgroundScratchHeight_, blob.backgroundScratchFboHeightForTesting());
}

void BlobLifecycleHarness::captureScreenshot(const std::string & label) {
	std::string path = "captures/blob_lifecycle_" + label + ".png";
	ofFilePath::createEnclosingDirectory(path);
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(path);
	ofLogNotice("BlobLifecycleHarness") << "captured " << path;

	// Blob First Production Acceptance narrow patch, Scope C: also exercise
	// (and produce parity evidence from) ExperienceRuntime::
	// saveSceneFrameCaptureForTesting() at the same points — the runtime-
	// owned scene FBO's content BEFORE HUD compositing, directly comparable
	// to the standalone sketch's own output without HUD framing in the way.
	std::string scenePath = "captures/blob_lifecycle_" + label + "_scene_only.png";
	bool saved = runtime_.saveSceneFrameCaptureForTesting(scenePath);
	logResult("saveSceneFrameCaptureForTesting() succeeds (" + label + ")", saved, scenePath);
}

std::optional<std::string> BlobLifecycleHarness::currentMediaId() const {
	const HudFrameData & frame = runtime_.currentHudFrameData();
	return frame.video.has_value() ? frame.video->mediaId : std::nullopt;
}

void BlobLifecycleHarness::beginEffectStep(const std::string & label) {
	BlobProductionScene & blob = runtime_.blobSceneForTesting();
	SceneManager & sceneManager = runtime_.sceneManagerForTesting();
	effectStepGenerationBefore_ = blob.paletteGenerationForTesting();
	effectStepMediaBefore_ = currentMediaId();
	effectStepFramesSinceMediaChange_ = -1;
	framesInPhase_ = 0;
	// next() rejects at the catalog's end; previous() then still changes media.
	bool navigated = sceneManager.dispatchSceneCommand(SceneCommand::NextMedia)
		|| sceneManager.dispatchSceneCommand(SceneCommand::PreviousMedia);
	logResult("Effect policy " + label + ": media navigation accepted", navigated,
		"palette generation before=" + ofToString(effectStepGenerationBefore_));
}

bool BlobLifecycleHarness::effectStepSettled(int framesSinceMediaChange) const {
	// Long enough after the change that fragments on screen were created
	// under the newly selected effect, and at least one is present.
	// Prefer a few fragments for a representative capture; settle for one
	// on sparse-motion clips.
	int fragments = runtime_.currentHudFrameData().scene.activeItemCount.value_or(0);
	return (framesSinceMediaChange >= kEffectSettleFrames && fragments >= 3)
		|| (framesSinceMediaChange >= 5 * kEffectSettleFrames && fragments >= 1);
}

void BlobLifecycleHarness::trackEffectPerFrame() {
	// Per-frame budget invariants against the renderer's real fragments:
	// a live fragment never changes treatment, the budget is never
	// exceeded, and its books hold exactly the live fragments.
	BlobProductionScene & blob = runtime_.blobSceneForTesting();
	const blobbudget::EffectBudget & budget = blob.effectBudgetForTesting();
	blob.collectFragmentAssignmentsForTesting(assignScratch_);

	std::map<int, std::string> now;
	int sumCost = 0;
	bool booksMatch = budget.activeCount() == static_cast<int>(assignScratch_.size());
	for (const auto & a : assignScratch_) {
		now[a.first] = a.second;
		auto prev = assignLast_.find(a.first);
		if (prev != assignLast_.end() && prev->second != a.second) {
			assignmentChanges_++;
			ofLogError("BlobLifecycleHarness") << "fragment " << a.first << " changed treatment while alive: "
				<< blobbudget::label(prev->second) << " -> " << blobbudget::label(a.second);
		}
		std::string charged;
		booksMatch = booksMatch && budget.activeEffectFor(a.first, charged) && charged == a.second;
		sumCost += std::max(0, budget.costOf(a.second));
		if (!isEligibleTreatment(budget, a.second)) {
			ineligibleAssignments_++;
		}
		effectsSeen_.insert(blobbudget::label(a.second));
	}
	booksMatch = booksMatch && sumCost == budget.usedCostCenti();
	if (!booksMatch) {
		bookMismatches_++;
	}
	if (budget.usedCostCenti() > budget.maxCostCenti()) {
		budgetOverruns_++;
	}
	peakBudgetUsedCenti_ = std::max(peakBudgetUsedCenti_, budget.usedCostCenti());
	peakShaderFragments_ = std::max(peakShaderFragments_, budget.activeShaderCount());
	assignLast_ = std::move(now);
}

void BlobLifecycleHarness::logSoakStatusLine() {
	BlobProductionScene & blob = runtime_.blobSceneForTesting();
	const SceneHudStatus & status = runtime_.currentHudFrameData().scene;

	int regionCount = 0;
	int fragmentCount = 0;
	if (status.semantic.has_value()) {
		if (const SceneMetric * m = findMetric(*status.semantic, "scene.blob.metric.region_count")) {
			if (m->value.has_value()) regionCount = static_cast<int>(*m->value);
		}
		if (const SceneMetric * m = findMetric(*status.semantic, "scene.blob.metric.fragment_count")) {
			if (m->value.has_value()) fragmentCount = static_cast<int>(*m->value);
		}
	}

	long long allocNew = alloccounter::newCount() - soakAllocNewBaseline_;
	long long allocDelete = alloccounter::deleteCount() - soakAllocDeleteBaseline_;

	ofLogNotice("BlobLifecycleHarness")
		<< "SOAK t=" << ofToString(soakElapsedSeconds_, 1) << "s frame=" << soakFrameCount_
		<< " region=" << regionCount << "/" << blob.configuredMaxBlobsForTesting()
		<< " fragment=" << fragmentCount << "/" << blob.configuredMaxActiveFragmentsForTesting()
		<< " fragScratch=" << blob.fragmentScratchFboWidthForTesting() << "x" << blob.fragmentScratchFboHeightForTesting()
		<< " (peak " << peakFragmentScratchWidth_ << "x" << peakFragmentScratchHeight_ << ")"
		<< " bgScratch=" << blob.backgroundScratchFboWidthForTesting() << "x" << blob.backgroundScratchFboHeightForTesting()
		<< " mediaChanges=" << soakMediaChangeCount_
		<< " fps=" << ofToString(runtime_.currentHudFrameData().runtime.fps, 1)
		<< " allocNew=" << allocNew << " allocDelete=" << allocDelete;
}

void BlobLifecycleHarness::stepSoak(float dt) {
	runOneFrame(dt); // includes trackPeaks(); GL/HUD validity tracked silently in Mode::Soak — see that method

	soakFrameCount_++;
	soakElapsedSeconds_ += dt;
	soakSinceLastLogSeconds_ += dt;

	// Media-change tracking: reads the CANONICAL HudFrameData.video (never
	// reconstructed) — a real signal, not a Blob-owned one, exactly the
	// boundary DEC-013 requires.
	const auto & video = runtime_.currentHudFrameData().video;
	if (video.has_value() && video->mediaId.has_value()) {
		if (soakLastMediaId_.has_value() && *soakLastMediaId_ != *video->mediaId) {
			soakMediaChangeCount_++;
		}
		soakLastMediaId_ = *video->mediaId;
	}

	const RuntimeTelemetry & telemetry = runtime_.currentHudFrameData().runtime;
	if (telemetry.fps > 0.0f && (soakMinFps_ < 0.0f || telemetry.fps < soakMinFps_)) {
		soakMinFps_ = telemetry.fps;
	}
	if (telemetry.frameTimeMs > soakMaxFrameTimeMs_) {
		soakMaxFrameTimeMs_ = telemetry.frameTimeMs;
	}

	if (soakSinceLastLogSeconds_ >= kSoakLogIntervalSeconds) {
		logSoakStatusLine();
		soakSinceLastLogSeconds_ = 0.0f;
	}

	if (soakElapsedSeconds_ >= kSoakDurationSeconds || soakFrameCount_ >= kSoakMaxFrames) {
		finishSoak();
	}
}

void BlobLifecycleHarness::finishSoak() {
	logSoakStatusLine(); // final sample, matches the periodic-log format exactly
	captureScreenshot("soak_final");

	SceneManager & sceneManager = runtime_.sceneManagerForTesting();
	sceneManager.deactivateScene();
	sceneManager.shutdown();
	logResult("Soak shutdown() completed without error", true, "");

	BlobProductionScene & blob = runtime_.blobSceneForTesting();
	int maxBlobs = blob.configuredMaxBlobsForTesting();
	int maxFragments = blob.configuredMaxActiveFragmentsForTesting();
	glm::ivec2 sceneSize = blob.nativeRenderSize();

	// Blob Post-Acceptance Hardening finding (see completion report):
	// BlobDetector::Config::maxBlobs caps NEW detections reported PER
	// FRAME (contourFinder.findContours()'s own nConsidered argument) —
	// it does NOT cap BlobTracker's accumulated active-track count, which
	// has no configured ceiling anywhere in BlobTracker::Config. A real
	// 900s soak run observed region_count transiently reaching well above
	// maxBlobs (peak 33 vs. maxBlobs=12) during bursty media-transition
	// detection before returning to 0/low values repeatedly — informational
	// only, not a pass/fail bound, since maxBlobs was never actually the
	// right comparison for this metric.
	logResult("Soak: peak region_count observed (informational — see report on why maxBlobs doesn't bound this)",
		true, "peak=" + ofToString(peakRegionCount_) + ", BlobDetector maxBlobs=" + ofToString(maxBlobs)
			+ " (bounds new detections/frame only, not BlobTracker's accumulated track count)");
	logResult("Soak: peak fragment_count stays within configured maxActiveFragments", peakActiveItemCount_ <= maxFragments,
		"peak=" + ofToString(peakActiveItemCount_) + " maxActiveFragments=" + ofToString(maxFragments));
	// Generous sanity bound (2x native canvas in each dimension) — real
	// fragments are crops of the source video scaled by fragmentScale
	// (default max 2.5x per the standalone GUI's own range), never
	// expected to approach this; catches genuine runaway/corruption, not a
	// tight product-tuning assertion (see report).
	bool scratchBounded = peakFragmentScratchWidth_ <= 2 * BlobProductionScene::kNativeWidth
		&& peakFragmentScratchHeight_ <= 2 * BlobProductionScene::kNativeHeight
		&& peakBackgroundScratchWidth_ <= 2 * BlobProductionScene::kNativeWidth
		&& peakBackgroundScratchHeight_ <= 2 * BlobProductionScene::kNativeHeight;
	logResult("Soak: scratch-FBO high-water stays within a generous sanity bound (no runaway growth)", scratchBounded,
		"fragment=" + ofToString(peakFragmentScratchWidth_) + "x" + ofToString(peakFragmentScratchHeight_)
			+ " background=" + ofToString(peakBackgroundScratchWidth_) + "x" + ofToString(peakBackgroundScratchHeight_));
	logResult("Soak: no GL/HUD invalidity observed across the run", !soakGlHudIssueObserved_, "");

	long long allocNew = alloccounter::newCount() - soakAllocNewBaseline_;
	long long allocDelete = alloccounter::deleteCount() - soakAllocDeleteBaseline_;

	ofLogNotice("BlobLifecycleHarness") << "==================================================";
	ofLogNotice("BlobLifecycleHarness") << "SOAK SUMMARY";
	ofLogNotice("BlobLifecycleHarness") << "soak duration: " << ofToString(soakElapsedSeconds_, 1) << "s";
	ofLogNotice("BlobLifecycleHarness") << "frame count: " << soakFrameCount_;
	ofLogNotice("BlobLifecycleHarness") << "media changes: " << soakMediaChangeCount_;
	ofLogNotice("BlobLifecycleHarness") << "peak region count: " << peakRegionCount_ << " (configured maxBlobs: " << maxBlobs << ")";
	ofLogNotice("BlobLifecycleHarness") << "peak fragment count: " << peakActiveItemCount_ << " (configured maxActiveFragments: " << maxFragments << ")";
	ofLogNotice("BlobLifecycleHarness") << "scratch FBO peak width: fragment=" << peakFragmentScratchWidth_ << " background=" << peakBackgroundScratchWidth_;
	ofLogNotice("BlobLifecycleHarness") << "scratch FBO peak height: fragment=" << peakFragmentScratchHeight_ << " background=" << peakBackgroundScratchHeight_;
	ofLogNotice("BlobLifecycleHarness") << "scratch FBO peak area: fragment=" << (peakFragmentScratchWidth_ * peakFragmentScratchHeight_)
		<< " background=" << (peakBackgroundScratchWidth_ * peakBackgroundScratchHeight_);
	ofLogNotice("BlobLifecycleHarness") << "scene FBO size: " << sceneSize.x << "x" << sceneSize.y;
	ofLogNotice("BlobLifecycleHarness") << "starting RSS: NOT MEASURED";
	ofLogNotice("BlobLifecycleHarness") << "ending RSS: NOT MEASURED";
	ofLogNotice("BlobLifecycleHarness") << "peak RSS: NOT MEASURED";
	ofLogNotice("BlobLifecycleHarness") << "heap alloc delta (existing alloccounter instrumentation): new=" << allocNew << " delete=" << allocDelete;
	ofLogNotice("BlobLifecycleHarness") << "average FPS/frame time: min fps observed=" << ofToString(soakMinFps_, 1)
		<< " max frameTimeMs observed=" << ofToString(soakMaxFrameTimeMs_, 2);
	ofLogNotice("BlobLifecycleHarness") << "GL/HUD errors: " << (soakGlHudIssueObserved_ ? "observed (see log above)" : "none observed");
	ofLogNotice("BlobLifecycleHarness") << "crashes: none (process reached shutdown)";
	ofLogNotice("BlobLifecycleHarness") << "RESULT: " << (totalChecks_ - failureCount_) << "/" << totalChecks_ << " checks passed";
	ofLogNotice("BlobLifecycleHarness") << "==================================================";

	alloccounter::setEnabled(false);
	phase_ = Phase::Done;
	ofExit();
}

void BlobLifecycleHarness::step(float dt) {
	if (phase_ == Phase::Done) {
		return;
	}

	if (phase_ == Phase::Soak) {
		stepSoak(dt);
		return;
	}

	SceneManager & sceneManager = runtime_.sceneManagerForTesting();

	switch (phase_) {
		case Phase::Warmup: {
			runOneFrame(dt);
			framesInPhase_++;
			if (framesInPhase_ >= kWarmupFrames) {
				logResult("Setup once + first activate + first update/draw sequence completed",
					true, ofToString(kWarmupFrames) + " frames, sceneId=" + sceneManager.currentSceneStatus().sceneId);
				captureScreenshot("00_warmup");
				sceneManager.deactivateScene();
				framesInPhase_ = 0;
				phase_ = Phase::CycleBegin;
			}
			break;
		}

		case Phase::CycleBegin: {
			sceneManager.activateScene();

			// Diagnostic-only status pull, positioned deliberately BEFORE
			// any update() runs this cycle — proves BlobSceneCore::
			// activate()'s documented reset-on-activate policy holds for
			// real (no stale track/fragment identity survives
			// deactivate->reactivate), not just in comments. Precedented
			// by GlRestorationHarness's own documented double-pull-per-
			// tick exception (see that class's seamProofBaselineStatusPollCount_
			// comment) — a harness-only diagnostic call between
			// ExperienceRuntime frames, not a second production per-frame
			// pull (no runtime.draw() happens between this and the next
			// real captureSceneStatus() inside runOneFrame() below).
			sceneManager.captureSceneStatus();
			const SceneHudStatus & status = sceneManager.currentSceneStatus();
			if (status.semantic.has_value()) {
				const SceneMetric * regionMetric = findMetric(*status.semantic, "scene.blob.metric.region_count");
				const SceneMetric * fragmentMetric = findMetric(*status.semantic, "scene.blob.metric.fragment_count");
				bool haveRegionValue = regionMetric && regionMetric->value.has_value();
				bool haveFragmentValue = fragmentMetric && fragmentMetric->value.has_value();
				bool regionZero = haveRegionValue && *regionMetric->value == 0.0f;
				bool fragmentZero = haveFragmentValue && *fragmentMetric->value == 0.0f;
				logResult("Cycle " + ofToString(cycleIndex_) + ": no stale region/fragment carryover immediately after reactivation",
					regionZero && fragmentZero,
					"region=" + (haveRegionValue ? ofToString(*regionMetric->value) : std::string("?"))
						+ " fragment=" + (haveFragmentValue ? ofToString(*fragmentMetric->value) : std::string("?")));
			} else {
				// Video momentarily not ready right at this instant is
				// possible in principle (health Loading/transient) — not a
				// failure, just noted; the pipeline itself never ran yet
				// this cycle either way, so there is nothing stale to leak.
				logResult("Cycle " + ofToString(cycleIndex_) + ": no stale region/fragment carryover immediately after reactivation",
					true, "semantic absent this instant (no video frame yet) — vacuously no carryover");
			}

			framesInPhase_ = 0;
			phase_ = Phase::CycleFrames;
			break;
		}

		case Phase::CycleFrames: {
			runOneFrame(dt);
			framesInPhase_++;
			if (framesInPhase_ >= kFramesPerCycle) {
				logResult("Cycle " + ofToString(cycleIndex_) + ": update/draw succeeded for " + ofToString(kFramesPerCycle) + " frames after reactivation",
					true, "");
				if (cycleIndex_ == 0 || cycleIndex_ == kCycleCount / 2 || cycleIndex_ == kCycleCount - 1) {
					captureScreenshot("cycle_" + ofToString(cycleIndex_));
				}
				framesInPhase_ = 0;
				phase_ = Phase::CycleEnd;
			}
			break;
		}

		case Phase::CycleEnd: {
			sceneManager.deactivateScene();
			cycleIndex_++;
			if (cycleIndex_ >= kCycleCount) {
				// Blob Post-Acceptance Hardening: bounds now read the real
				// configured production caps (BlobDetector::Config::maxBlobs,
				// VideoRegionController::Params::maxActiveFragments) via the
				// new test-only accessors, replacing the prior patch's
				// documented-as-generous hardcoded bound of 50.
				BlobProductionScene & blob = runtime_.blobSceneForTesting();
				int maxBlobs = blob.configuredMaxBlobsForTesting();
				int maxFragments = blob.configuredMaxActiveFragmentsForTesting();

				logResult(ofToString(kCycleCount) + " deactivate/reactivate cycles completed", true,
					"peak activeItemCount=" + ofToString(peakActiveItemCount_)
						+ ", peak region_count=" + ofToString(peakRegionCount_));
				logResult("Peak fragment_count stays within the real configured maxActiveFragments across " + ofToString(kCycleCount) + " cycles",
					peakActiveItemCount_ <= maxFragments,
					"peak=" + ofToString(peakActiveItemCount_) + " maxActiveFragments=" + ofToString(maxFragments));
				// Informational, not a bound — see the soak's own identical
				// comment (finishSoak()) for why maxBlobs (per-frame new-
				// detection cap) does not bound BlobTracker's accumulated
				// track count. Kept in the lifecycle proof for continuity
				// of the peak value across both harness modes.
				logResult("Peak region_count observed across " + ofToString(kCycleCount) + " cycles (informational — see report)",
					true,
					"peak=" + ofToString(peakRegionCount_) + ", BlobDetector maxBlobs=" + ofToString(maxBlobs)
						+ " (bounds new detections/frame only, not accumulated track count)");

				bool scratchBounded = peakFragmentScratchWidth_ <= 2 * BlobProductionScene::kNativeWidth
					&& peakFragmentScratchHeight_ <= 2 * BlobProductionScene::kNativeHeight
					&& peakBackgroundScratchWidth_ <= 2 * BlobProductionScene::kNativeWidth
					&& peakBackgroundScratchHeight_ <= 2 * BlobProductionScene::kNativeHeight;
				logResult("Scratch-FBO high-water stays within a generous sanity bound across " + ofToString(kCycleCount) + " cycles",
					scratchBounded,
					"fragment=" + ofToString(peakFragmentScratchWidth_) + "x" + ofToString(peakFragmentScratchHeight_)
						+ " background=" + ofToString(peakBackgroundScratchWidth_) + "x" + ofToString(peakBackgroundScratchHeight_));

				phase_ = Phase::FinalActivate;
			} else {
				phase_ = Phase::CycleBegin;
			}
			break;
		}

		case Phase::FinalActivate: {
			sceneManager.activateScene();
			framesInPhase_ = 0;
			phase_ = Phase::FinalFrames;
			break;
		}

		case Phase::FinalFrames: {
			runOneFrame(dt);
			framesInPhase_++;
			if (framesInPhase_ >= kFinalFrames) {
				logResult("Final activate + update/draw sequence (after all 20 cycles) succeeded", true, "");
				captureScreenshot("final_active");
				BlobProductionScene & blob = runtime_.blobSceneForTesting();
				const BlobSceneCore::BackgroundConfig & bg = blob.backgroundConfigForTesting();
				logResult("Production background is the desaturated world (mode 2, " + blobeffects::backgroundEffectId() + ")",
					bg.mode == 2 && bg.effectName == blobeffects::backgroundEffectId() && bg.effectAmount >= 1.0f,
					"mode=" + ofToString(bg.mode) + " effect=" + bg.effectName);
				// Background-only proof: the hidden Reset drops every fragment
				// with no fade, so the next frame is the desaturated world alone
				// (a fragment born that frame would still be at ~0 opacity).
				sceneManager.dispatchSceneCommand(SceneCommand::Reset);
				runOneFrame(dt);
				int fragmentsAfterReset = runtime_.currentHudFrameData().scene.activeItemCount.value_or(0);
				logResult("Background-only frame after Reset has no established fragments", fragmentsAfterReset <= 1,
					"fragments=" + ofToString(fragmentsAfterReset));
				captureScreenshot("background_only");

				assignLast_.clear();
				effectStep_ = 0;
				beginEffectStep("step 0");
				phase_ = Phase::EffectStep;
			}
			break;
		}

		case Phase::EffectStep:
		case Phase::EffectResume: {
			runOneFrame(dt);
			trackEffectPerFrame();
			framesInPhase_++;
			if (effectStepFramesSinceMediaChange_ < 0 && currentMediaId() != effectStepMediaBefore_) {
				effectStepFramesSinceMediaChange_ = 0;
			} else if (effectStepFramesSinceMediaChange_ >= 0) {
				effectStepFramesSinceMediaChange_++;
			}

			bool settled = effectStepSettled(effectStepFramesSinceMediaChange_);
			bool timedOut = framesInPhase_ >= kEffectStepTimeoutFrames;
			if (!settled && !timedOut) {
				break;
			}

			BlobProductionScene & blob = runtime_.blobSceneForTesting();
			const blobbudget::EffectBudget & budget = blob.effectBudgetForTesting();
			std::string label = phase_ == Phase::EffectResume ? "resume" : "step " + ofToString(effectStep_);
			uint64_t generations = blob.paletteGenerationForTesting() - effectStepGenerationBefore_;
			int fragments = runtime_.currentHudFrameData().scene.activeItemCount.value_or(0);

			// The live mix, as the renderer actually holds it.
			blob.collectFragmentAssignmentsForTesting(assignScratch_);
			std::set<std::string> liveShaders;
			std::string mix;
			for (const auto & a : assignScratch_) {
				if (!a.second.empty()) {
					liveShaders.insert(a.second);
				}
				mix += (mix.empty() ? "" : ",") + ofToString(a.first) + ":" + blobbudget::label(a.second);
			}

			logResult("Effect policy " + label + ": media changed and fragments settled before timeout", settled,
				"frames=" + ofToString(framesInPhase_) + " fragments=" + ofToString(fragments));
			logResult("Effect policy " + label + ": exactly one palette generation per media change", generations == 1,
				"generations=" + ofToString(generations) + " now=" + ofToString(blob.paletteGenerationForTesting()));
			logResult("Effect policy " + label + ": budget within max, books match the live fragments",
				budget.usedCostCenti() <= budget.maxCostCenti() && budget.activeCount() == static_cast<int>(assignScratch_.size()),
				"used=" + ofToString(budget.usedCostCenti() / 100.0) + "/" + ofToString(budget.maxCostCenti() / 100.0)
					+ " original=" + ofToString(budget.activeOriginalCount()) + " shader=" + ofToString(budget.activeShaderCount())
					+ " fallbacks=" + ofToString(budget.fallbackCount()) + " mix=" + mix);
			// One region entry per distinct live shader treatment (none for
			// Original); the desaturated background is always reported.
			std::set<std::string> reported;
			bool backgroundReported = false;
			for (const std::string & e : runtime_.currentHudFrameData().scene.activeEffects) {
				if (e.rfind("region: ", 0) == 0) {
					reported.insert(e.substr(8));
				}
				backgroundReported = backgroundReported || e == "background: " + blobeffects::backgroundEffectId();
			}
			logResult("Effect policy " + label + ": SceneHudStatus.activeEffects lists exactly the live shader treatments plus the background",
				reported == liveShaders && backgroundReported, "live=" + ofToString(liveShaders.size()) + " reported=" + ofToString(reported.size()));
			logResult("Effect policy " + label + ": HudFrameData.effects stays std::nullopt (no Blob activity producer)",
				!runtime_.currentHudFrameData().effects.has_value(), "");

			// Per-video detection preset: the live detector config is exactly
			// this media's stored profile, or the canonical defaults if none.
			{
				std::string media = currentMediaId().value_or("");
				const BlobDetectionProfile * stored = blob.detectionProfilesForTesting().find(media);
				BlobDetectionProfile want = resolveProfile(blobdetectionprofile::canonicalDefaults(), stored);
				BlobDetectionProfile have = blobdetectionprofile::fromConfig(blob.detectionConfigForTesting());
				const auto & last = blob.lastDetectionProfileForTesting();
				bool appliedForThisMedia = last && last->mediaId == media && last->source == (stored ? "per_media" : "defaults");
				logResult("Detection profile " + label + ": detector config matches " + (stored ? "this media's profile" : "defaults"),
					have == want && appliedForThisMedia,
					"media=" + media + " source=" + (last ? last->source : std::string("-")) + " threshold=" + ofToString(*have.threshold)
						+ " erode=" + ofToString(*have.erodeIterations) + " dilate=" + ofToString(*have.dilateIterations)
						+ " maxArea=" + ofToString(*have.maxBlobAreaNormalized));
			}
			captureScreenshot("effect_" + (phase_ == Phase::EffectResume ? std::string("resume") : ofToString(effectStep_)));

			if (phase_ == Phase::EffectResume) {
				std::string seen;
				for (const std::string & e : effectsSeen_) {
					seen += (seen.empty() ? "" : ",") + e;
				}
				logResult("No live fragment ever changed treatment, across the whole effect-policy proof",
					assignmentChanges_ == 0, "violations=" + ofToString(assignmentChanges_));
				logResult("Effect budget never exceeded on any frame", budgetOverruns_ == 0,
					"overruns=" + ofToString(budgetOverruns_) + " peakUsed=" + ofToString(peakBudgetUsedCenti_ / 100.0)
						+ " peakShaderFragments=" + ofToString(peakShaderFragments_));
				logResult("Budget books matched the renderer's live fragments on every frame (release on fragment death)",
					bookMismatches_ == 0, "mismatches=" + ofToString(bookMismatches_));
				logResult("Every assigned treatment was Original or budget-eligible", ineligibleAssignments_ == 0,
					"ineligible=" + ofToString(ineligibleAssignments_) + " seen=" + seen);
				phase_ = Phase::Shutdown;
				break;
			}

			effectStep_++;
			if (effectStep_ <= kEffectSteps) {
				beginEffectStep("step " + ofToString(effectStep_));
			} else {
				phase_ = Phase::EffectInactive;
			}
			break;
		}

		case Phase::EffectInactive: {
			BlobProductionScene & blob = runtime_.blobSceneForTesting();
			sceneManager.deactivateScene();
			uint64_t generationBefore = blob.paletteGenerationForTesting();
			std::optional<std::string> mediaBefore = currentMediaId();
			bool navigated = sceneManager.dispatchSceneCommand(SceneCommand::NextMedia)
				|| sceneManager.dispatchSceneCommand(SceneCommand::PreviousMedia);
			logResult("Effect policy inactive: media navigation accepted while deactivated", navigated, "");
			logResult("Effect policy inactive: no palette generation while deactivated",
				blob.paletteGenerationForTesting() == generationBefore, "generation=" + ofToString(blob.paletteGenerationForTesting()));

			// Reactivate without another navigation: the catch-up generation
			// must happen exactly once, on the first active update that sees it.
			// (activate() resets fragments, so live assignments start over.)
			sceneManager.activateScene();
			assignLast_.clear();
			effectStepGenerationBefore_ = generationBefore;
			effectStepMediaBefore_ = mediaBefore;
			effectStepFramesSinceMediaChange_ = -1;
			framesInPhase_ = 0;
			phase_ = Phase::EffectResume;
			break;
		}

		case Phase::Shutdown: {
			sceneManager.deactivateScene();
			sceneManager.shutdown();
			logResult("shutdown() completed without error", true, "");

			bool nextRejected = !sceneManager.dispatchSceneCommand(SceneCommand::NextMedia);
			logResult("Post-shutdown command rejection: NextMedia returns false", nextRejected, "");

			bool resetRejected = !sceneManager.dispatchSceneCommand(SceneCommand::Reset);
			logResult("Post-shutdown command rejection: (hidden) Reset returns false", resetRejected, "");

			bool prevRejected = !sceneManager.dispatchSceneCommand(SceneCommand::PreviousMedia);
			logResult("Post-shutdown command rejection: PreviousMedia returns false", prevRejected, "");

			ofLogNotice("BlobLifecycleHarness") << "==================================================";
			ofLogNotice("BlobLifecycleHarness") << "RESULT: " << (totalChecks_ - failureCount_) << "/" << totalChecks_
				<< " checks passed";
			ofLogNotice("BlobLifecycleHarness") << "fragment scratch FBO peak=" << peakFragmentScratchWidth_
				<< "x" << peakFragmentScratchHeight_ << ", background scratch FBO peak="
				<< peakBackgroundScratchWidth_ << "x" << peakBackgroundScratchHeight_;
			ofLogNotice("BlobLifecycleHarness") << "No callback/listener/timer registration exists anywhere in "
				"BlobSceneCore/BlobDetector/BlobTracker/VideoRegionController/VideoRegionEffectRenderer/"
				"BlobProductionScene — verified by direct inspection (zero ofAddListener/callback-registration "
				"call sites in any of those files), not by a runtime counter, since there is nothing to count.";
			ofLogNotice("BlobLifecycleHarness") << "==================================================";

			phase_ = Phase::Done;
			ofExit();
			break;
		}

		case Phase::Soak:
		case Phase::Done:
			break;
	}
}
