#pragma once

#include "ExperienceRuntime.h"

#include "ofLog.h"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// TwoSceneAcceptanceHarness — RT-002: the first real two-scene production
// acceptance. Test/acceptance tooling only; changes no production behavior.
//
// Drives the real ExperienceRuntime (real BlobProductionScene +
// TemporalProductionScene, real SceneManager, real production HUD path, real
// VideoPlaybackService, real Temporal adapter/history path, real
// TFEffectPicker effect source) through 20 complete
// Blob -> NextScene -> Temporal -> PreviousScene -> Blob cycles, every switch
// issued through the production InputRouter route (runtime.keyPressed(']' /
// '[')), every media change through the production SceneCommand route
// (runtime.keyPressed('n')).
//
// Observes (never patches) the Temporal "texture is not allocated" warning by
// wrapping the console logger channel; likewise observes TFEffectPicker's
// natural "applying canonical eligible preset" log line.
//
// Cycle plan (dwell = live frames after the transition completes; every
// activation therefore has >= 12 FadingIn + 1 + dwell >= 73 active frames):
//   cycle 1  Temporal dwell: intentional canonical media change at dwell
//            frame 30, dwell extended until history refill is observed
//   cycle 2  Blob dwell: intentional canonical media change at dwell frame 30
//            (checked by the cycle-3 Temporal activation)
//   cycle 10 Temporal dwell: extended (one continuous activation) until a
//            natural TFComposition PATTERN_TRANSITION occurs — Temporal
//            restarts its 90 s pattern cycle on every activation, so a
//            mid-TFFragmentTransition frame cannot occur in a 60-frame dwell
//   all other dwells: kDwellFrames
//
// Activated by EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE; exits when done.
class TwoSceneAcceptanceHarness {
public:
	static bool isRequested();

	explicit TwoSceneAcceptanceHarness(ExperienceRuntime& runtime);
	~TwoSceneAcceptanceHarness();

	void step(float dt);

	// Shared with the log tap.
	struct LogTap {
		uint64_t frame = 0;
		int textureNotAllocatedWarnings = 0;
		int otherWarningsOrErrors = 0;
		std::vector<std::pair<uint64_t, std::string>> presetApplications;
		std::vector<std::pair<uint64_t, std::string>> otherMessages;
	};

private:
	enum class Phase { Warmup, Switch, Transition, Dwell, FinalValidation, Shutdown, Done };

	struct ActivationRecord {
		int cycle = 0;
		std::string sceneId;
		uint64_t firstFrame = 0;
		int liveFrames = 0;
		int warningFrames = 0;
		int warningCount = 0;
		int firstWarningLiveIndex = -1;
		int lastWarningLiveIndex = -1;
		int firstValidLiveIndex = -1; // Temporal: no warning and history > 0
		bool staleOutgoingVisible = false;
		bool backgroundOnlyObserved = false; // Temporal frames with history == 0
		bool hudValid = true;
		bool glValid = true;
		bool semanticChecked = false;
		// Temporal stale-playhead instrumentation (history empty but a
		// previously uploaded playhead texture is still allocated and drawn).
		int stalePlayheadFrames = 0;
		bool playheadReusedFromPreviousActivation = false; // playhead-0 hash == previous activation's last
		bool canonicalMediaChangedSincePrevious = false;
		std::string canonicalMediaAtStart;
		std::vector<int> historyAtWarning;
		std::vector<bool> rawVideoAllocatedAtWarning;
	};

	struct MediaChange {
		uint64_t frame = 0;
		std::string activeScene;
		std::string fromId;
		std::string toId;
		bool intentional = false;
	};

	struct ResourceRow {
		int cycle = 0;
		uint64_t frame = 0;
		uint64_t residentMB = 0;
		long long netAllocations = 0;
		uint64_t sceneFboAllocations = 0;
		uint64_t sceneFboSwitchReallocations = 0;
		int blobFragmentScratchW = 0, blobFragmentScratchH = 0;
		int blobBackgroundScratchW = 0, blobBackgroundScratchH = 0;
		int temporalHistory = 0, temporalCapacity = 0, temporalPlayheads = 0;
		int temporalAdapterReloads = 0;
		bool temporalHasMedia = false;
		int textureWarningsTotal = 0;
	};

	// -- helpers ----------------------------------------------------------
	void check(const std::string& name, bool ok, const std::string& detail = "", bool verbose = true);
	void runOneFrame(float dt);
	bool glBaselineOk(std::string& detail) const;
	uint64_t sceneFboHash();
	std::string legLabel() const;
	static std::string phaseName(SceneTransitionPhase phase);
	long long netLiveAllocations() const;
	uint64_t residentBytes() const;
	void captureScreenshot(const std::string& label);
	std::string viewportFingerprint() const;

	void issueSwitch();
	void onTransitionFrame();
	void onFirstIncomingFrame();
	void onDwellFrame();
	void finishLeg();
	void recordResourceRow();
	void observeTemporalFrame(int warningsThisFrame);
	void observeMediaFollow();
	void observeMidTransition();
	void runFinalValidation();
	void runShutdown();
	void writeReportTables();

