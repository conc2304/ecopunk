#pragma once

#include "VideoPlaybackService.h"
#include "TimeOffsetPlaybackAdapter.h"
#include <cstdint>
#include <string>
#include <vector>

// Shared Video — Temporal Fields Specialized Adapter Seam session's
// required real-decoder proof (prompt §8/§11): exercises the REAL,
// compiled VideoPlaybackService and TimeOffsetPlaybackAdapter classes
// against real files under the canonical assets/shared/media/ root — no
// mocks, no fake decoder, no compatibility symlink.
//
// SHAPE: a per-frame state machine, not a single blocking setup()-time
// call. A first implementation tried to prove real frame arrival by
// busy-waiting (sleep + poll) synchronously inside ofApp::setup() — this
// reliably FAILED (0 frames ever arrived, even given an 8-second
// wall-clock budget) because this environment's ofVideoPlayer backend
// only delivers decoded frames while the real oF run loop is actually
// pumping window/event-loop callbacks between update() calls; a
// synchronous wait inside setup() (before that loop has started) blocks
// exactly the thing frame delivery depends on. See this session's
// implementation report, "Real canonical-root decode test," for the
// discarded synchronous version and this finding.
//
// Correct shape: begin() runs the synchronous part (service/adapter
// setup, identity/reload-count/path-match checks — none of which need a
// real decoded frame) from ofApp::setup(), then tick() is called once per
// REAL ofApp::update() until isDone(), advancing through the two phases
// that genuinely need real frames (initial decode, adapter history fill)
// plus the remaining synchronous checks (next()/previous()/failed-load)
// in between.
class TFVideoAdapterSelfTest {
public:
	void begin();
	void tick(float dt);
	bool isDone() const { return stage_ == Stage::Done; }

private:
	enum class Stage {
		WaitForFirstFrame,
		AfterFirstFrame,
		WaitForHistoryFill,
		AfterHistoryFill,
		WaitForRefillAfterReactivation, // TEMP-004
		RepetitionTrigger,              // TEMP-004: 5 reactivations + 3 canonical media changes
		RepetitionWaitForRefill,
		RepetitionHold,
		Done
	};

	// TEMP-004 repetition phase
	enum class RepEvent { Reactivation, MediaChange };
	void triggerRepetitionEvent();
	void finishRepetitionEvent();
	std::vector<RepEvent> repPlan_;
	size_t repIndex_ = 0;
	int repFramesWaited_ = 0;
	int repStaleFrames_ = 0;
	int repHoldFrames_ = 0;
	int repReloadsAfterTrigger_ = 0;
	uint64_t repPreClearHash_ = 0;
	std::string repPreClearFile_;
	std::string repMediaId_;
	std::string repPath_;
	std::vector<float> repOffsets_;
	int totalStaleAfterReactivation_ = 0;
	int totalStaleAfterMediaChange_ = 0;
	int reactivationCount_ = 0;
	int mediaChangeCount_ = 0;

	void finish();
	void finalize();

	// TEMP-004 helpers/state
	int allocatedPlayheadCount();
	uint64_t firstAllocatedPlayheadHash();
	uint64_t preChangePlayheadHash_ = 0;

	Stage stage_ = Stage::WaitForFirstFrame;
	float stageElapsedSeconds_ = 0.0f;

	VideoPlaybackService service_;
	TimeOffsetPlaybackAdapter adapter_;

	std::string firstMediaId_;
	std::optional<std::string> firstPath_;
	std::string secondMediaId_;
	std::optional<std::string> secondPath_;

	bool sawFrame_ = false;
	bool sawNonEmptyPixels_ = false;
};

// Convenience free function matching TFActivityStatusSelfTest.h's
// runXSelfTest() naming — owns a static instance internally so ofApp only
// needs one begin-once/tick-every-frame pair of call sites.
void beginTemporalVideoAdapterSelfTest();
void tickTemporalVideoAdapterSelfTest(float dt);
