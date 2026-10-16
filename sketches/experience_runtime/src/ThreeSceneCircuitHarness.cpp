#include "ThreeSceneCircuitHarness.h"

#include "BESettings.h" // MAX_FRAGMENTS
#include "Rt004StallProbe.h" // env-gated scopes: attributes harness readback cost

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofGraphics.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#if defined(__APPLE__)
#include <mach/mach.h>
#else
#include <unistd.h>
#endif

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;
const std::string kBlueprintId = BlueprintProductionScene::kSceneId;
const std::string kFragmentId = FragmentProductionScene::kSceneId;
const std::string kQuadrantId = QuadrantProductionScene::kSceneId;
constexpr int kCircuits = 10;
constexpr float kDwellSeconds = 3.0f;
constexpr float kStateTimeoutSeconds = 45.0f;
// RSS growth allowed between the end of circuit 2 (every scene set up and
// visited twice) and the end of the run.
constexpr double kRssGrowthBudgetMiB = 32.0;

bool hasCommand(const SceneCapabilities& caps, SceneCommand command) {
	return std::any_of(caps.commands.begin(), caps.commands.end(),
		[&](const SceneCommandDescriptor& d) { return d.command == command; });
}

std::string semanticPrefixFor(const std::string& sceneId) {
	if (sceneId == kBlobId) return "scene.blob.";
	if (sceneId == kTemporalId) return "scene.temporal.";
	if (sceneId == kBlueprintId) return "scene.blueprint.";
	if (sceneId == kFragmentId) return "scene.fragment.";
	if (sceneId == kQuadrantId) return "scene.quadrant.";
	return "?";
}

std::string shortName(const std::string& sceneId) {
	if (sceneId == kBlobId) return "blob";
	if (sceneId == kTemporalId) return "temporal";
	if (sceneId == kBlueprintId) return "blueprint";
	if (sceneId == kFragmentId) return "fragment";
	if (sceneId == kQuadrantId) return "quadrant";
	return sceneId;
}

std::string mediaIdOf(const HudFrameData& f) {
	return (f.video && f.video->mediaId) ? *f.video->mediaId : std::string("(none)");
}

double residentMiB() {
#if defined(__APPLE__)
	mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
		return -1.0;
	}
	return static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
#else
	std::ifstream statm("/proc/self/statm");
	long pages = 0, resident = 0;
	if (!(statm >> pages >> resident)) return -1.0;
	return static_cast<double>(resident) * static_cast<double>(sysconf(_SC_PAGESIZE)) / (1024.0 * 1024.0);
#endif
}

const char* phaseName(SceneTransitionPhase p) {
	switch (p) {
		case SceneTransitionPhase::Idle: return "Idle";
		case SceneTransitionPhase::FadingOut: return "FadingOut";
		case SceneTransitionPhase::Loading: return "Loading";
		case SceneTransitionPhase::FadingIn: return "FadingIn";
		case SceneTransitionPhase::Failed: return "Failed";
	}
	return "?";
}

} // namespace

bool ThreeSceneCircuitHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_THREE_SCENE_HARNESS") != nullptr || isFullRingRequested();
}

bool ThreeSceneCircuitHarness::isFullRingRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_FULL_RING_HARNESS") != nullptr;
}

ThreeSceneCircuitHarness::ThreeSceneCircuitHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	fullRing_ = isFullRingRequested();
	captureDir_ = fullRing_ ? "captures/five-scene-ring" : "captures/rt005-three-scene";
	std::filesystem::create_directories(ofToDataPath(captureDir_, true));
	ring_ = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
	std::vector<std::string> expected = {kBlobId, kTemporalId, kBlueprintId};
	if (fullRing_) {
		expected.push_back(kFragmentId);
		expected.push_back(kQuadrantId);
	}
	check(fullRing_ ? "full roster [Blob, Temporal, Blueprint, Fragment, Quadrant]" : "three-scene roster [Blob, Temporal, Blueprint]",
		ring_ == expected, ofJoinString(ring_, ","));
	if (ring_ != expected) {
		finish();
		return;
	}
	const int n = static_cast<int>(ring_.size());
	int at = 0; // Blob
	for (int c = 1; c <= kCircuits; ++c) {
		const bool forward = (c % 2) == 1;
		for (int i = 0; i < n; ++i) {
			Leg leg;
			leg.circuit = c;
			leg.index = i;
			leg.key = forward ? ']' : '[';
			leg.fromId = ring_[static_cast<size_t>(at)];
			at = forward ? (at + 1) % n : (at + n - 1) % n;
			leg.toId = ring_[static_cast<size_t>(at)];
			legs_.push_back(leg);
		}
	}
}

void ThreeSceneCircuitHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void ThreeSceneCircuitHarness::note(const std::string& line) {
	ofLogNotice("ThreeSceneCircuitHarness") << line;
	lines_.push_back(line);
}

void ThreeSceneCircuitHarness::captureWindow(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(captureDir_ + "/" + name + ".png");
	capturedThisFrame_ = true;
	note("capture: bin/data/" + captureDir_ + "/" + name + ".png");
}

void ThreeSceneCircuitHarness::captureScene(const std::string& name) {
	const std::string path = captureDir_ + "/" + name + ".png";
	capturedThisFrame_ = true;
	if (runtime_.saveSceneFrameCaptureForTesting(path)) note("capture: bin/data/" + path);
}

bool ThreeSceneCircuitHarness::readScene(ofPixels& out) const {
	return runtime_.readSceneFboPixelsForTesting(out) && out.getWidth() > 0;
}

float ThreeSceneCircuitHarness::meanAbsDiff(const ofPixels& a, const ofPixels& b) {
	if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight() || a.getWidth() == 0) return 255.0f;
	double sum = 0.0;
	int n = 0;
	for (size_t y = 4; y < a.getHeight(); y += 8) {
		for (size_t x = 4; x < a.getWidth(); x += 8) {
			const ofColor ca = a.getColor(x, y);
			const ofColor cb = b.getColor(x, y);
			sum += (std::abs(ca.r - cb.r) + std::abs(ca.g - cb.g) + std::abs(ca.b - cb.b)) / 3.0;
			n++;
		}
	}
	return n > 0 ? static_cast<float>(sum / n) : 0.0f;
}

float ThreeSceneCircuitHarness::meanLuma(const ofPixels& px) {
	double sum = 0.0;
	int n = 0;
	for (size_t y = 8; y < px.getHeight(); y += 16) {
		for (size_t x = 8; x < px.getWidth(); x += 16) {
			const ofColor c = px.getColor(x, y);
			sum += (c.r + c.g + c.b) / 3.0;
			n++;
		}
	}
	return n > 0 ? static_cast<float>(sum / n) : 0.0f;
}

int ThreeSceneCircuitHarness::drainGlErrors() {
	int n = 0;
	while (glGetError() != GL_NO_ERROR && n < 32) n++;
	return n;
}

std::string ThreeSceneCircuitHarness::legLabel() const {
	const Leg& l = legs_[leg_];
	return "c" + ofToString(l.circuit) + (l.key == ']' ? "F" : "B") + "." + ofToString(l.index + 1) + " "
		+ shortName(l.fromId) + "->" + shortName(l.toId);
}

