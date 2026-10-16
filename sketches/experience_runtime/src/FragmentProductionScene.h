#pragma once

#include "FragmentSceneCore.h"
#include "SceneContract.h"
#include "ShaderLibrary.h"
#include "TimeOffsetPlaybackAdapter.h"
#include "VideoPlaybackService.h"

#include <cstdint>
#include <string>

// FragmentProductionScene — Fragment Trail as the fourth real IEcopunkScene
// (RT-006). A thin adapter over fragmenttrail::FragmentSceneCore (the same
// artwork the standalone fragment-trail sketch runs), driven by
// ExperienceRuntime/SceneManager into the runtime-owned scene FBO.
//
// Media: Fragment's time-slice fragments need a history of recent frames, so
// it keeps a specialized follower — TimeOffsetPlaybackAdapter over the shared
// canonical TimeOffsetVideoBuffer, the same pattern TemporalProductionScene
// uses — fed only by the runtime's VideoPlaybackService selection (DEC-013/
// DEC-014). No local folder scan, shuffle or selection; the standalone's
// forked buffer is not compiled into the runtime at all.
//
// Effects: per-fragment ShaderLibrary effects are artwork-internal; there is
// no canonical EffectActivityStatus producer, so the scene registers without
// an effect source (HudFrameData.effects = nullopt), same as Blob/Blueprint.
//
// Not constructed here: FTParameterPanel's GUI (compiled headless — its
// defaults only), the hud_overlay dial panel, and the standalone's debug keys.
class FragmentProductionScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "fragment-trail"; // matches HudPresentationProfile's flexible profile
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;
	static constexpr int kNumPlayheads = 6; // the standalone's own playhead pool size

	explicit FragmentProductionScene(VideoPlaybackService& video);

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

	fragmenttrail::FragmentSceneCore& coreForTesting() { return core_; }
	const TimeOffsetPlaybackAdapter& videoAdapterForTesting() const { return videoAdapter_; }

private:
	void syncVideoAdapter();
	SceneSemanticData buildSemanticData() const;

	VideoPlaybackService& video_; // not owned — RuntimeServices' canonical instance
	TimeOffsetPlaybackAdapter videoAdapter_;
	TimeOffsetVideoBuffer::Settings bufferSettings_;
	ShaderLibrary shaderLib_;
	fragmenttrail::FragmentSceneCore core_;

	float activeSeconds_ = 0.0f;
	uint64_t generation_ = 0;
	int lastSpawnCount_ = 0;
	float spawnsPerSecond_ = 0.0f;

	bool hasActivatedBefore_ = false;
	bool didSetup_ = false;
	bool didShutdown_ = false;
};
