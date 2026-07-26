#pragma once
#include "HudElements.h"
#include "TFMoireUnderlay.h"
#include "ofVec2f.h"
#include "ofxGui.h"
#include <array>
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

	// Re-lays-out every full-canvas and fixed-region widget's bounds for a
	// new canvas size (e.g. on a window resize). Every widget here reads
	// its bounds live at draw time (hud::HudWidget::setBounds() just
	// updates the stored rect, no internal geometry is cached in absolute
	// pixels), so re-issuing setBounds() is all a resize needs -- no
	// widget's setup()/rebuild() has to run again.
	void resize(int canvasW, int canvasH);
	// avgPlayheadDepth01: average of TimeOffsetVideoBuffer::getPlayheadOffset()
	// across all active playheads (0 = all live, 1 = as far back as
	// maxHistorySeconds allows) — drives the DEPTH gauge. Continuously
	// fluctuates as fragments reassign playheads via noise, unlike the
	// history-ring-buffer-fill metric this replaced (which pins at 100%
	// after ~maxHistorySeconds and sits static for the rest of the
	// session — see conversation notes). Also a real, footage-derived
	// reading, and arguably a better thematic fit: it's literally "how far
	// back in time is the composition looking right now." activeFragmentCentersNorm:
	// this frame's active fragment centers (normalized [0,1], from
	// TFComposition::getActiveFragmentCenters()) — feeds the reticle
	// overlay's Locate behavior so it travels between actual fragment
	// centers instead of random points, AND periodically reseeds its label
	// text with real coordinates from this same array (see
	// reticleLabelRefreshTimer). patternDrift01: the active pattern's own
	// characteristic irregularity/variation param (e.g. BSP's irregularity,
	// Blob Grid's sizeVariation), already 0..1 and already wobbling live via
	// TFParameterPanel's evolving-dial system — drives the contour
	// underlay's noiseScale so it visibly tracks the same drift shaping the
	// pattern, instead of animating on its own disconnected clock.
	void update(float dt, float motionEnergy01, bool hasMedia, const std::string& currentMediaFilename,
		float avgPlayheadDepth01, const std::vector<ofVec2f>& activeFragmentCentersNorm, float patternDrift01);
	void drawUnderlay();
	void drawOverlay();

	// Fires a glitch tear + advances the specimen ticker unconditionally,
	// plus a canvas-center HexGridWidget ripple gated by the pattern-switch
	// cadence toggle (see setEventCadence()). Specimen ticker cadence is
	// deliberately untouched by the Event Layer phase — still exactly once
	// per pattern switch, not per fragment reassignment. cycleSeed: this
	// cycle's real RNG seed (TFComposition::getCycleSeed()) — the actual
	// value that generated this specimen's geometry, used as its displayed
	// ID instead of an arbitrary incrementing tally. isPatternTransition:
	// composition.getPhase() == TFComposition::CyclePhase::PATTERN_TRANSITION
	// at the caller — passed as a plain bool rather than including
	// TFComposition.h here, per this class's decoupling (see class comment).
	void onPatternSwitch(const std::string& patternName, int cycleSeed, bool isPatternTransition);

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

	// HUD Visibility phase — one ofParameterGroup with three subgroups
	// (Underlay pool / Overlay pool / Always-on effects), each with its own
	// master toggle plus one toggle per widget. Built in setup(); mounted
	// into TFParameterPanel by the caller the same way
	// TFAmbientTextureLayer's per-texture opacity group is (see
	// TFParameterPanel::registerBackgroundTextures() — an externally-owned
	// group added to rootGroup post-hoc, then panel.setup(rootGroup)
	// re-called to rebuild the widget tree).
	ofParameterGroup& getVisibilityParamGroup() { return visibilityGroup; }

