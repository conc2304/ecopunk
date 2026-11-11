#pragma once

#include "EffectActivityStatus.h"
#include "SceneContract.h"
#include "RawRevealController.h"
#include "SceneFxModel.h"
#include "VideoEffectService.h"
#include "VideoPlaybackService.h"

#include "ofFbo.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

// SharedEffectsGalleryScene — one canonical Shared Effect per scene MODE, with at most two curated
// controls per mode, on the canonical Shared Video frame.
//
//   VideoPlaybackService frame -> (cover-fit) -> ONE canonical shared effect -> scene target -> HUD
//
// * Mode  = an effect. The existing mode action (SceneCommand::NextMode) advances to the next
//   registered effect and wraps. Mode, controls, HUD labels and the effect-activity snapshot change in
//   the SAME call, so no frame ever shows a stale label.
// * Controls = scenefx::ControlDescriptor rows (Numeric / Enum) bound to canonical parameter ids and
//   stepped through the Tier C option port (capabilities().optionAxes + stepOption) into the SAME
//   interactive HUD selector group the CRT scene uses. 0 controls = the group hides (never traps focus).
// * Identity, parameters and assets are canonical: this scene owns NO shader source, NO playlist and
//   NO effect-selection logic (a manual mode choice is the user's, not the weighted auto-selector's).
//   Only single-pass, stateless canonical effects are included; stateful/processor effects and any
//   effect that is not safeForAutomaticSelection or has piSafe == false are excluded (see roster()).
// Desktop GL 2.1 only; Raspberry Pi / GLES2 untested.
class SharedEffectsGalleryScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "shared-effects-gallery";
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	explicit SharedEffectsGalleryScene(VideoPlaybackService& video);

	void setup(const SceneServices& services) override;
	void activate() override;
	void deactivate() override;
	void update(float dt) override;
	void drawToCurrentTarget() override;

	glm::ivec2 nativeRenderSize() const override { return glm::ivec2(kNativeWidth, kNativeHeight); }
	std::string sceneId() const override { return kSceneId; }
	std::string displayName() const override { return "FX GALLERY"; }

	SceneHudStatus hudStatus() const override;
	SceneCapabilities capabilities() const override;
	bool executeCommand(SceneCommand command) override;
	bool stepOption(int axisIndex, int delta) override;

	void reset() override;
	void shutdown() override;

	// Canonical effect activity for the active mode (SceneManager's EffectActivitySource).
	std::optional<videoeffects::EffectActivityStatus> currentEffectActivitySnapshot() const;

	// Inspection / tests.
	const scenefx::ModeController& controllerForTesting() const { return *controller_; }
	bool instanceReadyForTesting() const;
	uint64_t activationCountForTesting() const { return activations_; }
	const scenefx::RawRevealController& revealForTesting() const { return reveal_; }
	// QA only: scales reveal time AFTER the dt clamp (0 freezes it). Not a visitor control.
	void setRevealTimeScaleForTesting(float s) { revealTimeScale_ = s; }

private:
	void buildRoster();
	void ensureInstance();
	void rebuildParams();

	VideoPlaybackService& video_; // not owned
	videoeffects::VideoEffectService effects_;
	std::unique_ptr<scenefx::ModeController> controller_;
	std::map<std::string, std::unique_ptr<videoeffects::VideoEffectInstance>> instances_;
	std::set<std::string> failed_;           // effect ids whose instance could not be created
	std::vector<std::string> excluded_;      // "id: reason" (logged + reported)
	videoeffects::VideoEffectParameters params_;
	bool paramsDirty_ = true;

	ofFbo srcFbo_;   // cover-fit canonical frame: the ONE source for both the raw and the effected path
	ofFbo effFbo_;   // the active canonical effect's output (reused; never reallocated per frame)
	scenefx::RawRevealController reveal_; // autonomous raw-video window (scene-local, effect-independent)
	float revealTimeScale_ = 1.f;
	float time_ = 0.f, dt_ = 1.f / 60.f;
	bool active_ = false, didSetup_ = false, didShutdown_ = false;
	uint64_t activations_ = 0;
};
