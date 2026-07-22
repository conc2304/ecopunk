#include "TFParameterPanel.h"
#include "TFSettings.h"
#include "ofFileUtils.h"
#include "ofJson.h"
#include "ofLog.h"
#include "ofMath.h"
#include "ofUtils.h"
#include <cmath>

namespace {
	// How far each dial drifts from its spring-hover base value, as a
	// fraction of its own (max-min) range. Internal tuning constant, not a
	// panel dial — the brief says every dial should "drift slightly", not
	// that the amount itself needs to be user-exposed. Slowed for a calmer
	// feel (was 0.05).
	constexpr float WOBBLE_AMPLITUDE_FRACTION = 0.02f;
}

void TFParameterPanel::setup() {
	bspIrregularity.set("Irregularity", BSP_IRREGULARITY, 0.0f, 1.0f);
	bspCellDensity.set("Cell Density", BSP_CELL_DENSITY, 0.01f, 0.3f);
	bspGeometryReshuffleRate.set("Geometry Reshuffle Rate", BSP_GEOMETRY_RESHUFFLE_RATE, 0.5f, 10.0f);
	bspRegionsTouchedPerTick.set("Regions Touched Per Tick", BSP_REGIONS_TOUCHED_PER_TICK, 1, 10);
	bspTransparencyAmount.set("Transparency Amount", BSP_TRANSPARENCY_AMOUNT, 0.0f, 1.0f);
	bspGroup.setName("BSP");
	bspGroup.add(bspIrregularity);
	bspGroup.add(bspCellDensity);
	bspGroup.add(bspGeometryReshuffleRate);
	bspGroup.add(bspRegionsTouchedPerTick);
	bspGroup.add(bspTransparencyAmount);

	blobGridResolution.set("Grid Resolution", BLOBGRID_GRID_RESOLUTION, 4, 40);
	blobGridBlobCenters.set("Blob Centers", BLOBGRID_BLOB_CENTERS, 1, 8);
	blobGridDriftSpeed.set("Drift Speed", BLOBGRID_DRIFT_SPEED, 0.0f, 3.0f);
	blobGridBlobRadius.set("Blob Radius", BLOBGRID_BLOB_RADIUS, 0.05f, 0.6f);
	blobGridEdgeSoftness.set("Edge Softness", BLOBGRID_EDGE_SOFTNESS, 0.0f, 1.0f);
	blobGridSizeVariation.set("Size Variation", BLOBGRID_SIZE_VARIATION, 0.0f, 1.0f);
	blobGridFragmentRefreshRate.set("Fragment Refresh Rate", BLOBGRID_FRAGMENT_REFRESH_RATE, 0.5f, 10.0f);
	blobGridMaskToBlob.set("Mask To Blob", BLOBGRID_MASK_TO_BLOB);
	blobGridTransparentBackground.set("Transparent Background", BLOBGRID_TRANSPARENT_BACKGROUND);
	blobGridGroup.setName("BlobGrid");
	blobGridGroup.add(blobGridResolution);
	blobGridGroup.add(blobGridBlobCenters);
	blobGridGroup.add(blobGridDriftSpeed);
	blobGridGroup.add(blobGridBlobRadius);
	blobGridGroup.add(blobGridEdgeSoftness);
	blobGridGroup.add(blobGridSizeVariation);
	blobGridGroup.add(blobGridFragmentRefreshRate);
	blobGridGroup.add(blobGridMaskToBlob);
	blobGridGroup.add(blobGridTransparentBackground);

	transitionDurationParam.set("Transition Duration", FRAGMENT_TRANSITION_DURATION, 0.1f, 3.0f);
	hardCutWeightParam.set("Hard Cut Weight", TRANSITION_HARD_CUT_WEIGHT, 0.0f, 100.0f);
	crossfadeWeightParam.set("Crossfade Weight", TRANSITION_CROSSFADE_WEIGHT, 0.0f, 100.0f);
	erosionWeightParam.set("Erosion Weight", TRANSITION_EROSION_WEIGHT, 0.0f, 100.0f);
	quantizeBandsParam.set("Quantize Bands", TIME_OFFSET_QUANTIZE_BANDS, 8, 16);
	transitionGroup.setName("Transition");
	transitionGroup.add(transitionDurationParam);
	transitionGroup.add(hardCutWeightParam);
	transitionGroup.add(crossfadeWeightParam);
	transitionGroup.add(erosionWeightParam);
	transitionGroup.add(quantizeBandsParam);

	bgEffectCycleInterval.set("Cycle Interval", BACKGROUND_EFFECT_CYCLE_INTERVAL, 1.0f, 30.0f);
	bgEffectRawWeight.set("Raw Weight", BACKGROUND_EFFECT_RAW_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightDesaturate.set("Desaturate", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightInvert.set("Invert", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightRecolor.set("Recolor", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightThreshold.set("Threshold", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightDither.set("Dither", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightSolarize.set("Solarize", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightScanlines.set("Scanlines", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightChannelshift.set("Channelshift", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightAsciiSolarpunk.set("Ascii Solarpunk", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightBioluminescence.set("Bioluminescence", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightChromaticAberration.set("Chromatic Aberration", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightEdgeGlow.set("Edge Glow", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightInkOutlines.set("Ink Outlines", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightPixelDrift.set("Pixel Drift", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightPixelSorting.set("Pixel Sorting", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightWaterRefraction.set("Water Refraction", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	backgroundEffectGroup.setName("Effects");
	backgroundEffectGroup.add(bgEffectCycleInterval);
	backgroundEffectGroup.add(bgEffectRawWeight);
	backgroundEffectGroup.add(bgEffectWeightDesaturate);
	backgroundEffectGroup.add(bgEffectWeightInvert);
	backgroundEffectGroup.add(bgEffectWeightRecolor);
	backgroundEffectGroup.add(bgEffectWeightThreshold);
	backgroundEffectGroup.add(bgEffectWeightDither);
	backgroundEffectGroup.add(bgEffectWeightSolarize);
	backgroundEffectGroup.add(bgEffectWeightScanlines);
	backgroundEffectGroup.add(bgEffectWeightChannelshift);
	backgroundEffectGroup.add(bgEffectWeightAsciiSolarpunk);
	backgroundEffectGroup.add(bgEffectWeightBioluminescence);
	backgroundEffectGroup.add(bgEffectWeightChromaticAberration);
	backgroundEffectGroup.add(bgEffectWeightEdgeGlow);
	backgroundEffectGroup.add(bgEffectWeightInkOutlines);
	backgroundEffectGroup.add(bgEffectWeightPixelDrift);
	backgroundEffectGroup.add(bgEffectWeightPixelSorting);
	backgroundEffectGroup.add(bgEffectWeightWaterRefraction);

	bgFullVideoWeight.set("Full Video Weight", BACKGROUND_FULL_VIDEO_WEIGHT, 0.0f, 100.0f);
	bgFullImageWeight.set("Full Image Weight", BACKGROUND_FULL_IMAGE_WEIGHT, 0.0f, 100.0f);
	bgSplitWeight.set("Split Weight", BACKGROUND_SPLIT_WEIGHT, 0.0f, 100.0f);
	bgModeChangeInterval.set("Mode Change Interval", BACKGROUND_MODE_CHANGE_INTERVAL, 5.0f, 120.0f);
	bgSplitRatio.set("Split Ratio", BACKGROUND_SPLIT_RATIO, 0.2f, 0.8f);
	bgSplitAxisChoice.set("Split Axis Choice", 0, 0, 2); // 0 Random / 1 Vertical / 2 Horizontal
	bgImageCycleInterval.set("Image Cycle Interval", BACKGROUND_IMAGE_CYCLE_INTERVAL, 1.0f, 60.0f);
	bgImageFadeDuration.set("Image Fade Duration", BACKGROUND_IMAGE_FADE_DURATION, 0.1f, 10.0f);
	backgroundGroup.setName("Background");
	backgroundGroup.add(bgFullVideoWeight);
	backgroundGroup.add(bgFullImageWeight);
	backgroundGroup.add(bgSplitWeight);
	backgroundGroup.add(bgModeChangeInterval);
	backgroundGroup.add(bgSplitRatio);
	backgroundGroup.add(bgSplitAxisChoice);
	backgroundGroup.add(bgImageCycleInterval);
	backgroundGroup.add(bgImageFadeDuration);
	backgroundGroup.add(backgroundEffectGroup);

	evolutionEnabledParam.set("Evolution Enabled", EVOLUTION_ENABLED);
	waypointChangeIntervalParam.set("Waypoint Change Interval", WAYPOINT_CHANGE_INTERVAL, 5.0f, 120.0f);
	waypointPullStrengthParam.set("Waypoint Pull Strength", WAYPOINT_PULL_STRENGTH, 0.01f, 1.0f);
	evolutionGroup.setName("Evolution");
	evolutionGroup.add(evolutionEnabledParam);
	evolutionGroup.add(waypointChangeIntervalParam);
	evolutionGroup.add(waypointPullStrengthParam);

	presetNameParam.set("Preset Name", "preset_001");
	saveButton.set("Save Preset");
	presetGroup.setName("Presets");
	presetGroup.add(presetNameParam);
	presetGroup.add(saveButton);

	hudCadenceOnFragmentReassign.set("On Fragment Reassign", HUD_CADENCE_ON_FRAGMENT_REASSIGN);
	hudCadenceOnPatternSwitch.set("On Pattern Switch", HUD_CADENCE_ON_PATTERN_SWITCH);
	hudEventGroup.setName("HUD Events");
	hudEventGroup.add(hudCadenceOnFragmentReassign);
	hudEventGroup.add(hudCadenceOnPatternSwitch);

	rootGroup.setName("temporal-fields");
	rootGroup.add(bspGroup);
	rootGroup.add(blobGridGroup);
	rootGroup.add(transitionGroup);
	rootGroup.add(backgroundGroup);
	rootGroup.add(evolutionGroup);
	rootGroup.add(hudEventGroup);
	rootGroup.add(presetGroup);

	panel.setup(rootGroup);

	saveButton.addListener(this, &TFParameterPanel::onSavePressed);

	// Register every continuous numeric dial for wobble+hover — bools
	// (Mask To Blob, Transparent Background, Evolution Enabled) are
	// deliberately excluded, since toggling them continuously would be
	// disruptive rather than "drift slightly".
	registerEvolving(bspIrregularity, 0.0f, 1.0f);
	registerEvolving(bspCellDensity, 0.01f, 0.3f);
	registerEvolving(bspGeometryReshuffleRate, 0.5f, 10.0f);
	registerEvolving(bspRegionsTouchedPerTick, 1.0f, 10.0f);
	registerEvolving(bspTransparencyAmount, 0.0f, 1.0f);

	registerEvolving(blobGridResolution, 4.0f, 40.0f);
	registerEvolving(blobGridBlobCenters, 1.0f, 8.0f);
	registerEvolving(blobGridDriftSpeed, 0.0f, 3.0f);
	registerEvolving(blobGridBlobRadius, 0.05f, 0.6f);
	registerEvolving(blobGridEdgeSoftness, 0.0f, 1.0f);
	registerEvolving(blobGridSizeVariation, 0.0f, 1.0f);
	registerEvolving(blobGridFragmentRefreshRate, 0.5f, 10.0f);

	registerEvolving(transitionDurationParam, 0.1f, 3.0f);
	registerEvolving(hardCutWeightParam, 0.0f, 100.0f);
	registerEvolving(crossfadeWeightParam, 0.0f, 100.0f);
	registerEvolving(erosionWeightParam, 0.0f, 100.0f);
	registerEvolving(quantizeBandsParam, 8.0f, 16.0f);

	registerEvolving(bgFullVideoWeight, 0.0f, 100.0f);
	registerEvolving(bgFullImageWeight, 0.0f, 100.0f);
	registerEvolving(bgSplitWeight, 0.0f, 100.0f);
	registerEvolving(bgModeChangeInterval, 5.0f, 120.0f);
	registerEvolving(bgSplitRatio, 0.2f, 0.8f);
	registerEvolving(bgImageCycleInterval, 1.0f, 60.0f);
	registerEvolving(bgImageFadeDuration, 0.1f, 10.0f);
	registerEvolving(bgEffectCycleInterval, 1.0f, 30.0f);
	registerEvolving(bgEffectRawWeight, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightDesaturate, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightInvert, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightRecolor, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightThreshold, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightDither, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightSolarize, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightScanlines, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightChannelshift, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightAsciiSolarpunk, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightBioluminescence, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightChromaticAberration, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightEdgeGlow, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightInkOutlines, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightPixelDrift, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightPixelSorting, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightWaterRefraction, 0.0f, 100.0f);

	wobbleLfo.setup(static_cast<int>(evolvingParams.size()));
	for (int i = 0; i < static_cast<int>(evolvingParams.size()); i++) {
		// Slow, non-synchronized drift: full oscillation periods roughly
		// 50-200s (was 12-50s), randomized phase so lanes don't all start
		// in lockstep.
		wobbleLfo.setFrequency(i, ofRandom(0.005f, 0.02f));
		wobbleLfo.setPhaseOffset(i, ofRandom(TWO_PI));
	}

	rescanPresets();
	pickNewWaypoint();
}

void TFParameterPanel::registerEvolving(ofParameter<float>& p, float lo, float hi) {
	EvolvingParam e;
	e.escapedName = p.getEscapedName();
	e.get = [&p]() { return p.get(); };
	e.set = [&p](float v) { p.set(v); };
	e.baseValue = p.get();
	e.minValue = lo;
	e.maxValue = hi;
	e.lfoLane = static_cast<int>(evolvingParams.size());
	evolvingParams.push_back(e);
}

void TFParameterPanel::registerEvolving(ofParameter<int>& p, float lo, float hi) {
	EvolvingParam e;
	e.escapedName = p.getEscapedName();
	e.get = [&p]() { return static_cast<float>(p.get()); };
	e.set = [&p](float v) { p.set(static_cast<int>(std::round(v))); };
	e.baseValue = static_cast<float>(p.get());
	e.minValue = lo;
	e.maxValue = hi;
	e.lfoLane = static_cast<int>(evolvingParams.size());
	evolvingParams.push_back(e);
}

void TFParameterPanel::update(float dt) {
	wobbleLfo.update(dt);

	if (!evolutionEnabledParam.get()) {
		// Manual tuning mode — don't fight the user's own slider drags.
		return;
	}

	waypointTimer += dt;
	if (waypointTimer >= waypointChangeIntervalParam.get()) {
		waypointTimer = 0.0f;
		pickNewWaypoint();
	}

	float pull = ofClamp(waypointPullStrengthParam.get() * dt, 0.0f, 1.0f);

	for (auto& e : evolvingParams) {
		auto targetIt = waypointTargets.find(e.escapedName);
		if (targetIt != waypointTargets.end()) {
			e.baseValue = ofLerp(e.baseValue, targetIt->second, pull);
		}

		float wobble = wobbleLfo.get(e.lfoLane) * (e.maxValue - e.minValue) * WOBBLE_AMPLITUDE_FRACTION;
		e.set(ofClamp(e.baseValue + wobble, e.minValue, e.maxValue));
	}
}

void TFParameterPanel::pickNewWaypoint() {
	if (presetFiles.empty()) {
		ofLogNotice("TFParameterPanel") << "no presets found — evolution has nothing to hover toward yet";
		return;
	}

	int idx = ofClamp(static_cast<int>(ofRandom(0.0f, static_cast<float>(presetFiles.size()))), 0,
		static_cast<int>(presetFiles.size()) - 1);
	if (presetFiles.size() > 1 && idx == currentPresetIndex) {
		idx = (idx + 1) % static_cast<int>(presetFiles.size());
	}
	currentPresetIndex = idx;

	ofJson json = ofLoadJson(presetFiles[idx]);
	if (json.empty()) {
		return;
	}

	std::string patternStr = json.value("pattern", std::string("bsp"));
	TFPatternType presetPattern = (patternStr == "blobgrid") ? TFPatternType::BLOB_GRID : TFPatternType::BSP;

	waypointTargets.clear();
	auto extractInto = [this](const ofJson& obj) {
		for (auto it = obj.begin(); it != obj.end(); ++it) {
			try {
				waypointTargets[it.key()] = std::stof(it.value().get<std::string>());
			} catch (...) {
				// non-numeric leaf (shouldn't happen for our own dials) — skip it
			}
		}
	};
	if (presetPattern == TFPatternType::BSP && json.contains("bsp")) {
		extractInto(json["bsp"]);
	}
	if (presetPattern == TFPatternType::BLOB_GRID && json.contains("blobgrid")) {
		extractInto(json["blobgrid"]);
	}
	if (json.contains("transition")) {
		extractInto(json["transition"]);
	}
	if (json.contains("background")) {
		extractInto(json["background"]);
		if (json["background"].contains("Effects")) {
			extractInto(json["background"]["Effects"]);
		}
	}

	// Always report the pattern — TFComposition::forcePattern() already
	// no-ops if it's already the active one, so there's no need to know
	// the current pattern here just to decide whether to report this.
	pendingWaypointPattern = presetPattern;
	waypointPatternSwitchPending = true;

	ofLogNotice("TFParameterPanel") << "new waypoint -> " << presetFiles[idx] << " (" << patternStr << ")";
}

bool TFParameterPanel::consumeWaypointPatternSwitch(TFPatternType& outPattern) {
	if (!waypointPatternSwitchPending) {
		return false;
	}
	waypointPatternSwitchPending = false;
	outPattern = pendingWaypointPattern;
	return true;
}

void TFParameterPanel::draw() {
	if (visible) {
		panel.draw();
	}
}

void TFParameterPanel::onSavePressed() {
	saveRequested = true;
}

bool TFParameterPanel::consumeSaveRequest() {
	bool r = saveRequested;
	saveRequested = false;
	return r;
}

TFPatternBSP::Params TFParameterPanel::getBSPParams() const {
	TFPatternBSP::Params p;
	p.irregularity = bspIrregularity;
	p.cellDensity = bspCellDensity;
	p.geometryReshuffleRate = bspGeometryReshuffleRate;
	p.regionsTouchedPerTick = bspRegionsTouchedPerTick;
	p.transparencyAmount = bspTransparencyAmount;
	p.transitionDuration = transitionDurationParam;
	p.hardCutWeight = hardCutWeightParam;
	p.crossfadeWeight = crossfadeWeightParam;
	p.erosionWeight = erosionWeightParam;
	return p;
}

TFPatternBlobGrid::Params TFParameterPanel::getBlobGridParams() const {
	TFPatternBlobGrid::Params p;
	p.gridResolution = blobGridResolution;
	p.blobCenters = blobGridBlobCenters;
	p.driftSpeed = blobGridDriftSpeed;
	p.blobRadius = blobGridBlobRadius;
	p.edgeSoftness = blobGridEdgeSoftness;
	p.sizeVariation = blobGridSizeVariation;
	p.fragmentRefreshRate = blobGridFragmentRefreshRate;
	p.maskToBlob = blobGridMaskToBlob;
	p.transparentBackground = blobGridTransparentBackground;
	p.transitionDuration = transitionDurationParam;
	p.hardCutWeight = hardCutWeightParam;
	p.crossfadeWeight = crossfadeWeightParam;
	p.erosionWeight = erosionWeightParam;
	return p;
}

int TFParameterPanel::getQuantizeBands() const {
	return quantizeBandsParam.get();
}

TFBackgroundLayer::Params TFParameterPanel::getBackgroundParams() const {
	TFBackgroundLayer::Params p;
	p.fullVideoWeight = bgFullVideoWeight;
	p.fullImageWeight = bgFullImageWeight;
	p.splitWeight = bgSplitWeight;
	p.modeChangeInterval = bgModeChangeInterval;
	p.splitRatio = bgSplitRatio;
	p.splitAxisChoice = bgSplitAxisChoice;
	p.imageParams.cycleInterval = bgImageCycleInterval;
	p.imageParams.fadeDuration = bgImageFadeDuration;

	p.effectWeights.cycleInterval = bgEffectCycleInterval;
	p.effectWeights.rawWeight = bgEffectRawWeight;
	p.effectWeights.effectWeights["desaturate"] = bgEffectWeightDesaturate;
	p.effectWeights.effectWeights["invert"] = bgEffectWeightInvert;
	p.effectWeights.effectWeights["recolor"] = bgEffectWeightRecolor;
	p.effectWeights.effectWeights["threshold"] = bgEffectWeightThreshold;
	p.effectWeights.effectWeights["dither"] = bgEffectWeightDither;
	p.effectWeights.effectWeights["solarize"] = bgEffectWeightSolarize;
	p.effectWeights.effectWeights["scanlines"] = bgEffectWeightScanlines;
	p.effectWeights.effectWeights["channelshift"] = bgEffectWeightChannelshift;
	p.effectWeights.effectWeights["ascii_solarpunk"] = bgEffectWeightAsciiSolarpunk;
	p.effectWeights.effectWeights["bioluminescence"] = bgEffectWeightBioluminescence;
	p.effectWeights.effectWeights["chromatic_aberration"] = bgEffectWeightChromaticAberration;
	p.effectWeights.effectWeights["edge_glow"] = bgEffectWeightEdgeGlow;
	p.effectWeights.effectWeights["ink_outlines"] = bgEffectWeightInkOutlines;
	p.effectWeights.effectWeights["pixel_drift"] = bgEffectWeightPixelDrift;
	p.effectWeights.effectWeights["pixel_sorting"] = bgEffectWeightPixelSorting;
	p.effectWeights.effectWeights["water_refraction"] = bgEffectWeightWaterRefraction;

	return p;
}

void TFParameterPanel::savePreset(TFPatternType activePattern) {
	std::string name = presetNameParam.get();
	if (name.empty()) {
		name = "preset";
	}

	ofJson json;
	json["pattern"] = (activePattern == TFPatternType::BSP) ? "bsp" : "blobgrid";

	ofJson bspJson;
	ofSerialize(bspJson, bspGroup);
	json["bsp"] = bspJson["BSP"];

	ofJson blobGridJson;
	ofSerialize(blobGridJson, blobGridGroup);
	json["blobgrid"] = blobGridJson["BlobGrid"];

	ofJson transitionJson;
	ofSerialize(transitionJson, transitionGroup);
	json["transition"] = transitionJson["Transition"];

	// Background is pattern-agnostic (like Transition) — always included
	// regardless of which pattern this preset belongs to. First *nested*
	// ofParameterGroup in this schema (Effects lives inside Background) —
	// ofSerialize walks nested groups recursively, so this should just work,
	// but it's new territory here; verify the round-trip explicitly.
	ofJson backgroundJson;
	ofSerialize(backgroundJson, backgroundGroup);
	json["background"] = backgroundJson["Background"];

	std::string path = ofToDataPath("presets/" + name + ".json");
	if (ofSavePrettyJson(path, json)) {
		ofLogNotice("TFParameterPanel") << "saved preset -> " << path;
	} else {
		ofLogError("TFParameterPanel") << "failed to save preset -> " << path;
	}

	rescanPresets();
}

bool TFParameterPanel::loadNextPreset(TFPatternType& outPattern) {
	if (presetFiles.empty()) {
		ofLogNotice("TFParameterPanel") << "no presets found in bin/data/presets/";
		return false;
	}

	currentPresetIndex = (currentPresetIndex + 1) % static_cast<int>(presetFiles.size());
	std::string path = presetFiles[currentPresetIndex];

	ofJson json = ofLoadJson(path);
	if (json.empty()) {
		return false;
	}

	std::string patternStr = json.value("pattern", std::string("bsp"));
	outPattern = (patternStr == "blobgrid") ? TFPatternType::BLOB_GRID : TFPatternType::BSP;

	if (json.contains("bsp")) {
		ofJson wrapper;
		wrapper["BSP"] = json["bsp"];
		ofDeserialize(wrapper, bspGroup);
	}
	if (json.contains("blobgrid")) {
		ofJson wrapper;
		wrapper["BlobGrid"] = json["blobgrid"];
		ofDeserialize(wrapper, blobGridGroup);
	}
	if (json.contains("transition")) {
		ofJson wrapper;
		wrapper["Transition"] = json["transition"];
		ofDeserialize(wrapper, transitionGroup);
	}
	if (json.contains("background")) {
		ofJson wrapper;
		wrapper["Background"] = json["background"];
		ofDeserialize(wrapper, backgroundGroup);
	}

	ofLogNotice("TFParameterPanel") << "loaded preset -> " << path;
	return true;
}

void TFParameterPanel::rescanPresets() {
	presetFiles.clear();

	ofDirectory dir;
	dir.allowExt("json");
	dir.listDir("presets");
	dir.sort();

	for (const auto& file : dir.getFiles()) {
		presetFiles.push_back(file.getAbsolutePath());
	}

	currentPresetIndex = -1;
}
