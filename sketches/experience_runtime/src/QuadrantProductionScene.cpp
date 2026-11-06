#include "QuadrantProductionScene.h"

#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)

#include "ofFileUtils.h"

#include <algorithm>
#include <cctype>

namespace {

quadrantsemantics::Expansion expansionOf(quadrantcrosshair::ExpansionState s) {
	using quadrantcrosshair::ExpansionState;
	switch (s) {
		case ExpansionState::IDLE: return quadrantsemantics::Expansion::Idle;
		case ExpansionState::TRAVEL_OUT: return quadrantsemantics::Expansion::TravelOut;
		case ExpansionState::HOLD: return quadrantsemantics::Expansion::Hold;
		case ExpansionState::TRAVEL_BACK: return quadrantsemantics::Expansion::TravelBack;
	}
	return quadrantsemantics::Expansion::Idle;
}

} // namespace

QuadrantProductionScene::QuadrantProductionScene(VideoPlaybackService& video)
	: video_(video) {
}

void QuadrantProductionScene::setup(const SceneServices& services) {
	// Same as the other production scenes: consumes the runtime's canonical
	// VideoPlaybackService directly (video_), not services' root strings.
	(void)services;

	quadrantcrosshair::QuadrantSceneCore::Config config;
	config.canvasW = kNativeWidth;
	config.canvasH = kNativeHeight;
	config.trackWindow = false;
	// Canonical shader files delivered by scripts/sync-video-effect-assets.py
	// per this sketch's effect-manifest.json (byte-identical to the
	// standalone's copies).
	config.shaderLayout = quadrantcrosshair::ShaderLibrary::AssetLayout::CanonicalSync;
	// Motion extraction: the same files the runtime's shared MotionExtraction
	// (Blueprint) loads.
	config.motionVertShader = "shared-video-effects/common/vert.glsl";
	config.motionAccumShader = "shaders/motion_accum.glsl";
	config.motionExtractShader = "shaders/motion_extract.glsl";
	config.showHudInformationLayer = false; // the production HUD carries it
	{ RT004_SCOPE("Quadrant.setup.core"); core_.setup(config); }
	didSetup_ = true;
}

quadrantcrosshair::QuadrantSceneCore::VideoFrame QuadrantProductionScene::canonicalFrame() const {
	quadrantcrosshair::QuadrantSceneCore::VideoFrame frame;
	frame.texture = video_.currentTexture();
	frame.pixels = video_.currentPixels();
	frame.isFrameNew = video_.isFrameNew();
	return frame;
}

void QuadrantProductionScene::observeCanonicalMedia(bool notifyCore) {
	const VideoPlaybackStatus status = video_.status();
	if (!status.mediaId.has_value() || *status.mediaId == observedMediaId_) return;
	observedMediaId_ = *status.mediaId;
	mediaChangesSeen_++;
	if (notifyCore) {
		// The standalone passes VideoSystem's filename; HudManager classifies
		// it by keyword for its nature copy. Same here from the canonical path.
		const std::optional<std::string> path = video_.currentAbsolutePath();
		core_.onMediaChanged(path ? ofFilePath::getFileName(*path) : observedMediaId_);
	}
}

void QuadrantProductionScene::activate() {
	RT004_SCOPE("Quadrant.activate");
	// Fresh composition on every activation (incl. reactivation): idle
	// quadrants, re-dealt decks, cleared erosion and motion history,
	// crosshair/trigger/expansion/HUD-layer state rebuilt.
	core_.activate();
	// Label the HUD layer for the media playing right now.
	observedMediaId_.clear();
	observeCanonicalMedia(true);
	activeSeconds_ = 0.0f;
	lastPrimaryStateId_.clear();
	stateElapsedSeconds_ = 0.0f;
	generation_++;
}

void QuadrantProductionScene::deactivate() {
	// SceneManager stops calling update()/draw() on an inactive scene; the
	// composition simply stops advancing. Allocated FBOs are kept (reused on
	// reactivation, which clears them) so repeated switching does not churn
	// GPU memory.
	core_.deactivate();
}

void QuadrantProductionScene::update(float dt) {
	if (!core_.isActive()) {
		return;
	}
	activeSeconds_ += dt;
	// Canonical video already advanced this frame in RuntimeServices::update().
	observeCanonicalMedia(true);
	{ RT004_SCOPE("Quadrant.core.update"); core_.update(dt, canonicalFrame()); }

	const std::string state = quadrantsemantics::primaryStateId(semanticInputs());
	if (state != lastPrimaryStateId_) {
		lastPrimaryStateId_ = state;
		stateElapsedSeconds_ = 0.0f;
	} else {
		stateElapsedSeconds_ += dt;
	}
}

void QuadrantProductionScene::drawToCurrentTarget() {
	// ExperienceRuntime has already bound + cleared the runtime-owned scene
	// FBO. Artwork only — no DebugMode, no hud_overlay takeover.
	RT004_SCOPE("Quadrant.core.draw");
	core_.draw(canonicalFrame());
}

glm::ivec2 QuadrantProductionScene::nativeRenderSize() const {
	return glm::ivec2(kNativeWidth, kNativeHeight);
}

std::string QuadrantProductionScene::sceneId() const {
	return kSceneId;
}

std::string QuadrantProductionScene::displayName() const {
	return "FIELD SURVEY";
}

