#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "ofxGui.h"
#include "ofJson.h"
#include "LFOBank.h"
#include "TFPresetTimeline.h"
#include "TFTimelineBinding.h"
#include "TFBackgroundLayer.h"
#include "TFPatternBSP.h"
#include "TFPatternBlobGrid.h"
#include "TFPatternBands.h"
#include "TFPatternColumnGrid.h"
#include "TFPatternTelescopingFrames.h"
#include "TFPatternParticleField.h"
#include "TFPatternEcologicalSuccession.h"
#include "TFPatternNetworkGrowth.h"
#include "TFPatternTemporalTides.h"
#include "TFPatternType.h"

// Owns every live-tunable dial from Phases 2-6 (both patterns, the shared
// transition weights, the time-offset quantize bands, and the masking/
// compositing toggles), an ofxPanel bound to them, the JSON preset
// save/recall system, and — from Phase 7 — the autonomous evolution
// system that continuously wobbles every dial and periodically hovers
// them toward a freshly-picked preset ("waypoint"). Deliberately the only
// class in this sketch that knows about ofxGui/ofParameter — both pattern
// classes stay GUI-agnostic and just receive plain Params structs via
// setParams() each frame.
//
// No existing preset schema exists anywhere in this codebase to match
// (confirmed in docs/temporal-fields-implementation-brief.md's Section 1
// verification) — every "preset" elsewhere (CrosshairPreset, BEPreset) is
// a compile-time C++ construct, never saved to disk. This is a genuinely
// new format: one JSON file per preset under bin/data/presets/, built with
// ofSerialize/ofDeserialize against the relevant ofParameterGroup(s), plus
// a top-level "pattern" field recording which pattern (bsp/blobgrid) the
// preset belongs to. Note ofSerialize stores every leaf value as a JSON
// *string* (parameter.toString()), not a native JSON number — that's how
// this OF utility works, not a bug in this class.
class TFParameterPanel {
	public:
		void setup();
		void update(float dt);
		void draw();

		// Mounts an externally-owned group (e.g. TFAmbientTextureLayer's
		// per-texture opacity sliders) into the panel after setup(). The
		// group's parameter count is only known once its owner has scanned
		// its data folder, which happens after this panel's own setup() --
		// every other group here is a fixed, compile-time set of dials, so
		// this is the one group built and owned outside this class.
		void registerBackgroundTextures(ofParameterGroup& textureGroup);

		// Same externally-owned-group pattern as registerBackgroundTextures()
		// above, for TFHudLayer's HUD Visibility toggle group (see
		// TFHudLayer::getVisibilityParamGroup()) — that group's per-widget
		// toggle count is a fixed compile-time set, but it's built and owned
		// by TFHudLayer itself (mirroring how it already owns its widgets),
		// not duplicated here. Unlike textureGroup (registerBackgroundTextures),
		// this group IS included in preset save/load (see savePreset()/
		// loadNextPreset()) — a pointer is kept so those methods can
		// (de)serialize it without TFParameterPanel needing to own it.
		void registerHudVisibility(ofParameterGroup& hudVisibilityGroup);

		void toggleVisible() { visible = !visible; }
		bool isVisible() const { return visible; }

		TFPatternBSP::Params getBSPParams() const;
		TFPatternBlobGrid::Params getBlobGridParams() const;
		TFPatternBands::Params getBandsParams() const;
		TFPatternColumnGrid::Params getColumnGridParams() const;
		TFPatternTelescopingFrames::Params getTelescopingFramesParams() const;
		TFPatternParticleField::Params getParticleFieldParams() const;
		TFPatternEcologicalSuccession::Params getEcologicalSuccessionParams() const;
		TFPatternNetworkGrowth::Params getNetworkGrowthParams() const;
		TFPatternTemporalTides::Params getTemporalTidesParams() const;
		TFBackgroundLayer::Params getBackgroundParams() const;
		int getQuantizeBands() const;
		float getMaxHistorySeconds() const { return maxHistorySecondsParam; }
		float getTransitionDuration() const { return transitionDurationParam; }
		float getHardCutWeight() const { return hardCutWeightParam; }
		float getCrossfadeWeight() const { return crossfadeWeightParam; }
		float getErosionWeight() const { return erosionWeightParam; }

