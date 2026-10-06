#include "TwoSceneAcceptanceHarness.h"

#include "ofAppRunner.h"
#include "ofFileUtils.h"
#include "ofGLUtils.h"
#include "ofGraphics.h"
#include "ofImage.h"
#include "ofUtils.h"

#include <algorithm>
#include <cstdlib>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;
const char* kLogModule = "TwoSceneAcceptance";

// Wraps the console channel: everything is still printed, and two message
// kinds are counted/attributed to the current runtime frame.
class AcceptanceLogChannel : public ofConsoleLoggerChannel {
public:
	explicit AcceptanceLogChannel(std::shared_ptr<TwoSceneAcceptanceHarness::LogTap> tap)
		: tap_(std::move(tap)) {}
	using ofConsoleLoggerChannel::log;
	void log(ofLogLevel level, const std::string& module, const std::string& message) override {
		if (message.find("texture is not allocated") != std::string::npos) {
			tap_->textureNotAllocatedWarnings++;
		} else if (level >= OF_LOG_WARNING && level < OF_LOG_SILENT) {
			tap_->otherWarningsOrErrors++;
			tap_->otherMessages.emplace_back(tap_->frame, module + ": " + message);
		}
		if (message.find("applying canonical eligible preset") != std::string::npos) {
			tap_->presetApplications.emplace_back(tap_->frame, message);
		}
		ofConsoleLoggerChannel::log(level, module, message);
	}

private:
	std::shared_ptr<TwoSceneAcceptanceHarness::LogTap> tap_;
};

bool startsWith(const std::string& s, const std::string& prefix) {
	return s.compare(0, prefix.size(), prefix) == 0;
}

std::string semanticPrefixFor(const std::string& sceneId) {
	if (sceneId == kBlobId) return "scene.blob.";
	if (sceneId == kTemporalId) return "scene.temporal.";
	return "";
}

bool sameCommands(const SceneCapabilities& a, const SceneCapabilities& b) {
	if (a.commands.size() != b.commands.size()) return false;
	for (size_t i = 0; i < a.commands.size(); ++i) {
		if (a.commands[i].command != b.commands[i].command) return false;
	}
	return true;
}

bool advertises(const SceneCapabilities& caps, SceneCommand command) {
	for (const auto& d : caps.commands) {
		if (d.command == command) return true;
	}
	return false;
}

uint64_t totalStatusPulls(SceneManager& sm) {
	uint64_t total = 0;
	for (const std::string& id : sm.registeredSceneIdsForTesting()) {
		total += static_cast<uint64_t>(sm.countersForTesting(id)->statusPulls);
	}
	return total;
}

std::string fixedGlState() {
	GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
	GLboolean stencil = glIsEnabled(GL_STENCIL_TEST);
	GLint program = 0, tex = 0, fbo = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
	return "scissor=" + ofToString(static_cast<int>(scissor)) + " stencil=" + ofToString(static_cast<int>(stencil))
		+ " program=" + ofToString(program) + " tex2D=" + ofToString(tex) + " fbo=" + ofToString(fbo);
}

const std::string kExpectedFixedGlState = "scissor=0 stencil=0 program=0 tex2D=0 fbo=0";

// Baseline-relative state: blend, viewport, model-view matrix, style. Compared
// against the snapshot taken after warmup (the accepted Blob-only state).
std::string relativeGlState() {
	GLboolean blend = glIsEnabled(GL_BLEND);
	GLint src = 0, dst = 0;
	glGetIntegerv(GL_BLEND_SRC_RGB, &src);
	glGetIntegerv(GL_BLEND_DST_RGB, &dst);
	ofRectangle vp = ofGetCurrentViewport();
	glm::mat4 mv = ofGetCurrentMatrix(OF_MATRIX_MODELVIEW);
	ofStyle style = ofGetStyle();
	std::string s = "blend=" + ofToString(static_cast<int>(blend)) + "/" + ofToString(src) + "/" + ofToString(dst)
		+ " viewport=" + ofToString(vp.x) + "," + ofToString(vp.y) + "," + ofToString(vp.width) + "x" + ofToString(vp.height)
		+ " mv=";
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) s += ofToString(mv[c][r], 2) + ",";
	}
	s += " style=" + ofToString(static_cast<int>(style.blendingMode)) + "/" + ofToString(style.bFill) + "/"
		+ ofToString(style.lineWidth) + "/" + ofToString(static_cast<int>(style.color.r)) + ","
		+ ofToString(static_cast<int>(style.color.g)) + "," + ofToString(static_cast<int>(style.color.b)) + ","
		+ ofToString(static_cast<int>(style.color.a));
	return s;
}

} // namespace

bool TwoSceneAcceptanceHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE") != nullptr;
}

TwoSceneAcceptanceHarness::TwoSceneAcceptanceHarness(ExperienceRuntime& runtime)
	: runtime_(runtime), tap_(std::make_shared<LogTap>()) {
	previousChannel_ = ofGetLoggerChannel();
	ofSetLoggerChannel(std::make_shared<AcceptanceLogChannel>(tap_));
	alloccounter::setEnabled(true);

	ofLogNotice(kLogModule) << "RT-002 Blob <-> Temporal " << kCycleCount
							<< "-cycle production switching acceptance (EXPERIENCE_RUNTIME_TWO_SCENE_ACCEPTANCE)";

	// -- §1 Registry / startup proof --------------------------------------
	SceneManager& sm = runtime_.sceneManagerForTesting();
	std::vector<std::string> ids = sm.registeredSceneIdsForTesting();
	check("§1 registry = [blob-region-prototype, temporal-fields] (order fixed)",
		ids.size() == 2 && ids[0] == kBlobId && ids[1] == kTemporalId, "registry=" + ofJoinString(ids, ","));
	check("§1 FakeScene not in production registry",
		std::find(ids.begin(), ids.end(), sm.devScene().sceneId()) == ids.end(), "fake=" + sm.devScene().sceneId());
	BlobProductionScene& blob = runtime_.blobSceneForTesting();
	TemporalProductionScene& temporal = runtime_.temporalSceneForTesting();
	ofLogNotice(kLogModule) << "REGISTRY [0] id=" << blob.sceneId() << " displayName=\"" << blob.displayName()
							<< "\" native=" << blob.nativeRenderSize().x << "x" << blob.nativeRenderSize().y;
	ofLogNotice(kLogModule) << "REGISTRY [1] id=" << temporal.sceneId() << " displayName=\"" << temporal.displayName()
							<< "\" native=" << temporal.nativeRenderSize().x << "x" << temporal.nativeRenderSize().y;
	SceneManagerStatus st = sm.status();
	check("§1 startup scene = Blob, Idle", st.activeSceneId == kBlobId && st.transitionPhase == SceneTransitionPhase::Idle,
		"active=" + st.activeSceneId);
	std::string caps;
	for (const auto& d : sm.activeCapabilities().commands) caps += d.label + " ";
	ofLogNotice(kLogModule) << "STARTUP capabilities (Blob): " << caps;
	const SceneManager::EntryCounters* bc = sm.countersForTesting(kBlobId);
	const SceneManager::EntryCounters* tc = sm.countersForTesting(kTemporalId);
	check("§1 startup: Blob setup=1 activate=1 capabilityQueries=1; Temporal not yet set up",
		bc->setupCalls == 1 && bc->successfulActivations == 1 && bc->capabilityQueries == 1 && tc->setupCalls == 0);

	current_ = ActivationRecord{};
	current_.cycle = -1;
	current_.sceneId = kBlobId;
}

TwoSceneAcceptanceHarness::~TwoSceneAcceptanceHarness() {
	if (previousChannel_) ofSetLoggerChannel(previousChannel_);
}

