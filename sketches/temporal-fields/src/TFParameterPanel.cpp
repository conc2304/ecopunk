#include "TFParameterPanel.h"
#include "TFSettings.h"
#include "ofFileUtils.h"
#include "ofJson.h"
#include "ofLog.h"
#include "ofMath.h"
#include "ofUtils.h"
#include <cmath>
#include <set>
#include <typeinfo>

namespace {
	// How far each dial drifts from its spring-hover base value, as a
	// fraction of its own (max-min) range. Internal tuning constant, not a
	// panel dial — the brief says every dial should "drift slightly", not
	// that the amount itself needs to be user-exposed. Slowed for a calmer
	// feel (was 0.05).
	constexpr float WOBBLE_AMPLITUDE_FRACTION = 0.02f;

	// Discrete-choice int dials that are stored as ofParameter<int> but are
	// really an enum selector (orientation/offset-mode/drift-direction/...),
	// same set already called out by registerEvolving()'s "excluded"
	// comments above — a timeline should switch these at 50% progress
	// rather than sweep numerically through every intermediate integer,
	// since e.g. Orientation 0/1/2/3 has no meaningful "1.6" in between.
	// Paths use the exact same dotted convention as the JSON schema itself
	// (top-level preset key + "." + escaped GUI label).
	const std::set<std::string> TIMELINE_ENUM_LIKE_PATHS = {
		"bands.Orientation",
		"bands.Offset_Mode",
		"particlefield.Drift_Direction",
		"particlefield.Depth_Order",
		"temporaltides.Wave_Direction",
		"background.Split_Axis_Choice",
	};

	// Recursively walks an ofParameterGroup (mirroring ofSerialize's own
	// group-walk in ofJson.h) building a flat path -> TFTimelineBinding
	// table, so the timeline system's parameter registry is derived from
	// the existing ofParameterGroup tree instead of hand-declaring a third
	// parallel list (setup() and registerEvolving() already each declare
	// these dials once). `prefix` is the desired top-level JSON path
	// segment (e.g. "blobgrid", "background") for the initial call; nested
	// groups (only backgroundEffectGroup today) fold their own escaped name
	// in for the recursive call, producing paths like
	// "background.Effects.Bioluminescence".
	void walkGroupForTimelineBindings(ofParameterGroup& group, const std::string& prefix,
		std::map<std::string, TFTimelineBinding>& out) {
		for (std::size_t i = 0; i < group.size(); i++) {
			ofAbstractParameter& p = group.get(i);

			if (p.type() == typeid(ofParameterGroup).name()) {
				ofParameterGroup& nested = p.castGroup();
				walkGroupForTimelineBindings(nested, prefix + "." + nested.getEscapedName(), out);
				continue;
			}

			std::string path = prefix + "." + p.getEscapedName();
			TFTimelineBinding binding;

			if (p.isOfType<float>()) {
				ofParameter<float>& fp = p.cast<float>();
				binding.kind = TFTimelineBinding::Kind::Float;
				binding.get = [&fp]() { return static_cast<double>(fp.get()); };
				binding.set = [&fp](double v) { fp.set(static_cast<float>(v)); };
				binding.hasRange = true;
				binding.minValue = fp.getMin();
				binding.maxValue = fp.getMax();
			} else if (p.isOfType<int>()) {
				ofParameter<int>& ip = p.cast<int>();
				binding.kind = TIMELINE_ENUM_LIKE_PATHS.count(path) ? TFTimelineBinding::Kind::EnumInt : TFTimelineBinding::Kind::Int;
				binding.get = [&ip]() { return static_cast<double>(ip.get()); };
				binding.set = [&ip](double v) { ip.set(static_cast<int>(std::lround(v))); };
				binding.hasRange = true;
				binding.minValue = ip.getMin();
				binding.maxValue = ip.getMax();
			} else if (p.isOfType<bool>()) {
				ofParameter<bool>& bp = p.cast<bool>();
				binding.kind = TFTimelineBinding::Kind::Bool;
				binding.get = [&bp]() { return bp.get() ? 1.0 : 0.0; };
				binding.set = [&bp](double v) { bp.set(v >= 0.5); };
				binding.hasRange = true;
				binding.minValue = 0.0;
				binding.maxValue = 1.0;
			} else {
				binding.kind = TFTimelineBinding::Kind::Unsupported;
			}

			out[path] = binding;
		}
	}
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

	bandsOrientation.set("Orientation", 0, 0, 3); // 0 Vertical / 1 Horizontal / 2 Axis Flip / 3 Diagonal
	bandsBandCount.set("Band Count", BANDS_BAND_COUNT, 2, 40);
	bandsWidthVariation.set("Width Variation", BANDS_WIDTH_VARIATION, 0.0f, 1.0f);
	bandsDiagonalAngleDeg.set("Diagonal Angle", BANDS_DIAGONAL_ANGLE_DEG, 0.0f, 90.0f);
	bandsOffsetMode.set("Offset Mode", 0, 0, 1); // 0 Dynamic / 1 Strata
	bandsGroup.setName("Bands");
	bandsGroup.add(bandsOrientation);
	bandsGroup.add(bandsBandCount);
	bandsGroup.add(bandsWidthVariation);
	bandsGroup.add(bandsDiagonalAngleDeg);
	bandsGroup.add(bandsOffsetMode);

