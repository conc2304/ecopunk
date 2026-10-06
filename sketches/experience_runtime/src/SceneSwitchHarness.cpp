#include "SceneSwitchHarness.h"

#include "ofAppRunner.h"
#include "ofFileUtils.h"
#include "ofGLUtils.h"
#include "ofGraphics.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cstdlib>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;

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

int totalStatusPulls(SceneManager& sm) {
	int total = 0;
	for (const std::string& id : sm.registeredSceneIdsForTesting()) {
		total += sm.countersForTesting(id)->statusPulls;
	}
	return total;
}

} // namespace

bool SceneSwitchHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_SWITCH_HARNESS") != nullptr;
}

SceneSwitchHarness::SceneSwitchHarness(ExperienceRuntime& runtime)
	: runtime_(runtime) {
	alloccounter::setEnabled(true);
	ofLogNotice("SceneSwitchHarness") << "starting RT-003 switching mechanism self-test "
		"(EXPERIENCE_RUNTIME_SWITCH_HARNESS set) — " << kCycleCount << " Blob -> Temporal -> Blob cycles";

	SceneManager& sm = runtime_.sceneManagerForTesting();
	std::vector<std::string> ids = sm.registeredSceneIdsForTesting();
	logResult("registry holds exactly the two real production scenes, in order [Blob, Temporal]",
		ids.size() == 2 && ids[0] == kBlobId && ids[1] == kTemporalId,
		"registry=" + ofJoinString(ids, ","));
	bool noFake = true;
	for (const std::string& id : ids) {
		if (id == sm.devScene().sceneId()) noFake = false;
	}
	logResult("FakeScene is not a production registry entry", noFake, "fake id=" + sm.devScene().sceneId());
	logResult("deterministic startup scene is Blob", sm.status().activeSceneId == kBlobId,
		"activeSceneId=" + sm.status().activeSceneId);
	const SceneManager::EntryCounters* blob = sm.countersForTesting(kBlobId);
	const SceneManager::EntryCounters* temporal = sm.countersForTesting(kTemporalId);
	logResult("startup: Blob set up once + activated once, capabilities queried once",
		blob && blob->setupCalls == 1 && blob->successfulActivations == 1 && blob->capabilityQueries == 1);
	logResult("startup: Temporal registered but not set up (lazy, at first switch)",
		temporal && temporal->setupCalls == 0 && temporal->activateCalls == 0);
	logResult("startup: no transition in progress", sm.status().transitionPhase == SceneTransitionPhase::Idle);
}

void SceneSwitchHarness::logResult(const std::string& checkName, bool passed, const std::string& detail) {
	totalChecks_++;
	if (!passed) failureCount_++;
	ofLogNotice("SceneSwitchHarness") << (passed ? "PASS" : "FAIL") << " — " << checkName
		<< (detail.empty() ? "" : (": " + detail));
}

std::string SceneSwitchHarness::phaseName(SceneTransitionPhase phase) {
	switch (phase) {
		case SceneTransitionPhase::Idle:      return "Idle";
		case SceneTransitionPhase::FadingOut: return "FadingOut";
		case SceneTransitionPhase::Loading:   return "Loading";
		case SceneTransitionPhase::FadingIn:  return "FadingIn";
		case SceneTransitionPhase::Failed:    return "Failed";
	}
	return "?";
}

std::string SceneSwitchHarness::legLabel() const {
	return "cycle" + ofToString(leg_ / 2) + (leg_ % 2 == 0 ? "_blob_to_temporal" : "_temporal_to_blob");
}

uint64_t SceneSwitchHarness::sceneFboHash() {
	ofPixels px;
	if (!runtime_.readSceneFboPixelsForTesting(px)) return 0;
	uint64_t h = 1469598103934665603ULL; // FNV-1a
	const unsigned char* data = px.getData();
	size_t n = px.size();
	for (size_t i = 0; i < n; ++i) {
		h ^= data[i];
		h *= 1099511628211ULL;
	}
	return h;
}