void TwoSceneAcceptanceHarness::check(const std::string& name, bool ok, const std::string& detail, bool verbose) {
	totalChecks_++;
	if (!ok) failureCount_++;
	if (verbose || !ok) {
		ofLogNotice(kLogModule) << (ok ? "PASS" : "FAIL") << " — [frame " << runtime_.frameNumber() << "] " << name
								<< (detail.empty() ? "" : (": " + detail));
	}
}

std::string TwoSceneAcceptanceHarness::phaseName(SceneTransitionPhase phase) {
	switch (phase) {
		case SceneTransitionPhase::Idle:      return "Idle";
		case SceneTransitionPhase::FadingOut: return "FadingOut";
		case SceneTransitionPhase::Loading:   return "Loading";
		case SceneTransitionPhase::FadingIn:  return "FadingIn";
		case SceneTransitionPhase::Failed:    return "Failed";
	}
	return "?";
}

std::string TwoSceneAcceptanceHarness::legLabel() const {
	return "cycle" + ofToString(cycle() + 1) + (leg_ % 2 == 0 ? "_blob_to_temporal" : "_temporal_to_blob");
}

uint64_t TwoSceneAcceptanceHarness::sceneFboHash() {
	ofPixels px;
	if (!runtime_.readSceneFboPixelsForTesting(px)) return 0;
	uint64_t h = 1469598103934665603ULL;
	const unsigned char* d = px.getData();
	for (size_t i = 0, n = px.size(); i < n; ++i) {
		h ^= d[i];
		h *= 1099511628211ULL;
	}
	return h;
}

uint64_t TwoSceneAcceptanceHarness::playheadHash(bool& allocated) {
	TimeOffsetVideoBuffer& buffer = runtime_.temporalSceneForTesting().videoAdapterForTesting().buffer();
	allocated = false;
	if (buffer.getNumPlayheads() == 0) return 0;
	const ofTexture& tex = buffer.getPlayheadTexture(0);
	if (!tex.isAllocated()) return 0;
	allocated = true;
	ofPixels px;
	tex.readToPixels(px);
	uint64_t h = 1469598103934665603ULL;
	const unsigned char* d = px.getData();
	for (size_t i = 0, n = px.size(); i < n; ++i) {
		h ^= d[i];
		h *= 1099511628211ULL;
	}
	return h;
}

long long TwoSceneAcceptanceHarness::netLiveAllocations() const {
	return alloccounter::newCount() - alloccounter::deleteCount();
}

uint64_t TwoSceneAcceptanceHarness::residentBytes() const {
#ifdef __APPLE__
	mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
		return info.resident_size;
	}
#endif
	return 0;
}

void TwoSceneAcceptanceHarness::captureScreenshot(const std::string& label) {
	std::string path = "captures/rt002_" + label + ".png";
	ofFilePath::createEnclosingDirectory(path);
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(path);
	runtime_.saveSceneFrameCaptureForTesting("captures/rt002_" + label + "_scene_only.png");
	ofLogNotice(kLogModule) << "captured " << path << " (" << ofGetWidth() << "x" << ofGetHeight() << " window)";
}

std::string TwoSceneAcceptanceHarness::viewportFingerprint() const {
	const ofMesh& mesh = runtime_.hudCompositorBridgeForTesting().renderer().mediaViewportMesh().mesh();
	if (mesh.getNumVertices() == 0) return "empty";
	glm::vec3 lo = mesh.getVertex(0), hi = lo;
	for (size_t i = 1; i < mesh.getNumVertices(); ++i) {
		lo = glm::min(lo, mesh.getVertex(i));
		hi = glm::max(hi, mesh.getVertex(i));
	}
	return ofToString(mesh.getNumVertices()) + "v [" + ofToString(lo.x, 1) + "," + ofToString(lo.y, 1) + "]-["
		+ ofToString(hi.x, 1) + "," + ofToString(hi.y, 1) + "]";
}

bool TwoSceneAcceptanceHarness::glBaselineOk(std::string& detail) const {
	std::string fixed = fixedGlState();
	std::string relative = relativeGlState();
	detail = fixed + " | " + relative;
	return fixed == kExpectedFixedGlState && (viewportBaseline_.empty() || relative == viewportBaseline_);
}

// -- per-frame -------------------------------------------------------------

void TwoSceneAcceptanceHarness::runOneFrame(float dt) {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	const ExperienceRuntime::PresentationCounters& p = runtime_.presentationCountersForTesting();
	tap_->frame = runtime_.frameNumber() + 1;
	int warningsBefore = tap_->textureNotAllocatedWarnings;
	uint64_t liveBefore = p.liveSceneFrames;
	uint64_t pullsBefore = totalStatusPulls(sm);

	runtime_.update(dt);
	runtime_.draw();

	const HudFrameData& f = runtime_.currentHudFrameData();
	uint64_t bridgeDraws = runtime_.hudCompositorBridgeForTesting().drawCallCount();
	check("one HudFrameData assembly per presented frame", p.hudFrameDataAssemblies == lastAssemblies_ + 1, "", false);
	check("one production HUD draw per presented frame (runtime + bridge counters)",
		p.hudDraws == lastHudDraws_ + 1 && bridgeDraws == lastBridgeDraws_ + 1, "", false);
	check("frameNumber strictly increasing", runtime_.frameNumber() > lastFrameNumber_, "", false);
	check("SceneFrame texture valid", f.sceneFrame.texture != nullptr && f.sceneFrame.texture->isAllocated(), "", false);
	lastAssemblies_ = p.hudFrameDataAssemblies;
	lastHudDraws_ = p.hudDraws;
	lastBridgeDraws_ = bridgeDraws;
	lastFrameNumber_ = runtime_.frameNumber();

	bool live = p.liveSceneFrames == liveBefore + 1;
	if (live) liveFrameCount_++; else staticFrameCount_++;
	uint64_t pulls = totalStatusPulls(sm) - pullsBefore;
	totalStatusPulls_ += pulls;
	check("SceneHudStatus pulls: 1 on live frames, 0 on transition-only static frames", pulls == (live ? 1u : 0u),
		"live=" + ofToString(live) + " pulls=" + ofToString(pulls), false);

	const std::string& active = f.sceneManager.activeSceneId;
	SceneTransitionPhase phase = f.sceneManager.transitionPhase;
	check("no mixed ownership: SceneHudStatus.sceneId == activeSceneId", f.scene.sceneId == active,
		"scene=" + f.scene.sceneId + " active=" + active, false);
	check("HUD profile == activeSceneId (no stale profile)",
		runtime_.hudCompositorBridgeForTesting().renderer().currentScene() == active,
		"hud=" + runtime_.hudCompositorBridgeForTesting().renderer().currentScene() + " active=" + active, false);
	check("effects present iff Temporal owns the frame (Blob = nullopt)", f.effects.has_value() == (active == kTemporalId),
		"active=" + active, false);
	if (active == kBlobId && f.effects.has_value()) blobFramesWithEffects_++;
	bool capsAllowed = phase == SceneTransitionPhase::Idle || phase == SceneTransitionPhase::FadingIn;
	check("capabilities published only while Idle/FadingIn",
		capsAllowed ? !f.capabilities.commands.empty() : f.capabilities.commands.empty(), phaseName(phase), false);
	if (f.scene.semantic.has_value()) {
		std::string prefix = semanticPrefixFor(active);
		bool owned = !prefix.empty()
			&& (f.scene.semantic->state.primaryStateId.empty() || startsWith(f.scene.semantic->state.primaryStateId, prefix));
		for (const SceneMetric& m : f.scene.semantic->metrics) {
			if (!startsWith(m.metricId, prefix)) owned = false;
		}
		check("semantic payload belongs to the active scene", owned, "active=" + active, false);
	}

	std::string glDetail;
	bool glOk = glBaselineOk(glDetail);
	check("GL/FBO baseline after full frame", glOk, glDetail, false);
	if (!glOk) {
		glFailureFrames_++;
		current_.glValid = false;
	}
	if (!viewportBaseline_.empty()) {
		bool hudGeometry = viewportFingerprint() == hudGeometryBaseline_;
		check("HUD media-viewport geometry unchanged", hudGeometry, viewportFingerprint(), false);
		if (!hudGeometry) current_.hudValid = false;
	}

	// Canonical media identity tracking (every frame, whichever scene).
	std::string mediaId = (f.video && f.video->mediaId) ? *f.video->mediaId : std::string();
	if (!lastVideoMediaId_.empty() && mediaId != lastVideoMediaId_) {
		MediaChange mc;
		mc.frame = runtime_.frameNumber();
		mc.activeScene = active;
		mc.fromId = lastVideoMediaId_;
		mc.toId = mediaId;
		mc.intentional = false;
		mediaChanges_.push_back(mc);
		ofLogNotice(kLogModule) << "MEDIA canonical selection changed at frame " << mc.frame << " (active=" << active
								<< "): " << mc.fromId << " -> " << mc.toId;
	}
	lastVideoMediaId_ = mediaId;

	int warnings = tap_->textureNotAllocatedWarnings - warningsBefore;
	if (live) {
		if (active == kTemporalId) {
			observeTemporalFrame(warnings);
		} else {
			current_.liveFrames++;
			if (warnings > 0) {
				current_.warningFrames++;
				current_.warningCount += warnings;
			}
		}
	} else if (warnings > 0) {
		ofLogNotice(kLogModule) << "OBSERVATION texture warning on a transition-only frame " << runtime_.frameNumber()
								<< " (" << phaseName(phase) << ")";
	}

	if (checkFrameAfterMidTransition_) {
		checkFrameAfterMidTransition_ = false;
		std::string d;
		check("§11 frame after mid-TFFragmentTransition valid (texture, GL/FBO baseline, HUD geometry)",
			f.sceneFrame.texture != nullptr && glBaselineOk(d) && viewportFingerprint() == hudGeometryBaseline_
				&& active == kTemporalId,
			d);
	}
}