void ThreeSceneCircuitHarness::checkIdleInvariants() {
	// GL state after a full presented frame (scene FBO pass + HUD): no scene
	// may leave scissor testing on or an FBO bound.
	{
		GLint boundFramebuffer = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFramebuffer);
		glStateFramesChecked_++;
		if (glIsEnabled(GL_SCISSOR_TEST) || boundFramebuffer != 0) {
			glStateLeaks_++;
			if (glStateLeaks_ <= 3) note(std::string("GL state leak after frame: scissor=") + (glIsEnabled(GL_SCISSOR_TEST) ? "1" : "0")
				+ " fbo=" + ofToString(boundFramebuffer) + " owner=" + runtime_.currentHudFrameData().sceneManager.activeSceneId);
		}
	}
	const HudFrameData& f = runtime_.currentHudFrameData();
	if (f.sceneManager.transitionPhase != SceneTransitionPhase::Idle) {
		// No live owner's controls may be published mid-transition.
		if (f.sceneManager.transitionPhase != SceneTransitionPhase::FadingIn && !f.capabilities.commands.empty()) {
			invariantFailures_++;
			if (invariantFailures_ <= 5) note(std::string("invariant violation: capabilities published during ") + phaseName(f.sceneManager.transitionPhase));
		}
		return;
	}
	const std::string& active = f.sceneManager.activeSceneId;
	idleFramesChecked_++;
	std::string why;
	if (f.scene.sceneId != active) why = "status sceneId " + f.scene.sceneId + " != " + active;
	else if (f.effects.has_value() != (active == kTemporalId)) why = "effects presence wrong for " + active;
	else if (hasCommand(f.capabilities, SceneCommand::Regenerate) != (active == kBlueprintId)) why = "Regenerate capability wrong for " + active;
	else if (!hasCommand(f.capabilities, SceneCommand::NextMedia)) why = "NextMedia missing";
	else if (f.scene.semantic && f.scene.semantic->state.primaryStateId.rfind(semanticPrefixFor(active), 0) != 0)
		why = "semantic state " + f.scene.semantic->state.primaryStateId + " not owned by " + active;
	else if (!f.video || !f.video->mediaId) why = "canonical video status missing";
	else if (active == kBlueprintId
		&& runtime_.blueprintSceneForTesting().coreForTesting().composition().getFragments().size() > static_cast<size_t>(MAX_FRAGMENTS))
		why = "Blueprint fragment count above MAX_FRAGMENTS";
	else if (active == kFragmentId
		&& runtime_.fragmentSceneForTesting().coreForTesting().pool().getFragmentCount()
			> runtime_.fragmentSceneForTesting().coreForTesting().parameters().getMaxFragments())
		why = "Fragment count above its maxFragments cap";
	if (why.empty() && active == kFragmentId) {
		// Canonical media authority: Fragment's follower must load exactly the
		// canonical selection (it may observe a change one frame late).
		const std::string& loaded = runtime_.fragmentSceneForTesting().videoAdapterForTesting().loadedMediaId();
		if (loaded != *f.video->mediaId) {
			if (++followerLagFrames_ > 3) why = "Fragment follower on " + loaded + ", canonical " + *f.video->mediaId;
		} else {
			followerLagFrames_ = 0;
		}
		fragmentFollowerFramesChecked_++;
	}
	if (why.empty() && active == kQuadrantId) {
		// Ordinary Shared Video consumer: the scene's observed media identity
		// must be the canonical selection (it observes a change at most one
		// frame late).
		const std::string observed = runtime_.quadrantSceneForTesting().observedMediaIdForTesting();
		if (observed != *f.video->mediaId) {
			if (++quadrantMediaLagFrames_ > 3) why = "Quadrant observes " + observed + ", canonical " + *f.video->mediaId;
		} else {
			quadrantMediaLagFrames_ = 0;
		}
		quadrantFramesChecked_++;
	}
	if (!why.empty()) {
		invariantFailures_++;
		if (invariantFailures_ <= 5) note("invariant violation: " + why);
	}
}

void ThreeSceneCircuitHarness::beginLeg() {
	const Leg& l = legs_[leg_];
	SceneManager& sm = runtime_.sceneManagerForTesting();
	countersAtRequest_.clear();
	for (const auto& id : ring_) countersAtRequest_[id] = *sm.countersForTesting(id);
	{ RT004_SCOPE("ThreeSceneHarness.readOutgoingFrame"); readScene(outgoingFrame_); }
	if (l.fromId == kBlueprintId) {
		blueprintExitSeed_ = runtime_.blueprintSceneForTesting().coreForTesting().composition().getCycleSeed();
		haveBlueprintExitSeed_ = true;
	}
	rejectAttempted_ = false;
	fadeInRejectAttempted_ = false;
	lastPhase_ = SceneTransitionPhase::Idle;
	framesInPhase_ = 0;
	maxFrameSeconds_ = 0.0f;
	{ RT004_SCOPE("ThreeSceneHarness.switchKey"); runtime_.keyPressed(l.key); } // real InputRouter -> NextScene / PreviousScene
	state_ = State::Transition;
}

