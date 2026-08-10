#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ShaderLibrary.h"
#include "TFAmbientTextureLayer.h"
#include "TFBackgroundLayer.h"
#include "TFComposition.h"
#include "TFPatternBSP.h"
#include "TFPatternBlobGrid.h"
#include "TFPatternBands.h"
#include "TFPatternColumnGrid.h"
#include "TFPatternTelescopingFrames.h"
#include "TFPatternParticleField.h"
#include "TFPatternEcologicalSuccession.h"
#include "TFPatternNetworkGrowth.h"
#include "TFPatternTemporalTides.h"
#include "TFParameterPanel.h"
#include "TFHudLayer.h"
#include "TimeOffsetVideoBuffer.h"
#include "TimeOffsetPlaybackAdapter.h"
#include "VideoPlaybackService.h"
#include "MotionExtraction.h"
#include "HudOverlayLayer.h"
#include "HudOverlayDialPanel.h"
#include "HudOverlayDialState.h"

class ofApp : public ofBaseApp {

public:
	void setup() override;
	void update() override;
	void draw() override;

	void keyPressed(int key) override;
	void windowResized(int w, int h) override;

private:
	// Phase 2 verification only — tiles each playhead's current buffered
	// frame across the bottom of the screen so the ring buffer + playhead
	// pool can be visually confirmed before any real pattern (Phase 3/4)
	// consumes it. Remove once a pattern is driving playheads for real.
	void drawTimeOffsetDebugStrip();

	// Reads videoPlaybackService.status()/currentAbsolutePath() and passes
	// the result to temporalVideoAdapter.synchronizeSelectedMedia() —
	// called once at setup() (to load the startup selection before any
	// pattern's first update/draw) and once per frame in update() (so an
	// automatic/manual media change is picked up the same frame
	// videoPlaybackService makes it). A no-op most frames — see
	// TimeOffsetPlaybackAdapter::synchronizeSelectedMedia()'s own
	// unchanged-mediaId contract.
	void syncTemporalVideoAdapter();

	TFComposition composition;
	TFPatternBSP bspPattern;
	TFPatternBlobGrid blobGridPattern;
	TFPatternBands bandsPattern;
	TFPatternColumnGrid columnGridPattern;
	TFPatternTelescopingFrames telescopingFramesPattern;
	TFPatternParticleField particleFieldPattern;
	TFPatternEcologicalSuccession ecologicalSuccessionPattern;
	TFPatternNetworkGrowth networkGrowthPattern;
	TFPatternTemporalTides temporalTidesPattern;
	TFParameterPanel paramPanel;
	ShaderLibrary shaderLib;
	TFBackgroundLayer backgroundLayer;
	TFAmbientTextureLayer ambientTextures;

	// Shared Video — Temporal Fields Specialized Adapter Seam (DEC-013/
	// DEC-014): videoPlaybackService is the sole canonical media catalog/
	// selection/session-history/Previous-Next/hold-timing authority;
	// temporalVideoAdapter couples its selection to the dedicated
	// TimeOffsetVideoBuffer decoder/history/playhead pipeline, never
	// selecting media itself. Every existing TFPattern*/TFBackgroundLayer
	// call site that used to take `&timeOffsetBuffer` now takes
	// `&temporalVideoAdapter.buffer()` instead — same TimeOffsetVideoBuffer
	// public API, unchanged, just reached through the adapter.
	// synchronizeSelectedMedia() is called every frame with whatever
	// videoPlaybackService.status() currently reports — the adapter
	// itself (not this class) is what makes an unchanged mediaId a no-op,
	// so no duplicate "did it change" tracking is needed here.
	VideoPlaybackService videoPlaybackService;
	TimeOffsetPlaybackAdapter temporalVideoAdapter;

	MotionExtraction motionEx;
	TFHudLayer       hudLayer;

	bool showDebugGui = true;

	// HUD Glitch Overlay System — standalone, isolated module (see
	// shared/src/hud_overlay/README.md). Toggled with 'o'; while active it
	// fully replaces this sketch's draw with the overlay alone over a
	// solid near-black canvas, no composition wiring.
	hudoverlay::HudOverlayLayer hudOverlay;
	hudoverlay::HudOverlayDialPanel hudOverlayPanel;
	hudoverlay::HudOverlayDialState hudOverlayDials;
	bool hudOverlayActive = false;
};