		// Event Layer phase (Section 5) — which event source(s) drive the
		// hex-grid ripple + connection-thread + tick-stamp effects. Plain
		// runtime toggles, not wired into preset save/load or the evolution
		// system — they're an event-layer switch, not a composition-shape
		// dial the brief asked to be part of either system.
		bool getHudCadenceOnFragmentReassign() const { return hudCadenceOnFragmentReassign; }
		bool getHudCadenceOnPatternSwitch() const { return hudCadenceOnPatternSwitch; }

		// True once, the frame after the panel's Save button was pressed —
		// callers should follow up with savePreset(currentActivePattern),
		// since only the caller (ofApp) knows which pattern is active.
		bool consumeSaveRequest();

		// Saves the current dial state as a new preset file, tagged as
		// belonging to `activePattern`.
		void savePreset(TFPatternType activePattern);

		// Cycles to the next scanned preset file and applies it to the
		// live dials. Returns true and fills `outPattern` with the pattern
		// the preset belongs to if there was a preset to load.
		bool loadNextPreset(TFPatternType& outPattern);

		// True once, the frame after the evolution system picked a new
		// waypoint whose pattern differs from whatever's currently active
		// — the caller (ofApp) should follow with composition.forcePattern().
		bool consumeWaypointPatternSwitch(TFPatternType& outPattern);

		// Multi-state transition presets (see TFPresetTimeline) — a loaded
		// preset's optional "timeline" object drives these automatically via
		// update()/loadNextPreset(); these are the runtime controls exposed
		// per the feature's requirement #10, plus what ofApp's keyPressed()
		// (pause/resume, advance) and debug overlay (status line) use.
		bool isTimelineActive() const;
		bool isTimelinePaused() const;
		void restartTimeline();
		void pauseTimeline();
		void resumeTimeline();
		void advanceTimelineState();
		bool jumpTimelineState(const std::string& name);
		std::string getTimelineDebugStatus() const;

	private:
		void rescanPresets();
		void onSavePressed();

		// Phase 7 — parameter evolution
		struct EvolvingParam {
			std::string escapedName; // matches ofSerialize's key, for waypoint lookup
			std::function<float()> get;
			std::function<void(float)> set;
			float baseValue = 0.0f; // pure spring-hover value — wobble is added on top, never folded back in
			float minValue = 0.0f;
			float maxValue = 1.0f;
			int lfoLane = 0;
		};

		void registerEvolving(ofParameter<float>& p, float lo, float hi);
		void registerEvolving(ofParameter<int>& p, float lo, float hi);
		void pickNewWaypoint();

		ofParameterGroup bspGroup;
		ofParameter<float> bspIrregularity;
		ofParameter<float> bspCellDensity;
		ofParameter<float> bspGeometryReshuffleRate;
		ofParameter<int> bspRegionsTouchedPerTick;
		ofParameter<float> bspTransparencyAmount;

		ofParameterGroup blobGridGroup;
		ofParameter<int> blobGridResolution;
		ofParameter<int> blobGridBlobCenters;
		ofParameter<float> blobGridDriftSpeed;
		ofParameter<float> blobGridBlobRadius;
		ofParameter<float> blobGridEdgeSoftness;
		ofParameter<float> blobGridSizeVariation;
		ofParameter<float> blobGridFragmentRefreshRate;
		ofParameter<bool> blobGridMaskToBlob;
		ofParameter<bool> blobGridTransparentBackground;

		ofParameterGroup bandsGroup;
		ofParameter<int> bandsOrientation; // 0 Vertical / 1 Horizontal / 2 Axis Flip / 3 Diagonal
		ofParameter<int> bandsBandCount;
		ofParameter<float> bandsWidthVariation;
		ofParameter<float> bandsDiagonalAngleDeg;
		ofParameter<int> bandsOffsetMode; // 0 Dynamic / 1 Strata

		ofParameterGroup columnGridGroup;
		ofParameter<int> columnGridColumnCount;
		ofParameter<int> columnGridRowsPerColumn;
		ofParameter<float> columnGridRowHeightVariation;
		ofParameter<bool> columnGridBrickOffset;

