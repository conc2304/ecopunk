#pragma once

#include "CrtPhosphorPipeline.h"
#include "CrtRegistries.h"
#include "SceneContract.h"
#include "VideoPlaybackService.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

// CrtPhosphorScene — the green-phosphor CRT treatment (shared crt::Pipeline,
// also used by sketches/crt-phosphor) as an IEcopunkScene. Sixth scene of the
// normal-run ProductionRoster::Production (after Quadrant), per explicit user
// instruction. Harness rosters (BlobTemporal/ThreeScene/Full) are unchanged.
// Starting in it is opt-in via EXPERIENCE_RUNTIME_CRT_SCENE.
//
// Media: consumes ONLY the runtime's canonical VideoPlaybackService texture
// (currentTexture()); no player, no selection, no folder scan. NextMedia /
// PreviousMedia delegate to that service. Persistence history is cleared on
// activation, media change, and reset.
//
// Selectable state (scene-owned): three INDEPENDENT axes held in crt::Selection
// - composition mode (implemented modes only: 4), pixel structure (6) and colour
// palette (8; theme_primary_secondary is hidden because no scene-accessible
// theme API exists). They drive crt::Pipeline for real. The ONLY input path that
// exists today is SceneCommand::NextMode, which steps the COMPOSITION axis
// forward (wrapping); structure/palette have no contract-preserving command yet
// (see docs/reviews/crt-selectors-tier-c-proposal.md), so they are chosen at
// startup (EXPERIENCE_RUNTIME_CRT_MODE / _STRUCTURE / _PALETTE, stable ids).
//
// HUD: hudStatus() publishes SceneHudStatus.mode (composition; presented ONCE, in
// universal.media_title) plus structure/palette SceneMetrics; the CRT HUD profile renders
// the two PIXELS / PALETTE rows as one interactive selector group (no MODE row).
// No effect-activity producer.
// Desktop GL 2.1 only; Pi/GLES2 untested.
class CrtPhosphorScene : public IEcopunkScene {
public:
	static constexpr const char* kSceneId = "crt-phosphor";
	static constexpr int kNativeWidth = 1280;
	static constexpr int kNativeHeight = 720;

	explicit CrtPhosphorScene(VideoPlaybackService& video);

	void setup(const SceneServices& services) override;
	void activate() override;
	void deactivate() override;

	void update(float dt) override;
	void drawToCurrentTarget() override;

	glm::ivec2 nativeRenderSize() const override { return glm::ivec2(kNativeWidth, kNativeHeight); }

	std::string sceneId() const override { return kSceneId; }
	std::string displayName() const override { return "PHOSPHOR"; }

	SceneHudStatus hudStatus() const override;
	SceneCapabilities capabilities() const override;
	bool executeCommand(SceneCommand command) override;
	// Axis 0 = PIXELS (structure), axis 1 = PALETTE; wraps. Dispatched by SceneManager::dispatchSceneOption.
	bool stepOption(int axisIndex, int delta) override;

	void reset() override;
	void shutdown() override;

	const crt::Selection& selectionForTesting() const { return selection_; }
	// Test/seam hook: the exact scene-side operation a future typed command would call.
	// NOT wired to any runtime input for structure/palette.
	void stepSelectionForTesting(crt::Axis a, int delta) { selection_.step(a, delta); }
	int presetIndexForTesting() const { return presetIdx_; }
	uint64_t historyClearsForTesting() const { return historyClears_; }

private:
	void clearHistory();

	VideoPlaybackService& video_; // not owned — RuntimeServices' canonical instance
	crt::Pipeline pipe_;
	std::array<crt::Params, 3> presets_;
	int presetIdx_ = 0; // look preset: fixed REFERENCE (no selector in this slice)
	crt::Selection selection_;

	float time_ = 0.f;
	float dt_ = 1.f / 60.f;
	bool active_ = false;
	bool didSetup_ = false;
	bool didShutdown_ = false;

	std::optional<std::string> lastMediaId_;
	uint64_t lastActivationCount_ = 0;
	uint64_t historyClears_ = 0;
};