void TwoSceneAcceptanceHarness::observeTemporalFrame(int warnings) {
	TemporalProductionScene& temporal = runtime_.temporalSceneForTesting();
	TemporalSceneCore& core = temporal.coreForTesting();
	TimeOffsetPlaybackAdapter& adapter = temporal.videoAdapterForTesting();
	const HudFrameData& f = runtime_.currentHudFrameData();
	int hist = core.historyFrameCount();
	int idx = current_.liveFrames;
	current_.liveFrames++;

	if (warnings > 0) {
		current_.warningFrames++;
		current_.warningCount += warnings;
		if (current_.firstWarningLiveIndex < 0) current_.firstWarningLiveIndex = idx;
		current_.lastWarningLiveIndex = idx;
		current_.historyAtWarning.push_back(hist);
		current_.rawVideoAllocatedAtWarning.push_back(adapter.buffer().getRawVideoTexture().isAllocated());
	}
	if (hist == 0) current_.backgroundOnlyObserved = true;
	if (idx == 0) {
		current_.canonicalMediaAtStart = (f.video && f.video->mediaId) ? *f.video->mediaId : std::string();
		current_.canonicalMediaChangedSincePrevious =
			!lastTemporalActivationMedia_.empty() && current_.canonicalMediaAtStart != lastTemporalActivationMedia_;
	}
	if (hist == 0) {
		// Read-only here: with empty history getPlayheadTexture() cannot
		// upload, it returns whatever the texture already holds — i.e. what
		// Temporal just drew for this playhead.
		bool allocated = false;
		uint64_t h = playheadHash(allocated);
		if (allocated) {
			current_.stalePlayheadFrames++;
			if (idx == 0 && lastTemporalPlayheadHash_ != 0 && h == lastTemporalPlayheadHash_) {
				current_.playheadReusedFromPreviousActivation = true;
			}
		}
	}
	if (current_.firstValidLiveIndex < 0 && warnings == 0 && hist > 0) current_.firstValidLiveIndex = idx;

	if (f.effects.has_value()) {
		if (f.effects->slots.empty()) {
			temporalPresentEmptyFrames_++;
			if (firstPresentEmptyFrame_ == 0) firstPresentEmptyFrame_ = runtime_.frameNumber();
		} else {
			temporalPresentActiveFrames_++;
			if (firstPresentActiveFrame_ == 0) firstPresentActiveFrame_ = runtime_.frameNumber();
			for (const auto& s : f.effects->slots) temporalEffectIdsSeen_.insert(s.effectId);
		}
	}

	observeMediaFollow();
	if (cycle() == kLongTemporalCycle && phase_ == Phase::Dwell) observeMidTransition();
	lastHistory_ = hist;
}

void TwoSceneAcceptanceHarness::observeMediaFollow() {
	TemporalProductionScene& temporal = runtime_.temporalSceneForTesting();
	TimeOffsetPlaybackAdapter& adapter = temporal.videoAdapterForTesting();
	const HudFrameData& f = runtime_.currentHudFrameData();
	std::string canonical = (f.video && f.video->mediaId) ? *f.video->mediaId : std::string();

	// Temporal syncs from VideoPlaybackService at the start of its own
	// update; RuntimeServices advances the service later in the same frame,
	// so an automatic advance can be observed one frame before Temporal
	// follows. More than one frame of mismatch would mean Temporal is not
	// following the canonical selection.
	if (!canonical.empty() && adapter.loadedMediaId() != canonical) {
		adapterMismatchStreak_++;
	} else {
		adapterMismatchStreak_ = 0;
	}
	maxAdapterMismatchStreak_ = std::max(maxAdapterMismatchStreak_, adapterMismatchStreak_);

	if (!intentionalTemporalChangeIssued_ || temporalRefillFrame_ != 0) return;
	int hist = temporal.coreForTesting().historyFrameCount();
	uint64_t frame = runtime_.frameNumber();
	if (temporalSelectionFrame_ == 0 && canonical != temporalChangeFromId_ && !canonical.empty()) {
		temporalSelectionFrame_ = frame;
		temporalChangeToId_ = canonical;
		if (!mediaChanges_.empty() && mediaChanges_.back().frame == frame) mediaChanges_.back().intentional = true;
	}
	if (temporalSelectionFrame_ != 0 && temporalAdapterFrame_ == 0 && adapter.loadedMediaId() == temporalChangeToId_
		&& adapter.reloadCount() > temporalReloadsBefore_) {
		temporalAdapterFrame_ = frame;
	}
	if (temporalAdapterFrame_ != 0 && temporalHistoryClearFrame_ == 0 && hist < lastHistory_) {
		temporalHistoryClearFrame_ = frame;
	}
	if (temporalAdapterFrame_ != 0 && temporalHistoryClearFrame_ == 0 && hist <= 1) {
		temporalHistoryClearFrame_ = frame; // cleared and refilled by one frame within the same update
	}
	if (temporalHistoryClearFrame_ != 0 && temporalRefillFrame_ == 0 && hist >= 10) {
		temporalRefillFrame_ = frame;
		temporalFileAfter_ = temporal.videoAdapterForTesting().buffer().getCurrentMediaFilename();
	}
}

