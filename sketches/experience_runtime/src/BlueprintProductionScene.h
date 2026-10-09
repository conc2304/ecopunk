#pragma once

#include "BlueprintSceneCore.h"
#include "SceneContract.h"
#include "VideoPlaybackService.h"

#include <cstdint>
#include <string>

// BlueprintProductionScene — Blueprint Emergence as the third real
// IEcopunkScene (BLUE-001). A thin adapter over BlueprintSceneCore (the same
// artwork the standalone blueprint_emergence sketch runs), driven by
// ExperienceRuntime/SceneManager into the runtime-owned scene FBO.
//
// Media: reads the runtime's single canonical VideoPlaybackService (injected,
// not owned). The standalone sketch's one-advance-per-composition-cycle hook
// is disabled here — the runtime service's own hold timer is the only media
// cadence (DEC-013/014: one selection authority, no competing cadence).
//
// Effects: per-fragment ShaderLibrary effects are artwork-internal; there is
// no canonical EffectActivityStatus producer for this scene, so it registers
// without an effect source (HudFrameData.effects = nullopt), same as Blob.
//
// Not constructed here at all: hud_overlay glitch panel, occupancy debug,
// fps/debug text, raw debug keys (they live only in the standalone ofApp).
class BlueprintProductionScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "blueprint_emergence"; // matches HudPresentationProfile's flexible profile
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	explicit BlueprintProductionScene(VideoPlaybackService& video);

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

	BlueprintSceneCore& coreForTesting() { return core_; }

private:
	SceneSemanticData buildSemanticData() const;

	VideoPlaybackService& video_; // not owned — RuntimeServices' canonical instance
	BlueprintSceneCore core_;

	float activeSeconds_ = 0.0f;
	uint64_t generation_ = 0;

	bool didSetup_ = false;
	bool didShutdown_ = false;
};
