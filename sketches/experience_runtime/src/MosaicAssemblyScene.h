#pragma once

#include "MosaicEngine.h"

#include "EffectActivityStatus.h"
#include "SceneContract.h"
#include "VideoEffectService.h"
#include "VideoPlaybackService.h"

#include "ofFbo.h"
#include "ofMesh.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

// MosaicAssemblyScene — a mosaic of regular square cells whose tiles carry crops of the current shared video
// (mirrored, duplicated, magnified), source-derived or authored solid colours, or stand vacant over one of four
// backgrounds (video / desaturated / black / palette). Tiles slide, fade, swap, grow, subdivide and mirror in
// 13 modes. Independent IEcopunkScene: NOT a Shred or Blueprint mode, and it does not depend on either.
//
// All composition lives in MosaicEngine.h (OF-free, unit-tested). This adapter owns only: the canonical video
// texture consumption (VideoPlaybackService, read-only; NextMedia/PreviousMedia delegate to it), cover-fit /
// desaturated source FBOs, one tiny 16x9 colour map, and the batched tile renderer (<= 8 draw calls/frame).
//
// HUD dials: the ACTIVE mode's two dials use the same production pipeline as FX Gallery / CRT Phosphor /
// Phosphor Contours — capabilities().optionAxes + stepOption (Tier C option port, no new command), per-row scene
// metrics (scene.mosaic.metric.c0/c1_label|value|pos) and a data-only `mosaic-assembly` presentation profile in the
// shared interactive selector group. No debug window, no developer overlay.
//
// Effects: none. The default look uses the canonical `desaturate` instance only as the desaturated look; the scene
// registers without an EffectActivityStatus source (HudFrameData.effects = nullopt), like Blueprint/Blob.
// Desktop GL 2.1 validated only; Pi 3B+ not measured (EXPERIENCE_RUNTIME_MOSAIC_QUALITY=pi = half-size FBOs, slower colour sampling).
class MosaicAssemblyScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "mosaic-assembly";
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	explicit MosaicAssemblyScene(VideoPlaybackService& video);

	void setup(const SceneServices& services) override;
	void activate() override;
	void deactivate() override;
	void update(float dt) override;
	void drawToCurrentTarget() override;

	glm::ivec2 nativeRenderSize() const override { return glm::ivec2(kNativeWidth, kNativeHeight); }
	std::string sceneId() const override { return kSceneId; }
	std::string displayName() const override { return "MOSAIC ASSEMBLY"; }

	SceneHudStatus hudStatus() const override;
	SceneCapabilities capabilities() const override;
	bool executeCommand(SceneCommand command) override;
	// Axis 0/1 = the ACTIVE mode's two dials, 11 detents (0..10 -> normalized 0..1), clamped.
	bool stepOption(int axisIndex, int delta) override;

	void reset() override;
	void shutdown() override;

	mosaic::Engine& engineForTesting() { return engine_; }

private:
	void renderSource(const ofTexture* tex);
	void sampleColors();
	void drawBackground(mosaic::Bg bg, float alpha);
	void drawTiles();

	VideoPlaybackService& video_;
	mosaic::Engine engine_;
	videoeffects::VideoEffectService effects_;
	std::unique_ptr<videoeffects::VideoEffectInstance> desat_;
	bool pi_ = false;
	int rw_ = kNativeWidth, rh_ = kNativeHeight;
	ofFbo srcFbo_, desatFbo_, colorFbo_;
	ofMesh solidMesh_, texMesh_;
	std::string lastMediaId_;
	float time_ = 0, dt_ = 1.f / 60.f, colorClock_ = 0;
	bool active_ = false, didSetup_ = false, didShutdown_ = false, haveFrame_ = false, desatOk_ = false;
	uint64_t generation_ = 0;
};
