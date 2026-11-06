#pragma once

#include "CrosshairSystem.h"
#include "FTBackgroundLayer.h"
#include "FTFragmentPool.h"
#include "FTModeController.h"
#include "FTOverlayDirector.h"
#include "FTParameterPanel.h"
#include "FTVideoTypes.h"
#include "HudOverlayDialState.h"
#include "HudOverlayLayer.h"
#include "LFOBank.h"
#include "TriggerBus.h"

#include <string>

namespace fragmenttrail {

// FragmentSceneCore — the Fragment Trail artwork, independent of who owns
// the window or the media. The standalone sketch (ofApp) and
// ExperienceRuntime's FragmentProductionScene both run this same core:
// crosshair -> trigger bus -> mode controller -> fragment pool, over a
// dimmed split-video backdrop, with the hud_overlay "room" (ambient atoms
// below, organisms driven by real fragment events above) composited live.
//
// Media is borrowed, never chosen here: the caller owns the
// TimeOffsetVideoBuffer (the standalone's local fork, or in production the
// canonical-follower buffer inside TimeOffsetPlaybackAdapter) and the
// ShaderLibrary used for per-fragment effects.
//
// Not owned here: ofxGui panels (FTParameterPanel's GUI, the overlay dial
// panel) and raw debug keys — those stay in the standalone ofApp.
class FragmentSceneCore {
public:
	struct Config {
		int canvasW = 1280;
		int canvasH = 720;
		std::string backgroundsFolder = "backgrounds";
		std::string bgVertShaderPath = "shaders/vert.glsl";
		std::string bgShaderPath = "shaders/bg_dim.glsl";
		int numPlayheads = 6;
		// Standalone: the canvas follows the live window (ofGetWidth()/
		// ofGetHeight(), exactly as before the core existed). Production: fixed
		// at canvasW x canvasH, the scene FBO's native size.
		bool trackWindow = false;
		// Canvas-pixel area the viewer actually sees (production: the part
		// of the canvas the cover-cropped HUD media viewport shows). Empty =
		// whole canvas. Keeps the overlay's corner radar on screen.
		ofRectangle safeArea;
	};

	// Same initial overlay dials the standalone sketch has always started
	// with (seeded into the standalone's dial panel too).
	static hudoverlay::HudOverlayDialState initialOverlayDials();

	void setup(TimeOffsetVideoBuffer* video, ShaderLibrary* shaders, const Config& config);

	// Fresh trail: no fragments, playheads released, crosshair/trigger/mode/
	// overlay state rebuilt — nothing drawn before deactivation survives.
	void activate();
	void deactivate() { active_ = false; }

	void update(float dt);
	void draw(); // into the currently bound target; artwork only

	// Standalone window resize (only meaningful with Config::trackWindow).
	void windowResized(int w, int h) { hudOverlay_.windowResized(static_cast<float>(w), static_cast<float>(h)); }

	bool didSetup() const { return didSetup_; }
	bool isActive() const { return active_; }

	FTParameterPanel& parameters() { return params_; }
	hudoverlay::HudOverlayDialState& overlayDials() { return dials_; }
	CrosshairSystem& crosshair() { return crosshair_; }
	FTModeController& modeController() { return modeController_; }
	const CrosshairSystem& crosshair() const { return crosshair_; }
	const CrosshairSystem& crosshair2() const { return crosshair2_; }
	const FTModeController& modeController() const { return modeController_; }
	const FTFragmentPool& pool() const { return pool_; }
	const FTParameterPanel& parameters() const { return params_; }
	const hudoverlay::HudOverlayLayer& overlay() const { return hudOverlay_; }

	// Crosshair speed mapped through TriggerBus's own high/low thresholds to
	// 0..1 — the same "energy" the spawn-cadence speed response uses.
	float crosshairEnergy01() const;

	// Seconds since the current crosshair preset was applied (activation or
	// a preset change).
	float presetElapsedSeconds() const { return presetElapsed_; }

private:
	void resetComposition();
	void applyCanvasSize();
	void updateRadarContacts();

	Config config_;
	TimeOffsetVideoBuffer* video_ = nullptr;
	ShaderLibrary* shaders_ = nullptr;

	CrosshairSystem crosshair_;
	CrosshairSystem crosshair2_; // drives the second parallel trail
	TriggerBus triggerBus_;
	LFOBank lfoBank_;
	FTFragmentPool pool_;
	FTModeController modeController_;
	FTParameterPanel params_;
	FTBackgroundLayer backgroundLayer_;
	hudoverlay::HudOverlayLayer hudOverlay_;
	hudoverlay::HudOverlayDialState dials_;
	FTOverlayDirector overlayDirector_;

	int lastPreset_ = -1;
	float presetElapsed_ = 0.0f;
	bool didSetup_ = false;
	bool active_ = false;
};

} // namespace fragmenttrail