void TwoSceneAcceptanceHarness::observeMidTransition() {
	TemporalSceneCore& core = runtime_.temporalSceneForTesting().coreForTesting();
	int patternType = static_cast<int>(core.activePatternType());
	if (lastPatternType_ >= 0 && patternType != lastPatternType_) {
		naturalPatternChanges_++;
		ofLogNotice(kLogModule) << "PATTERN natural change #" << naturalPatternChanges_ << " at frame " << runtime_.frameNumber()
								<< ": type " << lastPatternType_ << " -> " << patternType << " (observed "
								<< transitionFramesObserved_ << " PATTERN_TRANSITION frames observed before this frame; "
								<< "transition length is logged when the phase ends)";
	}
	lastPatternType_ = patternType;
	if (core.compositionPhase() == TFComposition::CyclePhase::PATTERN_TRANSITION) {
		float elapsed = core.compositionPhaseElapsed();
		float progress = elapsed / kProductionTransitionSeconds;
		transitionFramesObserved_++;
		maxTransitionProgressObserved_ = std::max(maxTransitionProgressObserved_, progress);
		if (!midTransitionCaptured_ && progress >= 0.3f && progress <= 0.7f) {
			midTransitionCaptured_ = true;
			midTransitionFrame_ = runtime_.frameNumber();
			const HudFrameData& f = runtime_.currentHudFrameData();
			std::string glDetail;
			bool glOk = glBaselineOk(glDetail);
			GLint fbo = 0;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
			ofRectangle vp = ofGetCurrentViewport();
			ofLogNotice(kLogModule) << "MID-TFFRAGMENTTRANSITION frame=" << midTransitionFrame_ << " active=" << f.sceneManager.activeSceneId
									<< " patternType=" << static_cast<int>(core.activePatternType())
									<< " semanticState=" << (f.scene.semantic ? f.scene.semantic->state.primaryStateId : std::string("(none)"))
									<< " phaseElapsed=" << ofToString(elapsed, 3) << "s progress~" << ofToString(progress, 2)
									<< " sceneFbo=" << f.sceneFrame.nativeSize.x << "x" << f.sceneFrame.nativeSize.y
									<< " windowViewport=" << vp.width << "x" << vp.height << " framebuffer=" << fbo
									<< " hudProfile=" << runtime_.hudCompositorBridgeForTesting().renderer().currentScene()
									<< " hudBindings=" << runtime_.hudCompositorBridgeForTesting().renderer().compiledProfile().bindings.size()
									<< " hudDraws=" << runtime_.presentationCountersForTesting().hudDraws
									<< " hudGeometry=" << viewportFingerprint() << " gl={" << glDetail << "}";
			check("§11 mid-TFFragmentTransition captured under ExperienceRuntime: Temporal active, HUD geometry unchanged, "
				  "no scissor/shader/texture/framebuffer leak, viewport correct",
				f.sceneManager.activeSceneId == kTemporalId && glOk && viewportFingerprint() == hudGeometryBaseline_
					&& f.sceneFrame.nativeSize == glm::ivec2(1280, 720)
					&& runtime_.hudCompositorBridgeForTesting().renderer().currentScene() == kTemporalId,
				"progress~" + ofToString(progress, 2));
			check("§11 mid-transition: no stale Blob content (scene FBO differs from last Blob static frame)",
				sceneFboHash() != frozenHash_);
			captureScreenshot("mid_tffragmenttransition");
			checkFrameAfterMidTransition_ = true;
		}
	} else {
		if (transitionFramesObserved_ > 0) {
			ofLogNotice(kLogModule) << "PATTERN_TRANSITION ended at frame " << runtime_.frameNumber() << " after "
									<< transitionFramesObserved_ << " frames (max progress~" << ofToString(maxTransitionProgressObserved_, 2)
									<< ")";
		}
		transitionFramesObserved_ = 0;
		maxTransitionProgressObserved_ = 0.0f;
		if (midTransitionCaptured_) patternTransitionCompleted_ = true;
	}
}

// -- switching ---------------------------------------------------------------

void TwoSceneAcceptanceHarness::issueSwitch() {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	bool forward = leg_ % 2 == 0;
	outgoingId_ = sm.status().activeSceneId;
	incomingId_ = forward ? kTemporalId : kBlobId;
	check("[" + legLabel() + "] outgoing owner is " + std::string(forward ? kBlobId : kTemporalId),
		outgoingId_ == (forward ? kBlobId : kTemporalId), "", leg_ < 2);
	outgoingBefore_ = *sm.countersForTesting(outgoingId_);
	incomingBefore_ = *sm.countersForTesting(incomingId_);
	presentationBefore_ = runtime_.presentationCountersForTesting();
	frozenHash_ = sceneFboHash();
	frozenTextureId_ = runtime_.sceneFboTextureIdForTesting();
	if (forward) {
		BlobProductionScene& blob = runtime_.blobSceneForTesting();
		blobScratchBeforeTemporal_ = glm::ivec4(blob.fragmentScratchFboWidthForTesting(), blob.fragmentScratchFboHeightForTesting(),
			blob.backgroundScratchFboWidthForTesting(), blob.backgroundScratchFboHeightForTesting());
	}
	fadeOutFrames_ = loadingFrames_ = fadeInFrames_ = 0;
	sawFirstIncoming_ = false;
	firstIncomingHashChecked_ = false;
	phaseTrace_.assign(1, "Idle");

	runtime_.keyPressed(forward ? ']' : '['); // InputRouter -> RuntimeCommand -> ExperienceRuntime -> SceneManager

	SceneManagerStatus st = sm.status();
	check("[" + legLabel() + "] " + (forward ? "NextScene" : "PreviousScene") + " accepted via InputRouter: FadingOut, pending="
			+ incomingId_ + ", active=" + outgoingId_ + ", outgoing deactivated once, outgoing capabilities withdrawn",
		st.transitionPhase == SceneTransitionPhase::FadingOut && st.pendingSceneId == incomingId_
			&& st.activeSceneId == outgoingId_
			&& sm.countersForTesting(outgoingId_)->deactivateCalls == outgoingBefore_.deactivateCalls + 1
			&& sm.activeCapabilities().commands.empty()
			&& sm.countersForTesting(incomingId_)->activateCalls == incomingBefore_.activateCalls,
		"", leg_ < 2);
}

