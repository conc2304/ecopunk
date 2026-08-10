#pragma once

#include "SceneContract.h"
#include "TemporalSceneCore.h"
#include "TimeOffsetPlaybackAdapter.h"
#include "VideoPlaybackService.h"

#include <cstdint>
#include <optional>
#include <string>

// TemporalProductionScene — Temporal Production Scene #2 Migration: the
// second real IEcopunkScene implementation hosted by SceneManager/
// ExperienceRuntime (see SceneManager.h's installProductionScene()),
// mirroring BlobProductionScene's own role for Blob (production scene #1).
//
// Delegates all real visual/composition/pattern/effect work to
// TemporalSceneCore (the same reusable core described in that class's own
// header comment); this adapter's own job is strictly: implement the frozen
// IEcopunkScene contract, own the Shared Video seam objects Temporal
// specifically needs (see below), and forward TemporalSceneCore's canonical
// videoeffects::EffectActivityStatus without reconstructing it.
//
// Owns (Temporal-specialized, per DEC-013/DEC-014 — see
// TimeOffsetPlaybackAdapter.h's own header comment):
//   - a TimeOffsetPlaybackAdapter (dedicated decoder/history/six playheads)
// Does NOT own:
//   - a VideoPlaybackService instance — video_ below is a reference to the
//     ONE canonical instance RuntimeServices publishes (same rule as
//     BlobProductionScene's video_ — see that class's own constructor
//     comment); there must never be a second/local instance.
//   - a TFParameterPanel/ofxGui surface, TFHudLayer, hud_overlay, or any
//     local HUD/debug presentation — production draw is TemporalSceneCore's
//     artwork only (see drawToCurrentTarget()).
//
// Deliberately does NOT advertise Regenerate (no existing curated,
// production-safe variation action was found — see this migration's
// completion report, "Deviations from prompt": TFComposition::
// forceNextPattern() is a raw pattern-forcing debug action, not a curated
// variant table, and does not qualify per this session's own instruction)
// or Reset (product direction: no visible reset/restart cinematic HUD
// control) as scene HUD commands — SceneCommand::Reset is still honored
// directly by executeCommand() for parity with IEcopunkScene::reset()/
// BlobProductionScene's own identical split.
class TemporalProductionScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "temporal-fields"; // canonical spelling — see
	                                                            // shared/src/hud-compositor/HudPresentationProfile.cpp's
	                                                            // profileId and fake/FakeHudScenarioRegistry.cpp's sceneId switch
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	// video: the ONE canonical VideoPlaybackService instance this runtime
	// owns (RuntimeServices::video()) — not owned here, must outlive this
	// object. Never a second/local instance — see class header comment.
	explicit TemporalProductionScene(VideoPlaybackService& video);

	void setup(const SceneServices& services) override;
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

	// Temporal Production Scene #2 Migration: the real, currently-owned
	// canonical effect-activity snapshot — see TemporalSceneCore::
	// effectActivityStatus() -> TFBackgroundLayer::effectActivityStatus() ->
	// TFEffectPicker::activityStatus() (DEC-015). Reached by
	// ExperienceRuntime via SceneManager's generic (non-test,
	// non-scene-ID-branching) production effect-activity source — see
	// SceneManager::installProductionScene()'s EffectActivitySource
	// parameter and ExperienceRuntime::installTemporalProductionScene().
	//
	// std::nullopt whenever no producer is actually available yet/anymore
	// (before setup() or after shutdown()) — present (possibly with empty
	// slots, per DEC-015's "raw/no-effect is present-empty" rule) at every
	// other point in this scene's lifecycle, matching TFEffectPicker's own
	// "always has a real, current, honest activity value once constructed"
	// nature; this is not gated on isActive() the way the optional
	// SceneHudStatus::semantic field is; see class header comment for
	// TemporalSceneCore's own scope note on why no evolution/timeline state
	// participates here.
	std::optional<videoeffects::EffectActivityStatus> currentEffectActivitySnapshot() const;

	// Development/test-only access to the real TemporalSceneCore/
	// TimeOffsetPlaybackAdapter instances — same rationale as
	// BlobProductionScene's own testing-only accessors: SceneManager's
	// IEcopunkScene-level public surface has no way to expose
	// Temporal-local instrumentation without widening the frozen
	// IEcopunkScene contract itself. Production code never calls these.
	TemporalSceneCore& coreForTesting() { return core_; }
	TimeOffsetPlaybackAdapter& videoAdapterForTesting() { return temporalVideoAdapter_; }

private:
	SceneSemanticData buildSemanticData() const;
	void syncVideoAdapter();

	VideoPlaybackService& video_; // not owned, see constructor comment
	TimeOffsetPlaybackAdapter temporalVideoAdapter_;
	TemporalSceneCore core_;

	// Real, scene-owned timing state for SceneSemanticData.timing — reset
	// on every activate()/reset(), never fabricated.
	float activeSeconds_ = 0.0f;
	uint64_t generation_ = 0;

	// Temporal Production Scene #2 Migration lifecycle discipline — see
	// BlobProductionScene's own identical didSetup_/didShutdown_ comment
	// for why executeCommand() must reject every command (including the
	// hidden, non-advertised Reset) before setup() has run or after
	// shutdown() has already happened.
	bool didSetup_ = false;
	bool didShutdown_ = false;

	// True once activate() has run at least once — distinguishes the
	// FIRST activation (no reactivation-staleness risk: nothing was ever
	// captured yet) from every subsequent one, which must force a fresh
	// TimeOffsetPlaybackAdapter reload/history-refill against whatever
	// media is currently canonically selected (see activate()'s own
	// comment and TimeOffsetPlaybackAdapter::invalidateForReactivation()).
	bool hasActivatedBefore_ = false;
};