void ThreeSceneCircuitHarness::onTransitionFrame() {
	const Leg& l = legs_[leg_];
	const HudFrameData& f = runtime_.currentHudFrameData();
	const SceneTransitionPhase phase = f.sceneManager.transitionPhase;
	if (phase != lastPhase_) {
		lastPhase_ = phase;
		framesInPhase_ = 0;
	}
	framesInPhase_++;
	SceneManager& sm = runtime_.sceneManagerForTesting();

	if (phase == SceneTransitionPhase::FadingOut && framesInPhase_ == 3 && !rejectAttempted_) {
		rejectAttempted_ = true;
		// Real keys first (same and opposite direction, plus a scene command);
		// the landing check below proves none of them took effect.
		runtime_.keyPressed(l.key);
		runtime_.keyPressed(l.key == ']' ? '[' : ']');
		runtime_.keyPressed('2');
		const auto r = sm.handleSceneSwitchCommand(RuntimeCommand::NextScene);
		const bool cmd = sm.dispatchSceneCommand(SceneCommand::NextMedia);
		if (r == sceneswitch::RequestResult::RejectedTransitionActive) rejectedSwitchInputs_++;
		else check("[" + legLabel() + "] switch request during FadingOut rejected", false);
		if (!cmd) rejectedSceneCommands_++;
		else check("[" + legLabel() + "] scene command during FadingOut rejected", false);
	}
	if (phase == SceneTransitionPhase::FadingIn && framesInPhase_ == 2 && !fadeInRejectAttempted_) {
		fadeInRejectAttempted_ = true;
		runtime_.keyPressed(l.key);
		const auto r = sm.handleSceneSwitchCommand(RuntimeCommand::PreviousScene);
		if (r == sceneswitch::RequestResult::RejectedTransitionActive) rejectedSwitchInputs_++;
		else check("[" + legLabel() + "] switch request during FadingIn rejected", false);
	}

	// Transition captures involving Blueprint (first forward and first backward circuit).
	const bool featured = fullRing_ ? (l.fromId == kFragmentId || l.toId == kFragmentId || l.fromId == kQuadrantId || l.toId == kQuadrantId)
		: (l.fromId == kBlueprintId || l.toId == kBlueprintId);
	if ((l.circuit == 1 || l.circuit == 2) && featured) {
		const std::string tag = "t_c" + ofToString(l.circuit) + "_" + shortName(l.fromId) + "_to_" + shortName(l.toId);
		if (phase == SceneTransitionPhase::FadingOut && framesInPhase_ == 6) captureWindow(tag + "_fadeout_mid");
		if (phase == SceneTransitionPhase::FadingIn && framesInPhase_ == 6) captureWindow(tag + "_fadein_mid");
	}

	if (phase == SceneTransitionPhase::Failed) {
		check("[" + legLabel() + "] transition did not fail", false, f.sceneManager.message.value_or(""));
		finish();
		return;
	}
	if (phase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId != l.fromId) {
		onLanded();
	}
}

void ThreeSceneCircuitHarness::onLanded() {
	const Leg& l = legs_[leg_];
	const HudFrameData& f = runtime_.currentHudFrameData();
	SceneManager& sm = runtime_.sceneManagerForTesting();
	std::string why;

	if (f.sceneManager.activeSceneId != l.toId) why = "landed on " + f.sceneManager.activeSceneId + ", expected " + l.toId;
	for (const auto& id : ring_) {
		const auto& before = countersAtRequest_[id];
		const auto& now = *sm.countersForTesting(id);
		const int expectActivations = (id == l.toId) ? 1 : 0;
		const int expectDeactivations = (id == l.fromId) ? 1 : 0;
		if (now.successfulActivations - before.successfulActivations != expectActivations
			|| now.activateCalls - before.activateCalls != expectActivations)
			why += " | " + shortName(id) + " activations +" + ofToString(now.successfulActivations - before.successfulActivations);
		if (now.deactivateCalls - before.deactivateCalls != expectDeactivations)
			why += " | " + shortName(id) + " deactivations +" + ofToString(now.deactivateCalls - before.deactivateCalls);
		if (now.capabilityQueries - before.capabilityQueries != expectActivations)
			why += " | " + shortName(id) + " capability queries +" + ofToString(now.capabilityQueries - before.capabilityQueries);
		if (now.setupCalls > 1) why += " | " + shortName(id) + " set up " + ofToString(now.setupCalls) + "x";
	}
	if (f.scene.sceneId != l.toId) why += " | status owned by " + f.scene.sceneId;
	if (f.effects.has_value() != (l.toId == kTemporalId)) why += " | effects presence wrong";
	if (hasCommand(f.capabilities, SceneCommand::Regenerate) != (l.toId == kBlueprintId)) why += " | capabilities wrong";

	ofPixels incoming;
	const float diff = readScene(incoming) ? meanAbsDiff(outgoingFrame_, incoming) : 0.0f;
	if (diff <= 1.0f) why += " | scene FBO unchanged from outgoing frame (diff " + ofToString(diff, 2) + ")";

	if (l.toId == kBlueprintId) {
		const BEComposition& c = runtime_.blueprintSceneForTesting().coreForTesting().composition();
		if (c.getCycleElapsedSeconds() > 2.0f) why += " | Blueprint cycle not fresh (" + ofToString(c.getCycleElapsedSeconds(), 2) + " s)";
		if (haveBlueprintExitSeed_ && c.getCycleSeed() == blueprintExitSeed_) why += " | Blueprint seed unchanged since exit";
	}
	if (l.toId == kFragmentId) {
		const auto& pool = runtime_.fragmentSceneForTesting().coreForTesting().pool();
		const int spawned = pool.getSpawnCount();
		for (const auto& frag : pool.getFragments()) {
			if (frag.getSpawnIndex() > spawned || frag.getSpawnIndex() <= 0) {
				why += " | Fragment pool holds a fragment not spawned this activation";
				break;
			}
		}
		if (spawned > static_cast<int>(framesInState_) + 1) why += " | Fragment spawn count " + ofToString(spawned) + " exceeds live frames";
	}
	if (l.fromId == kFragmentId) {
		const auto& buffer = runtime_.fragmentSceneForTesting().videoAdapterForTesting().buffer();
		if (buffer.getHistoryFrameCount() != 0 || buffer.hasMedia())
			why += " | Fragment follower not released while inactive (history " + ofToString(buffer.getHistoryFrameCount())
				+ " frames, decoder " + (buffer.hasMedia() ? "open" : "closed") + ")";
	}
	if (l.toId == kQuadrantId) {
		auto& qs = runtime_.quadrantSceneForTesting();
		if (qs.coreForTesting().expansionDirector().isActive()) why += " | Quadrant expansion carried over into a new activation";
		if (f.scene.semantic && f.scene.semantic->timing.generation <= lastQuadrantGeneration_)
			why += " | Quadrant generation did not advance";
		if (f.scene.semantic) lastQuadrantGeneration_ = f.scene.semantic->timing.generation;
	}
	const int gl = drainGlErrors();
	glErrors_ += gl;
	if (gl > 0) why += " | " + ofToString(gl) + " GL error(s)";

	check("[" + legLabel() + "] landed: one activation/capability query, owned state, fresh frame", why.empty(),
		why.empty() ? "diff " + ofToString(diff, 1) + ", max frame " + ofToString(maxFrameSeconds_ * 1000.0f, 1) + " ms" : why);
	legFrameMaxMs_.push_back(static_cast<size_t>(maxFrameSeconds_ * 1000.0f));

	state_ = State::Dwell;
	framesInState_ = 0;
	secondsInState_ = 0.0f;
	nextMediaPressed_ = nextMediaChecked_ = false;
	reseedPressed_ = reseedChecked_ = false;
	capturedThisLeg_ = false;
}

