#include "TemporalProductionScene.h"
#include "TemporalSemanticMapping.h"
#include "TFSettings.h"

namespace {
	// Lowers TFPatternType to the identifier this migration's semantic
	// mapping reports for scene.temporal.metric.pattern (Identifier/
	// Literal — see TemporalSemanticMapping.cpp's own field table).
	// Deliberately its own small mapping, not tfPatternTypePresetKey()
	// (TFPatternType.h) — that function's format is preset-file-key
	// specific ("blobgrid", no underscore) and unrelated to this HUD
	// identifier; this one instead matches the snake_case convention the
	// metricId itself uses and the exact example value ("particle_field")
	// shared/src/hud-compositor/fake/FakeTemporalScenario.cpp already uses
	// for this same slot.
	std::string patternSemanticId(TFPatternType type) {
		switch (type) {
			case TFPatternType::BSP: return "bsp";
			case TFPatternType::BLOB_GRID: return "blob_grid";
			case TFPatternType::BANDS: return "bands";
			case TFPatternType::COLUMN_GRID: return "column_grid";
			case TFPatternType::TELESCOPING_FRAMES: return "telescoping_frames";
			case TFPatternType::PARTICLE_FIELD: return "particle_field";
			case TFPatternType::ECOLOGICAL_SUCCESSION: return "ecological_succession";
			case TFPatternType::NETWORK_GROWTH: return "network_growth";
			case TFPatternType::TEMPORAL_TIDES: return "temporal_tides";
		}
		return "unknown";
	}
} // namespace

TemporalProductionScene::TemporalProductionScene(VideoPlaybackService& video)
	: video_(video) {
}

void TemporalProductionScene::syncVideoAdapter() {
	// Same real, only-source contract as the standalone sketch's own
	// syncTemporalVideoAdapter() (ofApp.cpp) — video_.status()/
	// currentAbsolutePath() are the ONLY source of the media identity
	// handed to the adapter; no local scan/shuffle/selection happens here
	// (DEC-013/DEC-014). See TemporalProductionScene::update()'s own
	// comment for why this may observe the canonical selection up to one
	// frame later than RuntimeServices::update() itself advances it —
	// harmless (converges the next frame), not a second selection
	// authority.
	VideoPlaybackStatus status = video_.status();
	if (!status.mediaId.has_value()) {
		return; // nothing selected yet
	}
	std::optional<std::string> absolutePath = video_.currentAbsolutePath();
	if (!absolutePath.has_value()) {
		return; // should be impossible whenever mediaId is set, but never assume
	}
	temporalVideoAdapter_.synchronizeSelectedMedia(*status.mediaId, *absolutePath);
}

void TemporalProductionScene::setup(const SceneServices& services) {
	// Temporal does not currently read sceneAssetRoot/sharedMediaRoot/
	// sharedEffectAssetRoot — same as BlobProductionScene::setup() (see
	// that method's own comment) — it consumes the runtime's single
	// canonical VideoPlaybackService directly (video_, injected via the
	// constructor).
	(void)services;

	// Real production defaults — exactly the values TFParameterPanel's own
	// ofxGui defaults seed at standalone startup (TFSettings.h constants),
	// not TimeOffsetVideoBuffer::Settings' generic in-class defaults. See
	// TFParameterPanel.cpp's quantizeBandsParam.set()/
	// maxHistorySecondsParam.set() calls for the source of truth this
	// mirrors.
	TimeOffsetVideoBuffer::Settings bufferSettings;
	bufferSettings.numQuantizeBands = TIME_OFFSET_QUANTIZE_BANDS;
	bufferSettings.maxHistorySeconds = TIME_OFFSET_MAX_HISTORY_SECONDS;
	bufferSettings.minPlaytimeSeconds = MEDIA_MIN_PLAYTIME_SECONDS;
	bufferSettings.minLoopCount = MEDIA_MIN_LOOP_COUNT;
	temporalVideoAdapter_.setup(bufferSettings);

	// Load the startup selection before the first update/draw — same
	// ordering as the standalone sketch's own setup().
	syncVideoAdapter();

	core_.setup(&temporalVideoAdapter_.buffer(), kNativeWidth, kNativeHeight);
	didSetup_ = true;
}

