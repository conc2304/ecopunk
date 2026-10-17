#pragma once

#include "BlobCapacityPlan.h"
#include "VideoRegion.h"
#include "VideoRegionEffectRenderer.h"

#include <fstream>
#include <set>
#include <string>
#include <vector>

// BlobEffectCapacityBench — Blob Experiment 2 (multi-effect capacity), the
// runtime half: operator keys, optional unattended script, 5 s measurement
// windows (media-load stalls excluded), grep-able log lines and a TSV.
//
// EXPERIMENT ONLY. Constructed by BlobProductionScene only when
// EXPERIENCE_RUNTIME_BLOB_EFFECT_CAPACITY is set; with it unset, nothing in
// this file runs and production behavior is unchanged. Never exposed
// through the production HUD. Operator guide:
// docs/reviews/blob-effect-capacity-pi-runbook.md.
class BlobEffectCapacityBench {
public:
	// Measures at the runtime's normal frame cap on purpose: BlobDetector
	// differences consecutive app frames, so running uncapped repeats video
	// frames, collapses detected motion and with it the fragment load.
	static bool isRequested();

	// Reads the env configuration, opens the TSV, prints the banner.
	void setup();

	// Operator keys; true if the key was a capacity key (consumed).
	bool handleKey(int key);

	const blobcapacity::Plan & plan() const { return plan_; }

	// True once after any configuration change: the caller drops every
	// fragment so the next window measures only the new configuration.
	bool takeResetRequest();

	// Substring of the mediaId/title to pin (EXPERIENCE_RUNTIME_BLOB_CAPACITY_MEDIA).
	const std::string & wantedMedia() const { return wantedMedia_; }

	// Fragment load. Synthetic (default): fixed ids 1..N with slowly
	// drifting bounds, so every configuration sees the same, exactly known
	// load (tracked load follows each clip's bursty motion and is not
	// reproducible). Tracked: BlobTracker's real regions.
	bool syntheticLoad() const { return syntheticLoad_; }
	// Regions for this frame (synthetic load only), rebuilt in place.
	const std::vector<VideoRegion> & syntheticRegions(float timeSeconds);

	// Pending visual capture label (scene-only + full HUD), taken once.
	bool takeCaptureRequest(std::string & label);

	struct FrameSample {
		float frameSeconds = 0.0f;
		float detectorMs = 0.0f;
		float trackerMs = 0.0f;
		float regionUpdateMs = 0.0f;
		float sceneDrawMs = 0.0f;
		VideoRegionEffectRenderer::StageTimings fragment;
		VideoRegionEffectRenderer::StageTimings background;
		const std::vector<std::string> * fragmentEffects = nullptr; // one per live fragment, "" = Original
		std::string mediaId;
	};
	void recordFrame(const FrameSample & s);

private:
	struct Window {
		int frames = 0;
		double frameMs = 0.0, sceneDrawMs = 0.0, regionRenderMs = 0.0, backgroundMs = 0.0;
		double detectorMs = 0.0, trackerMs = 0.0, regionUpdateMs = 0.0;
		double fragments = 0.0, shaderFragments = 0.0, originalFragments = 0.0, distinctUsed = 0.0;
		double directDraws = 0.0, legacyDraws = 0.0, unshadedDraws = 0.0, shaderBinds = 0.0, shaderSwitches = 0.0;
		int stalls = 0;
		double maxStallMs = 0.0;
		float elapsedSeconds = 0.0f;
		std::set<std::string> effectsUsed;
		std::string mediaId;
	};
	struct StepSummary {
		std::string spec;
		std::string fields;
		int windows = 0;
		double fps = 0.0, frameMs = 0.0, regionRenderMs = 0.0, backgroundMs = 0.0, fragments = 0.0;
		double shaderFragments = 0.0, distinctUsed = 0.0, shaderSwitches = 0.0;
		int stalls = 0;
	};

	void applyPlan(const blobcapacity::Plan & plan, const std::string & why);
	void closeWindow();
	void advanceScript(float dt);
	void finishScript();
	void openTsv();
	void focusEffect(int step);

	blobcapacity::Plan plan_;
	bool resetRequested_ = false;
	std::string wantedMedia_;
	std::vector<std::string> customEffects_;

	std::string loadLabel() const;
	void logBanner() const;

	bool syntheticLoad_ = true;
	int syntheticCount_ = blobcapacity::kSlots;
	float regionScale_ = 1.0f;
	std::vector<VideoRegion> synthetic_;

	// Interactive "single effect" focus (j/l) through effectTable().
	int focusIndex_ = -1;

	Window window_;
	float warmupRemaining_ = 0.0f;
	float interactiveWarmupSeconds_ = 3.0f;
	float scriptWarmupSeconds_ = 10.0f;
	bool markNext_ = false;
	std::string captureLabel_;

	std::vector<blobcapacity::ScriptStep> script_;
	int scriptIndex_ = -1;
	float stepElapsed_ = 0.0f;
	bool stepCaptured_ = false;
	std::vector<StepSummary> summaries_;

	std::ofstream tsv_;
	std::string tsvPath_;

	static constexpr float kWindowSeconds = 5.0f;
	static constexpr float kStallSeconds = 0.25f;
};