long long SceneSwitchHarness::netLiveAllocations() const {
	return alloccounter::newCount() - alloccounter::deleteCount();
}

uint64_t SceneSwitchHarness::residentBytes() const {
#ifdef __APPLE__
	mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
		return info.resident_size;
	}
#endif
	return 0;
}

void SceneSwitchHarness::captureScreenshot(const std::string& label) {
	std::string path = "captures/scene_switch_" + legLabel() + "_" + label + ".png";
	ofFilePath::createEnclosingDirectory(path);
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(path);
	ofLogNotice("SceneSwitchHarness") << "captured " << path;
}

void SceneSwitchHarness::checkGlBaseline(const std::string& context) {
	GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
	GLboolean stencilEnabled = glIsEnabled(GL_STENCIL_TEST);
	GLint currentProgram = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
	GLint boundTexture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
	GLint currentFbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFbo);
	bool ok = scissorEnabled == GL_FALSE && stencilEnabled == GL_FALSE && currentProgram == 0
		&& boundTexture == 0 && currentFbo == 0;
	logResult("GL baseline after full draw (" + context + ")", ok,
		"scissor=" + ofToString(static_cast<int>(scissorEnabled)) + " stencil=" + ofToString(static_cast<int>(stencilEnabled))
			+ " program=" + ofToString(currentProgram) + " tex2D=" + ofToString(boundTexture)
			+ " fbo=" + ofToString(currentFbo));
}

void SceneSwitchHarness::logCounters(const std::string& label) {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	for (const std::string& id : sm.registeredSceneIdsForTesting()) {
		const SceneManager::EntryCounters* c = sm.countersForTesting(id);
		ofLogNotice("SceneSwitchHarness") << "COUNTERS [" << label << "] " << id
			<< " setup=" << c->setupCalls << " activate=" << c->activateCalls
			<< " successfulActivations=" << c->successfulActivations << " deactivate=" << c->deactivateCalls
			<< " shutdown=" << c->shutdownCalls << " capabilityQueries=" << c->capabilityQueries
			<< " statusPulls=" << c->statusPulls << " effectPulls=" << c->effectPulls
			<< " update=" << c->updateCalls << " draw=" << c->drawCalls;
	}
	const ExperienceRuntime::PresentationCounters& p = runtime_.presentationCountersForTesting();
	ofLogNotice("SceneSwitchHarness") << "COUNTERS [" << label << "] runtime"
		<< " frames=" << runtime_.frameNumber() << " hudFrameDataAssemblies=" << p.hudFrameDataAssemblies
		<< " hudDraws=" << p.hudDraws << " liveSceneFrames=" << p.liveSceneFrames
		<< " staticSceneFrames=" << p.staticSceneFrames << " sceneFboAllocations=" << p.sceneFboAllocations
		<< " sceneFboSwitchReallocations=" << p.sceneFboSwitchReallocations
		<< " neutralStatusPublications=" << sm.transitionStatusPublicationsForTesting();
}