void TemporalProductionScene::activate() {
	if (hasActivatedBefore_) {
		// Reactivation, not the first activation — force a fresh
		// TimeOffsetPlaybackAdapter reload/history-refill against whatever
		// media is currently canonically selected, even if that identity
		// happens to be unchanged from what was loaded before this scene
		// was deactivated (see TimeOffsetPlaybackAdapter::
		// invalidateForReactivation()'s own comment for why the adapter's
		// normal unchanged-mediaId no-op would otherwise leave stale
		// pre-deactivation history/playhead state in place).
		temporalVideoAdapter_.invalidateForReactivation();
	}
	syncVideoAdapter();
	core_.activate();
	activeSeconds_ = 0.0f;
	generation_++;
	hasActivatedBefore_ = true;
}

void TemporalProductionScene::deactivate() {
	core_.deactivate();
	// No explicit decoder pause call needed — TemporalSceneCore::update()
	// (and therefore this scene's own update() below) simply stops being
	// called on the real per-frame path while inactive (SceneManager only
	// calls updateActiveScene() on the active scene), so
	// TimeOffsetPlaybackAdapter::update() (which is what pumps
	// TimeOffsetVideoBuffer's ofVideoPlayer::update() — see that method's
	// own header comment) never runs and the dedicated decoder/history
	// genuinely stop advancing, not just visually.
}

void TemporalProductionScene::update(float dt) {
	if (!core_.isActive()) {
		return;
	}
	activeSeconds_ += dt;

	// Runtime canonical video update already happened this frame in
	// RuntimeServices::update() (ExperienceRuntime.cpp step 7) for EVERY
	// scene, Temporal included, before SceneManager::updateActiveScene()
	// is called for the NEXT frame — see ExperienceRuntime::update()'s own
	// numbered-step comment. Selected-media synchronization + the
	// dedicated decoder/history update happen here, exactly once, as the
	// sole owner of the Temporal-specialized side of the pipeline.
	syncVideoAdapter();
	temporalVideoAdapter_.update(dt);

	// Composition/pattern/background/effect update — TFEffectPicker's
	// state for this frame is finalized inside this call (via
	// TFBackgroundLayer::update()), before ExperienceRuntime's later
	// captureEffectActivityStatus() step reads it through
	// currentEffectActivitySnapshot() below.
	core_.update(dt);
}

void TemporalProductionScene::drawToCurrentTarget() {
	// ExperienceRuntime has already bound + cleared the runtime-owned
	// scene FBO before calling this — draws the real artwork only
	// (background + composition + ambient textures, via
	// TemporalSceneCore::draw()). No GUI, no debug overlay, no local HUD
	// (TFHudLayer/hud_overlay are never constructed by this scene at all
	// — see class header comment).
	core_.draw();
}

glm::ivec2 TemporalProductionScene::nativeRenderSize() const {
	return glm::ivec2(kNativeWidth, kNativeHeight);
}

std::string TemporalProductionScene::sceneId() const {
	return kSceneId;
}

std::string TemporalProductionScene::displayName() const {
	return "Temporal Fields";
}

std::optional<videoeffects::EffectActivityStatus> TemporalProductionScene::currentEffectActivitySnapshot() const {
	if (!didSetup_ || didShutdown_) {
		return std::nullopt;
	}
	return core_.effectActivityStatus();
}

