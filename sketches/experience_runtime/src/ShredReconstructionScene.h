#pragma once

#include "ShredEngine.h"

#include "EffectActivityStatus.h"
#include "SceneContract.h"
#include "VideoEffectService.h"
#include "VideoPlaybackService.h"

#include "ofFbo.h"
#include "ofMesh.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// ShredReconstructionScene — a machine that keeps trying to rebuild the playing video from paper-shred-like
// fragments and never quite succeeds. Independent IEcopunkScene (NOT a Blueprint Emergence submode).
//
//   canonical VideoPlaybackService frame -> cover-fit srcFbo -> canonical `desaturate` (the default look)
//        -> fragments (batched meshes, GPU UV crops) ; up to TWO canonical non-default effects, each
//        rendered once into its own FBO and revealed only through <= 25% (conservative destination area) of fragments.
//
// All composition logic lives in ShredEngine.h (OF-free, unit-tested). This adapter owns only: canonical
// media/effect consumption, FBOs, and rendering.
//
// Authorities: media = RuntimeServices' VideoPlaybackService (read-only here; NextMedia/PreviousMedia delegate);
// effects = the shared VideoEffectService/knowledge base (own service instance, same pattern as
// SharedEffectsGalleryScene; no local catalog); effect activity = currentEffectActivitySnapshot() ->
// SceneManager's generic EffectActivitySource -> HudFrameData.effects.
//
// Modes: 13 (see ShredEngine.h modeSpecs()). NextMode (existing SceneCommand) steps modes; autonomous
// cycling runs with no GUI. The active mode's two dials are HUD selector rows (scene.shred.metric.c0/c1_*, data-only
// `shred-reconstruction` presentation profile), stepped via stepOption. There is no debug window or overlay. Regenerate = reseed.
//
// Desktop GL 2.1 validated only. Pi 3B+ not measured: EXPERIENCE_RUNTIME_SHRED_QUALITY=pi selects a Pi-safe
// profile (fewer fragments, 0.5 render scale, piSafe==true-only effects).
class ShredReconstructionScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "shred-reconstruction";
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	explicit ShredReconstructionScene(VideoPlaybackService& video);

	void setup(const SceneServices& services) override;
	void activate() override;
	void deactivate() override;
	void update(float dt) override;
	void drawToCurrentTarget() override;

	glm::ivec2 nativeRenderSize() const override { return glm::ivec2(kNativeWidth, kNativeHeight); }
	std::string sceneId() const override { return kSceneId; }
	std::string displayName() const override { return "SHRED RECONSTRUCTION"; }

	SceneHudStatus hudStatus() const override;
	SceneCapabilities capabilities() const override;
	bool executeCommand(SceneCommand command) override;
	// Axis 0/1 = the ACTIVE mode's two dials, 11 detents (0..10 -> normalized 0..1), clamped. Same Tier C option port
	// (capabilities().optionAxes + stepOption) CRT Phosphor / FX Gallery / Raw Video use; no new command.
	bool stepOption(int axisIndex, int delta) override;

	void reset() override;
	void shutdown() override;

	// Canonical effect activity (SceneManager's EffectActivitySource). nullopt before setup / after shutdown;
	// present with EMPTY slots when no non-default effect is visible ("present, zero active").
	std::optional<videoeffects::EffectActivityStatus> currentEffectActivitySnapshot() const;

	shred::Engine& engineForTesting() { return engine_; }
	const std::vector<std::string>& effectCandidatesForTesting() const { return candidates_; }

private:
	void buildCandidates();
	void ensureEffect(const std::string& id);
	void renderSource(const ofTexture* tex);
	void drawFragments();

	VideoPlaybackService& video_;
	shred::Engine engine_;
	videoeffects::VideoEffectService effects_;
	std::map<std::string, std::unique_ptr<videoeffects::VideoEffectInstance>> instances_;
	std::vector<std::string> candidates_;
	std::vector<std::string> excluded_;
	bool pi_ = false;
	float renderScale_ = 1.f;
	int rw_ = kNativeWidth, rh_ = kNativeHeight;

	ofFbo srcFbo_, desatFbo_, effFbo_[2];
	std::string effFboId_[2];

	ofMesh mesh_;
	std::string lastMediaId_;
	float time_ = 0, dt_ = 1.f / 60.f;
	bool active_ = false, didSetup_ = false, didShutdown_ = false, haveFrame_ = false, desatOk_ = false;
	uint64_t generation_ = 0;
};
