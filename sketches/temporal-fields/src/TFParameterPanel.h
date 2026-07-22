#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "ofxGui.h"
#include "LFOBank.h"
#include "TFBackgroundLayer.h"
#include "TFPatternBSP.h"
#include "TFPatternBlobGrid.h"
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

		void toggleVisible() { visible = !visible; }
		bool isVisible() const { return visible; }

		TFPatternBSP::Params getBSPParams() const;
		TFPatternBlobGrid::Params getBlobGridParams() const;
		TFBackgroundLayer::Params getBackgroundParams() const;
		int getQuantizeBands() const;
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

		ofParameterGroup transitionGroup;
		ofParameter<float> transitionDurationParam;
		ofParameter<float> hardCutWeightParam;
		ofParameter<float> crossfadeWeightParam;
		ofParameter<float> erosionWeightParam;
		ofParameter<int> quantizeBandsParam;

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
		ofParameter<float> waypointChangeIntervalParam;
		ofParameter<float> waypointPullStrengthParam;

		ofParameterGroup hudEventGroup;
		ofParameter<bool> hudCadenceOnFragmentReassign;
		ofParameter<bool> hudCadenceOnPatternSwitch;

		ofParameterGroup presetGroup; // UI-only — not serialized into presets
		ofParameter<std::string> presetNameParam;
		ofParameter<void> saveButton;

		ofParameterGroup rootGroup;
		ofxPanel panel;

		std::vector<std::string> presetFiles;
		int currentPresetIndex = -1;
		bool visible = false;
		bool saveRequested = false;

		std::vector<EvolvingParam> evolvingParams;
		LFOBank wobbleLfo;

		float waypointTimer = 0.0f;
		std::map<std::string, float> waypointTargets; // escapedName -> target value, empty = no active waypoint
		bool waypointPatternSwitchPending = false;
		TFPatternType pendingWaypointPattern = TFPatternType::BSP;
};