void TwoSceneAcceptanceHarness::onTransitionFrame() {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	const HudFrameData& f = runtime_.currentHudFrameData();
	SceneTransitionPhase phase = f.sceneManager.transitionPhase;
	bool verbose = leg_ < 2; // full detail for one transition in each direction
	if (phaseTrace_.back() != phaseName(phase)) phaseTrace_.push_back(phaseName(phase));

	const SceneManager::EntryCounters& out = *sm.countersForTesting(outgoingId_);
	check("[" + legLabel() + "] outgoing never updated/drawn/pulled after deactivation",
		out.updateCalls == outgoingBefore_.updateCalls && out.drawCalls == outgoingBefore_.drawCalls
			&& out.statusPulls == outgoingBefore_.statusPulls && out.effectPulls == outgoingBefore_.effectPulls,
		phaseName(phase), false);

	switch (phase) {
		case SceneTransitionPhase::FadingOut: {
			fadeOutFrames_++;
			if (fadeOutFrames_ == 1 || fadeOutFrames_ == SceneManager::kFadeOutFrames) {
				check("[" + legLabel() + "] FadingOut " + ofToString(fadeOutFrames_) + "/" + ofToString(SceneManager::kFadeOutFrames)
						+ ": retained static outgoing frame unchanged; outgoing identity current; progress="
						+ ofToString(f.sceneManager.transitionProgress, 2),
					sceneFboHash() == frozenHash_ && runtime_.sceneFboTextureIdForTesting() == frozenTextureId_
						&& f.scene.sceneId == outgoingId_ && f.sceneManager.pendingSceneId == incomingId_,
					"", verbose);
			}
			if (fadeOutFrames_ == 1 && verbose) captureScreenshot(legLabel() + "_1_fadingout");
			if (fadeOutFrames_ == 2) {
				runtime_.keyPressed(']');
				runtime_.keyPressed('[');
				SceneManagerStatus st = sm.status();
				check("[" + legLabel() + "] §3 switch commands during FadingOut rejected; no queued second switch",
					st.transitionPhase == SceneTransitionPhase::FadingOut && st.pendingSceneId == incomingId_
						&& sm.countersForTesting(outgoingId_)->deactivateCalls == outgoingBefore_.deactivateCalls + 1,
					"", verbose);
			}
			break;
		}
		case SceneTransitionPhase::Loading: {
			loadingFrames_++;
			const SceneManager::EntryCounters& in = *sm.countersForTesting(incomingId_);
			check("[" + legLabel() + "] Loading: ownership released (no identity/semantics/effects/capabilities), pending="
					+ incomingId_ + ", static frame retained, incoming setup<=1 and not yet active",
				f.scene.sceneId.empty() && !f.scene.semantic.has_value() && !f.effects.has_value()
					&& f.capabilities.commands.empty() && f.sceneManager.activeSceneId.empty()
					&& f.sceneManager.pendingSceneId == incomingId_ && sceneFboHash() == frozenHash_
					&& in.setupCalls == 1 && in.activateCalls == incomingBefore_.activateCalls,
				"setup=" + ofToString(in.setupCalls), verbose);
			runtime_.keyPressed(leg_ % 2 == 0 ? '[' : ']');
			check("[" + legLabel() + "] §3 switch command during Loading rejected",
				sm.status().transitionPhase == SceneTransitionPhase::Loading && sm.status().pendingSceneId == incomingId_,
				"", verbose);
			if (verbose) captureScreenshot(legLabel() + "_2_loading");
			break;
		}
		case SceneTransitionPhase::FadingIn: {
			fadeInFrames_++;
			if (!sawFirstIncoming_) {
				sawFirstIncoming_ = true;
				onFirstIncomingFrame();
			}
			if (fadeInFrames_ == 2) {
				runtime_.keyPressed(leg_ % 2 == 0 ? ']' : '[');
				check("[" + legLabel() + "] §3 switch command during FadingIn rejected",
					sm.status().transitionPhase == SceneTransitionPhase::FadingIn && sm.status().activeSceneId == incomingId_
						&& !sm.status().pendingSceneId.has_value(),
					"", verbose);
			}
			break;
		}
		case SceneTransitionPhase::Idle: {
			check("[" + legLabel() + "] transition " + ofJoinString(phaseTrace_, " -> ") + " (fadeOut=" + ofToString(fadeOutFrames_)
					+ " loading=" + ofToString(loadingFrames_) + " fadeIn=" + ofToString(fadeInFrames_) + ")",
				fadeOutFrames_ == SceneManager::kFadeOutFrames && loadingFrames_ == 1
					&& fadeInFrames_ == SceneManager::kFadeInFrames && sawFirstIncoming_ && f.sceneManager.activeSceneId == incomingId_,
				"", true);
			phase_ = Phase::Dwell;
			framesInPhase_ = 0;
			break;
		}
		case SceneTransitionPhase::Failed:
			check("[" + legLabel() + "] transition must not fail", false, f.sceneManager.message.value_or(""));
			phase_ = Phase::Shutdown;
			break;
	}
}

void TwoSceneAcceptanceHarness::onFirstIncomingFrame() {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	const HudFrameData& f = runtime_.currentHudFrameData();
	const SceneManager::EntryCounters& in = *sm.countersForTesting(incomingId_);
	bool verbose = leg_ < 2;

	current_.firstFrame = runtime_.frameNumber();

	check("[" + legLabel() + "] first incoming frame: owner=" + incomingId_ + ", status/HUD profile from incoming only",
		f.sceneManager.activeSceneId == incomingId_ && f.scene.sceneId == incomingId_ && !f.sceneManager.pendingSceneId.has_value()
			&& runtime_.hudCompositorBridgeForTesting().renderer().currentScene() == incomingId_,
		"", verbose);
	check("[" + legLabel() + "] §4 incoming activated once; capabilities queried once; capabilityQueries == successfulActivations",
		in.activateCalls == incomingBefore_.activateCalls + 1 && in.successfulActivations == incomingBefore_.successfulActivations + 1
			&& in.capabilityQueries == incomingBefore_.capabilityQueries + 1 && in.capabilityQueries == in.successfulActivations,
		"activations=" + ofToString(in.successfulActivations) + " capabilityQueries=" + ofToString(in.capabilityQueries), verbose);
	check("[" + legLabel() + "] §2 first incoming frame: one update, one status pull, one draw; setup total=1",
		in.updateCalls == incomingBefore_.updateCalls + 1 && in.statusPulls == incomingBefore_.statusPulls + 1
			&& in.drawCalls == incomingBefore_.drawCalls + 1 && in.setupCalls == 1,
		"", verbose);
	const SceneCapabilities expected = incomingId_ == kBlobId ? runtime_.blobSceneForTesting().capabilities()
															: runtime_.temporalSceneForTesting().capabilities();
	bool unsupportedHidden = incomingId_ != kTemporalId
		|| (!advertises(f.capabilities, SceneCommand::Regenerate) && !advertises(f.capabilities, SceneCommand::Reset));
	check("[" + legLabel() + "] §4 capabilities = incoming scene's own (not merged); unsupported not advertised",
		sameCommands(f.capabilities, expected) && unsupportedHidden, "commands=" + ofToString(f.capabilities.commands.size()), verbose);
	if (incomingId_ == kTemporalId) {
		check("[" + legLabel() + "] §6 effects: Blob missing -> Temporal present", f.effects.has_value(), "", verbose);
	} else {
		check("[" + legLabel() + "] §6 effects: Temporal present -> Blob missing (no stale Temporal effects)",
			!f.effects.has_value(), "", verbose);
		// Accepted Blob state (blob-post-acceptance-hardening-report.md §4):
		// the fragment scratch FBO is allocated on demand (legitimately 0x0
		// until a fragment effect first renders); the background scratch FBO
		// is never allocated in production. The §12 requirement is that the
		// Temporal activation leaves Blob's scratch state exactly as Blob
		// left it (Blob is not updated while inactive).
		BlobProductionScene& blob = runtime_.blobSceneForTesting();
		glm::ivec4 now(blob.fragmentScratchFboWidthForTesting(), blob.fragmentScratchFboHeightForTesting(),
			blob.backgroundScratchFboWidthForTesting(), blob.backgroundScratchFboHeightForTesting());
		check("[" + legLabel() + "] §12 Blob scratch FBO state unchanged across the Temporal activation (background stays "
				  "unallocated per accepted Blob state)",
			now == blobScratchBeforeTemporal_ && now.z == 0 && now.w == 0,
			"fragment=" + ofToString(blob.fragmentScratchFboWidthForTesting()) + "x"
				+ ofToString(blob.fragmentScratchFboHeightForTesting()) + " background="
				+ ofToString(blob.backgroundScratchFboWidthForTesting()) + "x"
				+ ofToString(blob.backgroundScratchFboHeightForTesting()), verbose);
	}
	if (incomingId_ == kTemporalId) {
		TimeOffsetPlaybackAdapter& adapter = runtime_.temporalSceneForTesting().videoAdapterForTesting();
		std::string canonical = (f.video && f.video->mediaId) ? *f.video->mediaId : std::string();
		check("[" + legLabel() + "] §7/§12 Temporal history follows current canonical media on activation",
			adapter.loadedMediaId() == canonical, "adapter=" + adapter.loadedMediaId() + " canonical=" + canonical, verbose);
		if (cycle() == kBlobMediaChangeCycle + 1 && !blobChangeFollowChecked_) {
			blobChangeFollowChecked_ = true;
			check("§7 media changed during Blob dwell is followed by the next Temporal activation",
				!blobChangeToId_.empty() && adapter.loadedMediaId() == canonical,
				"changedDuringBlob=" + blobChangeToId_ + " canonicalNow=" + canonical + " adapter=" + adapter.loadedMediaId());
		}
	}

	bool stale = sceneFboHash() == frozenHash_;
	current_.staleOutgoingVisible = stale;
	check("[" + legLabel() + "] §10 incoming live frame replaced the static outgoing frame at FadingIn (no stale outgoing image)",
		!stale && runtime_.presentationCountersForTesting().liveSceneFrames > presentationBefore_.liveSceneFrames, "", verbose);
	check("[" + legLabel() + "] same-size switch: no scene FBO reallocation",
		runtime_.presentationCountersForTesting().sceneFboSwitchReallocations == presentationBefore_.sceneFboSwitchReallocations,
		"", verbose);
	if (verbose) captureScreenshot(legLabel() + "_3_first_incoming");
}