void ThreeSceneCircuitHarness::onDwellFrame() {
	const Leg& l = legs_[leg_];
	const HudFrameData& f = runtime_.currentHudFrameData();
	const bool mediaCircuit = (l.circuit % 2) == 0;
	const bool reseedCircuit = l.toId == kBlueprintId && (l.circuit % 3) == 1;
	const BEComposition& bp = runtime_.blueprintSceneForTesting().coreForTesting().composition();

	if (mediaCircuit && !nextMediaPressed_ && secondsInState_ >= 0.5f) {
		nextMediaPressed_ = true;
		mediaBeforeNext_ = mediaIdOf(f);
		runtime_.keyPressed('n'); // canonical NextMedia through the active scene
	}
	if (mediaCircuit && nextMediaPressed_ && !nextMediaChecked_ && secondsInState_ >= 2.5f) {
		nextMediaChecked_ = true;
		const std::string now = mediaIdOf(f);
		ofPixels px;
		const float luma = readScene(px) ? meanLuma(px) : 0.0f;
		const bool followerHasFrame = l.toId != kBlueprintId || runtime_.blueprintSceneForTesting().coreForTesting().hasVideoFrame();
		if (now != mediaBeforeNext_) mediaChanges_++;
		check("[" + legLabel() + "] NextMedia changed canonical media; scene follows", now != mediaBeforeNext_ && followerHasFrame,
			mediaBeforeNext_ + " -> " + now + ", scene mean " + ofToString(luma, 1));
	}
	if (reseedCircuit && !reseedPressed_ && secondsInState_ >= 1.0f) {
		reseedPressed_ = true;
		seedBeforeReseed_ = bp.getCycleSeed();
		runtime_.keyPressed('2'); // SceneCommand::Regenerate (RESEED)
	}
	if (reseedCircuit && reseedPressed_ && !reseedChecked_ && framesInState_ >= 3) {
		// framesInState_ counted from landing; give the command a few frames.
		if (secondsInState_ >= 1.2f) {
			reseedChecked_ = true;
			const bool ok = bp.getCycleSeed() != seedBeforeReseed_ && bp.getCycleElapsedSeconds() < 1.0f;
			if (ok) reseeds_++;
			check("[" + legLabel() + "] RESEED starts a new Blueprint cycle", ok);
		}
	}

	// Recognizable per-scene captures on the first visit of each scene.
	const bool firstVisit = l.circuit == 1;
	bool dwellDone = secondsInState_ >= kDwellSeconds;
	if (firstVisit && l.toId == kBlueprintId) {
		// Wait for placed fragments so the capture shows the artwork, not BLANK.
		// Two, as in ParitySoakHarness: the artwork holds at most MAX_FRAGMENTS
		// (4) slots, each for tens of seconds, so placement is deliberately slow.
		dwellDone = (bp.getFragments().size() >= 2 && secondsInState_ >= kDwellSeconds) || secondsInState_ > 30.0f;
	}
	if (firstVisit && l.toId == kQuadrantId && !quadrantReticlesShown_ && secondsInState_ >= 0.5f) {
		// Harness-only: put the labelled reticle layer on screen so the
		// first-visit capture shows HUD-layer bitmap text drawn inside the
		// runtime's scene FBO (orientation check).
		quadrantReticlesShown_ = true;
		runtime_.quadrantSceneForTesting().coreForTesting().hudLayer().showElementForTesting(3);
	}
	if (firstVisit && l.toId == kQuadrantId && !quadrantExpansionChecked_) {
		// Harness-only: force the artwork's own expansion gesture (the
		// standalone's 'E' test key; not a production command), then switch
		// away while it is still active — the next activation must come back
		// with expansion idle (landing check above).
		auto& core = runtime_.quadrantSceneForTesting().coreForTesting();
		if (dwellDone && !quadrantExpansionTriggered_ && capturedThisLeg_) {
			quadrantExpansionTriggered_ = true;
			expansionTriggerSeconds_ = secondsInState_;
			core.expansionDirector().trigger({ core.crosshair().getState().cx, core.crosshair().getState().cy });
		}
		if (quadrantExpansionTriggered_) {
			const auto state = core.expansionDirector().getState();
			const bool held = state == quadrantcrosshair::ExpansionState::HOLD;
			if (held || secondsInState_ - expansionTriggerSeconds_ > 25.0f) {
				quadrantExpansionChecked_ = true;
				const HudFrameData& hf = runtime_.currentHudFrameData();
				const SceneMetric* progress = nullptr;
				if (hf.scene.semantic) {
					for (const auto& m : hf.scene.semantic->metrics)
						if (m.metricId == "scene.quadrant.metric.expansion_progress") progress = &m;
				}
				check("Quadrant expansion reaches HOLD and publishes expanding + expansion_progress",
					held && hf.scene.semantic && hf.scene.semantic->state.primaryStateId == "scene.quadrant.state.expanding"
						&& progress && progress->value && *progress->value > 0.99f,
					hf.scene.semantic ? hf.scene.semantic->state.primaryStateId : std::string("(no semantic)"));
				captureWindow("s_quadrant_expanded_window");
				captureScene("s_quadrant_expanded_scene");
			}
		}
	}
	if (firstVisit && dwellDone && !capturedThisLeg_) {
		capturedThisLeg_ = true;
		ofPixels px;
		const float luma = readScene(px) ? meanLuma(px) : 0.0f;
		if (l.toId == kBlueprintId) {
			check("Blueprint renders placed fragments through the production scene FBO",
				bp.getFragments().size() >= 2 && luma > 4.0f,
				ofToString(bp.getFragments().size()) + " fragments, scene mean " + ofToString(luma, 1));
		} else if (l.toId == kFragmentId) {
			const int count = runtime_.fragmentSceneForTesting().coreForTesting().pool().getFragmentCount();
			check("Fragment renders a live trail through the production scene FBO", count >= 3 && luma > 4.0f,
				ofToString(count) + " fragments, scene mean " + ofToString(luma, 1));
		} else if (l.toId == kQuadrantId) {
			const HudFrameData& hf = runtime_.currentHudFrameData();
			int channels = 0;
			if (hf.scene.semantic) {
				for (const auto& m : hf.scene.semantic->metrics)
					if (m.metricId.rfind("scene.quadrant.metric.channel_", 0) == 0 && m.metricId.size() > 6
						&& m.metricId.compare(m.metricId.size() - 6, 6, "_state") == 0) channels++;
			}
			check("Quadrant renders through the production scene FBO and publishes four channel states",
				luma > 4.0f && channels == 4, "scene mean " + ofToString(luma, 1) + ", " + ofToString(channels) + " channel states");
		} else if (l.toId == kTemporalId) {
			check("Temporal renders a live (non-black) scene frame", luma > 4.0f, "scene mean " + ofToString(luma, 1));
		} else {
			note("Blob scene mean " + ofToString(luma, 1) + " (content-dependent)");
		}
		captureWindow("s_" + shortName(l.toId) + "_window");
		captureScene("s_" + shortName(l.toId) + "_scene");
	}
	if (l.circuit == kCircuits && dwellDone && !capturedThisLeg_) {
		capturedThisLeg_ = true;
		captureWindow("s_final_c" + ofToString(l.circuit) + "_" + shortName(l.toId) + "_window");
	}

	const bool waitingOnMedia = mediaCircuit && !nextMediaChecked_;
	const bool waitingOnReseed = reseedCircuit && !reseedChecked_;
	const bool waitingOnExpansion = firstVisit && l.toId == kQuadrantId && fullRing_ && !quadrantExpansionChecked_;
	if (dwellDone && !waitingOnMedia && !waitingOnReseed && !waitingOnExpansion && framesInState_ >= 2) {
		if (l.index == static_cast<int>(ring_.size()) - 1) endOfCircuit();
		leg_++;
		if (leg_ >= legs_.size()) {
			finish();
			return;
		}
		beginLeg();
		framesInState_ = 0;
		secondsInState_ = 0.0f;
	}
}