// One real runtime frame plus the per-frame invariants. Per-frame passes are
// counted but only failures are logged individually (thousands of checks).
void SceneSwitchHarness::runOneFrame(float dt) {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	int pullsBefore = totalStatusPulls(sm);
	uint64_t liveBefore = runtime_.presentationCountersForTesting().liveSceneFrames;

	runtime_.update(dt);
	runtime_.draw();

	const ExperienceRuntime::PresentationCounters& p = runtime_.presentationCountersForTesting();
	const HudFrameData& f = runtime_.currentHudFrameData();
	auto quiet = [&](const std::string& name, bool ok, const std::string& detail) {
		totalChecks_++;
		if (!ok) {
			failureCount_++;
			ofLogNotice("SceneSwitchHarness") << "FAIL — [frame " << runtime_.frameNumber() << ", " << legLabel()
											  << "] " << name << (detail.empty() ? "" : (": " + detail));
		}
	};

	quiet("exactly one HudFrameData assembly this frame", p.hudFrameDataAssemblies == lastAssemblies_ + 1,
		ofToString(p.hudFrameDataAssemblies - lastAssemblies_));
	quiet("exactly one production HUD draw this frame", p.hudDraws == lastHudDraws_ + 1,
		ofToString(p.hudDraws - lastHudDraws_));
	quiet("frameNumber strictly increasing", runtime_.frameNumber() > lastFrameNumber_, "");
	quiet("SceneFrame texture valid", f.sceneFrame.texture != nullptr && f.sceneFrame.texture->isAllocated(), "");
	lastAssemblies_ = p.hudFrameDataAssemblies;
	lastHudDraws_ = p.hudDraws;
	lastFrameNumber_ = runtime_.frameNumber();

	bool live = p.liveSceneFrames == liveBefore + 1;
	int pullsDelta = totalStatusPulls(sm) - pullsBefore;
	quiet("SceneHudStatus pulls == 1 on live frames, 0 on transition-only frames", pullsDelta == (live ? 1 : 0),
		"live=" + ofToString(live) + " pulls=" + ofToString(pullsDelta));

	const std::string& active = f.sceneManager.activeSceneId;
	SceneTransitionPhase phase = f.sceneManager.transitionPhase;
	quiet("SceneHudStatus.sceneId matches SceneManagerStatus.activeSceneId (no mixed ownership)",
		f.scene.sceneId == active, "scene=" + f.scene.sceneId + " active=" + active);
	quiet("effects present iff Temporal owns the frame (Blob = nullopt)",
		f.effects.has_value() == (active == kTemporalId),
		"active=" + active + " effects=" + (f.effects.has_value() ? "present" : "nullopt"));
	bool capsAllowed = phase == SceneTransitionPhase::Idle || phase == SceneTransitionPhase::FadingIn;
	quiet("capabilities published only while Idle/FadingIn",
		capsAllowed ? !f.capabilities.commands.empty() : f.capabilities.commands.empty(),
		phaseName(phase) + " commands=" + ofToString(f.capabilities.commands.size()));
	if (f.scene.semantic.has_value()) {
		std::string prefix = semanticPrefixFor(active);
		bool owned = !prefix.empty() && (f.scene.semantic->state.primaryStateId.empty()
											|| startsWith(f.scene.semantic->state.primaryStateId, prefix));
		for (const SceneMetric& m : f.scene.semantic->metrics) {
			if (!startsWith(m.metricId, prefix)) owned = false;
		}
		quiet("semantic payload belongs to the active scene", owned,
			"active=" + active + " state=" + f.scene.semantic->state.primaryStateId);
	}
}

