#include "TemporalSceneCore.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)
#include "TFSettings.h"

void TemporalSceneCore::setup(TimeOffsetVideoBuffer* buffer, int canvasW, int canvasH) {
	buffer_ = buffer;
	canvasW_ = canvasW;
	canvasH_ = canvasH;

	// Same ordering as the standalone ofApp::setup(): the parameter model
	// first, so every layer/pattern is set up from its values.
	{ RT004_SCOPE("TemporalCore.setup.params"); params_.setup(); }

	{ RT004_SCOPE("TemporalCore.setup.shaderLib");
	shaderLib_.setup();
	}

	{ RT004_SCOPE("TemporalCore.setup.ambientTextures");
	ambientTextures_.setup(canvasW_, canvasH_, "backgrounds");
	}

	{ RT004_SCOPE("TemporalCore.setup.backgroundLayer");
	backgroundLayer_.setup(buffer_, &shaderLib_, "backgrounds", canvasW_, canvasH_, params_.getBackgroundParams());
	}

	{ RT004_SCOPE("TemporalCore.setup.bspPattern_"); bspPattern_.setup(buffer_, canvasW_, canvasH_, params_.getBSPParams()); }
	{ RT004_SCOPE("TemporalCore.setup.blobGridPattern_"); blobGridPattern_.setup(buffer_, canvasW_, canvasH_, params_.getBlobGridParams()); }
	{ RT004_SCOPE("TemporalCore.setup.bandsPattern_"); bandsPattern_.setup(buffer_, canvasW_, canvasH_, params_.getBandsParams()); }
	{ RT004_SCOPE("TemporalCore.setup.columnGridPattern_"); columnGridPattern_.setup(buffer_, canvasW_, canvasH_, params_.getColumnGridParams()); }
	{ RT004_SCOPE("TemporalCore.setup.telescopingFramesPattern_"); telescopingFramesPattern_.setup(buffer_, canvasW_, canvasH_, params_.getTelescopingFramesParams()); }
	{ RT004_SCOPE("TemporalCore.setup.particleFieldPattern_"); particleFieldPattern_.setup(buffer_, canvasW_, canvasH_, params_.getParticleFieldParams()); }
	{ RT004_SCOPE("TemporalCore.setup.ecologicalSuccessionPattern_"); ecologicalSuccessionPattern_.setup(buffer_, canvasW_, canvasH_, params_.getEcologicalSuccessionParams()); }
	{ RT004_SCOPE("TemporalCore.setup.networkGrowthPattern_"); networkGrowthPattern_.setup(buffer_, canvasW_, canvasH_, params_.getNetworkGrowthParams()); }
	{ RT004_SCOPE("TemporalCore.setup.temporalTidesPattern_"); temporalTidesPattern_.setup(buffer_, canvasW_, canvasH_, params_.getTemporalTidesParams()); }

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
	applyParameters(); // composition transition params + buffer settings, same as every update()

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
	// Same per-frame order as the standalone ofApp::update(): advance the
	// parameter model (evolution / timeline), push its values everywhere,
	// then background, then composition.
	{ RT004_SCOPE("TemporalCore.params.update"); params_.update(dt); applyParameters(); }
	{ RT004_SCOPE("TemporalCore.background.update"); backgroundLayer_.update(dt); }
	// A running authored timeline holds one pattern for its own duration.
	composition_.setAutoCycleSuspended(params_.isTimelineActive());
	{ RT004_SCOPE("TemporalCore.composition.update"); composition_.update(dt); }
}

void TemporalSceneCore::applyParameters() {
	bspPattern_.setParams(params_.getBSPParams());
	blobGridPattern_.setParams(params_.getBlobGridParams());
	bandsPattern_.setParams(params_.getBandsParams());
	columnGridPattern_.setParams(params_.getColumnGridParams());
	telescopingFramesPattern_.setParams(params_.getTelescopingFramesParams());
	particleFieldPattern_.setParams(params_.getParticleFieldParams());
	ecologicalSuccessionPattern_.setParams(params_.getEcologicalSuccessionParams());
	networkGrowthPattern_.setParams(params_.getNetworkGrowthParams());
	temporalTidesPattern_.setParams(params_.getTemporalTidesParams());
	if (buffer_ != nullptr) {
		buffer_->setNumQuantizeBands(params_.getQuantizeBands());
		buffer_->setMaxHistorySeconds(params_.getMaxHistorySeconds());
	}
	composition_.setTransitionParams(params_.getTransitionDuration(), params_.getHardCutWeight(),
		params_.getCrossfadeWeight(), params_.getErosionWeight());
	backgroundLayer_.setParams(params_.getBackgroundParams());
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