	columnGridColumnCount.set("Column Count", COLUMNGRID_COLUMN_COUNT, 1, 20);
	columnGridRowsPerColumn.set("Rows Per Column", COLUMNGRID_ROWS_PER_COLUMN, 1, 20);
	columnGridRowHeightVariation.set("Row Height Variation", COLUMNGRID_ROW_HEIGHT_VARIATION, 0.0f, 1.0f);
	columnGridBrickOffset.set("Brick Offset", COLUMNGRID_BRICK_OFFSET);
	columnGridGroup.setName("ColumnGrid");
	columnGridGroup.add(columnGridColumnCount);
	columnGridGroup.add(columnGridRowsPerColumn);
	columnGridGroup.add(columnGridRowHeightVariation);
	columnGridGroup.add(columnGridBrickOffset);

	telescopingFramesRingCount.set("Ring Count", TELESCOPINGFRAMES_RING_COUNT, 1, 20);
	telescopingFramesThicknessVariation.set("Thickness Variation", TELESCOPINGFRAMES_THICKNESS_VARIATION, 0.0f, 1.0f);
	telescopingFramesGroup.setName("TelescopingFrames");
	telescopingFramesGroup.add(telescopingFramesRingCount);
	telescopingFramesGroup.add(telescopingFramesThicknessVariation);

	particleFieldSpawnRate.set("Spawn Rate", PARTICLEFIELD_SPAWN_RATE, 0.1f, 20.0f);
	particleFieldMaxParticleCount.set("Max Particle Count", PARTICLEFIELD_MAX_PARTICLE_COUNT, 1, 200);
	particleFieldMinSize.set("Min Size", PARTICLEFIELD_MIN_SIZE, 0.01f, 0.5f);
	particleFieldMaxSize.set("Max Size", PARTICLEFIELD_MAX_SIZE, 0.01f, 0.5f);
	particleFieldMinLife.set("Min Life", PARTICLEFIELD_MIN_LIFE, 0.5f, 20.0f);
	particleFieldMaxLife.set("Max Life", PARTICLEFIELD_MAX_LIFE, 0.5f, 30.0f);
	particleFieldDriftSpeed.set("Drift Speed", PARTICLEFIELD_DRIFT_SPEED, 0.0f, 5.0f);
	particleFieldDriftDirection.set("Drift Direction", 0, 0, 2); // 0 Omnidirectional / 1 Upward / 2 Downward
	particleFieldDepthOrder.set("Depth Order", 0, 0, 1); // 0 Newest On Top / 1 Largest Behind
	particleFieldGroup.setName("ParticleField");
	particleFieldGroup.add(particleFieldSpawnRate);
	particleFieldGroup.add(particleFieldMaxParticleCount);
	particleFieldGroup.add(particleFieldMinSize);
	particleFieldGroup.add(particleFieldMaxSize);
	particleFieldGroup.add(particleFieldMinLife);
	particleFieldGroup.add(particleFieldMaxLife);
	particleFieldGroup.add(particleFieldDriftSpeed);
	particleFieldGroup.add(particleFieldDriftDirection);
	particleFieldGroup.add(particleFieldDepthOrder);

	successionGridResolution.set("Grid Resolution", SUCCESSION_GRID_RESOLUTION, 4, 30);
	successionMaturityTime.set("Maturity Time", SUCCESSION_MATURITY_TIME, 5.0f, 120.0f);
	successionYoungTurnoverInterval.set("Young Turnover", SUCCESSION_YOUNG_TURNOVER_INTERVAL, 0.2f, 10.0f);
	successionClimaxTurnoverInterval.set("Climax Turnover", SUCCESSION_CLIMAX_TURNOVER_INTERVAL, 2.0f, 60.0f);
	successionYoungHardCutWeight.set("Young Hard Cut Weight", SUCCESSION_YOUNG_HARD_CUT_WEIGHT, 0.0f, 100.0f);
	successionClimaxErosionWeight.set("Climax Erosion Weight", SUCCESSION_CLIMAX_EROSION_WEIGHT, 0.0f, 100.0f);
	successionCrossfadeWeight.set("Crossfade Weight", SUCCESSION_CROSSFADE_WEIGHT, 0.0f, 100.0f);
	successionSproutThreshold.set("Sprout Threshold", SUCCESSION_SPROUT_THRESHOLD, 0.0f, 0.5f);
	successionMinRenderScale.set("Min Render Scale", SUCCESSION_MIN_RENDER_SCALE, 0.05f, 1.0f);
	successionDisturbanceInterval.set("Disturbance Interval", SUCCESSION_DISTURBANCE_INTERVAL, 2.0f, 60.0f);
	successionDisturbanceRadius.set("Disturbance Radius", SUCCESSION_DISTURBANCE_RADIUS, 0.02f, 0.6f);
	successionDisturbanceFlashDuration.set("Disturbance Flash Duration", SUCCESSION_DISTURBANCE_FLASH_DURATION, 0.1f, 3.0f);
	ecologicalSuccessionGroup.setName("EcologicalSuccession");
	ecologicalSuccessionGroup.add(successionGridResolution);
	ecologicalSuccessionGroup.add(successionMaturityTime);
	ecologicalSuccessionGroup.add(successionYoungTurnoverInterval);
	ecologicalSuccessionGroup.add(successionClimaxTurnoverInterval);
	ecologicalSuccessionGroup.add(successionYoungHardCutWeight);
	ecologicalSuccessionGroup.add(successionClimaxErosionWeight);
	ecologicalSuccessionGroup.add(successionCrossfadeWeight);
	ecologicalSuccessionGroup.add(successionSproutThreshold);
	ecologicalSuccessionGroup.add(successionMinRenderScale);
	ecologicalSuccessionGroup.add(successionDisturbanceInterval);
	ecologicalSuccessionGroup.add(successionDisturbanceRadius);
	ecologicalSuccessionGroup.add(successionDisturbanceFlashDuration);

