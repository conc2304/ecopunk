#include "BlobProductionScene.h"
#include "Rt004StallProbe.h" // RT-004 diagnostic probe (env-gated, no behavior change)
#include "BlobSemanticMapping.h"
#include "BlobDetectionProfileApply.h"
#include "ofAppRunner.h"
#include "ofLog.h"
#include "ofUtils.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstdlib>

namespace {
// Scene & Pattern Navigation: Blob's three visual modes are its existing
// BlobSceneCore background draw modes — no new renderer. Debug frame,
// blob IDs and miss timing stay independent display toggles, not modes.
struct BlobBackgroundMode {
	const char * id;
	const char * name;
	int backgroundDrawMode; // BlobSceneCore::BackgroundConfig::mode
};
constexpr BlobBackgroundMode kBackgroundModes[] = {
	{"natural_background", "Natural Background", 1},         // unshaded cover-fit video
	{"desaturated_background", "Desaturated Background", 2}, // background effect (production model)
	{"no_background", "No Background", 0},                   // canvas left cleared; regions only
};
constexpr int kBackgroundModeCount = static_cast<int>(sizeof(kBackgroundModes) / sizeof(kBackgroundModes[0]));
} // namespace

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

	// Per-video detection presets: READ ONLY here — only developer tooling
	// (the standalone blob-region-prototype) ever writes the file. A missing
	// or invalid file just means every media item uses detector defaults.
	{
		std::string path = blobdetectionprofile::profilesPath();
		BlobDetectionProfileStore::LoadStatus st = detectionProfiles_.load(path);
		ofLogNotice("BlobDetectionProfile") << "profiles: "
			<< (st == BlobDetectionProfileStore::LoadStatus::Loaded ? "loaded " + ofToString(detectionProfiles_.size()) + " from "
				: st == BlobDetectionProfileStore::LoadStatus::Missing ? std::string("none at ") : std::string("REJECTED "))
			<< path;
		for (const std::string & m : detectionProfiles_.messages()) {
			ofLogNotice("BlobDetectionProfile") << m;
		}
	}

	// Per-fragment treatments come from the production effect budget
	// (BlobEffectBudget.h): chosen once at fragment creation, cost held
	// until the fragment is gone. The capacity bench, when enabled,
	// substitutes its own assignment except in its "budget" mode.
	if (const char * weights = std::getenv("EXPERIENCE_RUNTIME_BLOB_BUDGET_WEIGHTS")) {
		// Art-direction tuning only; costs/eligibility are not overridable.
		std::vector<std::string> ignored;
		budget_.setTable(blobbudget::withWeights(blobbudget::defaultTable(), weights, &ignored));
		ofLogNotice("BlobEffectBudget") << "weights override: " << weights;
		for (const std::string & e : ignored) {
			ofLogWarning("BlobEffectBudget") << "weights override entry ignored (not in the budget table): " << e;
		}
	}
	core_.regionController().effectAssigner = [this](int id) {
		if (capacityBench_ && capacityBench_->plan().mode != blobcapacity::AssignmentMode::Budget) {
			return capacityBench_->plan().effectForId(id);
		}
		return budget_.assign(id);
	};
	core_.regionController().effectReleaser = [this](int id) { budget_.release(id); };

	// Blob production visual model: desaturated full-frame world, tracked
	// regions reveal original color or a budgeted shader treatment (see
	// BlobEffectPolicy.h / BlobEffectBudget.h). Background mode 2 runs the full frame through
	// BlobSceneCore's existing background effect renderer — same cover-fit
	// crop as mode 1, so geometry is unchanged.
	BlobSceneCore::BackgroundConfig & background = core_.backgroundConfig();
	background.mode = 2;
	background.effectName = blobeffects::backgroundEffectId();
	background.effectAmount = 1.0f;
	if (std::getenv("EXPERIENCE_RUNTIME_BLOB_BACKGROUND_NORMAL") != nullptr) {
		// Profiling A/B only: the previous unshaded background.
		background.mode = 1;
		backgroundModeIndex_ = 0; // = Natural Background, so mode status stays truthful
		ofLogNotice("BlobRenderProfile") << "EXPERIENCE_RUNTIME_BLOB_BACKGROUND_NORMAL set: background drawn unshaded (pre-desaturated-world model)";
	}

	// Blob Experiment 2 (opt-in only): per-track-id treatment assignment
	// replaces the media-change policy while the capacity bench runs.
	if (BlobEffectCapacityBench::isRequested()) {
		capacityBench_ = std::make_unique<BlobEffectCapacityBench>();
		capacityBench_->setup();
		mediaPinPending_ = !capacityBench_->wantedMedia().empty();
	}

	renderProfileEnabled_ = std::getenv("EXPERIENCE_RUNTIME_BLOB_RENDER_PROFILE") != nullptr;
	if (std::getenv("EXPERIENCE_RUNTIME_BLOB_RENDER_LEGACY") != nullptr) {
		core_.regionController().renderer().setDirectOutputEnabled(false);
		ofLogNotice("BlobRenderProfile") << "EXPERIENCE_RUNTIME_BLOB_RENDER_LEGACY set: fragment renderer forced to 3-stage path";
	}
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

	// Curated fragment effect, one per canonical media item (see
	// BlobEffectPolicy.h). Only new fragments pick up a changed effect —
	// VideoRegionController captures effectName at fragment creation — so
	// fragments already on screen keep theirs until they dissolve.
	// New media -> new palette generation: new fragments draw fresh
	// treatments; fragments already on screen keep theirs until they
	// dissolve (VideoRegionController captures the treatment at creation).
	std::optional<std::string> mediaId = video_.status().mediaId;

	// Detection preset for this media (once per media change, including the
	// first media seen); no profile restores the canonical defaults so a
	// previous media's preset never leaks.
	if (auto applied = detectionProfileTracker_.observe(mediaId, detectionProfiles_, blobdetectionprofile::canonicalDefaults())) {
		blobdetectionprofile::applyTo(core_.detectionConfig(), applied->effective);
		std::istringstream lines(describeProfile(applied->mediaId, applied->source, applied->effective));
		std::string line;
		while (std::getline(lines, line)) {
			ofLogNotice("BlobDetectionProfile") << line;
		}
	}

	if (mediaGeneration_.observeMedia(mediaId)) {
		budget_.setGeneration(mediaGeneration_.generation());
		ofLogNotice("BlobProductionScene") << "palette generation -> " << mediaGeneration_.generation();
	}
	if (capacityBench_) {
		updateCapacityBench();
	}

	ofRectangle destRect(0, 0, static_cast<float>(kNativeWidth), static_cast<float>(kNativeHeight));
	RT004_SCOPE("Blob.core.update");
	lastUpdateTimings_ = core_.update(dt, destRect);
}