void TwoSceneAcceptanceHarness::onDwellFrame() {
	const HudFrameData& f = runtime_.currentHudFrameData();
	framesInPhase_++;
	check("dwell stays Idle (no surprise switch)", f.sceneManager.transitionPhase == SceneTransitionPhase::Idle,
		phaseName(f.sceneManager.transitionPhase), false);

	bool temporalDwell = incomingId_ == kTemporalId;
	bool done = framesInPhase_ >= kDwellFrames;

	// Semantic/profile check on the first frame of the activation carrying a semantic payload.
	if (f.scene.semantic.has_value() && !current_.semanticChecked) {
		current_.semanticChecked = true;
		std::string prefix = semanticPrefixFor(incomingId_);
		std::string other = semanticPrefixFor(incomingId_ == kBlobId ? kTemporalId : kBlobId);
		bool ok = startsWith(f.scene.semantic->state.primaryStateId, prefix);
		bool noZeroFill = true;
		std::vector<std::string> ids;
		for (const SceneMetric& m : f.scene.semantic->metrics) {
			ids.push_back(m.metricId);
			if (startsWith(m.metricId, other) || !startsWith(m.metricId, prefix)) ok = false;
			// A published metric must carry a real value in one of its value
			// forms (Blob's occupied_area uses normalizedValue). Unsupported
			// metrics are simply not published.
			if (!m.value.has_value() && !m.normalizedValue.has_value() && !m.valueId.has_value()) noZeroFill = false;
		}
		check("[" + legLabel() + "] §9 semantic payload is " + incomingId_ + "'s only (state + metrics), absent values not "
				  "zero-filled; HUD profile = " + incomingId_,
			ok && noZeroFill && runtime_.hudCompositorBridgeForTesting().renderer().currentScene() == incomingId_,
			"state=" + f.scene.semantic->state.primaryStateId + " metrics=" + ofJoinString(ids, ","), leg_ < 2);
	}

	if (temporalDwell && cycle() == kMediaChangeCycle) {
		TemporalProductionScene& temporal = runtime_.temporalSceneForTesting();
		if (framesInPhase_ == 30 && !intentionalTemporalChangeIssued_) {
			intentionalTemporalChangeIssued_ = true;
			temporalChangeFromId_ = lastVideoMediaId_;
			temporalFileBefore_ = temporal.videoAdapterForTesting().buffer().getCurrentMediaFilename();
			temporalReloadsBefore_ = temporal.videoAdapterForTesting().reloadCount();
			historyBeforeTemporalChange_ = temporal.coreForTesting().historyFrameCount();
			temporalChangeIssueFrame_ = runtime_.frameNumber();
			bool useNext = f.video && f.video->canSelectNext;
			ofLogNotice(kLogModule) << "MEDIA intentional " << (useNext ? "NextMedia" : "PreviousMedia")
									<< " during Temporal dwell (key -> InputRouter -> SceneCommand -> active scene -> "
									   "VideoPlaybackService) at frame " << temporalChangeIssueFrame_
									<< ": from=" << temporalChangeFromId_ << " file=" << temporalFileBefore_
									<< " history=" << historyBeforeTemporalChange_;
			runtime_.keyPressed(useNext ? 'n' : 'p');
		}
		bool refilled = temporalRefillFrame_ != 0;
		done = (framesInPhase_ >= kDwellFrames && refilled) || framesInPhase_ >= kMaxExtendedDwellFrames;
	}

	if (!temporalDwell && cycle() == kBlobMediaChangeCycle && framesInPhase_ == 30 && !intentionalBlobChangeIssued_) {
		intentionalBlobChangeIssued_ = true;
		blobChangeFrom_ = lastVideoMediaId_;
		bool useNext = f.video && f.video->canSelectNext;
		runtime_.keyPressed(useNext ? 'n' : 'p');
		ofLogNotice(kLogModule) << "MEDIA intentional " << (useNext ? "NextMedia" : "PreviousMedia")
								<< " during Blob dwell at frame " << runtime_.frameNumber() << " from=" << blobChangeFrom_;
	}
	if (!temporalDwell && cycle() == kBlobMediaChangeCycle && framesInPhase_ == 32 && intentionalBlobChangeIssued_) {
		blobChangeToId_ = lastVideoMediaId_;
		bool changed = !blobChangeToId_.empty() && blobChangeToId_ != blobChangeFrom_;
		if (changed && !mediaChanges_.empty() && mediaChanges_.back().toId == blobChangeToId_) mediaChanges_.back().intentional = true;
		check("§7 canonical media change during Blob dwell (HudFrameData.video follows the canonical service)", changed,
			blobChangeFrom_ + " -> " + blobChangeToId_);
	}

	if (temporalDwell && cycle() == kLongTemporalCycle) {
		done = (framesInPhase_ >= kDwellFrames && midTransitionCaptured_ && patternTransitionCompleted_)
			|| framesInPhase_ >= kMaxTransitionDwellFrames;
	}

	if (done) {
		if (leg_ < 2) captureScreenshot(legLabel() + "_4_steady");
		finishLeg();
	}
}

