#pragma once
#include "HudElements.h"
#include "TFMoireUnderlay.h"
#include "ofVec2f.h"
#include <string>
#include <vector>

// Owns and draws Temporal Fields' HUD chrome as two full-canvas layers: an
// underlay (drawn beneath TFComposition, so it shows through Blob Grid's
// transparent mask gaps and along BSP's rectangle seams) and an overlay
// (drawn on top of it).
//
// Every widget here is geometry-agnostic — bounds are either the whole
// canvas or a fixed screen region, never coupled to individual BSP
// rectangles or blob-grid quadtree cells. blueprint_emergence's HUD
// placement (BEComposition::spawnFragmentInSlot) instead snap-grids a
// single widget into an empty slot of its own GridSystem; that placement
// strategy is BE-specific (tied to BE's snap-grid) and isn't reused here —
// TF has no equivalent fixed slot grid to snap to. This mirrors
// quadrant-crosshair's HudManager instead, which also treats these widgets
// as full-screen/fixed-region ambient layers rather than per-fragment
// annotations.
//
// Decoupled from MotionExtraction/TimeOffsetVideoBuffer/TFPattern on
// purpose — update()/onFragmentReassigned() take plain values so this
// class only ever needs to include HudElements.h.
//
// Connection-thread and tick/crosshair-stamp (Event Layer phase) are kept
// local here rather than built as shared/src/hud/ widgets: both were
// designed specifically for this sketch's observation/measurement
// aesthetic, and both sibling sketches already have their own established
// equivalents for this territory (blueprint_emergence's AnnotationRenderer
// measurement lines; quadrant-crosshair's ReticleWidget Tracking preset) —
// building a third parallel implementation in the shared library without a
// concrete second consumer asking for it would be premature generalization.
class TFHudLayer {
public:
	void setup(int canvasW, int canvasH);
	// historyBufferFill01: TimeOffsetVideoBuffer's history ring buffer fill
	// fraction (0 at startup, 1 once at capacity) — drives the charge-level
	// gauge as a real, footage-derived "depleting/filling resource" reading
	// rather than a fabricated value.
	void update(float dt, float motionEnergy01, bool hasMedia, const std::string& currentMediaFilename,
		float historyBufferFill01);
	void drawUnderlay();
	void drawOverlay();

	// Fires a glitch tear + advances the specimen ticker unconditionally,
	// plus a canvas-center HexGridWidget ripple gated by the pattern-switch
	// cadence toggle (see setEventCadence()). Specimen ticker cadence is
	// deliberately untouched by the Event Layer phase — still exactly once
	// per pattern switch, not per fragment reassignment.
	void onPatternSwitch(const std::string& patternName);

	// Fired per fragment reassignment (Event Layer phase — see
	// TFPattern::setOnFragmentReassigned()). nx/ny: the reassigned
	// fragment's center, normalized [0,1]. activeFragmentCentersNorm: this
	// frame's active fragment centers (normalized [0,1], from
	// TFComposition::getActiveFragmentCenters()), used to find the
	// connection-thread effect's nearest 2-3 targets.
	void onFragmentReassigned(float nx, float ny, const std::vector<ofVec2f>& activeFragmentCentersNorm);

	// Runtime-tunable event cadence (Section 5 of the Event Layer brief) —
	// which event source(s) drive the hex-grid ripple + connection-thread +
	// tick-stamp effects. Both default true: per-fragment reassignment is
	// the primary cadence (the browser-prototype pick), pattern-switch
	// stays available as an alternate/fallback rather than being removed.
	// Wired from TFParameterPanel's "HUD Events" toggles.
	void setEventCadence(bool onFragmentReassign, bool onPatternSwitchCadence) {
		cadenceOnFragmentReassign = onFragmentReassign;
		cadenceOnPatternSwitch = onPatternSwitchCadence;
	}

private:
	struct ConnectionThread {
		ofVec2f origin, target;
		float age = 0.0f;
	};
	struct TickStamp {
		ofVec2f pos;
		float age = 0.0f;
		int id = 0;
	};

	void spawnConnectionThreads(const ofVec2f& originPx, const std::vector<ofVec2f>& activeFragmentCentersNorm);
	void drawConnectionThreads() const;
	void drawTickStamps() const;

	hud::HudTheme theme;
	hud::HudTheme scanTheme; // same theme with FrameOptions::showScanLines on

	int canvasW = 0;
	int canvasH = 0;

	// Underlay
	TFMoireUnderlay moire;
	hud::ContourWidget contours;
	hud::HexGridWidget hexGrid;
	hud::NodeNetworkWidget network;        // PCB-trace/tech-mesh look, straight edges (default)
	hud::NodeNetworkWidget networkOrganic; // root/mycelium look, edgeStyle = Organic
	hud::FlowFieldWidget motes;            // configured sparse/slow/no-curves for a drifting-motes look

	// Overlay
	hud::ScannerWidget scanner;
	hud::ReticleWidget reticles;    // also carries FrameStyle::Registration corner marks
	hud::DataCardWidget motionCard;
	hud::DataCardWidget specimenCard;
	hud::GaugeWidget bufferGauge;
	hud::StatusLightWidget mediaStatus;
	hud::LogScrollWidget log;
	hud::GlitchTearWidget glitch;

	// Event Layer phase — event-triggered effects
	std::vector<ConnectionThread> connectionThreads;
	std::vector<TickStamp> tickStamps;
	int tickStampCounter = 0; // separate from specimenCount — different cadence, different role

	bool cadenceOnFragmentReassign = true;
	bool cadenceOnPatternSwitch = true;

	std::string lastMediaFilename;
	float logPushTimer = 0.0f;
	int specimenCount = 0;
};