void BlobProductionScene::drawToCurrentTarget() {
	// ExperienceRuntime has already bound + cleared the runtime-owned
	// scene FBO before calling this (see ExperienceRuntime::draw(), step
	// 8) — this draws background + region fragments only, straight into
	// whatever's currently bound. No GUI, no debug overlay, no local HUD
	// (see class header comment / docs/hud-double-hud-prevention-and-
	// migration-matrix.md's Blob row).
	ofRectangle destRect(0, 0, static_cast<float>(kNativeWidth), static_cast<float>(kNativeHeight));
	RT004_SCOPE("Blob.core.draw");
	if (!renderProfileEnabled_ && !capacityBench_) {
		core_.draw(destRect);
		return;
	}
	core_.regionController().renderer().resetStageTimings();
	core_.backgroundEffectRenderer().resetStageTimings();
	uint64_t t0 = ofGetElapsedTimeMicros();
	core_.draw(destRect);
	float drawMs = static_cast<float>(ofGetElapsedTimeMicros() - t0) / 1000.0f;
	if (renderProfileEnabled_) {
		recordRenderProfileFrame(drawMs);
	}
	if (capacityBench_) {
		BlobEffectCapacityBench::FrameSample s;
		s.frameSeconds = static_cast<float>(ofGetLastFrameTime());
		s.detectorMs = lastUpdateTimings_.detectorMs;
		s.trackerMs = lastUpdateTimings_.trackerMs;
		s.regionUpdateMs = lastUpdateTimings_.regionUpdateMs;
		s.sceneDrawMs = drawMs;
		s.fragment = core_.regionController().renderer().getStageTimings();
		s.background = core_.backgroundEffectRenderer().getStageTimings();
		core_.regionController().collectActiveEffectNames(capacityEffectNames_);
		s.fragmentEffects = &capacityEffectNames_;
		s.mediaId = video_.status().mediaId.value_or("");
		capacityBench_->recordFrame(s);
	}
}

