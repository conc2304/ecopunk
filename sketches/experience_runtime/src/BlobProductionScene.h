#pragma once

#include "BlobSceneCore.h"
#include "SceneContract.h"
#include "VideoPlaybackService.h"

#include <cstdint>
#include <string>

// BlobProductionScene — Blob First Complete Production Migration: the
// first real IEcopunkScene implementation hosted by SceneManager/
// ExperienceRuntime (see SceneManager.h's installProductionScene()).
//
// Delegates all real visual/detection/effect work to BlobSceneCore (the
// same reusable core the standalone blob-region-prototype dev sketch
// drives — see that class's header comment); this adapter's own job is
// strictly: implement the frozen IEcopunkScene contract, own no state
// BlobSceneCore doesn't already own, and forward canonical shared-service
// data (video) without reconstructing it.
//
// Deliberately does NOT:
//   - own a VideoPlaybackService instance — see video_'s own comment.
//   - author a canonical videoeffects::EffectActivityStatus — no real
//     effect-execution owner object currently exists for Blob (two bare
//     GUI-selected effect names, not an owned activity-tracking object);
//     see the migration report's "Effects" section. HudFrameData.effects
//     stays std::nullopt for this scene until that ownership gap is
//     closed by an Architecture-reviewed decision.
//   - advertise Regenerate (no curated/proven-safe variant table exists
//     yet) or Reset (product direction: no visible reset/restart control)
//     as scene HUD commands.
//   - draw ofxGui, debug overlays, or any local HUD — production draw is
//     background + region fragments only (BlobSceneCore::draw()).
class BlobProductionScene : public IEcopunkScene {
public:
	static constexpr const char * kSceneId = "blob-region-prototype";
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	// video: the ONE canonical VideoPlaybackService instance this runtime
	// owns (RuntimeServices::video()) — not owned here, must outlive this
	// object. Never a second/local instance: DEC-013 and this increment's
	// "no second decoder" constraint both require Blob to consume the
	// exact same instance ExperienceRuntime already publishes through
	// HudFrameData.video, not a Blob-private one that could show
	// different media than what HudFrameData.video reports.
	explicit BlobProductionScene(VideoPlaybackService & video);

	void setup(const SceneServices & services) override;
	void activate() override;
	void deactivate() override;

	void update(float dt) override;
	void drawToCurrentTarget() override;

	glm::ivec2 nativeRenderSize() const override;

	std::string sceneId() const override;
	std::string displayName() const override;

	SceneHudStatus hudStatus() const override;
	SceneCapabilities capabilities() const override;
	bool executeCommand(SceneCommand command) override;

	void reset() override;
	void shutdown() override;

	// Blob Post-Acceptance Hardening: development/test-only, Blob-local
	// resource/config instrumentation — not IEcopunkScene members, not a
	// shared contract. Every one of these forwards to an existing
	// BlobSceneCore/VideoRegionController/VideoRegionEffectRenderer getter
	// (see each method's own comment); nothing new is computed here.
	// Reached in test code via ExperienceRuntime::blobSceneForTesting().

	// VideoRegionController's own per-fragment scratch FBO pair (shared
	// across all managed fragments, not one-per-fragment — see that
	// class's own header comment). Not const: BlobSceneCore::
	// backgroundEffectRenderer() below only has a non-const overload, and
	// these are test-only diagnostic getters (always called through a
	// non-const BlobProductionScene& from the harness), so there's no
	// reason to fight for const-correctness here.
	int fragmentScratchFboWidthForTesting() { return core_.regionController().getScratchFboWidth(); }
	int fragmentScratchFboHeightForTesting() { return core_.regionController().getScratchFboHeight(); }

	// The SEPARATE VideoRegionEffectRenderer instance BlobSceneCore uses
	// for the full-frame background effect pass (backgroundMode==2) — see
	// BlobSceneCore.h's own comment on why this is a second scratch-FBO
	// pair, not shared with the fragment path above. Stays 0x0 whenever
	// backgroundMode != 2 (the production default is 1/"normal" — see
	// BlobSceneCore::BackgroundConfig), which is itself real, honest
	// information, not a bug.
	int backgroundScratchFboWidthForTesting() { return core_.backgroundEffectRenderer().getScratchWidth(); }
	int backgroundScratchFboHeightForTesting() { return core_.backgroundEffectRenderer().getScratchHeight(); }

	// The real, currently-configured production caps — BlobDetector::
	// Config::maxBlobs and VideoRegionController::Params::maxActiveFragments.
	// Both are fixed at their struct defaults in production (no GUI/command
	// path changes them for BlobProductionScene) but read live here rather
	// than duplicated as a second magic-number constant.
	int configuredMaxBlobsForTesting() const { return core_.detectionConfig().maxBlobs; }
	int configuredMaxActiveFragmentsForTesting() const { return core_.regionParams().maxActiveFragments; }

private:
	SceneSemanticData buildSemanticData() const;

	VideoPlaybackService & video_; // not owned, see constructor comment
	BlobSceneCore core_;

	// Real, scene-owned timing state for SceneSemanticData.timing — reset
	// on every activate()/reset(), never fabricated.
	float activeSeconds_ = 0.0f;
	uint64_t generation_ = 0;

	// Blob First Production Acceptance narrow patch: closes the "hidden
	// Reset" gap flagged by acceptance review — executeCommand() previously
	// had no lifecycle gating at all, so e.g. SceneCommand::Reset would
	// silently "succeed" (mutate core_ state) even if called before
	// setup() or after shutdown(). didSetup_/didShutdown_ are set once each
	// (setup()/shutdown()) and checked at the top of executeCommand(); see
	// that method's own comment. Deliberately NOT a full
	// FakeSceneLifecycleState-equivalent state machine (out of scope for a
	// narrow patch) — just enough to make command rejection in invalid
	// states correct and testable.
	bool didSetup_ = false;
	bool didShutdown_ = false;
};
