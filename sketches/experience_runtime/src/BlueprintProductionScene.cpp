#include "BlueprintProductionScene.h"

#include "BESettings.h"
#include "BlueprintSemanticMapping.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)

namespace {

std::string cycleModeSemanticId(CycleMode mode) {
	switch (mode) {
		case CycleMode::GHOST_LAYERS: return "ghost_layers";
		case CycleMode::PERPETUAL: return "perpetual";
	}
	return std::string();
}

} // namespace

BlueprintProductionScene::BlueprintProductionScene(VideoPlaybackService& video)
	: video_(video) {
}

void BlueprintProductionScene::setup(const SceneServices& services) {
	// Same as Blob/Temporal: consumes the runtime's canonical
	// VideoPlaybackService directly (video_), not services' root strings.
	(void)services;

	BlueprintSceneCore::Config config;
	config.canvasW = kNativeWidth;
	config.canvasH = kNativeHeight;
	config.advanceMediaPerCycle = kMediaAdvancesAtCycleBoundary; // runtime suspends auto-advance while live
	{ RT004_SCOPE("Blueprint.setup.core"); core_.setup(&video_, config); }
	didSetup_ = true;
}

void BlueprintProductionScene::activate() {
	// Fresh composition cycle on every activation (incl. reactivation) —
	// fragments, grid occupancy, annotations and erosion history are
	// cleared, so no pre-deactivation imagery survives.
	{ RT004_SCOPE("Blueprint.activate"); core_.activate(); }
	activeSeconds_ = 0.0f;
	generation_++;
}

void BlueprintProductionScene::deactivate() {
	// SceneManager stops calling update()/draw() on an inactive scene; the
	// composition simply stops advancing.
	core_.deactivate();
}

void BlueprintProductionScene::update(float dt) {
	if (!core_.isActive()) {
		return;
	}
	activeSeconds_ += dt;
	{ RT004_SCOPE("Blueprint.core.update"); core_.update(dt); }
}

void BlueprintProductionScene::drawToCurrentTarget() {
	// ExperienceRuntime has already bound + cleared the runtime-owned scene
	// FBO. Artwork only — no debug text, occupancy overlay or dev panel.
	RT004_SCOPE("Blueprint.core.draw");
	core_.draw();
}

glm::ivec2 BlueprintProductionScene::nativeRenderSize() const {
	return glm::ivec2(kNativeWidth, kNativeHeight);
}

std::string BlueprintProductionScene::sceneId() const {
	return kSceneId;
}

std::string BlueprintProductionScene::displayName() const {
	return "Blueprint Emergence";
}

SceneHudStatus BlueprintProductionScene::hudStatus() const {
	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = kSceneId;
	status.displayName = displayName();
	status.health = (core_.didSetup() && core_.isActive()) ? SceneHealth::Ready : SceneHealth::Loading;

	// Compatibility-only mirror of the canonical video status — same as
	// Blob/Temporal.
	{
		VideoPlaybackStatus videoStatus = video_.status();
		if (videoStatus.fallbackDisplayTitle) {
			status.mediaName = videoStatus.fallbackDisplayTitle;
		} else if (videoStatus.titleId) {
			status.mediaName = videoStatus.titleId;
		}
	}

	// activeEffects: left empty — no canonical effect-activity owner, and
	// per-fragment shader names are artwork internals, not curated labels.
	status.activeItemCount = static_cast<int>(core_.composition().getFragments().size());
	status.paused = false;

	// Semantic data only once there is real media to build fragments from.
	if (core_.didSetup() && core_.isActive() && core_.hasVideoFrame()) {
		status.semantic = buildSemanticData();
	}

	// Scene & Pattern Navigation: BEComposition's two CycleModes. Each cycle
	// still draws its mode at random (selectNextMode()); NEXT MODE
	// (BEComposition::requestNextMode()) switches the running cycle's mode
	// until the next natural mode point. Read live, so the status changes
	// exactly when the composition's mode does.
	if (core_.didSetup() && core_.isActive()) {
		const CycleMode current = core_.composition().getCurrentMode();
		SceneModeStatus mode;
		mode.modeId = cycleModeSemanticId(current);
		mode.displayName = mode.modeId;
		mode.index = current == CycleMode::GHOST_LAYERS ? 0 : 1; // CycleMode declaration order
		mode.count = 2;
		status.modeName = mode.displayName;
		status.mode = mode;
	}
	return status;
}

SceneSemanticData BlueprintProductionScene::buildSemanticData() const {
	const BEComposition& composition = core_.composition();
	const BECompositionState state = composition.getState();

	blueprintsemantics::Inputs in;
	in.phase = state.currentPhase;
	in.phaseElapsedSeconds = composition.getPhaseElapsed();
	in.phaseProgress = composition.getPhaseProgress();
	in.cycleModeId = cycleModeSemanticId(composition.getCurrentMode());
	in.fragmentCount = state.fragmentCount;
	in.zoneACount = state.zoneACount;
	in.zoneBCount = state.zoneBCount;
	in.maxFragments = MAX_FRAGMENTS;
	in.activeSeconds = activeSeconds_;
	in.generation = generation_;
	return blueprintsemantics::compute(in);
}

SceneCapabilities BlueprintProductionScene::capabilities() const {
	SceneCapabilities caps;

	// NextMedia/PreviousMedia delegate to the canonical shared
	// VideoPlaybackService — same pattern as Blob/Temporal.
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

	// Regenerate: a new composition cycle with a fresh seed — the exact path
	// CompositionBase::update() itself takes at the end of every cycle
	// (CompositionBase.cpp startCycle()), so it is a bounded, artwork-native
	// variation rather than a raw debug action.
	SceneCommandDescriptor regenCmd;
	regenCmd.command = SceneCommand::Regenerate;
	regenCmd.label = "Reseed";
	regenCmd.shortLabel = "RESEED";
	regenCmd.prominent = false;
	caps.commands.push_back(regenCmd);

	// NextMode: manual GHOST_LAYERS <-> PERPETUAL (separate from Regenerate,
	// which restarts the whole cycle and re-rolls the mode at random).
	SceneCommandDescriptor modeCmd;
	modeCmd.command = SceneCommand::NextMode;
	modeCmd.label = "Next mode";
	modeCmd.shortLabel = "NEXT MODE";
	modeCmd.prominent = false;
	caps.commands.push_back(modeCmd);

	// Reset: not advertised (product direction forbids a visible reset
	// control); still honored via executeCommand(), same split as Blob.
	return caps;
}

bool BlueprintProductionScene::executeCommand(SceneCommand command) {
	if (!didSetup_ || didShutdown_) {
		return false;
	}
	switch (command) {
		case SceneCommand::NextMedia:
			return video_.next();
		case SceneCommand::PreviousMedia:
			return video_.previous();
		case SceneCommand::Regenerate:
			if (!core_.isActive()) return false;
			core_.restartCycle();
			return true;
		case SceneCommand::NextMode:
			// GHOST_LAYERS <-> PERPETUAL; refused during DISSOLVE (the
			// composition's own mode change). Never advances shared media.
			if (!core_.isActive()) return false;
			return core_.requestNextMode();
		case SceneCommand::Reset:
			reset();
			return true;
		default:
			return false;
	}
}

void BlueprintProductionScene::reset() {
	if (core_.isActive()) core_.restartCycle();
	activeSeconds_ = 0.0f;
}

void BlueprintProductionScene::shutdown() {
	core_.deactivate();
	didShutdown_ = true;
}
