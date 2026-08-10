#pragma once

#include "ofMain.h"
#include "ofxGui.h"

#include "BlobSceneCore.h"
#include "VideoPlaybackService.h"

// Standalone development host for the Blob visual pipeline. Blob First
// Complete Production Migration: the pipeline itself (ShaderLibrary,
// BlobDetector, BlobTracker, VideoRegionController, background effect
// renderer) now lives in BlobSceneCore (shared, host-agnostic — see that
// class's header comment), reused unchanged by the production
// IEcopunkScene adapter (sketches/experience_runtime/src/
// BlobProductionScene.h). This class is now purely: own the standalone
// dev's own VideoPlaybackService instance, own the ofxGui/ofParameter dev
// tuning surface (pushed into BlobSceneCore's config accessors every
// frame, same as the pre-migration syncParamsToSystems() did directly),
// and own debug-overlay/perf-instrumentation drawing that must never
// appear in the production HUD path (see
// docs/hud-double-hud-prevention-and-migration-matrix.md's Blob row).
//
// See docs/blob-region-architecture.md for the full pipeline write-up.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

private:
	void syncParamsToSystems();
	void drawDebugOverlay();

	VideoPlaybackService videoPlayback;
	BlobSceneCore blobCore;

	// ── GUI / parameters (Part 3 of the handoff) ───────────────────────────
	ofxPanel gui;
	bool guiVisible = true;

	ofParameterGroup detectionGroup;
	ofParameter<bool> pDetectionEnabled { "enabled", true };
	ofParameter<int> pAnalysisWidth { "analysisWidth", 320, 80, 640 };
	ofParameter<int> pAnalysisHeight { "analysisHeight", 180, 45, 360 };
	ofParameter<int> pProcessEveryNFrames { "processEveryNFrames", 2, 1, 6 };
	ofParameter<int> pThreshold { "threshold", 40, 1, 255 };
	ofParameter<float> pMinBlobArea { "minBlobAreaNorm", 0.0008f, 0.0001f, 0.05f };
	ofParameter<float> pMaxBlobArea { "maxBlobAreaNorm", 0.35f, 0.05f, 0.9f };
	ofParameter<int> pMaxBlobs { "maxBlobs", 12, 1, 24 };
	ofParameter<int> pErodeIterations { "erodeIterations", 1, 0, 4 };
	ofParameter<int> pDilateIterations { "dilateIterations", 2, 0, 4 };

	ofParameterGroup trackingGroup;
	ofParameter<float> pAssociationDistance { "associationDistanceNorm", 0.18f, 0.02f, 0.6f };
	ofParameter<float> pMinIoU { "minIoU", 0.08f, 0.0f, 0.9f };
	ofParameter<float> pPositionSmoothing { "positionSmoothing", 0.35f, 0.01f, 1.0f };
	ofParameter<float> pSizeSmoothing { "sizeSmoothing", 0.25f, 0.01f, 1.0f };
	ofParameter<float> pMaxMissingSeconds { "maxMissingSeconds", 0.5f, 0.0f, 3.0f };

	ofParameterGroup backgroundGroup;
	// 0 = off (canvas stays cleared to black), 1 = normal (unshaded
	// cover-fit draw, the original behavior), 2 = effect (background run
	// through one ShaderLibrary shader, via BlobSceneCore::backgroundConfig()).
	ofParameter<int> pBackgroundMode { "backgroundMode (0=off,1=normal,2=effect)", 1, 0, 2 };
	ofParameter<int> pBackgroundEffectIndex { "backgroundEffectIndex (see log)", 0, 0, 17 };
	ofParameter<float> pBackgroundEffectAmount { "backgroundEffectAmount", 1.0f, 0.0f, 1.0f };

	ofParameterGroup renderingGroup;
	ofParameter<bool> pFragmentsEnabled { "fragmentsEnabled", true };
	// ofxGui has no built-in editable-string widget wired up here, so the
	// effect is chosen by index into BlobSceneCore::effectChoices() (see
	// .cpp) rather than as an ofParameter<string> — VideoRegionController::
	// Params::effectName itself is still a plain std::string either way.
	// Range covers all 18 ShaderLibrary effects — NOT "ridgeline" (a
	// BEFragment-only special path using RidgelineRenderer + full-frame CPU
	// pixels, not a ShaderLibrary shader) or "contour" (doesn't exist as an
	// effect anywhere — ContourWidget is unrelated decorative HUD art).
	ofParameter<int> pEffectIndex { "effectIndex (see log)", 0, 0, 17 };
	ofParameter<float> pEffectAmount { "effectAmount", 1.0f, 0.0f, 1.0f };
	ofParameter<float> pFragmentScale { "fragmentScale", 1.0f, 0.2f, 2.5f };
	ofParameter<float> pCropPadding { "cropPadding", 0.0f, 0.0f, 0.5f };
	ofParameter<int> pMaxActiveFragments { "maxActiveFragments", 10, 1, 12 };
	ofParameter<int> pDrawMode { "drawMode (0/1/2)", 0, 0, 2 };

	ofParameterGroup perfGroup;
	ofParameter<bool> pDebugVisualization { "debugVisualization", true };
	ofParameter<bool> pDebugTiming { "debugTiming", true };

	// ── Rolling performance averages (Part 4 of the handoff) ───────────────
	struct RollingAverage {
		float samples[120] = { 0 };
		int index = 0;
		int count = 0;
		void push(float v) {
			samples[index] = v;
			index = (index + 1) % 120;
			if (count < 120) count++;
		}
		float average() const {
			if (count == 0) return 0.0f;
			float sum = 0.0f;
			for (int i = 0; i < count; i++) sum += samples[i];
			return sum / count;
		}
	};
	RollingAverage detectorMsAvg, trackerMsAvg, regionUpdateMsAvg, drawMsAvg, totalFrameMsAvg;
	uint64_t lastFrameStartMicros = 0;

	// Periodic console stats dump — lets performance numbers be captured
	// from stdout/a log file on a headless Pi run, not just read off the
	// on-screen debug overlay (see docs/blob-region-architecture.md's
	// performance report, gathered this way on desktop).
	float statsLogAccumSeconds = 0.0f;
};