void ThreeSceneCircuitHarness::endOfCircuit() {
	const Leg& l = legs_[leg_];
	const double rss = residentMiB();
	rssMiBPerCircuit_.push_back(rss);
	fboAllocsPerCircuit_.push_back(runtime_.presentationCountersForTesting().sceneFboAllocations);
	fboTexPerCircuit_.push_back(runtime_.sceneFboTextureIdForTesting());
	note("circuit " + ofToString(l.circuit) + (l.key == ']' ? " (forward)" : " (backward)") + " complete: RSS "
		+ ofToString(rss, 1) + " MiB, scene FBO allocations " + ofToString(fboAllocsPerCircuit_.back())
		+ ", scene FBO texture " + ofToString(fboTexPerCircuit_.back()));
}

void ThreeSceneCircuitHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	framesInState_++;
	secondsInState_ += dt;
	// Frame-time evidence excludes the frame after a harness PNG capture: the
	// grab + encode runs inside this process's frame and is not runtime cost.
	if (excludeNextDt_) captureFramesExcluded_++;
	else maxFrameSeconds_ = std::max(maxFrameSeconds_, dt);
	excludeNextDt_ = false;
	capturedThisFrame_ = false;
	checkIdleInvariants();

	switch (state_) {
		case State::Startup: {
			const HudFrameData& f = runtime_.currentHudFrameData();
			if (f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == kBlobId
				&& f.scene.semantic.has_value() && secondsInState_ >= 2.0f) {
				drainGlErrors(); // startup noise from window/context creation is not a scene-switch error
				note("startup in Blob; RSS " + ofToString(residentMiB(), 1) + " MiB");
				beginLeg();
				framesInState_ = 0;
				secondsInState_ = 0.0f;
			}
			break;
		}
		case State::Transition: onTransitionFrame(); break;
		case State::Dwell: onDwellFrame(); break;
		case State::Done: return;
	}
	excludeNextDt_ = capturedThisFrame_;
	if (!done_ && secondsInState_ > kStateTimeoutSeconds) {
		check("state progress (" + (leg_ < legs_.size() ? legLabel() : std::string("startup")) + ")", false, "timed out");
		finish();
	}
}