quadrantsemantics::Inputs QuadrantProductionScene::semanticInputs() const {
	quadrantsemantics::Inputs in;
	const auto& quadrants = core_.quadrants();
	for (int q = 0; q < 4; ++q) {
		const auto telemetry = quadrants.getTelemetry(q);
		in.channelPhase[static_cast<size_t>(q)] = quadrantsemantics::channelPhaseFromTelemetry(telemetry.phase);
		in.channelEffect[static_cast<size_t>(q)] = telemetry.activeShader;
	}
	in.expansion = expansionOf(core_.expansionDirector().getState());
	if (core_.expansionDirector().isActive()) in.expansionQuadrant = core_.expansionDirector().getTargetQuadrant();
	in.expansionT = core_.expansionDirector().getExpansionT();
	in.crosshairPreset = core_.crosshair().getCurrentPreset();
	in.motionEnergy = core_.motionExtraction().getMotionEnergy();
	in.stateElapsedSeconds = stateElapsedSeconds_;
	in.activeSeconds = activeSeconds_;
	in.generation = generation_;
	return in;
}

SceneSemanticData QuadrantProductionScene::buildSemanticData() const {
	return quadrantsemantics::compute(semanticInputs());
}

SceneHudStatus QuadrantProductionScene::hudStatus() const {
	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = kSceneId;
	status.displayName = displayName();
	status.health = (core_.didSetup() && core_.isActive()) ? SceneHealth::Ready : SceneHealth::Loading;

	// Compatibility-only mirror of the canonical video status — same as the
	// other production scenes.
	{
		VideoPlaybackStatus videoStatus = video_.status();
		if (videoStatus.fallbackDisplayTitle) {
			status.mediaName = videoStatus.fallbackDisplayTitle;
		} else if (videoStatus.titleId) {
			status.mediaName = videoStatus.titleId;
		}
	}

	// activeEffects: left empty — no canonical effect-activity owner; the
	// per-quadrant shader names are published as semantic channel metrics.
	int online = 0;
	for (int q = 0; q < 4; ++q) {
		if (core_.quadrants().getTelemetry(q).phase == "ONLINE") online++;
	}
	status.activeItemCount = online;
	status.paused = false;

	if (core_.didSetup() && core_.isActive() && video_.currentTexture() != nullptr) {
		status.semantic = buildSemanticData();
	}

	// Scene & Pattern Navigation: the crosshair's five motion strategies
	// (CrosshairSystem presets DRIFT/SCAN/HUNT/NERVOUS/ORBIT). The crosshair
	// position drives the quadrant split, colour sampling and expansions,
	// and its speed drives VELOCITY re-deals, so each strategy changes the
	// composition's pacing — rendering itself is shared.
	if (core_.didSetup() && core_.isActive()) {
		const quadrantcrosshair::CrosshairSystem& crosshair = core_.crosshair();
		const int index = crosshair.getCurrentPreset();
		std::string id = crosshair.getPresetName(index);
		std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		SceneModeStatus mode;
		mode.modeId = "motion_" + id;
		mode.displayName = crosshair.getPresetName(index);
		mode.index = index;
		mode.count = crosshair.getPresetCount();
		status.modeName = mode.displayName;
		status.mode = mode;
	}
	return status;
}

SceneCapabilities QuadrantProductionScene::capabilities() const {
	SceneCapabilities caps;

	// NextMedia/PreviousMedia delegate to the canonical shared
	// VideoPlaybackService — same pattern as every other production scene.
	SceneCommandDescriptor nextCmd;
	nextCmd.command = SceneCommand::NextMedia;
	nextCmd.label = "Next";
	nextCmd.shortLabel = "NEXT";
	nextCmd.prominent = false;
	caps.commands.push_back(nextCmd);

	SceneCommandDescriptor prevCmd;
	prevCmd.command = SceneCommand::PreviousMedia;
	prevCmd.label = "Previous";
	prevCmd.shortLabel = "PREV";
	prevCmd.prominent = false;
	caps.commands.push_back(prevCmd);

	// NextMode (Scene & Pattern Navigation): next crosshair motion strategy.
	SceneCommandDescriptor modeCmd;
	modeCmd.command = SceneCommand::NextMode;
	modeCmd.label = "Next mode";
	modeCmd.shortLabel = "NEXT MODE";
	modeCmd.prominent = false;
	caps.commands.push_back(modeCmd);

	// Regenerate: not advertised. The standalone's only candidates are the
	// expansion trigger keys (E/F1-F4, commented "for testing") and raw preset
	// / overlay / output-mode cycling — no curated reseed exists. Reset: not
	// advertised (product direction); still honored via executeCommand().
	return caps;
}

bool QuadrantProductionScene::executeCommand(SceneCommand command) {
	if (!didSetup_ || didShutdown_) {
		return false;
	}
	switch (command) {
		case SceneCommand::NextMedia:
			return video_.next();
		case SceneCommand::PreviousMedia:
			return video_.previous();
		case SceneCommand::Reset:
			reset();
			return true;
		case SceneCommand::NextMode:
			if (!core_.isActive()) return false;
			core_.crosshair().nextPreset();
			return true;
		case SceneCommand::Regenerate:
		default:
			return false;
	}
}

void QuadrantProductionScene::reset() {
	if (core_.isActive()) {
		core_.activate();
		observedMediaId_.clear();
		observeCanonicalMedia(true);
	}
	activeSeconds_ = 0.0f;
	lastPrimaryStateId_.clear();
	stateElapsedSeconds_ = 0.0f;
}

void QuadrantProductionScene::shutdown() {
	core_.deactivate();
	didShutdown_ = true;
}