void SceneSwitchHarness::issueSwitch() {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	bool forward = leg_ % 2 == 0;
	std::string expectedOutgoing = forward ? kBlobId : kTemporalId;
	outgoingId_ = sm.status().activeSceneId;
	incomingId_ = forward ? kTemporalId : kBlobId;
	logResult("[" + legLabel() + "] outgoing owner before switch is " + expectedOutgoing, outgoingId_ == expectedOutgoing,
		"activeSceneId=" + outgoingId_);

	outgoingBefore_ = *sm.countersForTesting(outgoingId_);
	incomingBefore_ = *sm.countersForTesting(incomingId_);
	presentationBefore_ = runtime_.presentationCountersForTesting();
	frozenHash_ = sceneFboHash();
	frozenTextureId_ = runtime_.sceneFboTextureIdForTesting();
	forcedDifferentSize_ = false;
	fadeOutFrames_ = loadingFrames_ = fadeInFrames_ = 0;
	sawFirstIncoming_ = false;
	phaseTrace_.assign(1, "Idle");

	// Real production command path: key -> InputRouter -> RuntimeCommand ->
	// ExperienceRuntime::handleRuntimeCommand -> SceneManager.
	runtime_.keyPressed(forward ? ']' : '[');

	SceneManagerStatus st = sm.status();
	logResult("[" + legLabel() + "] " + (forward ? "NextScene" : "PreviousScene") + " accepted: FadingOut, pending=" + incomingId_
			+ ", active still " + outgoingId_,
		st.transitionPhase == SceneTransitionPhase::FadingOut && st.pendingSceneId == incomingId_
			&& st.activeSceneId == outgoingId_,
		phaseName(st.transitionPhase) + " pending=" + st.pendingSceneId.value_or("-") + " active=" + st.activeSceneId);
	logResult("[" + legLabel() + "] outgoing deactivate() called exactly once at acceptance",
		sm.countersForTesting(outgoingId_)->deactivateCalls == outgoingBefore_.deactivateCalls + 1);
	logResult("[" + legLabel() + "] outgoing capabilities no longer advertised after handoff start",
		sm.activeCapabilities().commands.empty());
	logResult("[" + legLabel() + "] incoming not activated at acceptance",
		sm.countersForTesting(incomingId_)->activateCalls == incomingBefore_.activateCalls);
}