void ThreeSceneCircuitHarness::finish() {
	if (done_) return;
	done_ = true;
	state_ = State::Done;
	SceneManager& sm = runtime_.sceneManagerForTesting();
	const std::string active = sm.status().activeSceneId;

	const bool allLegs = leg_ >= legs_.size();
	check("all " + ofToString(legs_.size()) + " switches (" + ofToString(kCircuits) + " circuits, both directions) completed",
		allLegs, ofToString(std::min(leg_, legs_.size())) + " done");
	if (allLegs) check("run returns to Blob after the final circuit", active == kBlobId, active);

	std::vector<int> visits(ring_.size(), 0);
	if (!visits.empty()) visits[0] = 1; // Blob startup activation
	for (size_t i = 0; i < std::min(leg_, legs_.size()); ++i) {
		for (size_t s = 0; s < ring_.size(); ++s) {
			if (legs_[i].toId == ring_[s]) visits[s]++;
		}
	}
	long long totalUpdates = 0;
	for (size_t s = 0; s < ring_.size(); ++s) {
		const std::string& id = ring_[static_cast<size_t>(s)];
		const auto& c = *sm.countersForTesting(id);
		totalUpdates += c.updateCalls;
		const int expectDeact = visits[s] - (id == active ? 1 : 0);
		check(shortName(id) + " lifecycle: one setup, activations == visits, capabilities once per activation, "
				"one status pull + draw per live update",
			c.setupCalls == 1 && c.successfulActivations == visits[s] && c.activateCalls == visits[s]
				&& c.deactivateCalls == expectDeact && c.capabilityQueries == c.successfulActivations
				&& c.statusPulls == c.updateCalls && c.drawCalls == c.updateCalls && c.shutdownCalls == 0,
			"setup " + ofToString(c.setupCalls) + ", activations " + ofToString(c.successfulActivations) + "/" + ofToString(visits[s])
				+ ", deactivations " + ofToString(c.deactivateCalls) + ", caps " + ofToString(c.capabilityQueries)
				+ ", updates " + ofToString(c.updateCalls) + ", status " + ofToString(c.statusPulls)
				+ ", draws " + ofToString(c.drawCalls));
	}

	const auto& pc = runtime_.presentationCountersForTesting();
	check("no inactive scene updates (scene updates == live scene frames)",
		totalUpdates == static_cast<long long>(pc.liveSceneFrames),
		ofToString(totalUpdates) + " updates, " + ofToString(pc.liveSceneFrames) + " live frames");
	if (fullRing_) {
		check("Fragment follower always on the canonical selection", fragmentFollowerFramesChecked_ > 0,
			ofToString(fragmentFollowerFramesChecked_) + " Fragment Idle frames checked");
		check("Quadrant always observes the canonical media selection", quadrantFramesChecked_ > 0,
			ofToString(quadrantFramesChecked_) + " Quadrant Idle frames checked");
		check("Quadrant expansion exercised", quadrantExpansionChecked_);
	}
	check("one HudFrameData assembly per presented frame", pc.hudFrameDataAssemblies == pc.hudDraws,
		ofToString(pc.hudFrameDataAssemblies) + " assemblies, " + ofToString(pc.hudDraws) + " HUD draws, "
			+ ofToString(pc.liveSceneFrames) + " live / " + ofToString(pc.staticSceneFrames) + " static scene frames");
	check("no stale status/effects/capabilities/semantics on any frame", invariantFailures_ == 0,
		ofToString(idleFramesChecked_) + " Idle frames, " + ofToString(invariantFailures_) + " violations");
	check("every mid-transition switch request / scene command rejected", allLegs
			&& rejectedSwitchInputs_ == static_cast<int>(legs_.size()) * 2 && rejectedSceneCommands_ == static_cast<int>(legs_.size()),
		ofToString(rejectedSwitchInputs_) + " switch, " + ofToString(rejectedSceneCommands_) + " scene-command rejections");
	note("canonical media changes observed: " + ofToString(mediaChanges_) + "; Blueprint reseeds: " + ofToString(reseeds_));
	const int gl = drainGlErrors();
	glErrors_ += gl;
	check("no GL errors at any landing or at end of run", glErrors_ == 0, ofToString(glErrors_));
	check("no scissor/FBO state left set after any presented frame", glStateLeaks_ == 0 && glStateFramesChecked_ > 0,
		ofToString(glStateFramesChecked_) + " frames, " + ofToString(glStateLeaks_) + " leaks");

	if (rssMiBPerCircuit_.size() >= 3) {
		const double base = rssMiBPerCircuit_[1];
		const double last = rssMiBPerCircuit_.back();
		const double peak = *std::max_element(rssMiBPerCircuit_.begin() + 1, rssMiBPerCircuit_.end());
		std::string series;
		for (double r : rssMiBPerCircuit_) series += ofToString(r, 1) + " ";
		check("RSS bounded across circuits (end of c2 -> end of run within " + ofToString(kRssGrowthBudgetMiB, 0) + " MiB)",
			last - base <= kRssGrowthBudgetMiB,
			"c2 " + ofToString(base, 1) + " -> last " + ofToString(last, 1) + " MiB (peak " + ofToString(peak, 1) + "); series: " + series);
		const bool fboStable = std::all_of(fboAllocsPerCircuit_.begin(), fboAllocsPerCircuit_.end(),
			[&](uint64_t v) { return v == fboAllocsPerCircuit_.front(); })
			&& std::all_of(fboTexPerCircuit_.begin(), fboTexPerCircuit_.end(),
				[&](unsigned int v) { return v == fboTexPerCircuit_.front(); });
		check("runtime-owned scene FBO not reallocated per circuit", fboStable,
			ofToString(fboAllocsPerCircuit_.front()) + " allocations, texture " + ofToString(fboTexPerCircuit_.front()));
	}
	if (!legFrameMaxMs_.empty()) {
		std::string s;
		for (size_t v : legFrameMaxMs_) s += ofToString(v) + " ";
		note("max frame ms per switch (request -> landing; " + ofToString(captureFramesExcluded_)
			+ " post-capture frames excluded): " + s);
	}

	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(captureDir_ + "/results.txt", true));
	for (const auto& line : lines_) out << line << "\n";
	ofExit(failures_ == 0 ? 0 : 1);
}
