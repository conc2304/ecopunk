#pragma once

#include "IVideoDecoder.h"
#include "MediaCatalog.h"
#include "VideoPlaybackStatus.h"
#include "VideoSelectionPolicy.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// ============================================================================
// VideoPlaybackService.h — the single shared owner of video playback for
// the Ecopunk runtime, per Implement-Shared-Video-Playback-System-Agent-
// Prompt.md §2/§7.
//
//   ExperienceRuntime
//   └── RuntimeServices
//       └── VideoPlaybackService   <- this class
//           ├── canonical media catalog       (MediaCatalog)
//           ├── media-root resolution         (Config::mediaRoot, this class)
//           ├── playlist / selection history  (VideoSelectionPolicy)
//           ├── automatic selection policy    (VideoSelectionPolicy)
//           ├── hold timer                    (VideoSelectionPolicy)
//           ├── previous/next semantics       (this class + VideoSelectionPolicy)
//           ├── active decoder                (IVideoDecoder / OfVideoDecoder)
//           ├── media health                  (this class)
//           └── immutable VideoPlaybackStatus (this class, status())
//
// Ownership is split three ways on purpose:
//   - VideoSelectionPolicy: pure, dependency-free, unit-tested directly
//     (test/video_selection_policy_tests.cpp).
//   - IVideoDecoder/OfVideoDecoder: the one piece that talks to a real
//     ofVideoPlayer; swappable for a test double per §14.5 (not exercised
//     by a fake-decoder harness in this increment — see this increment's
//     implementation report).
//   - VideoPlaybackService itself: the retry-on-failure orchestration
//     between the two, plus catalog/root setup and status assembly. This
//     is the "smallest amount of glue that has to be OF-dependent."
//
// A scene adapter never touches MediaCatalog, VideoSelectionPolicy, or
// IVideoDecoder directly — only this class's public surface (§7's "Do not
// weaken these requirements" list: one clear owner, immutable status,
// texture/pixel access, previous/next, hold timing, health, explicit
// shutdown, no destructive status getters).
// ============================================================================

class VideoPlaybackService {
public:
	struct Config {
		std::string mediaRoot;   // scanned for playable files (.mp4 today)
		std::string catalogPath; // optional; empty = auto-synthesized catalog only
		float holdDurationSeconds = 30.0f;  // minimum dwell per selection
		float maxDwellSeconds = 120.0f;     // long-clip viewing window (see VideoSelectionPolicy.h)
		float longClipThresholdSeconds = 0.0f; // <= 0: 2 x maxDwellSeconds
		bool automaticAdvance = true;
	};

	// decoder: injectable for tests/DI. Null (the production default)
	// means setup() constructs a real OfVideoDecoder internally.
	explicit VideoPlaybackService(std::unique_ptr<IVideoDecoder> decoder = nullptr);

	// Always returns true unless config itself is unusable (currently:
	// only an empty mediaRoot) — "catalog loading must not crash the
	// runtime" (§5.1/§12) means an empty/missing/all-invalid media root is
	// a normal, handled outcome reflected in status().health, not a
	// setup() failure. See this class's .cpp for the exact validation/
	// recovery sequence.
	bool setup(const Config& config);

	void update(float dt);
	void shutdown();

	// Both reject cleanly (return false, no state change) when navigation
	// is not currently possible — see VideoPlaybackStatus::canSelectPrevious/
	// canSelectNext, which reflect exactly the conditions checked here.
	bool next();
	bool previous();

	// Cadence: true (the default, from Config::automaticAdvance) = timer-
	// driven automatic advance; false = the caller drives media changes with
	// next() at its own boundaries. Selection, history, resume windows and
	// manual navigation are identical in both. Re-enabling starts a fresh
	// dwell window — see VideoSelectionPolicy::setAutomaticAdvanceEnabled().
	void setAutomaticAdvanceEnabled(bool enabled) { policy_.setAutomaticAdvanceEnabled(enabled); }
	// The boundary-driven counterpart of the hold timer: the exact same
	// automatic-advance selection (and MediaSelectionOrigin::Automatic, not a
	// user's ManualNext), triggered at the caller's own boundary — e.g.
	// Blueprint's composition-cycle start. Works whether or not the timer is on.
	bool advanceAtBoundary();
	bool automaticAdvanceEnabled() const { return policy_.automaticAdvanceEnabled(); }
	// Installation Controls v0.1 — see VideoSelectionPolicy::setMinimumDwellSeconds().
	void setMinimumDwellSeconds(float seconds) { policy_.setMinimumDwellSeconds(seconds); }
	float minimumDwellSeconds() const { return policy_.minimumDwellSeconds(); }

	const ofTexture* currentTexture() const;
	const ofPixels* currentPixels() const;
	bool isFrameNew() const;
	glm::ivec2 sourceSize() const;

	// The absolute, on-disk path of the currently active media item —
	// nullopt if none is active. This is the SAME path this service's own
	// decoder was loaded with (see activateCandidate()'s
	// ofFilePath::join(mediaRoot_, item.relativePath)), just exposed
	// read-only rather than recomputed a second time elsewhere.
	//
	// Added for DEC-014 (Temporal Playback / History Boundary): a
	// specialized consumer adapter (e.g.
	// shared/src/video-playback/adapters/TimeOffsetPlaybackAdapter.h) needs
	// to explicit-load the exact file this service selected, and
	// VideoPlaybackStatus::mediaId is a one-way synthesized identity
	// (MediaCatalog::synthesizeMediaId() hashes relativePath — see that
	// function's own comment) that cannot be reversed back into a path by
	// any caller outside this class. Exposing the already-computed path
	// here is the only alternative to a second, duplicate, unowned
	// path-resolution algorithm living in a consumer adapter — which would
	// itself be a second source of truth for media identity, exactly what
	// DEC-013 forbids. This method changes no ownership, no selection
	// semantics, and no VideoPlaybackStatus field — it is purely additive
	// read access to information this class already computes internally.
	std::optional<std::string> currentAbsolutePath() const;

	VideoPlaybackStatus status() const;

	// Diagnostics/tests only — not part of VideoPlaybackStatus.
	const VideoSelectionPolicy& selectionPolicyForTesting() const { return policy_; }

private:
	// Long-clip resume windows (VideoSelectionPolicy owns the state; this
	// class only feeds it the decoder's playhead and applies the seek).
	std::optional<float> currentPositionSeconds() const;
	void beginPlaybackIfReady();
	void traceResume(const std::string& line) const;

	bool activateCandidate(int catalogIndex);
	// Shared retry-on-failure loop for startup/next/previous/automatic
	// advance. firstCandidateFn supplies the first index to try;
	// on failure, requestRecoveryCandidate() supplies fallbacks up to one
	// full pass over the playable pool (§12). The final successful
	// activation is reported with origin `Recovery` if any prior attempt
	// this call failed, else `baseOrigin` — see VideoPlaybackStatus.h's
	// MediaSelectionOrigin.
	bool attemptSelection(const std::function<std::optional<int>()>& firstCandidateFn, MediaSelectionOrigin baseOrigin);

	std::string mediaRoot_;
	std::vector<MediaMetadata> catalog_;
	VideoSelectionPolicy policy_;
	std::unique_ptr<IVideoDecoder> decoder_;

	VideoPlaybackHealth health_ = VideoPlaybackHealth::Unavailable;
	std::optional<std::string> messageId_;
	std::optional<std::string> errorCode_;

	bool setupComplete_ = false;
	bool resumeTrace_ = false; // EXPERIENCE_RUNTIME_VIDEO_RESUME_TRACE — verification only
};