void SceneSwitchHarness::onTransitionFrame() {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	const HudFrameData& f = runtime_.currentHudFrameData();
	SceneTransitionPhase phase = f.sceneManager.transitionPhase;
	if (phaseTrace_.back() != phaseName(phase)) phaseTrace_.push_back(phaseName(phase));

	const SceneManager::EntryCounters& out = *sm.countersForTesting(outgoingId_);
	const SceneManager::EntryCounters& in = *sm.countersForTesting(incomingId_);
	bool outgoingIdle = out.updateCalls == outgoingBefore_.updateCalls && out.drawCalls == outgoingBefore_.drawCalls
		&& out.statusPulls == outgoingBefore_.statusPulls && out.effectPulls == outgoingBefore_.effectPulls;
	if (!outgoingIdle) {
		logResult("[" + legLabel() + "] outgoing scene never updated/drawn/pulled after deactivation", false,
			"phase=" + phaseName(phase));
	}

	switch (phase) {
		case SceneTransitionPhase::FadingOut: {
			fadeOutFrames_++;
			bool staticFrame = runtime_.presentationCountersForTesting().staticSceneFrames
				== presentationBefore_.staticSceneFrames + static_cast<uint64_t>(fadeOutFrames_);
			if (!staticFrame) logResult("[" + legLabel() + "] FadingOut frames are static (no scene render)", false);
			if (f.sceneManager.activeSceneId != outgoingId_ || f.sceneManager.pendingSceneId != incomingId_) {
				logResult("[" + legLabel() + "] FadingOut: active=outgoing, pending=incoming", false,
					"active=" + f.sceneManager.activeSceneId + " pending=" + f.sceneManager.pendingSceneId.value_or("-"));
			}
			if (fadeOutFrames_ == 1 || fadeOutFrames_ == SceneManager::kFadeOutFrames) {
				logResult("[" + legLabel() + "] FadingOut frame " + ofToString(fadeOutFrames_)
						+ ": retained outgoing frame unchanged (pixel hash + texture id), outgoing identity still current",
					sceneFboHash() == frozenHash_ && runtime_.sceneFboTextureIdForTesting() == frozenTextureId_
						&& f.scene.sceneId == outgoingId_ && staticFrame,
					"scene=" + f.scene.sceneId + " progress=" + ofToString(f.sceneManager.transitionProgress, 2));
			}
			if (fadeOutFrames_ == 1 && leg_ < 2) captureScreenshot("1_fadingout");
			if (fadeOutFrames_ == 2) {
				// Suppression probe: repeated + opposite switch input mid-FadingOut.
				runtime_.keyPressed(']');
				runtime_.keyPressed('[');
				SceneManagerStatus st = sm.status();
				logResult("[" + legLabel() + "] switch input during FadingOut suppressed (no queue, no second switch)",
					st.transitionPhase == SceneTransitionPhase::FadingOut && st.pendingSceneId == incomingId_
						&& sm.countersForTesting(outgoingId_)->deactivateCalls == outgoingBefore_.deactivateCalls + 1
						&& sm.countersForTesting(incomingId_)->activateCalls == incomingBefore_.activateCalls);
			}
			break;
		}

		case SceneTransitionPhase::Loading: {
			loadingFrames_++;
			bool neutral = f.scene.sceneId.empty() && !f.scene.semantic.has_value() && !f.effects.has_value()
				&& f.capabilities.commands.empty() && f.sceneManager.activeSceneId.empty()
				&& f.sceneManager.pendingSceneId == incomingId_;
			logResult("[" + legLabel() + "] Loading: ownership released — no outgoing/incoming identity, semantics, "
					  "effects or capabilities; pending=incoming",
				neutral, "scene='" + f.scene.sceneId + "' active='" + f.sceneManager.activeSceneId + "'");
			logResult("[" + legLabel() + "] Loading: static outgoing frame still retained",
				sceneFboHash() == frozenHash_ && runtime_.sceneFboTextureIdForTesting() == frozenTextureId_);
			int expectedSetup = incomingBefore_.setupCalls == 0 ? 1 : incomingBefore_.setupCalls;
			logResult("[" + legLabel() + "] Loading: incoming setup() at most once per instance (setup="
					+ ofToString(in.setupCalls) + ")",
				in.setupCalls == expectedSetup && in.setupCalls == 1);
			logResult("[" + legLabel() + "] Loading: incoming not yet activated",
				in.activateCalls == incomingBefore_.activateCalls);
			runtime_.keyPressed(leg_ % 2 == 0 ? '[' : ']');
			SceneManagerStatus st = sm.status();
			logResult("[" + legLabel() + "] switch input during Loading suppressed",
				st.transitionPhase == SceneTransitionPhase::Loading && st.pendingSceneId == incomingId_);
			checkGlBaseline(legLabel() + " Loading frame");
			if (leg_ < 2) captureScreenshot("2_loading");
			if (leg_ == kDifferentSizeLeg) {
				// Give the real switch path a genuine size difference to fix:
				// Blob and Temporal share 1280x720, so shrink the runtime FBO
				// after the Loading frame was presented; the incoming
				// activation must reallocate it back to its native size.
				runtime_.forceSceneFboReallocationForTesting(glm::ivec2(640, 360));
				forcedTextureId_ = runtime_.sceneFboTextureIdForTesting();
				forcedDifferentSize_ = true;
				presentationBefore_.sceneFboAllocations = runtime_.presentationCountersForTesting().sceneFboAllocations;
			}
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
				SceneManagerStatus st = sm.status();
				logResult("[" + legLabel() + "] switch input during FadingIn suppressed",
					st.transitionPhase == SceneTransitionPhase::FadingIn && st.activeSceneId == incomingId_
						&& !st.pendingSceneId.has_value());
			}
			break;
		}

		case SceneTransitionPhase::Idle: {
			std::string trace = ofJoinString(phaseTrace_, " -> ");
			logResult("[" + legLabel() + "] transition completed: " + trace,
				fadeOutFrames_ == SceneManager::kFadeOutFrames && loadingFrames_ == 1
					&& fadeInFrames_ == SceneManager::kFadeInFrames && sawFirstIncoming_
					&& f.sceneManager.activeSceneId == incomingId_,
				"fadeOut=" + ofToString(fadeOutFrames_) + " loading=" + ofToString(loadingFrames_)
					+ " fadeIn=" + ofToString(fadeInFrames_));
			phase_ = Phase::Dwell;
			framesInPhase_ = 0;
			break;
		}

		case SceneTransitionPhase::Failed:
			logResult("[" + legLabel() + "] transition reached Failed", false, f.sceneManager.message.value_or(""));
			phase_ = Phase::Shutdown;
			break;
	}
}

