#include "BlobProductionScene.h"
#include "BlobSemanticMapping.h"

BlobProductionScene::BlobProductionScene(VideoPlaybackService & video)
	: video_(video) {
}

void BlobProductionScene::setup(const SceneServices & services) {
	// Blob does not currently read sceneAssetRoot/sharedMediaRoot/
	// sharedEffectAssetRoot — it consumes the runtime's single canonical
	// VideoPlaybackService directly (video_, injected via the constructor)
	// rather than re-deriving a media root from SceneServices itself,
	// matching VideoPlaybackService.h's own rule ("a scene adapter never
	// touches MediaCatalog/VideoSelectionPolicy/IVideoDecoder directly —
	// only this class's public surface").
	(void)services;
	core_.setup(video_);
	didSetup_ = true;
}

void BlobProductionScene::activate() {
	core_.activate(); // also clears transient detector/tracker/fragment state — see BlobSceneCore::activate()
	activeSeconds_ = 0.0f;
	generation_++;
}

void BlobProductionScene::deactivate() {
	core_.deactivate();
}

void BlobProductionScene::update(float dt) {
	if (!core_.isActive()) {
		return;
	}
	activeSeconds_ += dt;
	ofRectangle destRect(0, 0, static_cast<float>(kNativeWidth), static_cast<float>(kNativeHeight));
	core_.update(dt, destRect);
}

void BlobProductionScene::drawToCurrentTarget() {
	// ExperienceRuntime has already bound + cleared the runtime-owned
	// scene FBO before calling this (see ExperienceRuntime::draw(), step
	// 8) — this draws background + region fragments only, straight into
	// whatever's currently bound. No GUI, no debug overlay, no local HUD
	// (see class header comment / docs/hud-double-hud-prevention-and-
	// migration-matrix.md's Blob row).
	ofRectangle destRect(0, 0, static_cast<float>(kNativeWidth), static_cast<float>(kNativeHeight));
	core_.draw(destRect);
}

glm::ivec2 BlobProductionScene::nativeRenderSize() const {
	return glm::ivec2(kNativeWidth, kNativeHeight);
}

std::string BlobProductionScene::sceneId() const {
	return kSceneId;
}

std::string BlobProductionScene::displayName() const {
	return "Blob Region Prototype";
}

SceneHudStatus BlobProductionScene::hudStatus() const {
	SceneHudStatus status;
	status.schemaVersion = 1;
	status.sceneId = kSceneId;
	status.displayName = displayName();

	// Scene-owned health only — deliberately NOT mirroring
	// VideoPlaybackStatus::health here. HudFrameData.video is the
	// authoritative channel for media health; duplicating it into
	// scene-owned health would violate the "do not duplicate video/effect
	// health into scene health" rule (see migration report's semantic-
	// mapping notes). Blob's own health reflects only whether the scene
	// itself is set up and active.
	status.health = (core_.didSetup() && core_.isActive()) ? SceneHealth::Ready : SceneHealth::Loading;

	// Compatibility-only mirror of the canonical video status — HudFrameData.
	// video remains authoritative; this just lets any consumer still
	// reading SceneHudStatus directly see the same active-media title.
	{
		VideoPlaybackStatus videoStatus = video_.status();
		if (videoStatus.fallbackDisplayTitle) {
			status.mediaName = videoStatus.fallbackDisplayTitle;
		} else if (videoStatus.titleId) {
			status.mediaName = videoStatus.titleId;
		}
	}

	// modeName: Blob has no structural mode concept (no scene mode/state
	// machine — see docs/blob-region-architecture.md) — left absent
	// rather than exposing a raw GUI draw-mode/background-mode enum.

	// activeEffects: compatibility-only, per DEC-015 (canonical effect
	// activity, when it exists, is authoritative; this field is not).
	// No canonical EffectActivityStatus owner exists for Blob yet (see
	// class header comment), so this mirrors the two real, currently-
	// selected effect names rather than being left to imply "no effects
	// running" when effects genuinely are.
	if (core_.regionParams().fragmentsEnabled) {
		status.activeEffects.push_back("region: " + core_.regionParams().effectName);
	}
	if (core_.backgroundConfig().mode == 2) {
		status.activeEffects.push_back("background: " + core_.backgroundConfig().effectName);
	}

	status.activeItemCount = core_.activeFragmentCount();
	status.paused = false;

	// semantic: only reported once there is a real video frame to derive
	// it from — std::nullopt (not a zeroed struct) whenever setup/active/
	// video-ready conditions aren't all met, so "genuinely zero regions"
	// and "no data yet" stay distinguishable (see SceneSemanticTypes.h /
	// migration report).
	if (core_.didSetup() && core_.isActive() && core_.hasVideoFrame()) {
		status.semantic = buildSemanticData();
	}

	return status;
}

SceneSemanticData BlobProductionScene::buildSemanticData() const {
	// Blob First Production Acceptance narrow patch: the actual derivation
	// logic (bucketing/clamping/metric construction) now lives in
	// blobsemantics::compute() — an OF-free pure function, unchanged in
	// behavior from what used to be inline here, extracted specifically so
	// it can be exercised deterministically by
	// test/blob_semantic_mapping_tests.cpp with controlled inputs instead
	// of only via live video. This method's only remaining job is
	// gathering real, already-computed BlobSceneCore/timing state into an
	// Inputs value.
	blobsemantics::Inputs in;
	in.regionCount = core_.trackCount();
	in.fragmentCount = core_.activeFragmentCount();
	in.maxActiveFragments = core_.regionParams().maxActiveFragments;
	in.occupiedAreaFraction = core_.occupiedAreaFraction();
	in.activeSeconds = activeSeconds_;
	in.generation = generation_;
	return blobsemantics::compute(in);
}

SceneCapabilities BlobProductionScene::capabilities() const {
	SceneCapabilities caps;

	// NextMedia/PreviousMedia delegate to the canonical shared
	// VideoPlaybackService (executeCommand() below) — advertised
	// unconditionally, matching FakeScene's own static-descriptor
	// pattern; VideoPlaybackService::next()/previous() already reject
	// cleanly (return false, no state change) when navigation isn't
	// currently possible, so executeCommand()'s return value is the
	// truthful per-call signal, not this static list.
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

	// Regenerate: not advertised — no curated/proven-safe visual-variant
	// table exists yet (see migration report "Explicitly out of scope").
	// Reset: not advertised — product direction forbids a visible
	// reset/restart control; still reachable via SceneCommand::Reset
	// directly (executeCommand() below still honors it) for parity with
	// IEcopunkScene::reset()/FakeScene's own direct-vs-command split.
	return caps;
}

bool BlobProductionScene::executeCommand(SceneCommand command) {
	// Blob First Production Acceptance narrow patch: reject every command
	// (including the hidden, non-advertised Reset) before setup() has run
	// or after shutdown() has already happened, rather than silently
	// mutating core_ state in an invalid lifecycle position — see
	// didSetup_/didShutdown_'s own comment.
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

void BlobProductionScene::reset() {
	core_.reset();
	activeSeconds_ = 0.0f;
}

void BlobProductionScene::shutdown() {
	core_.shutdown();
	didShutdown_ = true;
}