void BlobProductionScene::updateCapacityBench() {
	if (capacityBench_->takeResetRequest()) {
		// New configuration: drop every fragment so the next window
		// measures only fragments assigned under it.
		core_.reset();
	}
	core_.setRegionOverride(capacityBench_->syntheticLoad() ? &capacityBench_->syntheticRegions(activeSeconds_) : nullptr);
	if (!mediaPinPending_) {
		return;
	}
	// Step through the canonical catalog (one navigation per frame) until
	// the active media matches EXPERIENCE_RUNTIME_BLOB_CAPACITY_MEDIA.
	VideoPlaybackStatus st = video_.status();
	auto lower = [](std::string v) {
		std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return v;
	};
	std::string want = lower(capacityBench_->wantedMedia());
	std::string id = lower(st.mediaId.value_or("") + " " + st.fallbackDisplayTitle.value_or(""));
	if (id.find(want) != std::string::npos) {
		mediaPinPending_ = false;
		ofLogNotice("BlobCapacity") << "media pinned: " << st.mediaId.value_or("?");
		core_.reset();
		return;
	}
	bool moved = mediaPinForward_ ? video_.next() : video_.previous();
	if (!moved) {
		if (!mediaPinForward_) {
			mediaPinPending_ = false;
			ofLogError("BlobCapacity") << "no media matches '" << capacityBench_->wantedMedia() << "' — continuing on " << st.mediaId.value_or("?");
			return;
		}
		mediaPinForward_ = false;
	}
}

std::string BlobProductionScene::costByEffectLabel() const {
	std::string out;
	for (const auto & kv : budget_.activeCostByEffect()) {
		out += (out.empty() ? "" : "+") + kv.first + ":" + ofToString(kv.second / 100.0);
	}
	return out.empty() ? std::string("-") : out;
}

bool BlobProductionScene::handleCapacityKey(int key) {
	return capacityBench_ && capacityBench_->handleKey(key);
}

bool BlobProductionScene::takeCapacityCaptureRequest(std::string & label) {
	return capacityBench_ && capacityBench_->takeCaptureRequest(label);
}

