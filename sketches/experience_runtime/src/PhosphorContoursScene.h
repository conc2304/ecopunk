#pragma once

#include "PhosphorContoursPipeline.h"
#include "SceneContract.h"
#include "VideoPlaybackService.h"

#include <cstdint>
#include <optional>
#include <string>

// PhosphorContoursScene — the Phosphor Contours treatment (shared phc::Pipeline, also used by
// sketches/phosphor-contours) as an IEcopunkScene.
//
// ROSTER STATUS: the EIGHTH scene of the default Production roster (after the Shared Effects Gallery), registered
// with no flag; approved by the user. EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_SCENE only chooses it as the startup
// scene (see ExperienceRuntime::installPhosphorContoursProductionScene). Harness rosters are unchanged.
//
// Media: consumes ONLY the runtime's canonical VideoPlaybackService texture (currentTexture()); no player,
// no selection, no folder scan. NextMedia / PreviousMedia delegate to that service. Persistence history is
// cleared on activation, media change, reset/reseed and resize.
//
// Controls (existing SceneCommand + Tier C option-axis pathways only; no new command, no raw parameters):
//   NextMode   -> next MODE of four, wraps: TOPOLOGY (Hybrid) / ISOLINES (Isophote, sparse) /
//                 SCANLINES (RasterRelief) / STRATA (Isophote, clean topographic bands)
//   stepOption axis 0 = DENSITY (SPARSE / MEDIUM / DENSE: contour line count), wraps
//   stepOption axis 1 = DECAY   (CRISP / TRAIL / SMEAR: persistence, dropout, row slip, glow), wraps
//   Regenerate -> reseed (deterministic: seed N reproduces the same dropout/jitter/warp phase)
//   Reset      -> mode 0, DENSITY MEDIUM, DECAY TRAIL, seed 1 (the identity steps leave a mode's own preset untouched)
// HUD: SceneHudStatus.mode (mode) + health + message, plus two selector-row metrics (DENSITY / DECAY) that the
// data-only `phosphor-contours` presentation profile shows in the same interactive selector group the CRT and
// Shared Effects Gallery scenes use. No scene-specific branch in shared HUD rendering.
// Desktop GL 2.1 only; Pi/GLES2 untested. Quality profile: Desktop by default, PiSafe via
// EXPERIENCE_RUNTIME_PHOSPHOR_CONTOURS_QUALITY=pisafe (unmeasured on Pi 3B+).
class PhosphorContoursScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "phosphor-contours";
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	explicit PhosphorContoursScene(VideoPlaybackService& video);

	void setup(const SceneServices& services) override;
	void activate() override;
	void deactivate() override;

	void update(float dt) override;
	void drawToCurrentTarget() override;

	glm::ivec2 nativeRenderSize() const override { return glm::ivec2(width_, height_); }

	std::string sceneId() const override { return kSceneId; }
	std::string displayName() const override { return "CONTOURS"; }

	SceneHudStatus hudStatus() const override;
	SceneCapabilities capabilities() const override;
	bool executeCommand(SceneCommand command) override;
	// Axis 0 = DENSITY, axis 1 = DECAY; wraps. Dispatched by SceneManager::dispatchSceneOption.
	bool stepOption(int axisIndex, int delta) override;

	void reset() override;
	void shutdown() override;

	// --- test/seam hooks ------------------------------------------------------------------------
	int presetIndexForTesting() const { return presetIdx_; }
	phc::Algorithm algorithmForTesting() const { return look_.algorithm; }
	int densityForTesting() const { return density_; }
	int decayForTesting() const { return decay_; }
	const phc::Params& lookForTesting() const { return look_; }
	uint32_t seedForTesting() const { return seed_; }
	uint64_t historyClearsForTesting() const { return historyClears_; }
	bool pipelineUsableForTesting() const { return pipe_.usable(); }
	bool pipelineAllocatedForTesting() const { return pipe_.allocated(); }
	int pipelineWidthForTesting() const { return pipe_.width(); }
	// Resize the scene's internal targets (e.g. a different native size). Clears history; never stale.
	void resizeForTesting(int w, int h) { resizeTargets(w, h); }
	phc::Pipeline& pipelineForTesting() { return pipe_; }
	// GL state probe: when on, drawToCurrentTarget() compares blend/scissor/depth/viewport/FBO/program before and
	// after and counts differences (the scene must leave the HUD's GL state exactly as it found it).
	void enableGlStateProbeForTesting() { glProbe_ = true; }
	int glProbeSamplesForTesting() const { return glProbeSamples_; }
	int glStateMismatchesForTesting() const { return glMismatches_; }

private:
	void clearHistory();
	void resizeTargets(int w, int h);
	void refreshLook() { look_ = phc::resolveLook(presetIdx_, density_, decay_); }

	VideoPlaybackService& video_; // not owned — RuntimeServices' canonical instance
	phc::Pipeline pipe_;
	phc::Params look_; // resolved mode + DENSITY + DECAY
	int presetIdx_ = 0; // MODE
	int density_ = 1, decay_ = 1; // selector axes; 1 = identity (Medium / Trail)
	uint32_t seed_ = 1;
	phc::Quality quality_ = phc::Quality::Desktop;
	int width_ = kNativeWidth, height_ = kNativeHeight;

	float time_ = 0.f;
	float dt_ = 1.f / 60.f;
	bool active_ = false;
	bool didSetup_ = false;
	bool didShutdown_ = false;
	bool haveFrame_ = false; // a valid video texture was drawn this frame

	std::optional<std::string> lastMediaId_;
	uint64_t lastActivationCount_ = 0;
	uint64_t historyClears_ = 0;
	bool glProbe_ = false;
	int glProbeSamples_ = 0, glMismatches_ = 0;
};