void SceneSwitchHarness::onFirstIncomingFrame() {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	const HudFrameData& f = runtime_.currentHudFrameData();
	const SceneManager::EntryCounters& in = *sm.countersForTesting(incomingId_);
	const ExperienceRuntime::PresentationCounters& p = runtime_.presentationCountersForTesting();

	logResult("[" + legLabel() + "] first incoming frame: active owner=" + incomingId_
			+ ", status/identity from incoming only, no pending",
		f.sceneManager.activeSceneId == incomingId_ && f.scene.sceneId == incomingId_
			&& !f.sceneManager.pendingSceneId.has_value(),
		"scene=" + f.scene.sceneId);
	logResult("[" + legLabel() + "] incoming activated once; capabilities queried once; capabilityQueries == "
			  "successfulActivations",
		in.activateCalls == incomingBefore_.activateCalls + 1
			&& in.successfulActivations == incomingBefore_.successfulActivations + 1
			&& in.capabilityQueries == incomingBefore_.capabilityQueries + 1
			&& in.capabilityQueries == in.successfulActivations,
		"activations=" + ofToString(in.successfulActivations) + " capabilityQueries=" + ofToString(in.capabilityQueries));
	logResult("[" + legLabel() + "] first incoming frame: one update, one status pull, one draw",
		in.updateCalls == incomingBefore_.updateCalls + 1 && in.statusPulls == incomingBefore_.statusPulls + 1
			&& in.drawCalls == incomingBefore_.drawCalls + 1);

	const SceneCapabilities expected = incomingId_ == kBlobId ? runtime_.blobSceneForTesting().capabilities()
															: runtime_.temporalSceneForTesting().capabilities();
	logResult("[" + legLabel() + "] published capabilities are the incoming scene's own (not merged)",
		sameCommands(f.capabilities, expected), "commands=" + ofToString(f.capabilities.commands.size()));

	if (incomingId_ == kTemporalId) {
		logResult("[" + legLabel() + "] effects nullopt -> present on first Temporal frame", f.effects.has_value());
	} else {
		logResult("[" + legLabel() + "] effects present -> nullopt on first Blob frame (no stale Temporal effects)",
			!f.effects.has_value());
	}

	if (forcedDifferentSize_) {
		// GL texture *names* are recycled after glDeleteTextures (observed:
		// the same id comes back), so an id comparison proves nothing. The
		// evidence is the reallocation itself (counter, logged once), the
		// texture SceneFrame now references being re-specified at the
		// incoming native size (was 640x360), and a freshly rendered frame.
		logResult("[" + legLabel() + "] different-size activation: runtime FBO reallocated 640x360 -> incoming "
				  "native 1280x720 before the incoming draw; SceneFrame reflects the new texture",
			p.sceneFboSwitchReallocations == presentationBefore_.sceneFboSwitchReallocations + 1
				&& f.sceneFrame.nativeSize == glm::ivec2(1280, 720) && f.sceneFrame.texture != nullptr
				&& f.sceneFrame.texture->isAllocated()
				&& static_cast<int>(f.sceneFrame.texture->getWidth()) == 1280
				&& static_cast<int>(f.sceneFrame.texture->getHeight()) == 720,
			"reallocations=" + ofToString(p.sceneFboSwitchReallocations) + " nativeSize="
				+ ofToString(f.sceneFrame.nativeSize.x) + "x" + ofToString(f.sceneFrame.nativeSize.y)
				+ " (GL name before=" + ofToString(forcedTextureId_) + ", after="
				+ ofToString(runtime_.sceneFboTextureIdForTesting()) + " — names are recycled)");
	} else {
		logResult("[" + legLabel() + "] same-size activation: no FBO reallocation, same texture",
			p.sceneFboSwitchReallocations == presentationBefore_.sceneFboSwitchReallocations
				&& runtime_.sceneFboTextureIdForTesting() == frozenTextureId_);
	}
	logResult("[" + legLabel() + "] incoming frame rendered live into the runtime FBO (static frame replaced)",
		p.liveSceneFrames > presentationBefore_.liveSceneFrames);
	checkGlBaseline(legLabel() + " first incoming frame");
	if (leg_ < 2) captureScreenshot("3_first_incoming");
}