void BlobProductionScene::recordRenderProfileFrame(float drawMs) {
	VideoRegionEffectRenderer & renderer = core_.regionController().renderer();
	const VideoRegionEffectRenderer::StageTimings & frameStages = renderer.getStageTimings();
	RenderProfileWindow & w = profileWindow_;
	float frameSeconds = static_cast<float>(ofGetLastFrameTime());
	w.elapsedSeconds += frameSeconds;

	if (frameSeconds > kProfileStallSeconds) {
		// Keep media-load stalls out of the renderer averages entirely.
		w.stalls++;
		w.maxStallMs = std::max(w.maxStallMs, static_cast<double>(frameSeconds) * 1000.0);
	} else {
		w.frames++;
		w.frameMs += frameSeconds * 1000.0;
		w.detectorMs += lastUpdateTimings_.detectorMs;
		w.trackerMs += lastUpdateTimings_.trackerMs;
		w.regionUpdateMs += lastUpdateTimings_.regionUpdateMs;
		w.drawMs += drawMs;
		const VideoRegionEffectRenderer::StageTimings & bg = core_.backgroundEffectRenderer().getStageTimings();
		w.backgroundMs += bg.sourceMs + bg.shaderDirectMs + bg.shaderResultMs + bg.compositeMs;
		w.fragments += core_.activeFragmentCount();
		w.stages.directDraws += frameStages.directDraws;
		w.stages.legacyDraws += frameStages.legacyDraws;
		w.stages.sourceMs += frameStages.sourceMs;
		w.stages.shaderDirectMs += frameStages.shaderDirectMs;
		w.stages.shaderResultMs += frameStages.shaderResultMs;
		w.stages.compositeMs += frameStages.compositeMs;
	}

	if (w.elapsedSeconds < kProfileWindowSeconds) {
		return;
	}

	const VideoRegionEffectRenderer::StageTimings & st = w.stages;
	int draws = st.directDraws + st.legacyDraws;
	auto perFrame = [&](double total) { return w.frames > 0 ? total / w.frames : 0.0; };
	auto perDraw = [](double total, int n) { return n > 0 ? total / n : 0.0; };

	ofLogNotice("BlobRenderProfile")
		<< "path=" << (renderer.isDirectOutputEnabled() ? "direct" : "legacy")
		<< " frames=" << w.frames
		<< " fps=" << (w.frameMs > 0.0 ? 1000.0 * w.frames / w.frameMs : 0.0)
		<< " frameMs=" << perFrame(w.frameMs)
		<< " detectorMs=" << perFrame(w.detectorMs)
		<< " trackerMs=" << perFrame(w.trackerMs)
		<< " regionUpdateMs=" << perFrame(w.regionUpdateMs)
		<< " effectBudgetMax=" << budget_.maxCostCenti() / 100.0
		<< " effectBudgetUsed=" << budget_.usedCostCenti() / 100.0
		<< " activeOriginal=" << budget_.activeOriginalCount()
		<< " activeShader=" << budget_.activeShaderCount()
		<< " activeCostByEffect=" << costByEffectLabel()
		<< " budgetFallbacks=" << budget_.fallbackCount()
		<< " paletteGeneration=" << budget_.generation()
		<< " background=" << (core_.backgroundConfig().mode == 2 ? core_.backgroundConfig().effectName : std::string("normal"))
		<< " sceneDrawMs=" << perFrame(w.drawMs)
		<< " backgroundMs=" << perFrame(w.backgroundMs)
		<< " fragments=" << perFrame(w.fragments)
		<< " directDraws=" << st.directDraws
		<< " legacyDraws=" << st.legacyDraws
		<< " sourceMsPerDraw=" << perDraw(st.sourceMs, draws)
		<< " shaderDirectMsPerDraw=" << perDraw(st.shaderDirectMs, st.directDraws)
		<< " shaderResultMsPerDraw=" << perDraw(st.shaderResultMs, st.legacyDraws)
		<< " compositeMsPerDraw=" << perDraw(st.compositeMs, st.legacyDraws)
		<< " regionRenderMsPerFrame=" << perFrame(st.sourceMs + st.shaderDirectMs + st.shaderResultMs + st.compositeMs)
		<< " scratch=" << renderer.getScratchWidth() << "x" << renderer.getScratchHeight()
		<< " stalls=" << w.stalls
		<< " maxStallMs=" << w.maxStallMs;

	profileWindow_ = RenderProfileWindow {};
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
	// One entry per distinct shader treatment on a live fragment; Original
	// runs no effect, so it adds none.
	if (capacityBench_) {
		status.activeEffects.push_back("region: capacity-experiment " + capacityBench_->plan().caseId);
	} else if (core_.regionParams().fragmentsEnabled) {
		std::vector<std::string> names;
		core_.regionController().collectActiveEffectNames(names);
		std::sort(names.begin(), names.end());
		names.erase(std::unique(names.begin(), names.end()), names.end());
		for (const std::string & e : names) {
			if (!e.empty()) {
				status.activeEffects.push_back("region: " + e);
			}
		}
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

	if (core_.didSetup() && core_.isActive()) {
		const BlobBackgroundMode & current = kBackgroundModes[backgroundModeIndex_];
		SceneModeStatus mode;
		mode.modeId = current.id;
		mode.displayName = current.name;
		mode.index = backgroundModeIndex_;
		mode.count = kBackgroundModeCount;
		status.modeName = mode.displayName;
		status.mode = mode;
	}

	return status;
}

void BlobProductionScene::applyBackgroundMode() {
	BlobSceneCore::BackgroundConfig & background = core_.backgroundConfig();
	background.mode = kBackgroundModes[backgroundModeIndex_].backgroundDrawMode;
	if (background.mode == 2) background.effectName = blobeffects::backgroundEffectId();
	ofLogNotice("BlobProductionScene") << "mode -> " << kBackgroundModes[backgroundModeIndex_].name;
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

	// NextMode (Scene & Pattern Navigation): steps the three background
	// modes (Natural -> Desaturated -> No Background, wrapping).
	SceneCommandDescriptor modeCmd;
	modeCmd.command = SceneCommand::NextMode;
	modeCmd.label = "Next mode";
	modeCmd.shortLabel = "NEXT MODE";
	modeCmd.prominent = false;
	caps.commands.push_back(modeCmd);

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
		case SceneCommand::NextMode:
			if (!core_.isActive()) return false;
			backgroundModeIndex_ = (backgroundModeIndex_ + 1) % kBackgroundModeCount;
			applyBackgroundMode();
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