SceneHudStatus TemporalProductionScene::hudStatus() const {
	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = kSceneId;
	status.displayName = displayName();

	// Scene-owned health only — same rule as BlobProductionScene::
	// hudStatus() (see that method's own comment): HudFrameData.video
	// remains the authoritative channel for media health, and
	// HudFrameData.effects (via currentEffectActivitySnapshot() above) for
	// effect health; this reflects only whether the scene itself is set
	// up and active.
	status.health = (core_.didSetup() && core_.isActive()) ? SceneHealth::Ready : SceneHealth::Loading;

	// Compatibility-only mirror of the canonical video status — same as
	// BlobProductionScene.
	{
		VideoPlaybackStatus videoStatus = video_.status();
		if (videoStatus.fallbackDisplayTitle) {
			status.mediaName = videoStatus.fallbackDisplayTitle;
		} else if (videoStatus.titleId) {
			status.mediaName = videoStatus.titleId;
		}
	}

	// modeName/activeEffects: left absent/empty. Unlike Blob (which has no
	// canonical effect-activity owner at all and so mirrors its two raw
	// selected effect names into activeEffects as the least-bad
	// compatibility signal — see BlobProductionScene's own comment),
	// Temporal DOES have a real canonical producer (TFEffectPicker, via
	// currentEffectActivitySnapshot() -> HudFrameData.effects) — this
	// session's own instruction (§9.5) is explicit that
	// SceneHudStatus::activeEffects "is compatibility-only... Do not
	// populate it as a second canonical source merely to make old HUD
	// code look full," so it is deliberately left at its empty default
	// here rather than re-deriving a second, possibly-drifting mirror of
	// the same TFEffectPicker state.

	status.paused = false;

	// semantic: only reported once there is real media to derive it from —
	// std::nullopt (not a zeroed struct) whenever setup/active/media-ready
	// conditions aren't all met, mirroring BlobProductionScene's own
	// hasVideoFrame() gate.
	if (core_.didSetup() && core_.isActive() && core_.hasMedia()) {
		status.semantic = buildSemanticData();
	}

	return status;
}

SceneSemanticData TemporalProductionScene::buildSemanticData() const {
	temporalsemantics::Inputs in;
	in.isTransitioning = core_.compositionPhase() == TFComposition::CyclePhase::PATTERN_TRANSITION;
	in.activePatternId = patternSemanticId(core_.activePatternType());
	in.phaseElapsedSeconds = core_.compositionPhaseElapsed();
	in.historyFrameCount = core_.historyFrameCount();
	in.historyCapacityFrames = core_.historyCapacityFrames();
	in.playheadCount = core_.numPlayheads();
	in.activeSeconds = activeSeconds_;
	in.generation = generation_;
	return temporalsemantics::compute(in);
}

SceneCapabilities TemporalProductionScene::capabilities() const {
	SceneCapabilities caps;

	// NextMedia/PreviousMedia delegate to the canonical shared
	// VideoPlaybackService (executeCommand() below) — same unconditional-
	// advertisement pattern as BlobProductionScene/FakeScene;
	// VideoPlaybackService::next()/previous() already reject cleanly when
	// navigation isn't currently possible.
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

	// Regenerate: not advertised — this migration's own inspection (§14.2)
	// found no existing curated, bounded, production-safe variation action
	// for Temporal. TFComposition::forceNextPattern() is a raw pattern-
	// forcing debug key with no curation/bounding, which this session's
	// own instruction explicitly disqualifies ("a raw 'force next
	// pattern'... does not automatically qualify"). See completion report,
	// "Deviations from prompt."
	// Reset: not advertised — product direction forbids a visible
	// reset/restart control; still reachable via SceneCommand::Reset
	// directly (executeCommand() below still honors it), for parity with
	// BlobProductionScene's own identical split.
	return caps;
}

bool TemporalProductionScene::executeCommand(SceneCommand command) {
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
		case SceneCommand::Regenerate:
		default:
			return false;
	}
}

void TemporalProductionScene::reset() {
	core_.reset();
	activeSeconds_ = 0.0f;
}

void TemporalProductionScene::shutdown() {
	core_.shutdown();
	didShutdown_ = true;
}