		ofParameterGroup telescopingFramesGroup;
		ofParameter<int> telescopingFramesRingCount;
		ofParameter<float> telescopingFramesThicknessVariation;

		ofParameterGroup particleFieldGroup;
		ofParameter<float> particleFieldSpawnRate;
		ofParameter<int> particleFieldMaxParticleCount;
		ofParameter<float> particleFieldMinSize;
		ofParameter<float> particleFieldMaxSize;
		ofParameter<float> particleFieldMinLife;
		ofParameter<float> particleFieldMaxLife;
		ofParameter<float> particleFieldDriftSpeed;
		ofParameter<int> particleFieldDriftDirection; // 0 Omnidirectional / 1 Upward / 2 Downward
		ofParameter<float> particleFieldRotationSpeed;
		ofParameter<int> particleFieldDepthOrder; // 0 Newest On Top / 1 Largest Behind

		ofParameterGroup ecologicalSuccessionGroup;
		ofParameter<int> successionGridResolution;
		ofParameter<float> successionMaturityTime;
		ofParameter<float> successionYoungTurnoverInterval;
		ofParameter<float> successionClimaxTurnoverInterval;
		ofParameter<float> successionYoungHardCutWeight;
		ofParameter<float> successionClimaxErosionWeight;
		ofParameter<float> successionCrossfadeWeight;
		ofParameter<float> successionSproutThreshold;
		ofParameter<float> successionMinRenderScale;
		ofParameter<float> successionDisturbanceInterval;
		ofParameter<float> successionDisturbanceRadius;
		ofParameter<float> successionDisturbanceFlashDuration;

		ofParameterGroup networkGrowthGroup;
		ofParameter<int> networkGrowthSeedNodeCount;
		ofParameter<float> networkGrowthGrowthInterval;
		ofParameter<int> networkGrowthMaxNodeCount;
		ofParameter<float> networkGrowthBranchDistance;
		ofParameter<float> networkGrowthBranchDistanceJitter;
		ofParameter<float> networkGrowthBranchAngleJitterDeg;
		ofParameter<float> networkGrowthNodeMinSize;
		ofParameter<float> networkGrowthNodeMaxSize;
		ofParameter<float> networkGrowthNodeReassignInterval;
		ofParameter<bool> networkGrowthNodeLifespanEnabled;
		ofParameter<float> networkGrowthMaxNodeAge;
		ofParameter<float> networkGrowthEdgeThickness;
		ofParameter<float> networkGrowthRectangularChance;
		ofParameter<float> networkGrowthMinAspectRatio;
		ofParameter<float> networkGrowthMaxAspectRatio;
		ofParameter<float> networkGrowthCropChance;

		ofParameterGroup temporalTidesGroup;
		ofParameter<int> tidesGridResolution;
		ofParameter<float> tidesTideSpeed;
		ofParameter<float> tidesWaveLength;
		ofParameter<float> tidesAmplitude;
		ofParameter<int> tidesWaveDirection; // 0 Horizontal / 1 Vertical / 2 Diagonal
		ofParameter<float> tidesExposedThreshold;

		ofParameterGroup sharedPatternGroup;
		// Shared by the three wholesale-reshuffling patterns (Bands, Column
		// Grid, Telescoping Frames) — separate from BSP's
		// Geometry Reshuffle Rate and Blob Grid's Fragment Refresh Rate,
		// which stay pattern-specific. Particle Field doesn't use this at
		// all (Section 7).
		ofParameter<float> patternRegenRateParam;

		ofParameterGroup transitionGroup;
		ofParameter<float> transitionDurationParam;
		ofParameter<float> hardCutWeightParam;
		ofParameter<float> crossfadeWeightParam;
		ofParameter<float> erosionWeightParam;
		ofParameter<int> quantizeBandsParam;
		ofParameter<float> maxHistorySecondsParam;

		ofParameterGroup backgroundGroup;
		ofParameter<float> bgFullVideoWeight;
		ofParameter<float> bgFullImageWeight;
		ofParameter<float> bgSplitWeight;
		ofParameter<float> bgModeChangeInterval;
		ofParameter<float> bgSplitRatio;
		ofParameter<int> bgSplitAxisChoice; // 0 Random / 1 Vertical / 2 Horizontal
		ofParameter<float> bgImageCycleInterval;
		ofParameter<float> bgImageFadeDuration;