private:
	// The 7 persistent overlay widgets below used to all draw every frame at
	// once — far too busy. At most kMaxActiveOverlayWidgets of them are
	// drawn at any given moment; which ones is re-rolled on a slow timer.
	// Momentary/self-fading effects (glitch tears, connection threads, tick
	// stamps) are unaffected — they're already event-gated and brief.
	enum OverlayWidgetId {
		OW_Scanner,
		OW_Reticles,
		OW_MotionCard,
		OW_SpecimenCard,
		OW_BufferGauge,
		OW_MediaStatus,
		OW_Log,
		OW_Count
	};
	static constexpr int kMaxActiveOverlayWidgets = 2;

	void pickNewOverlayRotation();
	bool isOverlayWidgetActive(int id) const;

	// Re-randomizes position — and, for these six, size too, since all of
	// them scale responsively via su()-relative geometry/typography — of
	// the fixed-region overlay widgets (scanner, motionCard, specimenCard,
	// bufferGauge, mediaStatus, log). Full-canvas widgets (contours,
	// hexGrid, network*, motes, reticles, glitch) have no "location" to
	// vary and are untouched. Called from resize() and from every
	// pickNewOverlayRotation() reshuffle, so a widget re-entering the
	// active set after being hidden actually lands somewhere new rather
	// than snapping back to the one hardcoded spot it always used to use.
	void respawnLayout();
	// Lays out sizes.size() boxes stacked outward from `corner` (0=TL,
	// 1=TR, 2=BL, 3=BR), nearest-to-corner first.
	void layoutStack(int corner, const std::vector<ofVec2f>& sizes, std::vector<ofVec2f>& outPositions) const;

	int activeOverlayWidgets[kMaxActiveOverlayWidgets] = { OW_Scanner, OW_MotionCard };
	float overlayRotationTimer = 0.0f;
	float overlayRotationInterval = 20.0f;

	// Same idea for the 6 always-on underlay widgets — at most 1 draws at a
	// time, with a 20% chance of none.
	enum UnderlayWidgetId {
		UW_Moire,
		UW_Contours,
		UW_HexGrid,
		UW_Network,
		UW_NetworkOrganic,
		UW_Motes,
		UW_Count
	};

	void pickNewUnderlayRotation();
	bool isUnderlayWidgetActive(int id) const;

	int activeUnderlayWidget = UW_Moire;
	float underlayRotationTimer = 0.0f;
	float underlayRotationInterval = 20.0f;

	// Debounces onFragmentReassigned()'s connection-thread spawn — without
	// this, a mass reassignment burst (e.g. every fragment exiting at once
	// on a scene/pattern change) fires the spawn once per fragment in the
	// same frame or two, flooding the canvas with threads all at once.
	float connectionThreadCooldownTimer = 0.0f;

	// Debug visibility into the churn rate this debounce is fighting —
	// periodically logs how many reassignment events actually came in vs.
	// how many passed the cooldown, plus how many threads are alive right
	// now (i.e. how much overlap is on screen at once).
	float connectionThreadDebugLogTimer = 0.0f;
	int reassignEventsSinceLog = 0;
	int acceptedSpawnsSinceLog = 0;
	int rejectedSpawnsSinceLog = 0;

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

	// Stored so labelOverride can be refreshed (see reticleLabelRefreshTimer)
	// without losing the rest of the reticle's settings — same reasoning as
	// quadrant-crosshair's HudManager::reticleOpts_.
	hud::ReticleOptions reticleOpts_;
	// Real per-target reticle labels: periodically reseeded from live
	// fragment-center coordinates rather than the widget's built-in
	// decorative nature-word table. Throttled rather than done every frame
	// -- ReticleWidget::setOptions() fully rebuilds targets (restarting
	// every spawn/tracking animation) whenever labelOverride changes.
	float reticleLabelRefreshTimer = 0.0f;

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
	int tickStampCounter = 0; // separate from the specimen ID — different cadence, different role

	bool cadenceOnFragmentReassign = true;
	bool cadenceOnPatternSwitch = true;

	std::string lastMediaFilename;
	float logPushTimer = 0.0f;

	// HUD Visibility phase. isUnderlayWidgetActive()/isOverlayWidgetActive()
	// AND these into the existing rotation-pool check, so a disabled widget
	// simply never wins a rotation slot's draw -- update() still runs
	// unconditionally for every widget either way, matching the rotation
	// system's existing update()-always/draw()-gated split. The always-on
	// effects (glitch, connection threads, tick stamps) sit outside both
	// rotation enums, so they get their own flat bools rather than an array.
	ofParameterGroup visibilityGroup;

	ofParameterGroup underlayVisibilityGroup;
	ofParameter<bool> underlayGroupEnabled;
	std::array<ofParameter<bool>, UW_Count> underlayEnabled;

	ofParameterGroup overlayVisibilityGroup;
	ofParameter<bool> overlayGroupEnabled;
	std::array<ofParameter<bool>, OW_Count> overlayEnabled;

	ofParameterGroup alwaysOnVisibilityGroup;
	ofParameter<bool> alwaysOnGroupEnabled;
	ofParameter<bool> glitchEnabled;
	ofParameter<bool> connectionThreadsEnabled;
	ofParameter<bool> tickStampsEnabled;
};