	networkGrowthSeedNodeCount.set("Seed Node Count", NETWORKGROWTH_SEED_NODE_COUNT, 1, 3);
	networkGrowthGrowthInterval.set("Growth Interval", NETWORKGROWTH_GROWTH_INTERVAL, 0.1f, 10.0f);
	networkGrowthMaxNodeCount.set("Max Node Count", NETWORKGROWTH_MAX_NODE_COUNT, 2, 200);
	networkGrowthBranchDistance.set("Branch Distance", NETWORKGROWTH_BRANCH_DISTANCE, 0.01f, 0.3f);
	networkGrowthBranchDistanceJitter.set("Branch Distance Jitter", NETWORKGROWTH_BRANCH_DISTANCE_JITTER, 0.0f, 1.0f);
	networkGrowthBranchAngleJitterDeg.set("Branch Angle Jitter", NETWORKGROWTH_BRANCH_ANGLE_JITTER_DEG, 0.0f, 180.0f);
	networkGrowthNodeMinSize.set("Min Size", NETWORKGROWTH_NODE_MIN_SIZE, 0.01f, 0.3f);
	networkGrowthNodeMaxSize.set("Max Size", NETWORKGROWTH_NODE_MAX_SIZE, 0.01f, 0.3f);
	networkGrowthNodeReassignInterval.set("Node Reassign Interval", NETWORKGROWTH_NODE_REASSIGN_INTERVAL, 0.5f, 30.0f);
	networkGrowthNodeLifespanEnabled.set("Node Lifespan Enabled", NETWORKGROWTH_NODE_LIFESPAN_ENABLED);
	networkGrowthMaxNodeAge.set("Max Node Age", NETWORKGROWTH_MAX_NODE_AGE, 2.0f, 120.0f);
	networkGrowthEdgeThickness.set("Edge Thickness", NETWORKGROWTH_EDGE_THICKNESS, 0.5f, 6.0f);
	networkGrowthRectangularChance.set("Rectangular Chance", NETWORKGROWTH_RECTANGULAR_CHANCE, 0.0f, 100.0f);
	networkGrowthMinAspectRatio.set("Min Aspect Ratio", NETWORKGROWTH_MIN_ASPECT_RATIO, 1.0f, 5.0f);
	networkGrowthMaxAspectRatio.set("Max Aspect Ratio", NETWORKGROWTH_MAX_ASPECT_RATIO, 1.0f, 5.0f);
	networkGrowthCropChance.set("Crop Chance", NETWORKGROWTH_CROP_CHANCE, 0.0f, 100.0f);
	networkGrowthGroup.setName("NetworkGrowth");
	networkGrowthGroup.add(networkGrowthSeedNodeCount);
	networkGrowthGroup.add(networkGrowthGrowthInterval);
	networkGrowthGroup.add(networkGrowthMaxNodeCount);
	networkGrowthGroup.add(networkGrowthBranchDistance);
	networkGrowthGroup.add(networkGrowthBranchDistanceJitter);
	networkGrowthGroup.add(networkGrowthBranchAngleJitterDeg);
	networkGrowthGroup.add(networkGrowthNodeMinSize);
	networkGrowthGroup.add(networkGrowthNodeMaxSize);
	networkGrowthGroup.add(networkGrowthNodeReassignInterval);
	networkGrowthGroup.add(networkGrowthNodeLifespanEnabled);
	networkGrowthGroup.add(networkGrowthMaxNodeAge);
	networkGrowthGroup.add(networkGrowthEdgeThickness);
	networkGrowthGroup.add(networkGrowthRectangularChance);
	networkGrowthGroup.add(networkGrowthMinAspectRatio);
	networkGrowthGroup.add(networkGrowthMaxAspectRatio);
	networkGrowthGroup.add(networkGrowthCropChance);

	tidesGridResolution.set("Grid Resolution", TIDES_GRID_RESOLUTION, 4, 40);
	tidesTideSpeed.set("Tide Speed", TIDES_TIDE_SPEED, 0.0f, 3.0f);
	tidesWaveLength.set("Wave Length", TIDES_WAVE_LENGTH, 0.1f, 5.0f);
	tidesAmplitude.set("Amplitude", TIDES_AMPLITUDE, 0.0f, 1.0f);
	tidesWaveDirection.set("Wave Direction", TIDES_WAVE_DIRECTION, 0, 2); // 0 Horizontal / 1 Vertical / 2 Diagonal
	tidesExposedThreshold.set("Exposed Threshold", TIDES_EXPOSED_THRESHOLD, 0.0f, 1.0f);
	temporalTidesGroup.setName("TemporalTides");
	temporalTidesGroup.add(tidesGridResolution);
	temporalTidesGroup.add(tidesTideSpeed);
	temporalTidesGroup.add(tidesWaveLength);
	temporalTidesGroup.add(tidesAmplitude);
	temporalTidesGroup.add(tidesWaveDirection);
	temporalTidesGroup.add(tidesExposedThreshold);

	patternRegenRateParam.set("Pattern Regen Rate", PATTERN_REGEN_RATE, 0.5f, 30.0f);
	sharedPatternGroup.setName("Pattern Regen");
	sharedPatternGroup.add(patternRegenRateParam);

