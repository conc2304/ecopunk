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
#include "BECollage.h"
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
		// One canonical VideoPlaybackService::advanceAtBoundary() per composition-cycle
		// start — the sketch's original cadence. Both hosts use it; the
		// host keeps the service's timer-driven auto-advance off while this
		// scene is live (standalone: Config::automaticAdvance = false;
		// ExperienceRuntime: setAutomaticAdvanceEnabled(false) on activation).
		// The cycle that activate() starts keeps the current media (no change
		// just because the scene became active); every later cycle start —
		// natural cycle end or restartCycle() (RESEED / standalone 'r') —
		// advances, exactly as the standalone startCycle() path does.
		bool advanceMediaPerCycle = true;
		// Start in the EDITORIAL_COLLAGE visual grammar (off by default, so
		// existing hosts are unchanged). Switchable at runtime.
		bool editorialCollage = false;
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
	// Manual NEXT MODE (BEComposition::requestNextMode()). Entering
	// GHOST_LAYERS fires the composition's cycle-start callback; the shared
	// media is NOT advanced for a manual mode change.
	bool requestNextMode();
	void forceAxisFlip() { composition_.forceAxisFlip(); }

	// TEMP DIAGNOSTIC carried over from the standalone sketch (default true:
	// the artwork currently presents without erosion compositing).
	void setBypassErosion(bool bypass) { bypassErosion_ = bypass; }
	bool bypassErosion() const { return bypassErosion_; }

	bool hasVideoFrame() const;

	// Composition-cycle starts seen (BEComposition's cycle-start callback:
	// startCycle() AND a ghost-layers mini-cycle entry from enterMode()) —
	// the exact event that drives the per-cycle media advance.
	int cycleStartCount() const { return cycleStartCount_; }

	// ── EDITORIAL_COLLAGE grammar ─────────────────────────────────────────
	// A composition grammar inside this scene (not a separate scene): same
	// phase arc, same cycle/seed lifecycle, same shared video texture.
	// Switching restarts the current cycle (media is kept).
	void setEditorialCollage(bool on);
	bool editorialCollage() const { return editorialCollage_; }
	// Display name for status paths that want one; empty when classic.
	const char* visualGrammarName() const { return editorialCollage_ ? collage_.displayName() : ""; }
	BECollage& collage() { return collage_; }
	const BECollage& collage() const { return collage_; }
	// Dev/capture: show one frozen, fully-built plate for (seed, template).
	void debugShowCollage(uint32_t seed, int templateIndex, bool densityReached);

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
	BECollage collage_;
	bool editorialCollage_ = false;
	LFOBank lfoBank_;
	TriggerBus triggerBus_;
	GridState gridState_;
	ShaderLibrary shaderLib_;
	ErosionFBO erosionFBO_;
	MotionExtraction motionEx_;

	float gridPulseBoost_ = 0.0f;
	float motionOverlayOpacity_ = 0.0f;
	bool hadHudWidget_ = false;
	bool skipNextCycleMediaAdvance_ = false; // set by activate(); see Config::advanceMediaPerCycle
	int cycleStartCount_ = 0;
	bool bypassErosion_ = true;

	bool didSetup_ = false;
	bool active_ = false;
};
