#pragma once

#include "RawVideoAdjust.h"
#include "SceneContract.h"
#include "VideoPlaybackService.h"

#include "ofFbo.h"
#include "ofShader.h"

#include <string>

// RawVideoScene — the canonical Shared Video frame, shown RAW (no stylistic effect), with three live
// picture dials: BRIGHTNESS / CONTRAST / SATURATION.
//
// ROSTER STATUS: LAST scene of the default Production roster (always last; see ExperienceRuntime::installProductionScenes), registered with no
// flag. EXPERIENCE_RUNTIME_RAW_VIDEO_SCENE only chooses it as the startup scene (see
// ExperienceRuntime::installRawVideoProductionScene). Harness rosters are unchanged.
//
// Media: consumes ONLY the runtime's canonical VideoPlaybackService texture (currentTexture()); no player,
// no selection, no folder scan. NextMedia / PreviousMedia delegate to that service. At the identity dial
// position the output is the source frame, cover-fit, unmodified.
//
// Controls (existing pathways only; no new command, no contract change):
//   stepOption axis 0 = BRIGHTNESS, axis 1 = CONTRAST, axis 2 = SATURATION (11 detents, centre = identity,
//   CLAMPED at the ends).  Reset (honoured, not advertised) returns every dial to its centre.
// HUD: three selector rows (caption / value / position) published as scene metrics; the data-only
// `raw-video` presentation profile shows them as the interactive selector group in universal.effect_summary
// — the same focusable slot CRT / Shared Effects Gallery / Contours use (Enter to capture, Up/Down row,
// Left/Right value). No scene-specific branch in shared HUD rendering.
//
// The adjust pass is a scene-local single-pass shader with the same maths as
// shared/assets/video-effects/utility/video_adjust.glsl (which is not a cataloged canonical effect — see the
// implementation report). Desktop GL 2.1 only; Pi/GLES2 untested.
class RawVideoScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "raw-video";
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	explicit RawVideoScene(VideoPlaybackService& video);

	void setup(const SceneServices& services) override;
	void activate() override;
	void deactivate() override;

	void update(float dt) override;
	void drawToCurrentTarget() override;

	glm::ivec2 nativeRenderSize() const override { return glm::ivec2(kNativeWidth, kNativeHeight); }

	std::string sceneId() const override { return kSceneId; }
	std::string displayName() const override { return "RAW VIDEO"; }

	SceneHudStatus hudStatus() const override;
	SceneCapabilities capabilities() const override;
	bool executeCommand(SceneCommand command) override;
	// Axis 0 = BRIGHTNESS, 1 = CONTRAST, 2 = SATURATION; clamped. Dispatched by SceneManager::dispatchSceneOption.
	bool stepOption(int axisIndex, int delta) override;

	void reset() override;
	void shutdown() override;

	// --- test/seam hooks ---------------------------------------------------------------------------
	const rawvideo::Dials& dialsForTesting() const { return dials_; }
	bool shaderReadyForTesting() const { return shaderOk_; }

private:
	bool compileShader();

	VideoPlaybackService& video_; // not owned — RuntimeServices' canonical instance
	rawvideo::Dials dials_;
	ofFbo srcFbo_;
	ofShader adjust_;
	bool shaderOk_ = false;

	float time_ = 0.f;
	bool active_ = false;
	bool didSetup_ = false;
	bool didShutdown_ = false;
	bool haveFrame_ = false; // a valid video texture was drawn this frame
};