	bool isTemporalDwellExtendedForMedia() const;
	bool isTemporalDwellExtendedForTransition() const;

	ExperienceRuntime& runtime_;
	std::shared_ptr<LogTap> tap_;
	std::shared_ptr<ofBaseLoggerChannel> previousChannel_;

	Phase phase_ = Phase::Warmup;
	int framesInPhase_ = 0;
	int leg_ = 0; // even: Blob -> Temporal; odd: Temporal -> Blob
	int cycle() const { return leg_ / 2; }

	// Current leg
	std::string outgoingId_, incomingId_;
	SceneManager::EntryCounters outgoingBefore_, incomingBefore_;
	ExperienceRuntime::PresentationCounters presentationBefore_;
	uint64_t frozenHash_ = 0;
	unsigned int frozenTextureId_ = 0;
	int fadeOutFrames_ = 0, loadingFrames_ = 0, fadeInFrames_ = 0;
	bool sawFirstIncoming_ = false;
	std::vector<std::string> phaseTrace_;
	ActivationRecord current_;
	bool firstIncomingHashChecked_ = false;

	// Per-frame bookkeeping
	uint64_t lastFrameNumber_ = 0;
	uint64_t lastHudDraws_ = 0, lastAssemblies_ = 0, lastBridgeDraws_ = 0;
	std::string lastVideoMediaId_;
	int adapterMismatchStreak_ = 0;
	int maxAdapterMismatchStreak_ = 0;
	int lastHistory_ = 0;
	std::string viewportBaseline_;     // blend/viewport/matrix/style snapshot after warmup
	std::string hudGeometryBaseline_;  // media-viewport mesh fingerprint after warmup
	std::string blobChangeFrom_;

	// Counts
	uint64_t liveFrameCount_ = 0, staticFrameCount_ = 0;
	uint64_t totalStatusPulls_ = 0;
	int temporalPresentEmptyFrames_ = 0, temporalPresentActiveFrames_ = 0;
	uint64_t firstPresentEmptyFrame_ = 0, firstPresentActiveFrame_ = 0;
	std::set<std::string> temporalEffectIdsSeen_;
	int blobFramesWithEffects_ = 0;
	int glFailureFrames_ = 0;
	int minActiveFramesPerActivation_ = 1 << 30;

	// Media evidence
	std::vector<MediaChange> mediaChanges_;
	bool intentionalTemporalChangeIssued_ = false;
	bool intentionalBlobChangeIssued_ = false;
	std::string temporalChangeFromId_, temporalChangeToId_;
	std::string temporalFileBefore_, temporalFileAfter_;
	int temporalReloadsBefore_ = 0;
	uint64_t temporalChangeIssueFrame_ = 0, temporalSelectionFrame_ = 0, temporalAdapterFrame_ = 0,
			 temporalHistoryClearFrame_ = 0, temporalRefillFrame_ = 0;
	int historyBeforeTemporalChange_ = 0;
	std::string blobChangeToId_;
	bool blobChangeFollowChecked_ = false;

	// Mid-transition evidence
	bool midTransitionCaptured_ = false;
	bool patternTransitionCompleted_ = false;
	uint64_t midTransitionFrame_ = 0;
	bool checkFrameAfterMidTransition_ = false;
	uint64_t playheadHash(bool& allocated);
	glm::ivec4 blobScratchBeforeTemporal_{-1, -1, -1, -1}; // fragment w,h + background w,h at Blob -> Temporal
	uint64_t lastTemporalPlayheadHash_ = 0;      // playhead 0 at the last frame of the previous Temporal activation
	std::string lastTemporalActivationMedia_;    // canonical media at that frame
	int lastPatternType_ = -1;
	int naturalPatternChanges_ = 0;
	int transitionFramesObserved_ = 0;   // frames seen in PATTERN_TRANSITION for the current pattern change
	float maxTransitionProgressObserved_ = 0.0f;

	std::vector<ActivationRecord> activations_;
	std::vector<ResourceRow> resources_;

	int failureCount_ = 0;
	int totalChecks_ = 0;

	static constexpr int kCycleCount = 20;
	static constexpr int kWarmupFrames = 60;
	static constexpr int kDwellFrames = 60;
	static constexpr int kMediaChangeCycle = 0;      // cycle 1 (0-based 0), Temporal dwell
	static constexpr int kBlobMediaChangeCycle = 1;  // cycle 2, Blob dwell
	static constexpr int kLongTemporalCycle = 9;     // cycle 10, Temporal dwell
	static constexpr int kMaxExtendedDwellFrames = 4000;
	// The production core uses TFComposition's default style weights (hard
	// cut 33 / crossfade 34 / erosion 33); a hard cut has no mid-transition
	// frame. One natural pattern transition per 90 s activation-time, so allow
	// ~4 natural transitions (~6 min at 30 fps) before giving up.
	static constexpr int kMaxTransitionDwellFrames = 11000;
	static constexpr float kProductionTransitionSeconds = 0.8f; // TFComposition default; TemporalSceneCore sets none
};
