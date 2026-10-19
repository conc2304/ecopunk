#pragma once

#include "QuadrantSceneCore.h"
#include "QuadrantSemanticMapping.h"
#include "SceneContract.h"
#include "VideoPlaybackService.h"

#include <cstdint>
#include <string>

// QuadrantProductionScene — Quadrant Crosshair as a real IEcopunkScene. A
// thin adapter over quadrantcrosshair::QuadrantSceneCore (the same artwork the
// standalone quadrant-crosshair sketch runs), driven by ExperienceRuntime/
// SceneManager into the runtime-owned scene FBO.
//
// Media: an ordinary Shared Video consumer — the runtime's canonical
// VideoPlaybackService's current texture/pixels/isFrameNew feed the core
// directly (no specialized decoder; the standalone's VideoSystem scanner is
// not compiled into the runtime). A new canonical media identity triggers the
// core's media-changed path (erosion cleared, HUD-layer copy re-labelled).
//
// Effects: each quadrant's shader deck is artwork-internal (scene-local
// ShaderLibrary over canonical shader files; fixed defaults already from the
// shared catalog). There is no canonical EffectActivityStatus producer, so the
// scene registers without an effect source (HudFrameData.effects = nullopt).
//
// HUD: the in-artwork hud:: layer (contours, hex grid, node network,
// reticles, flow field, crosshair scanner, glitch tears) is drawn; its
// information layer (telemetry cards, media status, log, slot legend) is not —
// the production HUD's four-channel strip and media slot carry that.
//
// Not constructed here: DebugMode, the 'o' hud_overlay takeover, dev keys.
class QuadrantProductionScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "quadrant-crosshair"; // matches HudPresentationProfile's flexible profile
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	explicit QuadrantProductionScene(VideoPlaybackService& video);

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

	quadrantcrosshair::QuadrantSceneCore& coreForTesting() { return core_; }
	// Installation ENERGY v0.1 — ExperienceRuntime pushes the smoothed
	// runtime ENERGY each frame (concrete seam; IEcopunkScene has no
	// parameter API). Re-applied after activation's composition reset.
	void setEnergy(float energy01) { core_.crosshair().setEnergy(energy01); }
	int mediaChangesSeenForTesting() const { return mediaChangesSeen_; }
	const std::string& observedMediaIdForTesting() const { return observedMediaId_; }

private:
	quadrantcrosshair::QuadrantSceneCore::VideoFrame canonicalFrame() const;
	void observeCanonicalMedia(bool notifyCore);
	quadrantsemantics::Inputs semanticInputs() const;
	SceneSemanticData buildSemanticData() const;

	VideoPlaybackService& video_; // not owned — RuntimeServices' canonical instance
	quadrantcrosshair::QuadrantSceneCore core_;

	std::string observedMediaId_;
	int mediaChangesSeen_ = 0;
	float activeSeconds_ = 0.0f;
	uint64_t generation_ = 0;
	std::string lastPrimaryStateId_;
	float stateElapsedSeconds_ = 0.0f;

	bool didSetup_ = false;
	bool didShutdown_ = false;
};