void TwoSceneAcceptanceHarness::finishLeg() {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	const SceneManager::EntryCounters& out = *sm.countersForTesting(outgoingId_);
	const SceneManager::EntryCounters& in = *sm.countersForTesting(incomingId_);
	check("[" + legLabel() + "] §2 no inactive updates/draws/status pulls for " + outgoingId_ + " across transition + dwell",
		out.updateCalls == outgoingBefore_.updateCalls && out.drawCalls == outgoingBefore_.drawCalls
			&& out.statusPulls == outgoingBefore_.statusPulls && out.effectPulls == outgoingBefore_.effectPulls,
		"", leg_ < 2);
	int live = in.updateCalls - incomingBefore_.updateCalls;
	check("[" + legLabel() + "] §5 incoming: status pulls == updates == draws == live frames; >= 60 active frames",
		in.statusPulls - incomingBefore_.statusPulls == live && in.drawCalls - incomingBefore_.drawCalls == live && live >= 60,
		"activeFrames=" + ofToString(live), leg_ < 2);
	if (incomingId_ == kTemporalId) {
		check("[" + legLabel() + "] canonical effect source pulled once per live Temporal frame",
			in.effectPulls - incomingBefore_.effectPulls == live, "", false);
	}
	minActiveFramesPerActivation_ = std::min(minActiveFramesPerActivation_, live);

	if (cycle() == kLongTemporalCycle && incomingId_ == kTemporalId) {
		check("§11 natural mid-TFFragmentTransition captured during the extended cycle-" + ofToString(kLongTemporalCycle + 1)
				+ " Temporal activation",
			midTransitionCaptured_,
			"dwellFrames=" + ofToString(framesInPhase_) + " naturalPatternChanges=" + ofToString(naturalPatternChanges_));
	}
	if (cycle() == kMediaChangeCycle && incomingId_ == kTemporalId) {
		bool ok = temporalSelectionFrame_ != 0 && temporalAdapterFrame_ != 0 && temporalHistoryClearFrame_ != 0
			&& temporalRefillFrame_ != 0 && temporalChangeToId_ != temporalChangeFromId_ && temporalFileAfter_ != temporalFileBefore_;
		check("§7 mandatory live media change while Temporal active: selection -> adapter follow -> history clear -> refill from new media",
			ok,
			"from=" + temporalChangeFromId_ + " to=" + temporalChangeToId_ + " issued@" + ofToString(temporalChangeIssueFrame_)
				+ " selection@" + ofToString(temporalSelectionFrame_) + " adapter@" + ofToString(temporalAdapterFrame_)
				+ " historyClear@" + ofToString(temporalHistoryClearFrame_) + " refilled(>=10)@" + ofToString(temporalRefillFrame_)
				+ " file " + temporalFileBefore_ + " -> " + temporalFileAfter_);
	}

	if (incomingId_ == kTemporalId) {
		// §7/§12 acceptance: while Temporal's history is empty it must not
		// present a previously uploaded playhead texture (pre-deactivation or
		// pre-media-change content) as current output.
		check("[" + legLabel() + "] §7/§12 no stale playhead texture presented while Temporal history is empty "
				  "(TEMPORAL DOMAIN — observe only, not patched in RT-002)",
			current_.stalePlayheadFrames == 0,
			"staleFrames=" + ofToString(current_.stalePlayheadFrames) + " reusedPreviousActivationContent="
				+ ofToString(current_.playheadReusedFromPreviousActivation) + " canonicalMediaChangedSincePrevious="
				+ ofToString(current_.canonicalMediaChangedSincePrevious));
		// Snapshot playhead 0 as last shown in this activation (history is
		// full here; the scene already uploaded this frame's data during its
		// draw, so this read does not change what was presented).
		bool allocated = false;
		lastTemporalPlayheadHash_ = playheadHash(allocated);
		const HudFrameData& f = runtime_.currentHudFrameData();
		lastTemporalActivationMedia_ = (f.video && f.video->mediaId) ? *f.video->mediaId : std::string();
	}
	current_.liveFrames = live;
	activations_.push_back(current_);
	if (incomingId_ == kBlobId) recordResourceRow();

	leg_++;
	current_ = ActivationRecord{};
	current_.cycle = cycle() + 1;
	current_.sceneId = leg_ % 2 == 0 ? kBlobId : kTemporalId; // replaced at the next first-incoming frame
	phase_ = leg_ >= kCycleCount * 2 ? Phase::FinalValidation : Phase::Switch;
	framesInPhase_ = 0;
}

void TwoSceneAcceptanceHarness::recordResourceRow() {
	ResourceRow r;
	TemporalSceneCore& core = runtime_.temporalSceneForTesting().coreForTesting();
	BlobProductionScene& blob = runtime_.blobSceneForTesting();
	const ExperienceRuntime::PresentationCounters& p = runtime_.presentationCountersForTesting();
	r.cycle = cycle() + 1;
	r.frame = runtime_.frameNumber();
	r.residentMB = residentBytes() / (1024 * 1024);
	r.netAllocations = netLiveAllocations();
	r.sceneFboAllocations = p.sceneFboAllocations;
	r.sceneFboSwitchReallocations = p.sceneFboSwitchReallocations;
	r.blobFragmentScratchW = blob.fragmentScratchFboWidthForTesting();
	r.blobFragmentScratchH = blob.fragmentScratchFboHeightForTesting();
	r.blobBackgroundScratchW = blob.backgroundScratchFboWidthForTesting();
	r.blobBackgroundScratchH = blob.backgroundScratchFboHeightForTesting();
	r.temporalHistory = core.historyFrameCount();
	r.temporalCapacity = core.historyCapacityFrames();
	r.temporalPlayheads = core.numPlayheads();
	r.temporalAdapterReloads = runtime_.temporalSceneForTesting().videoAdapterForTesting().reloadCount();
	r.temporalHasMedia = core.hasMedia();
	r.textureWarningsTotal = tap_->textureNotAllocatedWarnings;
	resources_.push_back(r);
	ofLogNotice(kLogModule) << "RESOURCE cycle=" << r.cycle << " frame=" << r.frame << " residentMB=" << r.residentMB
							<< " netAllocs=" << r.netAllocations << " fboAllocs=" << r.sceneFboAllocations
							<< " fboSwitchReallocs=" << r.sceneFboSwitchReallocations << " blobFragScratch="
							<< r.blobFragmentScratchW << "x" << r.blobFragmentScratchH << " blobBgScratch=" << r.blobBackgroundScratchW
							<< "x" << r.blobBackgroundScratchH << " temporalHistory=" << r.temporalHistory << "/" << r.temporalCapacity
							<< " playheads=" << r.temporalPlayheads << " adapterReloads=" << r.temporalAdapterReloads
							<< " temporalHasMedia=" << r.temporalHasMedia << " textureWarnings=" << r.textureWarningsTotal;
}

// -- final validation / shutdown ---------------------------------------------

void TwoSceneAcceptanceHarness::runFinalValidation() {
	const HudFrameData& f = runtime_.currentHudFrameData();
	std::string glDetail;
	bool glOk = glBaselineOk(glDetail);
	bool videoOk = f.video.has_value() && f.video->mediaId.has_value()
		&& (f.video->health == VideoPlaybackHealth::Ready || f.video->health == VideoPlaybackHealth::Loading);
	check("§15 final validation: Blob active + Idle, status Ready, live render, effects missing, canonical video valid, "
		  "HUD profile Blob, GL/FBO baseline",
		f.sceneManager.activeSceneId == kBlobId && f.sceneManager.transitionPhase == SceneTransitionPhase::Idle
			&& f.scene.sceneId == kBlobId && f.scene.health == SceneHealth::Ready && !f.effects.has_value() && videoOk
			&& runtime_.hudCompositorBridgeForTesting().renderer().currentScene() == kBlobId && glOk
			&& f.sceneFrame.texture != nullptr && sceneFboHash() != 0,
		"video health=" + ofToString(f.video ? static_cast<int>(f.video->health) : -1));
	captureScreenshot("final_blob");
}

void TwoSceneAcceptanceHarness::runShutdown() {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	TemporalProductionScene& temporal = runtime_.temporalSceneForTesting();
	const SceneManager::EntryCounters& b = *sm.countersForTesting(kBlobId);
	const SceneManager::EntryCounters& t = *sm.countersForTesting(kTemporalId);

	check("§2 totals: setup once per scene (Blob=1, Temporal=1)", b.setupCalls == 1 && t.setupCalls == 1);
	check("§2/§4 totals: Blob activations=21, Temporal activations=20; capabilityQueries == successfulActivations",
		b.successfulActivations == 1 + kCycleCount && t.successfulActivations == kCycleCount
			&& b.capabilityQueries == b.successfulActivations && t.capabilityQueries == t.successfulActivations,
		"blob " + ofToString(b.capabilityQueries) + "/" + ofToString(b.successfulActivations) + " temporal "
			+ ofToString(t.capabilityQueries) + "/" + ofToString(t.successfulActivations));
	ofLogNotice(kLogModule) << "COUNTERS pre-shutdown blob setup=" << b.setupCalls << " activate=" << b.activateCalls
							<< " success=" << b.successfulActivations << " deactivate=" << b.deactivateCalls
							<< " capQ=" << b.capabilityQueries << " statusPulls=" << b.statusPulls << " update=" << b.updateCalls
							<< " draw=" << b.drawCalls << " effectPulls=" << b.effectPulls;
	ofLogNotice(kLogModule) << "COUNTERS pre-shutdown temporal setup=" << t.setupCalls << " activate=" << t.activateCalls
							<< " success=" << t.successfulActivations << " deactivate=" << t.deactivateCalls
							<< " capQ=" << t.capabilityQueries << " statusPulls=" << t.statusPulls << " update=" << t.updateCalls
							<< " draw=" << t.drawCalls << " effectPulls=" << t.effectPulls;

	int warningsBeforeExit = tap_->textureNotAllocatedWarnings;
	runtime_.exit(); // normal runtime shutdown: deactivate active scene, shut down every set-up scene, RuntimeServices

	check("§15 shutdown: active Blob deactivated; every activation matched by a deactivation",
		b.deactivateCalls == b.activateCalls && t.deactivateCalls == t.activateCalls,
		"blob " + ofToString(b.deactivateCalls) + "/" + ofToString(b.activateCalls) + " temporal "
			+ ofToString(t.deactivateCalls) + "/" + ofToString(t.activateCalls));
	check("§15 shutdown: both set-up scenes shut down exactly once", b.shutdownCalls == 1 && t.shutdownCalls == 1);
	check("§15 post-shutdown: scene commands and scene switches rejected",
		!sm.dispatchSceneCommand(SceneCommand::NextMedia)
			&& sm.handleSceneSwitchCommand(RuntimeCommand::NextScene) != sceneswitch::RequestResult::Accepted);
	check("§15 post-shutdown: no scene work runs (beginFrame reports no live scene)", !sm.beginFrame());
	ofLogNotice(kLogModule) << "SHUTDOWN observation: temporal history after shutdown=" << temporal.coreForTesting().historyFrameCount()
							<< " temporal buffer hasMedia=" << temporal.coreForTesting().hasMedia()
							<< " sceneFbo still allocated=" << (runtime_.sceneFboTextureIdForTesting() != 0)
							<< " (released by destructors at process teardown)"
							<< " textureWarningsDuringExit=" << (tap_->textureNotAllocatedWarnings - warningsBeforeExit);
}