void SceneSwitchHarness::finishLeg() {
	SceneManager& sm = runtime_.sceneManagerForTesting();
	const SceneManager::EntryCounters& out = *sm.countersForTesting(outgoingId_);
	const SceneManager::EntryCounters& in = *sm.countersForTesting(incomingId_);
	logResult("[" + legLabel() + "] no inactive updates: outgoing untouched through transition + "
			+ ofToString(kDwellFrames) + " dwell frames",
		out.updateCalls == outgoingBefore_.updateCalls && out.drawCalls == outgoingBefore_.drawCalls
			&& out.statusPulls == outgoingBefore_.statusPulls && out.effectPulls == outgoingBefore_.effectPulls);
	int liveFrames = fadeInFrames_ + 1 + kDwellFrames; // FadingIn + Idle completion frame + dwell
	logResult("[" + legLabel() + "] incoming: exactly one status pull / update / draw per live frame",
		in.statusPulls - incomingBefore_.statusPulls == liveFrames && in.updateCalls - incomingBefore_.updateCalls == liveFrames
			&& in.drawCalls - incomingBefore_.drawCalls == liveFrames,
		"liveFrames=" + ofToString(liveFrames) + " pulls=" + ofToString(in.statusPulls - incomingBefore_.statusPulls));
	if (incomingId_ == kTemporalId) {
		logResult("[" + legLabel() + "] Temporal effect source pulled once per live Temporal frame",
			in.effectPulls - incomingBefore_.effectPulls == liveFrames);
	}
	logCounters(legLabel());

	if (incomingId_ == kBlobId) {
		netAllocationsAtBlobReturn_.push_back(netLiveAllocations());
		residentAtBlobReturn_.push_back(residentBytes());
		ofLogNotice("SceneSwitchHarness") << "RESOURCES after " << legLabel() << ": net live allocations="
										  << netAllocationsAtBlobReturn_.back()
										  << " resident=" << (residentAtBlobReturn_.back() / (1024 * 1024)) << " MB";
	}

	leg_++;
	phase_ = leg_ >= kCycleCount * 2 ? Phase::Shutdown : Phase::Switch;
	framesInPhase_ = 0;
}