	transitionDurationParam.set("Transition Duration", FRAGMENT_TRANSITION_DURATION, 0.1f, 3.0f);
	hardCutWeightParam.set("Hard Cut Weight", TRANSITION_HARD_CUT_WEIGHT, 0.0f, 100.0f);
	crossfadeWeightParam.set("Crossfade Weight", TRANSITION_CROSSFADE_WEIGHT, 0.0f, 100.0f);
	erosionWeightParam.set("Erosion Weight", TRANSITION_EROSION_WEIGHT, 0.0f, 100.0f);
	quantizeBandsParam.set("Quantize Bands", TIME_OFFSET_QUANTIZE_BANDS, 8, 16);
	// Range kept modest (2-20s) since the history ring buffer lives in CPU
	// RAM (see TimeOffsetVideoBuffer.h's Pi 3B memory note) — larger values
	// make the most-delayed slice reach further back for a more pronounced
	// difference, smaller values make it subtler.
	maxHistorySecondsParam.set("Max History Seconds", TIME_OFFSET_MAX_HISTORY_SECONDS, 2.0f, 20.0f);
	transitionGroup.setName("Transition");
	transitionGroup.add(transitionDurationParam);
	transitionGroup.add(hardCutWeightParam);
	transitionGroup.add(crossfadeWeightParam);
	transitionGroup.add(erosionWeightParam);
	transitionGroup.add(quantizeBandsParam);
	transitionGroup.add(maxHistorySecondsParam);

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
	bgEffectWeightHueRotate.set("Hue Rotate", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightAsciiSolarpunk.set("Ascii Solarpunk", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightBioluminescence.set("Bioluminescence", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightChromaticAberration.set("Chromatic Aberration", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightEdgeGlow.set("Edge Glow", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightInkOutlines.set("Ink Outlines", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightPixelDrift.set("Pixel Drift", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightPixelSorting.set("Pixel Sorting", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightWaterRefraction.set("Water Refraction", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
	bgEffectWeightHeatmapRecolor.set("Heatmap Recolor", BACKGROUND_EFFECT_DEFAULT_WEIGHT, 0.0f, 100.0f);
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
	backgroundEffectGroup.add(bgEffectWeightHueRotate);
	backgroundEffectGroup.add(bgEffectWeightAsciiSolarpunk);
	backgroundEffectGroup.add(bgEffectWeightBioluminescence);
	backgroundEffectGroup.add(bgEffectWeightChromaticAberration);
	backgroundEffectGroup.add(bgEffectWeightEdgeGlow);
	backgroundEffectGroup.add(bgEffectWeightInkOutlines);
	backgroundEffectGroup.add(bgEffectWeightPixelDrift);
	backgroundEffectGroup.add(bgEffectWeightPixelSorting);
	backgroundEffectGroup.add(bgEffectWeightWaterRefraction);
	backgroundEffectGroup.add(bgEffectWeightHeatmapRecolor);

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
	waypointIntervalMinParam.set("Waypoint Interval Min", WAYPOINT_CHANGE_INTERVAL_MIN, 5.0f, 120.0f);
	waypointIntervalMaxParam.set("Waypoint Interval Max", WAYPOINT_CHANGE_INTERVAL_MAX, 5.0f, 180.0f);
	waypointPullStrengthParam.set("Waypoint Pull Strength", WAYPOINT_PULL_STRENGTH, 0.01f, 1.0f);
	evolutionGroup.setName("Evolution");
	evolutionGroup.add(evolutionEnabledParam);
	evolutionGroup.add(waypointIntervalMinParam);
	evolutionGroup.add(waypointIntervalMaxParam);
	evolutionGroup.add(waypointPullStrengthParam);
	nextWaypointInterval = ofRandom(waypointIntervalMinParam.get(), waypointIntervalMaxParam.get());

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
	rootGroup.add(bandsGroup);
	rootGroup.add(columnGridGroup);
	rootGroup.add(telescopingFramesGroup);
	rootGroup.add(particleFieldGroup);
	rootGroup.add(ecologicalSuccessionGroup);
	rootGroup.add(networkGrowthGroup);
	rootGroup.add(temporalTidesGroup);
	rootGroup.add(sharedPatternGroup);
	rootGroup.add(transitionGroup);
	rootGroup.add(backgroundGroup);
	rootGroup.add(evolutionGroup);
	rootGroup.add(hudEventGroup);
	rootGroup.add(presetGroup);

	panel.setup(rootGroup);

	// Registry used by savePreset()/loadNextPreset()/pickNewWaypoint() so
	// none of those need a hardcoded branch per pattern.
	patternGroups = {
		{ TFPatternType::BSP, &bspGroup },
		{ TFPatternType::BLOB_GRID, &blobGridGroup },
		{ TFPatternType::BANDS, &bandsGroup },
		{ TFPatternType::COLUMN_GRID, &columnGridGroup },
		{ TFPatternType::TELESCOPING_FRAMES, &telescopingFramesGroup },
		{ TFPatternType::PARTICLE_FIELD, &particleFieldGroup },
		{ TFPatternType::ECOLOGICAL_SUCCESSION, &ecologicalSuccessionGroup },
		{ TFPatternType::NETWORK_GROWTH, &networkGrowthGroup },
		{ TFPatternType::TEMPORAL_TIDES, &temporalTidesGroup },
	};

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

	// Discrete choice dials (orientation/mode/offset-mode/brick-offset/
	// drift-direction/depth-order) are deliberately excluded, same as
	// bgSplitAxisChoice above — toggling them continuously would be
	// disruptive rather than "drift slightly".
	registerEvolving(bandsBandCount, 2.0f, 40.0f);
	registerEvolving(bandsWidthVariation, 0.0f, 1.0f);
	registerEvolving(bandsDiagonalAngleDeg, 0.0f, 90.0f);

	registerEvolving(columnGridColumnCount, 1.0f, 20.0f);
	registerEvolving(columnGridRowsPerColumn, 1.0f, 20.0f);
	registerEvolving(columnGridRowHeightVariation, 0.0f, 1.0f);

	registerEvolving(telescopingFramesRingCount, 1.0f, 20.0f);
	registerEvolving(telescopingFramesThicknessVariation, 0.0f, 1.0f);

	registerEvolving(particleFieldSpawnRate, 0.1f, 20.0f);
	registerEvolving(particleFieldMaxParticleCount, 1.0f, 200.0f);
	registerEvolving(particleFieldMinSize, 0.01f, 0.5f);
	registerEvolving(particleFieldMaxSize, 0.01f, 0.5f);
	registerEvolving(particleFieldMinLife, 0.5f, 20.0f);
	registerEvolving(particleFieldMaxLife, 0.5f, 30.0f);
	registerEvolving(particleFieldDriftSpeed, 0.0f, 5.0f);

	registerEvolving(successionGridResolution, 4.0f, 30.0f);
	registerEvolving(successionMaturityTime, 5.0f, 120.0f);
	registerEvolving(successionYoungTurnoverInterval, 0.2f, 10.0f);
	registerEvolving(successionClimaxTurnoverInterval, 2.0f, 60.0f);
	registerEvolving(successionYoungHardCutWeight, 0.0f, 100.0f);
	registerEvolving(successionClimaxErosionWeight, 0.0f, 100.0f);
	registerEvolving(successionCrossfadeWeight, 0.0f, 100.0f);
	registerEvolving(successionSproutThreshold, 0.0f, 0.5f);
	registerEvolving(successionMinRenderScale, 0.05f, 1.0f);
	registerEvolving(successionDisturbanceInterval, 2.0f, 60.0f);
	registerEvolving(successionDisturbanceRadius, 0.02f, 0.6f);
	registerEvolving(successionDisturbanceFlashDuration, 0.1f, 3.0f);

	registerEvolving(networkGrowthSeedNodeCount, 1.0f, 3.0f);
	registerEvolving(networkGrowthGrowthInterval, 0.1f, 10.0f);
	registerEvolving(networkGrowthMaxNodeCount, 2.0f, 200.0f);
	registerEvolving(networkGrowthBranchDistance, 0.01f, 0.3f);
	registerEvolving(networkGrowthBranchDistanceJitter, 0.0f, 1.0f);
	registerEvolving(networkGrowthBranchAngleJitterDeg, 0.0f, 180.0f);
	registerEvolving(networkGrowthNodeMinSize, 0.01f, 0.3f);
	registerEvolving(networkGrowthNodeMaxSize, 0.01f, 0.3f);
	registerEvolving(networkGrowthNodeReassignInterval, 0.5f, 30.0f);
	registerEvolving(networkGrowthMaxNodeAge, 2.0f, 120.0f);
	registerEvolving(networkGrowthEdgeThickness, 0.5f, 6.0f);
	registerEvolving(networkGrowthRectangularChance, 0.0f, 100.0f);
	registerEvolving(networkGrowthMinAspectRatio, 1.0f, 5.0f);
	registerEvolving(networkGrowthMaxAspectRatio, 1.0f, 5.0f);
	registerEvolving(networkGrowthCropChance, 0.0f, 100.0f);
	// networkGrowthNodeLifespanEnabled excluded — bool, same reasoning as
	// Mask To Blob/Transparent Background/Evolution Enabled above.

	registerEvolving(tidesGridResolution, 4.0f, 40.0f);
	registerEvolving(tidesTideSpeed, 0.0f, 3.0f);
	registerEvolving(tidesWaveLength, 0.1f, 5.0f);
	registerEvolving(tidesAmplitude, 0.0f, 1.0f);
	registerEvolving(tidesExposedThreshold, 0.0f, 1.0f);
	// tidesWaveDirection excluded — discrete choice, same reasoning as
	// bgSplitAxisChoice/orientation/offsetMode above.

	registerEvolving(patternRegenRateParam, 0.5f, 30.0f);

	registerEvolving(transitionDurationParam, 0.1f, 3.0f);
	registerEvolving(hardCutWeightParam, 0.0f, 100.0f);
	registerEvolving(crossfadeWeightParam, 0.0f, 100.0f);
	registerEvolving(erosionWeightParam, 0.0f, 100.0f);
	registerEvolving(quantizeBandsParam, 8.0f, 16.0f);
	registerEvolving(maxHistorySecondsParam, 2.0f, 20.0f);

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
	registerEvolving(bgEffectWeightHueRotate, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightAsciiSolarpunk, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightBioluminescence, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightChromaticAberration, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightEdgeGlow, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightInkOutlines, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightPixelDrift, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightPixelSorting, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightWaterRefraction, 0.0f, 100.0f);
	registerEvolving(bgEffectWeightHeatmapRecolor, 0.0f, 100.0f);

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

	buildTimelineBindings();
	timeline.onWarning = [](const std::string& msg) { ofLogWarning("TFPresetTimeline") << msg; };
	timeline.onNotice = [](const std::string& msg) { ofLogNotice("TFPresetTimeline") << msg; };

	applyDefaultGuiCollapseState();
}

void TFParameterPanel::buildTimelineBindings() {
	timelineBindings.clear();
	for (const auto& entry : patternGroups) {
		walkGroupForTimelineBindings(*entry.second, tfPatternTypePresetKey(entry.first), timelineBindings);
	}
	walkGroupForTimelineBindings(transitionGroup, "transition", timelineBindings);
	walkGroupForTimelineBindings(sharedPatternGroup, "patternregen", timelineBindings);
	walkGroupForTimelineBindings(backgroundGroup, "background", timelineBindings);
}

void TFParameterPanel::registerBackgroundTextures(ofParameterGroup& textureGroup) {
	rootGroup.add(textureGroup);
	panel.setup(rootGroup); // rebuilds the widget tree; safe to call again with more groups already present
	applyDefaultGuiCollapseState(); // the rebuild above creates fresh, unminimized widgets
}

void TFParameterPanel::registerHudVisibility(ofParameterGroup& hudVisibilityGroup) {
	rootGroup.add(hudVisibilityGroup);
	panel.setup(rootGroup); // rebuilds the widget tree; safe to call again with more groups already present
	hudVisibilityGroupPtr = &hudVisibilityGroup; // kept so savePreset()/loadNextPreset() can (de)serialize it
	applyDefaultGuiCollapseState(); // the rebuild above creates fresh, unminimized widgets
}

void TFParameterPanel::setActivePattern(TFPatternType type) {
	if (type == displayedActivePattern) {
		return;
	}
	displayedActivePattern = type;
	applyActivePatternCollapse();
}

void TFParameterPanel::applyDefaultGuiCollapseState() {
	panel.minimizeAll();
	applyActivePatternCollapse();
}

void TFParameterPanel::applyActivePatternCollapse() {
	for (const auto& entry : patternGroups) {
		ofxGuiGroup& widget = panel.getGroup(entry.second->getName());
		if (entry.first == displayedActivePattern) {
			widget.maximize();
		} else {
			widget.minimize();
		}
	}
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
	timeline.update(dt);
	if (timeline.isActive()) {
		// An authored multi-state timeline owns these dials for the
		// duration of this preset — don't let the wobble/waypoint system
		// (below) fight it over the same values.
		return;
	}

	wobbleLfo.update(dt);

	if (!evolutionEnabledParam.get()) {
		// Manual tuning mode — don't fight the user's own slider drags.
		return;
	}

	waypointTimer += dt;
	if (waypointTimer >= nextWaypointInterval) {
		waypointTimer = 0.0f;
		nextWaypointInterval = ofRandom(waypointIntervalMinParam.get(), std::max(waypointIntervalMinParam.get(), waypointIntervalMaxParam.get()));
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
	TFPatternType presetPattern = tfPatternTypeFromPresetKey(patternStr);

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
	std::string presetKey = tfPatternTypePresetKey(presetPattern);
	if (json.contains(presetKey)) {
		extractInto(json[presetKey]);
	}
	if (json.contains("transition")) {
		extractInto(json["transition"]);
	}
	if (json.contains("patternregen")) {
		extractInto(json["patternregen"]);
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

bool TFParameterPanel::isTimelineActive() const { return timeline.isActive(); }

bool TFParameterPanel::isTimelinePaused() const { return timeline.isPaused(); }

void TFParameterPanel::restartTimeline() { timeline.restart(); }

void TFParameterPanel::pauseTimeline() { timeline.pause(); }

void TFParameterPanel::resumeTimeline() { timeline.resume(); }

void TFParameterPanel::advanceTimelineState() { timeline.advanceToNextState(); }

bool TFParameterPanel::jumpTimelineState(const std::string& name) { return timeline.jumpToState(name); }

std::string TFParameterPanel::getTimelineDebugStatus() const {
	if (!timeline.isActive()) {
		return "";
	}
	return "Timeline: " + currentPresetDisplayName + "\n" + timeline.getDebugStatusLine();
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

TFPatternBands::Params TFParameterPanel::getBandsParams() const {
	TFPatternBands::Params p;
	p.orientation = static_cast<TFPatternBands::Orientation>(bandsOrientation.get());
	p.bandCount = bandsBandCount;
	p.widthVariation = bandsWidthVariation;
	p.diagonalAngleDeg = bandsDiagonalAngleDeg;
	p.offsetMode = static_cast<TFPatternBands::OffsetMode>(bandsOffsetMode.get());
	p.patternRegenRate = patternRegenRateParam;
	p.transitionDuration = transitionDurationParam;
	p.hardCutWeight = hardCutWeightParam;
	p.crossfadeWeight = crossfadeWeightParam;
	p.erosionWeight = erosionWeightParam;
	return p;
}

TFPatternColumnGrid::Params TFParameterPanel::getColumnGridParams() const {
	TFPatternColumnGrid::Params p;
	p.columnCount = columnGridColumnCount;
	p.rowsPerColumn = columnGridRowsPerColumn;
	p.rowHeightVariation = columnGridRowHeightVariation;
	p.brickOffset = columnGridBrickOffset;
	p.patternRegenRate = patternRegenRateParam;
	p.transitionDuration = transitionDurationParam;
	p.hardCutWeight = hardCutWeightParam;
	p.crossfadeWeight = crossfadeWeightParam;
	p.erosionWeight = erosionWeightParam;
	return p;
}

TFPatternTelescopingFrames::Params TFParameterPanel::getTelescopingFramesParams() const {
	TFPatternTelescopingFrames::Params p;
	p.ringCount = telescopingFramesRingCount;
	p.thicknessVariation = telescopingFramesThicknessVariation;
	p.patternRegenRate = patternRegenRateParam;
	p.transitionDuration = transitionDurationParam;
	p.hardCutWeight = hardCutWeightParam;
	p.crossfadeWeight = crossfadeWeightParam;
	p.erosionWeight = erosionWeightParam;
	return p;
}

TFPatternParticleField::Params TFParameterPanel::getParticleFieldParams() const {
	TFPatternParticleField::Params p;
	p.spawnRate = particleFieldSpawnRate;
	p.maxParticleCount = particleFieldMaxParticleCount;
	p.minSize = particleFieldMinSize;
	p.maxSize = particleFieldMaxSize;
	p.minLife = particleFieldMinLife;
	p.maxLife = particleFieldMaxLife;
	p.driftSpeed = particleFieldDriftSpeed;
	p.driftDirection = static_cast<TFPatternParticleField::DriftDirection>(particleFieldDriftDirection.get());
	p.depthOrder = static_cast<TFPatternParticleField::DepthOrder>(particleFieldDepthOrder.get());
	// Reuses the shared transition weights/duration — deliberate reuse per
	// Section 6, not a separate dial set.
	p.transitionDuration = transitionDurationParam;
	p.hardCutWeight = hardCutWeightParam;
	p.crossfadeWeight = crossfadeWeightParam;
	p.erosionWeight = erosionWeightParam;
	return p;
}

TFPatternEcologicalSuccession::Params TFParameterPanel::getEcologicalSuccessionParams() const {
	TFPatternEcologicalSuccession::Params p;
	p.gridResolution = successionGridResolution;
	p.maturityTime = successionMaturityTime;
	p.youngTurnoverInterval = successionYoungTurnoverInterval;
	p.climaxTurnoverInterval = successionClimaxTurnoverInterval;
	p.youngHardCutWeight = successionYoungHardCutWeight;
	p.climaxErosionWeight = successionClimaxErosionWeight;
	p.crossfadeWeight = successionCrossfadeWeight;
	p.transitionDuration = transitionDurationParam;
	p.sproutThreshold = successionSproutThreshold;
	p.minRenderScale = successionMinRenderScale;
	p.disturbanceInterval = successionDisturbanceInterval;
	p.disturbanceRadius = successionDisturbanceRadius;
	p.disturbanceFlashDuration = successionDisturbanceFlashDuration;
	return p;
}

TFPatternNetworkGrowth::Params TFParameterPanel::getNetworkGrowthParams() const {
	TFPatternNetworkGrowth::Params p;
	p.seedNodeCount = networkGrowthSeedNodeCount;
	p.growthInterval = networkGrowthGrowthInterval;
	p.maxNodeCount = networkGrowthMaxNodeCount;
	p.branchDistance = networkGrowthBranchDistance;
	p.branchDistanceJitter = networkGrowthBranchDistanceJitter;
	p.branchAngleJitterDeg = networkGrowthBranchAngleJitterDeg;
	p.nodeMinSize = networkGrowthNodeMinSize;
	p.nodeMaxSize = networkGrowthNodeMaxSize;
	p.nodeReassignInterval = networkGrowthNodeReassignInterval;
	p.nodeLifespanEnabled = networkGrowthNodeLifespanEnabled;
	p.maxNodeAge = networkGrowthMaxNodeAge;
	p.edgeThickness = networkGrowthEdgeThickness;
	p.rectangularChance = networkGrowthRectangularChance;
	p.minAspectRatio = networkGrowthMinAspectRatio;
	p.maxAspectRatio = networkGrowthMaxAspectRatio;
	p.cropChance = networkGrowthCropChance;
	// Reuses the shared transition weights/duration — deliberate reuse, not
	// a separate dial set (same reasoning as Particle Field's params above).
	p.transitionDuration = transitionDurationParam;
	p.hardCutWeight = hardCutWeightParam;
	p.crossfadeWeight = crossfadeWeightParam;
	p.erosionWeight = erosionWeightParam;
	return p;
}

TFPatternTemporalTides::Params TFParameterPanel::getTemporalTidesParams() const {
	TFPatternTemporalTides::Params p;
	p.gridResolution = tidesGridResolution;
	p.tideSpeed = tidesTideSpeed;
	p.waveLength = tidesWaveLength;
	p.amplitude = tidesAmplitude;
	p.waveDirection = static_cast<TFPatternTemporalTides::WaveDirection>(tidesWaveDirection.get());
	p.exposedThreshold = tidesExposedThreshold;
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
	p.effectWeights.effectWeights["hue_rotate"] = bgEffectWeightHueRotate;
	p.effectWeights.effectWeights["ascii_solarpunk"] = bgEffectWeightAsciiSolarpunk;
	p.effectWeights.effectWeights["bioluminescence"] = bgEffectWeightBioluminescence;
	p.effectWeights.effectWeights["chromatic_aberration"] = bgEffectWeightChromaticAberration;
	p.effectWeights.effectWeights["edge_glow"] = bgEffectWeightEdgeGlow;
	p.effectWeights.effectWeights["ink_outlines"] = bgEffectWeightInkOutlines;
	p.effectWeights.effectWeights["pixel_drift"] = bgEffectWeightPixelDrift;
	p.effectWeights.effectWeights["pixel_sorting"] = bgEffectWeightPixelSorting;
	p.effectWeights.effectWeights["water_refraction"] = bgEffectWeightWaterRefraction;
	p.effectWeights.effectWeights["heatmap_recolor"] = bgEffectWeightHeatmapRecolor;

	return p;
}

void TFParameterPanel::savePreset(TFPatternType activePattern) {
	std::string name = presetNameParam.get();
	if (name.empty()) {
		name = "preset";
	}

	ofJson json;
	json["pattern"] = tfPatternTypePresetKey(activePattern);

	// Every pattern's own group is always included (not just the active
	// one) — the same behavior the original bsp/blobgrid-only version had,
	// generalized so six patterns don't need six hardcoded branches.
	for (const auto& entry : patternGroups) {
		ofJson groupJson;
		ofSerialize(groupJson, *entry.second);
		json[tfPatternTypePresetKey(entry.first)] = groupJson[entry.second->getName()];
	}

	ofJson transitionJson;
	ofSerialize(transitionJson, transitionGroup);
	json["transition"] = transitionJson["Transition"];

	ofJson patternRegenJson;
	ofSerialize(patternRegenJson, sharedPatternGroup);
	json["patternregen"] = patternRegenJson["Pattern Regen"];

	// Background is pattern-agnostic (like Transition) — always included
	// regardless of which pattern this preset belongs to. First *nested*
	// ofParameterGroup in this schema (Effects lives inside Background) —
	// ofSerialize walks nested groups recursively, so this should just work,
	// but it's new territory here; verify the round-trip explicitly.
	ofJson backgroundJson;
	ofSerialize(backgroundJson, backgroundGroup);
	json["background"] = backgroundJson["Background"];

	// HUD Visibility toggles (see TFHudLayer::getVisibilityParamGroup() /
	// registerHudVisibility()) — unlike hudEventGroup/presetGroup/
	// evolutionGroup/textureGroup, this group IS part of the preset schema:
	// which HUD elements are shown is a compositional choice a preset should
	// remember, not a plain runtime switch.
	if (hudVisibilityGroupPtr) {
		ofJson hudVisibilityJson;
		ofSerialize(hudVisibilityJson, *hudVisibilityGroupPtr);
		json["hudvisibility"] = hudVisibilityJson[hudVisibilityGroupPtr->getName()];
	}

	std::string path = ofToDataPath("presets/" + name + ".json");

	// Multi-state transition presets (see TFPresetTimeline) — this button
	// always serializes the *current live dial values* (which, while a
	// timeline is running, are whatever instant of the animated blend this
	// happens to be — same principle as saving live wobble/evolution
	// values today), never timeline authoring. Per the feature's
	// requirement #12 ("do not silently destroy an existing timeline"),
	// the loaded preset's original raw timeline JSON is preserved verbatim
	// only when re-saving back to that exact same file; saving under any
	// other name intentionally produces a plain static snapshot, with one
	// clear warning so the timeline's disappearance is never silent.
	if (!loadedTimelineJson.empty()) {
		if (loadedTimelineSourcePath == path) {
			json["timeline"] = loadedTimelineJson;
		} else {
			ofLogWarning("TFParameterPanel") << "saved '" << name << "' as a static snapshot -- "
				<< "the loaded preset's timeline is only preserved when re-saving to its original filename";
		}
	}

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
	outPattern = tfPatternTypeFromPresetKey(patternStr);

	for (const auto& entry : patternGroups) {
		std::string key = tfPatternTypePresetKey(entry.first);
		if (json.contains(key)) {
			ofJson wrapper;
			wrapper[entry.second->getName()] = json[key];
			ofDeserialize(wrapper, *entry.second);
		}
	}
	if (json.contains("transition")) {
		ofJson wrapper;
		wrapper["Transition"] = json["transition"];
		ofDeserialize(wrapper, transitionGroup);
	}
	if (json.contains("patternregen")) {
		ofJson wrapper;
		wrapper["Pattern Regen"] = json["patternregen"];
		ofDeserialize(wrapper, sharedPatternGroup);
	}
	if (json.contains("background")) {
		ofJson wrapper;
		wrapper["Background"] = json["background"];
		ofDeserialize(wrapper, backgroundGroup);
	}
	if (hudVisibilityGroupPtr && json.contains("hudvisibility")) {
		ofJson wrapper;
		wrapper[hudVisibilityGroupPtr->getName()] = json["hudvisibility"];
		ofDeserialize(wrapper, *hudVisibilityGroupPtr);
	}

	// Multi-state transition presets (see TFPresetTimeline) — always clear
	// first so a preset with no "timeline" key (or a timeline that fails
	// validation) never leaves a previous preset's stale timeline running;
	// this is what makes static presets keep loading exactly as before.
	timeline.clear();
	loadedTimelineJson = ofJson();
	loadedTimelineSourcePath.clear();
	currentPresetDisplayName = ofFilePath::getBaseName(path);

	if (json.contains("timeline")) {
		std::map<std::string, std::string> baseValues;
		for (const auto& entry : patternGroups) {
			std::string key = tfPatternTypePresetKey(entry.first);
			if (json.contains(key)) {
				TFPresetTimeline::flattenJsonToPaths(json[key], key, baseValues);
			}
		}
		if (json.contains("transition")) {
			TFPresetTimeline::flattenJsonToPaths(json["transition"], "transition", baseValues);
		}
		if (json.contains("patternregen")) {
			TFPresetTimeline::flattenJsonToPaths(json["patternregen"], "patternregen", baseValues);
		}
		if (json.contains("background")) {
			TFPresetTimeline::flattenJsonToPaths(json["background"], "background", baseValues);
		}

		if (timeline.load(json["timeline"], baseValues, timelineBindings)) {
			timeline.start();
			loadedTimelineJson = json["timeline"];
			loadedTimelineSourcePath = path;
		}
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
