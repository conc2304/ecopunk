#include "FragmentProductionScene.h"

#include "FragmentSemanticMapping.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)

#include <cmath>

namespace {

std::string contentModeSemanticId(fragmenttrail::FTContentMode mode) {
	switch (mode) {
		case fragmenttrail::FTContentMode::TIME_SLICE: return "time_slice";
		case fragmenttrail::FTContentMode::EFFECT_VARIED: return "effect_varied";
		case fragmenttrail::FTContentMode::EFFECT_PARAM_VARIANT: return "effect_param_variant";
	}
	return std::string();
}

// Spawn-rate smoothing time constant (seconds) for spawn_cadence.
constexpr float kSpawnRateTau = 1.5f;

} // namespace

FragmentProductionScene::FragmentProductionScene(VideoPlaybackService& video)
	: video_(video) {
}

void FragmentProductionScene::syncVideoAdapter() {
	// Same only-source contract as TemporalProductionScene::syncVideoAdapter():
	// the canonical service's status/currentAbsolutePath() are the ONLY
	// source of the media identity handed to the follower.
	VideoPlaybackStatus status = video_.status();
	if (!status.mediaId.has_value()) {
		return; // nothing selected yet
	}
	std::optional<std::string> absolutePath = video_.currentAbsolutePath();
	if (!absolutePath.has_value()) {
		return;
	}
	RT004_SCOPE("Fragment.synchronizeSelectedMedia");
	videoAdapter_.synchronizeSelectedMedia(*status.mediaId, *absolutePath, status.positionSeconds.value_or(0.0f));
}

void FragmentProductionScene::setup(const SceneServices& services) {
	// Same as Blob/Temporal/Blueprint: consumes the runtime's canonical
	// VideoPlaybackService directly (video_), not services' root strings.
	(void)services;

	// The standalone sketch's own buffer shape: TimeOffsetVideoBuffer::
	// Settings defaults (identical in the fork and the shared class) with
	// its playhead pool size.
	bufferSettings_ = TimeOffsetVideoBuffer::Settings{};
	bufferSettings_.numPlayheads = kNumPlayheads;
	{ RT004_SCOPE("Fragment.setup.adapterConfigure"); videoAdapter_.setup(bufferSettings_); }
	syncVideoAdapter();

	{ RT004_SCOPE("Fragment.setup.shaders"); shaderLib_.setup(); }

	fragmenttrail::FragmentSceneCore::Config config;
	config.canvasW = kNativeWidth;
	config.canvasH = kNativeHeight;
	config.numPlayheads = kNumPlayheads;
	// v0.3: the HUD shows this canvas cover-fit inside media_viewport
	// (HUD-006 frozen geometry: 0.60 x 0.70 of a 16:9 window = 768x504), so
	// the canvas's left/right edges (~91 px each) are cropped away. Keep the
	// overlay's corner radar inside the part the viewer actually sees.
	// No runtime service exposes the viewport's visible rect to scenes yet
	// (reported as a contract gap), hence derived from the frozen aspect.
	{
		constexpr float kViewportAspect = (0.60f * 16.0f) / (0.70f * 9.0f);
		const float visibleW = std::min(static_cast<float>(kNativeWidth), kNativeHeight * kViewportAspect);
		const float visibleH = std::min(static_cast<float>(kNativeHeight), kNativeWidth / kViewportAspect);
		config.safeArea.set((kNativeWidth - visibleW) * 0.5f, (kNativeHeight - visibleH) * 0.5f, visibleW, visibleH);
	}
	// No still images: the standalone's bin/data/backgrounds/ is empty, so its
	// backdrop never alternates with an image. The runtime's backgrounds/ is
	// Temporal's set; pointing Fragment at it would change the artwork.
	config.backgroundsFolder = "fragment-trail/backgrounds";
	config.bgVertShaderPath = "shaders/effects/vert.glsl"; // byte-identical to the standalone's shaders/vert.glsl
	config.bgShaderPath = "shaders/fragment-trail/bg_dim.glsl"; // scripts/sync-fragment-runtime-assets.py
	{ RT004_SCOPE("Fragment.setup.core"); core_.setup(&videoAdapter_.buffer(), &shaderLib_, config); }
	didSetup_ = true;
}

void FragmentProductionScene::activate() {
	RT004_SCOPE("Fragment.activate");
	if (hasActivatedBefore_) {
		// Force a fresh follower reload/history refill against the current
		// canonical selection, even if its identity is unchanged — same
		// reason as TemporalProductionScene::activate().
		videoAdapter_.invalidateForReactivation();
	}
	syncVideoAdapter();
	// Fresh trail on every activation (incl. reactivation): no fragments,
	// playheads released, crosshair/overlay state rebuilt.
	core_.activate();
	activeSeconds_ = 0.0f;
	lastSpawnCount_ = core_.pool().getSpawnCount();
	spawnsPerSecond_ = 0.0f;
	generation_++;
	hasActivatedBefore_ = true;
}