void SceneSwitchHarness::step(float dt) {
	switch (phase_) {
		case Phase::Warmup:
			runOneFrame(dt);
			if (++framesInPhase_ >= kWarmupFrames) {
				const HudFrameData& f = runtime_.currentHudFrameData();
				logResult("warmup: " + ofToString(kWarmupFrames) + " live Blob frames, effects nullopt (no FakeScene source)",
					f.sceneManager.activeSceneId == kBlobId && !f.effects.has_value());
				checkGlBaseline("warmup");
				logCounters("warmup");
				phase_ = Phase::Switch;
				framesInPhase_ = 0;
			}
			break;

		case Phase::Switch:
			issueSwitch();
			phase_ = Phase::Transition;
			break;

		case Phase::Transition:
			runOneFrame(dt);
			onTransitionFrame();
			if (phase_ == Phase::Transition && ++framesInPhase_ > 200) {
				logResult("[" + legLabel() + "] transition completes within 200 frames", false);
				phase_ = Phase::Shutdown;
			}
			break;

		case Phase::Dwell:
			runOneFrame(dt);
			if (runtime_.currentHudFrameData().sceneManager.transitionPhase != SceneTransitionPhase::Idle) {
				logResult("[" + legLabel() + "] dwell stays Idle (no surprise second switch)", false);
			}
			if (++framesInPhase_ >= kDwellFrames) {
				if (leg_ < 2) captureScreenshot("4_steady");
				finishLeg();
			}
			break;

		case Phase::Shutdown: {
			SceneManager& sm = runtime_.sceneManagerForTesting();
			logCounters("before-shutdown");
			if (netAllocationsAtBlobReturn_.size() >= 3) {
				long long g1 = netAllocationsAtBlobReturn_[1] - netAllocationsAtBlobReturn_[0];
				long long g2 = netAllocationsAtBlobReturn_[2] - netAllocationsAtBlobReturn_[1];
				ofLogNotice("SceneSwitchHarness") << "RESOURCES net live allocation growth per full cycle: cycle1->2="
												  << g1 << " cycle2->3=" << g2 << "; resident MB: "
												  << residentAtBlobReturn_[0] / (1024 * 1024) << " -> "
												  << residentAtBlobReturn_[1] / (1024 * 1024) << " -> "
												  << residentAtBlobReturn_[2] / (1024 * 1024);
			}
			const SceneManager::EntryCounters& blob = *sm.countersForTesting(kBlobId);
			const SceneManager::EntryCounters& temporal = *sm.countersForTesting(kTemporalId);
			logResult("lifecycle totals: Blob setup=1, Temporal setup=1 across all switches",
				blob.setupCalls == 1 && temporal.setupCalls == 1);
			logResult("lifecycle totals: capabilityQueries == successfulActivations for both scenes",
				blob.capabilityQueries == blob.successfulActivations
					&& temporal.capabilityQueries == temporal.successfulActivations,
				"blob " + ofToString(blob.capabilityQueries) + "/" + ofToString(blob.successfulActivations) + ", temporal "
					+ ofToString(temporal.capabilityQueries) + "/" + ofToString(temporal.successfulActivations));
			logResult("lifecycle totals: Blob activations=" + ofToString(1 + kCycleCount) + ", Temporal activations="
					+ ofToString(kCycleCount),
				blob.successfulActivations == 1 + kCycleCount && temporal.successfulActivations == kCycleCount);

			sm.deactivateScene();
			sm.shutdown();
			logResult("shutdown: both set-up scenes shut down exactly once, after deactivation",
				blob.shutdownCalls == 1 && temporal.shutdownCalls == 1
					&& blob.deactivateCalls == blob.activateCalls && temporal.deactivateCalls == temporal.activateCalls,
				"blob deact/act=" + ofToString(blob.deactivateCalls) + "/" + ofToString(blob.activateCalls)
					+ " temporal deact/act=" + ofToString(temporal.deactivateCalls) + "/" + ofToString(temporal.activateCalls));
			logResult("commands rejected after shutdown()", !sm.dispatchSceneCommand(SceneCommand::NextMedia));
			logResult("scene switch rejected after shutdown()",
				sm.handleSceneSwitchCommand(RuntimeCommand::NextScene) != sceneswitch::RequestResult::Accepted);
			logCounters("final");

			ofLogNotice("SceneSwitchHarness") << "==================================================";
			ofLogNotice("SceneSwitchHarness") << "RT-003 SCENE SWITCH HARNESS SUMMARY";
			ofLogNotice("SceneSwitchHarness") << "cycles: " << kCycleCount << " (Blob -> Temporal -> Blob)";
			ofLogNotice("SceneSwitchHarness") << "total checks: " << totalChecks_;
			ofLogNotice("SceneSwitchHarness") << "failures: " << failureCount_;
			ofLogNotice("SceneSwitchHarness") << (failureCount_ == 0 ? "RESULT: PASS" : "RESULT: FAIL");
			ofLogNotice("SceneSwitchHarness") << "==================================================";
			phase_ = Phase::Done;
			ofExit();
			break;
		}

		case Phase::Done:
			break;
	}
}
