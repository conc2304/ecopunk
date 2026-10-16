#pragma once

#include "CrosshairSystem.h"
#include "ExpansionDirector.h"
#include "GridState.h"
#include "HudManager.h"
#include "LFOBank.h"
#include "MotionExtraction.h"
#include "QuadrantManager.h"
#include "ShaderLibrary.h"
#include "TriggerBus.h"

#include <string>

namespace quadrantcrosshair {

// QuadrantSceneCore — the Quadrant Crosshair artwork, independent of who owns
// the window or the media. The standalone sketch (ofApp) and
// ExperienceRuntime's QuadrantProductionScene both run this same core:
// the crosshair divides the canvas into four quadrants, each cycling its own
// shader deck over the live video with an erosion trail; triggers from the
// crosshair's motion re-deal effects; the expansion director periodically
// sends the crosshair to a corner and grows one quadrant; motion extraction
// glows underneath; the in-artwork hud:: layer (HudManager) sits on top.
//
// Media is borrowed, never chosen here: the caller passes the current video
// frame each update (the standalone's VideoSystem, or in production the
// canonical VideoPlaybackService) and reports media changes.
//
// Not owned here: the standalone's DebugMode panel, the 'o' hud_overlay
// takeover mode, and raw debug keys — those stay in the standalone ofApp.
class QuadrantSceneCore {
public:
	struct Config {
		int canvasW = 1280;
		int canvasH = 720;
		// Standalone: the canvas follows the live window (exactly as the old
		// ofGetWidth()/ofGetHeight() calls did). Production: fixed at
		// canvasW x canvasH, the scene FBO's native size.
		bool trackWindow = false;
		ShaderLibrary::AssetLayout shaderLayout = ShaderLibrary::AssetLayout::Standalone;
		std::string motionVertShader = "shaders/vert.glsl";
		std::string motionAccumShader = "shaders/motion_accum.glsl";
		std::string motionExtractShader = "shaders/motion_extract.glsl";
		// HudManager's information layer (see HudManager::showInformationLayer).
		bool showHudInformationLayer = true;
	};

	// The current video frame, borrowed for this update only.
	struct VideoFrame {
		const ofTexture* texture = nullptr;
		const ofPixels* pixels = nullptr;
		bool isFrameNew = false;
	};

	void setup(const Config& config);

	// Fresh composition: idle quadrants with re-dealt decks and cleared
	// erosion, cleared motion history, crosshair/trigger/expansion/HUD-layer
	// state rebuilt — nothing drawn before deactivation survives.
	void activate();
	void deactivate() { active_ = false; }

	// The media changed (standalone: VideoSystem::fileChanged(); production:
	// a new canonical media identity). Clears erosion and re-labels the
	// HUD layer's nature copy, as the standalone always has.
	void onMediaChanged(const std::string& mediaName);

	void update(float dt, const VideoFrame& frame);
	void draw(const VideoFrame& frame); // into the currently bound target; artwork only

	// Standalone window resize (only meaningful with Config::trackWindow).
	void windowResized(int w, int h) { quadrants_.resize(w, h); }

	bool didSetup() const { return didSetup_; }
	bool isActive() const { return active_; }

	ShaderLibrary& shaderLibrary() { return shaders_; } // standalone DebugMode shares it
	CrosshairSystem& crosshair() { return crosshair_; }
	ExpansionDirector& expansionDirector() { return expansionDirector_; }
	MotionExtraction& motionExtraction() { return motionEx_; }
	HudManager& hudLayer() { return hud_; }
	bool& showHudLayer() { return showHud_; }
	int& motionOverlayAlpha() { return motionOverlayAlpha_; }
	const CrosshairSystem& crosshair() const { return crosshair_; }
	const ExpansionDirector& expansionDirector() const { return expansionDirector_; }
	const QuadrantManager& quadrants() const { return quadrants_; }
	const MotionExtraction& motionExtraction() const { return motionEx_; }

private:
	void resetComposition();
	void applyCanvasSize();
	static float sampleBrightness(const ofPixels& px);

	Config config_;
	ShaderLibrary shaders_;
	LFOBank lfo_;
	GridState grid_;
	CrosshairSystem crosshair_;
	TriggerBus triggerBus_;
	QuadrantManager quadrants_;
	MotionExtraction motionEx_;
	ExpansionDirector expansionDirector_;
	HudManager hud_;

	float steeringBrightness_ = 0.5f;
	int motionOverlayAlpha_ = 55;
	bool showHud_ = true;
	float motionStillTimer_ = 0.f; // time spent below the low-motion threshold
	float motionRampAlpha_ = 55.f; // overlay alpha target, ramps on onset
	ExpansionState prevExpansionState_ = ExpansionState::IDLE;

	bool didSetup_ = false;
	bool active_ = false;
};

} // namespace quadrantcrosshair