void FragmentProductionScene::deactivate() {
	core_.deactivate();
	// Release the follower while inactive — frame history (up to
	// maxHistorySeconds of 640x360 frames, ~166 MB at the defaults), playhead
	// textures and the dedicated decoder — through the same RT-004 path
	// Temporal uses (TimeOffsetPlaybackAdapter::releaseForDeactivation()).
	// Reactivation always reloads and refills it anyway (activate() above).
	videoAdapter_.releaseForDeactivation();
}

void FragmentProductionScene::update(float dt) {
	if (!core_.isActive()) {
		return;
	}
	activeSeconds_ += dt;

	// Canonical video already advanced this frame in RuntimeServices::update();
	// the follower observes it here (at most one frame later — converges the
	// next frame, not a second selection authority).
	syncVideoAdapter();
	{ RT004_SCOPE("Fragment.adapter.update"); videoAdapter_.update(dt); }
	{ RT004_SCOPE("Fragment.core.update"); core_.update(dt); }

	if (dt > 0.0f) {
		const int spawned = core_.pool().getSpawnCount();
		const float instantaneous = static_cast<float>(spawned - lastSpawnCount_) / dt;
		lastSpawnCount_ = spawned;
		const float k = 1.0f - std::exp(-dt / kSpawnRateTau);
		spawnsPerSecond_ += (instantaneous - spawnsPerSecond_) * k;
	}
}

void FragmentProductionScene::drawToCurrentTarget() {
	// ExperienceRuntime has already bound + cleared the runtime-owned scene
	// FBO. Artwork only — no parameter/dial panels, no debug text.
	RT004_SCOPE("Fragment.core.draw");
	core_.draw();
}

glm::ivec2 FragmentProductionScene::nativeRenderSize() const {
	return glm::ivec2(kNativeWidth, kNativeHeight);
}

std::string FragmentProductionScene::sceneId() const {
	return kSceneId;
}

std::string FragmentProductionScene::displayName() const {
	return "Fragment Trail";
}

SceneHudStatus FragmentProductionScene::hudStatus() const {
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

	// activeEffects: left empty — no canonical effect-activity owner, and
	// per-fragment shader names are artwork internals, not curated labels.
	status.activeItemCount = core_.pool().getFragmentCount();
	status.paused = false;

	// Semantic data only once the follower has real media.
	if (core_.didSetup() && core_.isActive() && videoAdapter_.buffer().hasMedia()) {
		status.semantic = buildSemanticData();
	}

	// Scene & Pattern Navigation: FTContentMode's three content modes, in
	// enum order (the controller's own rotation order); same IDs as
	// scene.fragment.metric.content_mode.
	if (core_.didSetup() && core_.isActive()) {
		const fragmenttrail::FTModeController& modes = core_.modeController();
		SceneModeStatus mode;
		mode.modeId = contentModeSemanticId(modes.getCurrentMode());
		mode.displayName = mode.modeId;
		mode.index = modes.currentModeIndex();
		mode.count = fragmenttrail::FTModeController::kModeCount;
		status.modeName = mode.displayName;
		status.mode = mode;
	}
	return status;
}

SceneSemanticData FragmentProductionScene::buildSemanticData() const {
	const fragmenttrail::FTParameterPanel& params = core_.parameters();
	fragmentsemantics::Inputs in;
	in.crosshairPreset = core_.crosshair().getCurrentPreset();
	in.presetElapsedSeconds = core_.presetElapsedSeconds();
	in.fragmentCount = core_.pool().getFragmentCount();
	in.maxFragments = params.getMaxFragments();
	in.crosshairEnergy01 = core_.crosshairEnergy01();
	in.trailLifetimeSeconds = params.getSustainSeconds() + params.getDecaySeconds();
	in.spawnsPerSecond = spawnsPerSecond_;
	in.contentModeId = contentModeSemanticId(core_.modeController().getCurrentMode());
	in.activeSeconds = activeSeconds_;
	in.generation = generation_;
	return fragmentsemantics::compute(in);
}

SceneCapabilities FragmentProductionScene::capabilities() const {
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

	// NextMode (Scene & Pattern Navigation): steps FTModeController's
	// content mode — the modes' own rendering is unchanged.
	SceneCommandDescriptor modeCmd;
	modeCmd.command = SceneCommand::NextMode;
	modeCmd.label = "Next mode";
	modeCmd.shortLabel = "NEXT MODE";
	modeCmd.prominent = false;
	caps.commands.push_back(modeCmd);

	// Regenerate: not advertised — Fragment Trail has no curated, bounded
	// variation action (its standalone keys are raw mode/preset debug
	// switches). Reset: not advertised (product direction); still honored
	// via executeCommand(), same split as the other scenes.
	return caps;
}

bool FragmentProductionScene::executeCommand(SceneCommand command) {
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
			core_.modeController().nextMode();
			return true;
		case SceneCommand::Regenerate:
		default:
			return false;
	}
}

void FragmentProductionScene::reset() {
	if (core_.isActive()) core_.activate(); // fresh trail, same as an activation
	activeSeconds_ = 0.0f;
	lastSpawnCount_ = core_.pool().getSpawnCount();
	spawnsPerSecond_ = 0.0f;
}

void FragmentProductionScene::shutdown() {
	core_.deactivate();
	didShutdown_ = true;
}
