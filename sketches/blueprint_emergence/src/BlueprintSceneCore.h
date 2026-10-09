#pragma once

// ============================================================================
// BlueprintSceneCore — the reusable, host-agnostic Blueprint Emergence
// artwork, driven by both the standalone blueprint_emergence ofApp and
// ExperienceRuntime's BlueprintProductionScene (same split as BlobSceneCore /
// TemporalSceneCore). Owns exactly the artwork pipeline:
//
//     GridSystem + AnnotationRenderer + BEComposition (BEFragment, hud::
//     widgets) + LFOBank + TriggerBus + GridState + ShaderLibrary +
//     ErosionFBO + MotionExtraction
//
// Deliberately does NOT own: the VideoPlaybackService (the caller owns the
// one appropriate instance and passes a pointer; this class only reads the
// current frame, and requests next() per composition cycle only when
// Config::advanceMediaPerCycle says the caller wants that), window/global
// state (frame rate, vsync, ofDisableArbTex, cursor, fullscreen, ofExit),
// developer UI (hud_overlay glitch panel, occupancy debug, fps/debug text,
// key handling) — those stay in the standalone ofApp.
//
// Draws into whatever target is currently bound, in canvas coordinates
// (Config::canvasW x canvasH), never ofGetWidth()/ofGetHeight().
// ============================================================================

#include "AnnotationRenderer.h"
#include "BEComposition.h"
#include "ErosionFBO.h"
#include "GridState.h"
#include "GridSystem.h"
#include "LFOBank.h"
#include "MotionExtraction.h"
#include "ShaderLibrary.h"
#include "TriggerBus.h"

#include <string>
#include <vector>

class VideoPlaybackService;

class BlueprintSceneCore {
public:
	struct Config {
		int canvasW = 1280;
		int canvasH = 720;
		// Standalone: true — one media advance per composition cycle (the
		// sketch's pre-runtime behavior, with automaticAdvance off). Runtime:
		// false — the runtime's canonical service hold timer is the only
		// media cadence (a second, cycle-driven cadence would double-advance).
		bool advanceMediaPerCycle = true;
	};

	void setup(VideoPlaybackService* video, const Config& config);

	// Starts a fresh composition cycle (clears fragments, grid, annotations,
	// erosion history) — never resumes stale pre-deactivation imagery.
	void activate();
	void deactivate() { active_ = false; }
	bool isActive() const { return active_; }
	bool didSetup() const { return didSetup_; }

	void update(float dt);
	void draw();

	// Same path the composition itself takes at every cycle end.
	void restartCycle();
	void forceAxisFlip() { composition_.forceAxisFlip(); }

	// TEMP DIAGNOSTIC carried over from the standalone sketch (default true:
	// the artwork currently presents without erosion compositing).
	void setBypassErosion(bool bypass) { bypassErosion_ = bypass; }
	bool bypassErosion() const { return bypassErosion_; }

	bool hasVideoFrame() const;

	const BEComposition& composition() const { return composition_; }
	const GridSystem& grid() const { return grid_; }

private:
	void wireTriggerResponses();
	void drawScene();
	std::vector<std::string> loadCodeFragments() const;

	Config config_;
	VideoPlaybackService* video_ = nullptr;

	GridSystem grid_;
	AnnotationRenderer annotations_;
	BEComposition composition_;
	LFOBank lfoBank_;
	TriggerBus triggerBus_;
	GridState gridState_;
	ShaderLibrary shaderLib_;
	ErosionFBO erosionFBO_;
	MotionExtraction motionEx_;

	float gridPulseBoost_ = 0.0f;
	float motionOverlayOpacity_ = 0.0f;
	bool hadHudWidget_ = false;
	bool bypassErosion_ = true;

	bool didSetup_ = false;
	bool active_ = false;
};
