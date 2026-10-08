#include "TemporalSceneCore.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)
#include "TFSettings.h"

TFBackgroundLayer::Params TemporalSceneCore::defaultBackgroundParams() {
	TFBackgroundLayer::Params p;
	// Real, in-class struct defaults for everything except effectWeights —
	// see class header comment.
	p.effectWeights.cycleInterval = BACKGROUND_EFFECT_CYCLE_INTERVAL;
	p.effectWeights.rawWeight = BACKGROUND_EFFECT_RAW_WEIGHT;
	// Exact same 18-effect list TFParameterPanel::getBackgroundParams()
	// seeds its own ofParameter<float>s with, at the exact same default
	// weight — see that method's own .cpp for the source of truth this
	// list is copied from.
	p.effectWeights.effectWeights["desaturate"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["invert"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["recolor"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["threshold"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["dither"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["solarize"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["scanlines"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["channelshift"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["hue_rotate"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["ascii_solarpunk"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["bioluminescence"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["chromatic_aberration"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["edge_glow"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["ink_outlines"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["pixel_drift"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["pixel_sorting"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["water_refraction"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	p.effectWeights.effectWeights["heatmap_recolor"] = BACKGROUND_EFFECT_DEFAULT_WEIGHT;
	return p;
}

void TemporalSceneCore::setup(TimeOffsetVideoBuffer* buffer, int canvasW, int canvasH) {
	buffer_ = buffer;
	canvasW_ = canvasW;
	canvasH_ = canvasH;

	{ RT004_SCOPE("TemporalCore.setup.shaderLib");
	shaderLib_.setup();
	}

	{ RT004_SCOPE("TemporalCore.setup.ambientTextures");
	ambientTextures_.setup(canvasW_, canvasH_, "backgrounds");
	}

	{ RT004_SCOPE("TemporalCore.setup.backgroundLayer");
	backgroundLayer_.setup(buffer_, &shaderLib_, "backgrounds", canvasW_, canvasH_, defaultBackgroundParams());
	}

	{ RT004_SCOPE("TemporalCore.setup.bspPattern_"); bspPattern_.setup(buffer_, canvasW_, canvasH_, TFPatternBSP::Params{}); }
	{ RT004_SCOPE("TemporalCore.setup.blobGridPattern_"); blobGridPattern_.setup(buffer_, canvasW_, canvasH_, TFPatternBlobGrid::Params{}); }
	{ RT004_SCOPE("TemporalCore.setup.bandsPattern_"); bandsPattern_.setup(buffer_, canvasW_, canvasH_, TFPatternBands::Params{}); }
	{ RT004_SCOPE("TemporalCore.setup.columnGridPattern_"); columnGridPattern_.setup(buffer_, canvasW_, canvasH_, TFPatternColumnGrid::Params{}); }
	{ RT004_SCOPE("TemporalCore.setup.telescopingFramesPattern_"); telescopingFramesPattern_.setup(buffer_, canvasW_, canvasH_, TFPatternTelescopingFrames::Params{}); }
	{ RT004_SCOPE("TemporalCore.setup.particleFieldPattern_"); particleFieldPattern_.setup(buffer_, canvasW_, canvasH_, TFPatternParticleField::Params{}); }
	{ RT004_SCOPE("TemporalCore.setup.ecologicalSuccessionPattern_"); ecologicalSuccessionPattern_.setup(buffer_, canvasW_, canvasH_, TFPatternEcologicalSuccession::Params{}); }
	{ RT004_SCOPE("TemporalCore.setup.networkGrowthPattern_"); networkGrowthPattern_.setup(buffer_, canvasW_, canvasH_, TFPatternNetworkGrowth::Params{}); }
	{ RT004_SCOPE("TemporalCore.setup.temporalTidesPattern_"); temporalTidesPattern_.setup(buffer_, canvasW_, canvasH_, TFPatternTemporalTides::Params{}); }

	TFComposition::Timing timing;
	timing.cycleDuration = CYCLE_DURATION;
	RT004_SCOPE("TemporalCore.setup.composition");
	composition_.setup(timing,
		{
			{ TFPatternType::BSP, &bspPattern_ },
			{ TFPatternType::BLOB_GRID, &blobGridPattern_ },
			{ TFPatternType::BANDS, &bandsPattern_ },
			{ TFPatternType::COLUMN_GRID, &columnGridPattern_ },
			{ TFPatternType::TELESCOPING_FRAMES, &telescopingFramesPattern_ },
			{ TFPatternType::PARTICLE_FIELD, &particleFieldPattern_ },
			{ TFPatternType::ECOLOGICAL_SUCCESSION, &ecologicalSuccessionPattern_ },
			{ TFPatternType::NETWORK_GROWTH, &networkGrowthPattern_ },
			{ TFPatternType::TEMPORAL_TIDES, &temporalTidesPattern_ },
		},
		canvasW_, canvasH_);
	// Transition params (duration/hard-cut/crossfade/erosion weights) and
	// auto-cycle-suspension are left at TFComposition's own in-class
	// defaults — nothing here corresponds to a live GUI dial in production.

	didSetup_ = true;
}

void TemporalSceneCore::activate() {
	// A fresh cycle every (re)activation — never carries over a
	// mid-transition/mid-cycle state from a previous activation. See class
	// header comment.
	composition_.startCycle();
	active_ = true;
}

void TemporalSceneCore::deactivate() {
	active_ = false;
}

void TemporalSceneCore::reset() {
	composition_.startCycle();
}

void TemporalSceneCore::shutdown() {
	// See header comment — no separate resource release needed.
}

void TemporalSceneCore::resizeCanvas(int canvasW, int canvasH) {
	canvasW_ = canvasW;
	canvasH_ = canvasH;
	ambientTextures_.resizeCanvas(canvasW_, canvasH_);
	backgroundLayer_.resizeCanvas(canvasW_, canvasH_);
	composition_.resizeCanvas(canvasW_, canvasH_);
}

void TemporalSceneCore::update(float dt) {
	if (!active_) {
		return;
	}
	{ RT004_SCOPE("TemporalCore.background.update"); backgroundLayer_.update(dt); }
	{ RT004_SCOPE("TemporalCore.composition.update"); composition_.update(dt); }
}

void TemporalSceneCore::draw() {
	if (!active_) {
		return;
	}
	{ RT004_SCOPE("TemporalCore.background.draw"); backgroundLayer_.draw(); }
	{ RT004_SCOPE("TemporalCore.ambient.underlay"); ambientTextures_.drawUnderlay(); }
	{ RT004_SCOPE("TemporalCore.composition.draw"); composition_.draw(); }
	{ RT004_SCOPE("TemporalCore.ambient.overlay"); ambientTextures_.drawOverlay(); }
}