		ofParameterGroup backgroundEffectGroup; // nested inside backgroundGroup
		ofParameter<float> bgEffectCycleInterval;
		ofParameter<float> bgEffectRawWeight;
		ofParameter<float> bgEffectWeightDesaturate;
		ofParameter<float> bgEffectWeightInvert;
		ofParameter<float> bgEffectWeightRecolor;
		ofParameter<float> bgEffectWeightThreshold;
		ofParameter<float> bgEffectWeightDither;
		ofParameter<float> bgEffectWeightSolarize;
		ofParameter<float> bgEffectWeightScanlines;
		ofParameter<float> bgEffectWeightChannelshift;
		ofParameter<float> bgEffectWeightHueRotate;
		ofParameter<float> bgEffectWeightAsciiSolarpunk;
		ofParameter<float> bgEffectWeightBioluminescence;
		ofParameter<float> bgEffectWeightChromaticAberration;
		ofParameter<float> bgEffectWeightEdgeGlow;
		ofParameter<float> bgEffectWeightInkOutlines;
		ofParameter<float> bgEffectWeightPixelDrift;
		ofParameter<float> bgEffectWeightPixelSorting;
		ofParameter<float> bgEffectWeightWaterRefraction;

		ofParameterGroup evolutionGroup;
		ofParameter<bool> evolutionEnabledParam;
		ofParameter<float> waypointIntervalMinParam;
		ofParameter<float> waypointIntervalMaxParam;
		ofParameter<float> waypointPullStrengthParam;

		ofParameterGroup hudEventGroup;
		ofParameter<bool> hudCadenceOnFragmentReassign;
		ofParameter<bool> hudCadenceOnPatternSwitch;

		ofParameterGroup presetGroup; // UI-only — not serialized into presets
		ofParameter<std::string> presetNameParam;
		ofParameter<void> saveButton;

		ofParameterGroup rootGroup;
		ofxPanel panel;

		// Set by registerHudVisibility(); null until ofApp calls it in
		// setup(). Only savePreset()/loadNextPreset() read this — everything
		// else about the group (building it, gating draw calls on it) lives
		// in TFHudLayer.
		ofParameterGroup* hudVisibilityGroupPtr = nullptr;

		// Registry of every pattern's own ofParameterGroup, keyed by type —
		// generalizes savePreset()/loadNextPreset()/pickNewWaypoint(), which
		// otherwise would've needed a hardcoded if/else per pattern for each
		// of the six groups. Built once in setup().
		std::vector<std::pair<TFPatternType, ofParameterGroup*>> patternGroups;

		std::vector<std::string> presetFiles;
		int currentPresetIndex = -1;
		bool visible = false;
		bool saveRequested = false;

		// Multi-state transition presets. `timelineBindings` is the flat
		// path -> live-parameter binding table (e.g. "blobgrid.Drift_Speed",
		// "background.Effects.Bioluminescence") built once in setup() by
		// recursively walking patternGroups/transitionGroup/
		// sharedPatternGroup/backgroundGroup — see buildTimelineBindings()
		// in the .cpp. `loadedTimelineJson`/`loadedTimelineSourcePath` cache
		// the most recently loaded preset's raw, unmodified "timeline"
		// object (empty if the loaded preset had none) purely so
		// savePreset() can re-attach it verbatim if the user re-saves back
		// to that same file — see savePreset()'s comment for why this
		// doesn't attempt full timeline-authoring-via-GUI.
		void buildTimelineBindings();
		TFPresetTimeline timeline;
		std::map<std::string, TFTimelineBinding> timelineBindings;
		ofJson loadedTimelineJson;
		std::string loadedTimelineSourcePath;
		std::string currentPresetDisplayName;

		std::vector<EvolvingParam> evolvingParams;
		LFOBank wobbleLfo;

		float waypointTimer = 0.0f;
		float nextWaypointInterval = 0.0f; // re-rolled from [min,max] each time a waypoint is picked
		std::map<std::string, float> waypointTargets; // escapedName -> target value, empty = no active waypoint
		bool waypointPatternSwitchPending = false;
		TFPatternType pendingWaypointPattern = TFPatternType::BSP;
};