void TwoSceneAcceptanceHarness::writeReportTables() {
	ofLogNotice(kLogModule) << "TABLE activations: cycle | scene | activeFrames | warningFrames | warnings | firstWarn | lastWarn | "
							   "firstValid | historyAtWarnings | rawVideoAllocatedAtWarnings | staleOutgoing | backgroundOnly | hud | gl"
							   " || stalePlayheadFrames | playheadReusedFromPrevious | canonicalMediaChangedSincePrevious | mediaAtStart";
	for (const ActivationRecord& a : activations_) {
		std::string hw, rv;
		for (int h : a.historyAtWarning) hw += ofToString(h) + ",";
		for (bool r : a.rawVideoAllocatedAtWarning) rv += std::string(r ? "1" : "0") + ",";
		ofLogNotice(kLogModule) << "ACT | " << a.cycle << " | " << a.sceneId << " | " << a.liveFrames << " | " << a.warningFrames
								<< " | " << a.warningCount << " | " << a.firstWarningLiveIndex << " | " << a.lastWarningLiveIndex
								<< " | " << a.firstValidLiveIndex << " | " << hw << " | " << rv << " | " << a.staleOutgoingVisible
								<< " | " << a.backgroundOnlyObserved << " | " << a.hudValid << " | " << a.glValid << " || "
								<< a.stalePlayheadFrames << " | " << a.playheadReusedFromPreviousActivation << " | "
								<< a.canonicalMediaChangedSincePrevious << " | " << a.canonicalMediaAtStart;
	}
	for (const MediaChange& m : mediaChanges_) {
		ofLogNotice(kLogModule) << "MEDIACHANGE frame=" << m.frame << " active=" << m.activeScene << " " << m.fromId << " -> "
								<< m.toId << (m.intentional ? " (intentional)" : "");
	}
	for (const auto& pa : tap_->presetApplications) {
		ofLogNotice(kLogModule) << "PRESET frame=" << pa.first << " " << pa.second;
	}
	for (const auto& om : tap_->otherMessages) {
		ofLogNotice(kLogModule) << "OTHERWARN frame=" << om.first << " " << om.second;
	}
	std::string ids;
	for (const auto& id : temporalEffectIdsSeen_) ids += id + " ";
	ofLogNotice(kLogModule) << "EFFECTS temporal presentEmptyFrames=" << temporalPresentEmptyFrames_ << " (first@" << firstPresentEmptyFrame_
							<< ") presentActiveFrames=" << temporalPresentActiveFrames_ << " (first@" << firstPresentActiveFrame_
							<< ") effectIds={" << ids << "} blobFramesWithEffects=" << blobFramesWithEffects_;
	ofLogNotice(kLogModule) << "TOTALS frames=" << runtime_.frameNumber() << " live=" << liveFrameCount_ << " static=" << staticFrameCount_
							<< " statusPulls=" << totalStatusPulls_ << " hudFrameData=" << runtime_.presentationCountersForTesting().hudFrameDataAssemblies
							<< " hudDraws=" << runtime_.presentationCountersForTesting().hudDraws
							<< " bridgeDraws=" << runtime_.hudCompositorBridgeForTesting().drawCallCount()
							<< " glFailureFrames=" << glFailureFrames_ << " minActiveFramesPerActivation=" << minActiveFramesPerActivation_
							<< " maxAdapterMismatchStreak=" << maxAdapterMismatchStreak_
							<< " textureWarnings=" << tap_->textureNotAllocatedWarnings << " otherWarnings=" << tap_->otherWarningsOrErrors;
}

// -- driver -------------------------------------------------------------------

void TwoSceneAcceptanceHarness::step(float dt) {
	switch (phase_) {
		case Phase::Warmup:
			runOneFrame(dt);
			if (++framesInPhase_ >= kWarmupFrames) {
				viewportBaseline_ = relativeGlState();
				hudGeometryBaseline_ = viewportFingerprint();
				ofLogNotice(kLogModule) << "BASELINE gl={" << fixedGlState() << " | " << viewportBaseline_ << "} hudGeometry="
										<< hudGeometryBaseline_;
				const HudFrameData& f = runtime_.currentHudFrameData();
				check("warmup: 60 live Blob frames, effects missing, GL fixed baseline",
					f.sceneManager.activeSceneId == kBlobId && !f.effects.has_value() && fixedGlState() == kExpectedFixedGlState);
				current_.liveFrames = kWarmupFrames;
				activations_.push_back(current_);
				current_ = ActivationRecord{};
				current_.cycle = 1;
				current_.sceneId = kTemporalId;
				phase_ = Phase::Switch;
				framesInPhase_ = 0;
			}
			break;
		case Phase::Switch:
			issueSwitch();
			current_ = ActivationRecord{};
			current_.cycle = cycle() + 1;
			current_.sceneId = incomingId_;
			phase_ = Phase::Transition;
			framesInPhase_ = 0;
			break;
		case Phase::Transition:
			runOneFrame(dt);
			onTransitionFrame();
			if (phase_ == Phase::Transition && ++framesInPhase_ > 200) {
				check("[" + legLabel() + "] transition completes within 200 frames", false);
				phase_ = Phase::Shutdown;
			}
			break;
		case Phase::Dwell:
			runOneFrame(dt);
			onDwellFrame();
			break;
		case Phase::FinalValidation:
			runOneFrame(dt);
			if (++framesInPhase_ >= 10) {
				runFinalValidation();
				phase_ = Phase::Shutdown;
			}
			break;
		case Phase::Shutdown:
			runShutdown();
			writeReportTables();
			ofLogNotice(kLogModule) << "==================================================";
			ofLogNotice(kLogModule) << "RT-002 TWO-SCENE ACCEPTANCE SUMMARY";
			ofLogNotice(kLogModule) << "complete cycles: " << (leg_ / 2) << "/" << kCycleCount;
			ofLogNotice(kLogModule) << "total checks: " << totalChecks_;
			ofLogNotice(kLogModule) << "failures: " << failureCount_;
			ofLogNotice(kLogModule) << (failureCount_ == 0 ? "RESULT: PASS" : "RESULT: FAIL");
			ofLogNotice(kLogModule) << "==================================================";
			phase_ = Phase::Done;
			ofExit();
			break;
		case Phase::Done:
			break;
	}
}
