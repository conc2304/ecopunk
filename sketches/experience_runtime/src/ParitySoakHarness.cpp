#include "ParitySoakHarness.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;
const std::string kBlueprintId = BlueprintProductionScene::kSceneId;
constexpr const char* kCaptureDir = "captures/parity-soak";
constexpr float kStepTimeoutSeconds = 45.0f;
// (A) Bound on the incoming scene publishing semantic data (needs real media)
// after a switch request. Observed latency is logged per leg; Temporal's
// worst observed reactivation includes a ~2.3 s frame. Kept well inside the
// 45 s step timeout so a readiness failure is reported as itself.
constexpr float kMediaReadyTimeoutSeconds = 20.0f;
// (B) Attempts at a timer-bounded Blueprint cycle (each Regenerate draws a new
// random CycleMode, 50/50): exhausting 10 is a 1-in-1024 legitimate outcome,
// reported as such rather than passed.
constexpr int kBoundedCycleAttempts = 10;
constexpr float kBoundedSampleSeconds = 1.0f;   // advancing progress required for this long
constexpr float kUnboundedCycleSeconds = 1.0f;  // a cycle showing no progress this long is not timer-bounded

bool hasCommand(const SceneCapabilities& caps, SceneCommand command) {
	return std::any_of(caps.commands.begin(), caps.commands.end(),
		[&](const SceneCommandDescriptor& d) { return d.command == command; });
}

std::string semanticPrefixFor(const std::string& sceneId) {
	if (sceneId == kBlobId) return "scene.blob.";
	if (sceneId == kTemporalId) return "scene.temporal.";
	if (sceneId == kBlueprintId) return "scene.blueprint.";
	return "?";
}

std::string mediaIdOf(const HudFrameData& f) {
	return (f.video && f.video->mediaId) ? *f.video->mediaId : std::string("(none)");
}

const SceneMetric* findMetric(const SceneSemanticData& d, const std::string& id) {
	for (const auto& m : d.metrics) {
		if (m.metricId == id) return &m;
	}
	return nullptr;
}

} // namespace

bool ParitySoakHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_PARITY_HARNESS") != nullptr;
}

ParitySoakHarness::ParitySoakHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	const auto ids = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
	check("default roster [Blob, Temporal, Blueprint]",
		ids.size() == 3 && ids[0] == kBlobId && ids[1] == kTemporalId && ids[2] == kBlueprintId);
	buildScript();
}

void ParitySoakHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void ParitySoakHarness::note(const std::string& line) {
	ofLogNotice("ParitySoakHarness") << line;
	lines_.push_back(line);
}

void ParitySoakHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png");
}

// Mean RGB over a sparse grid of the runtime-owned scene FBO — a black/empty
// scene target (broken nested-FBO restore, stale clear) reads ~0.
bool ParitySoakHarness::sceneFrameNotBlack(float* meanOut) {
	ofPixels px;
	if (!runtime_.readSceneFboPixelsForTesting(px) || px.getWidth() == 0) return false;
	double sum = 0.0;
	int n = 0;
	for (size_t y = 8; y < px.getHeight(); y += 16) {
		for (size_t x = 8; x < px.getWidth(); x += 16) {
			const ofColor c = px.getColor(x, y);
			sum += (c.r + c.g + c.b) / 3.0;
			n++;
		}
	}
	const float mean = n > 0 ? static_cast<float>(sum / n) : 0.0f;
	if (meanOut) *meanOut = mean;
	return mean > 4.0f;
}

bool ParitySoakHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId
		&& f.scene.sceneId == sceneId;
}

void ParitySoakHarness::checkIdleInvariants() {
	const HudFrameData& f = runtime_.currentHudFrameData();
	if (f.sceneManager.transitionPhase != SceneTransitionPhase::Idle) return;
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
	if (active == kBlueprintId && f.scene.semantic) {
		const auto progress = runtime_.blueprintSceneForTesting().coreForTesting().composition().getPhaseProgress();
		const SceneMetric* metric = findMetric(*f.scene.semantic, "scene.blueprint.metric.phase_progress");
		if (progress) {
			blueprintBoundedFrames_++;
			blueprintMaxProgress_ = std::max(blueprintMaxProgress_, *progress);
		}
		// Status is captured before the harness reads the composition, so a
		// value may lag by one frame; presence must still agree.
		if (progress.has_value() != (metric != nullptr)) blueprintProgressMismatches_++;
	}
	if (!why.empty()) {
		invariantFailures_++;
		if (invariantFailures_ <= 5) note("invariant violation: " + why);
	}
}

void ParitySoakHarness::addSwitchLeg(const std::string& sceneId, const std::string& label) {
	const bool pressSwitch = !script_.empty();
	if (pressSwitch) {
		script_.push_back({label + " switch", [this] { return framesInStep_ >= 2; }, [this] {
			// Remember what the outgoing scene's clock read, to tell a fresh
			// start from carry-over when it is visited again.
			const HudFrameData& out = runtime_.currentHudFrameData();
			if (out.scene.semantic) {
				exitActiveSeconds_[out.scene.sceneId] = out.scene.semantic->timing.activeSeconds;
				exitGeneration_[out.scene.sceneId] = out.scene.semantic->timing.generation;
			}
			maxFrameSeconds_ = 0.0f;
			switchRequestWall_ = ofGetElapsedTimef();
			runtime_.keyPressed(']'); // real InputRouter -> RuntimeCommand::NextScene
		}});
	}
	// Bounded wait for media readiness (semantic data needs a real frame):
	// a timeout is reported as its own failure, never skipped.
	script_.push_back({label + " live", [this, sceneId] {
			const bool ready = idleOn(sceneId) && framesInStep_ >= 2 && runtime_.currentHudFrameData().scene.semantic.has_value();
			// The bound runs from the switch request whatever the phase, so a
			// scene stuck in Loading (e.g. a blocked media open) is reported as
			// a readiness failure, not a generic step timeout.
			return ready || ofGetElapsedTimef() - switchRequestWall_ > kMediaReadyTimeoutSeconds;
		},
		[this, sceneId, label, pressSwitch] {
			const HudFrameData& f = runtime_.currentHudFrameData();
			const float sinceRequest = ofGetElapsedTimef() - switchRequestWall_;
			if (!idleOn(sceneId)) {
				const SceneTransitionPhase phase = f.sceneManager.transitionPhase;
				check("[" + label + "] scene active (transition complete) within " + ofToString(kMediaReadyTimeoutSeconds, 0) + " s",
					false, "still " + std::string(phase == SceneTransitionPhase::Loading ? "Loading" : phase == SceneTransitionPhase::FadingOut
						? "FadingOut" : phase == SceneTransitionPhase::FadingIn ? "FadingIn" : phase == SceneTransitionPhase::Failed
						? "Failed" : "Idle (other scene)") + " after " + ofToString(sinceRequest, 1) + " s; max frame "
						+ ofToString(maxFrameSeconds_ * 1000.0f, 0) + " ms");
			} else if (!f.scene.semantic) {
				check("[" + label + "] media ready (semantic data published) within " + ofToString(kMediaReadyTimeoutSeconds, 0) + " s",
					false, "not ready after " + ofToString(sinceRequest, 1) + " s");
			} else {
				// Fresh = the clock started at THIS activation: it cannot exceed the
				// wall time since the switch request (plus the one frame whose dt
				// may begin before the request), and the activation generation
				// advanced. Carry-over would add the previous visit's time.
				const float active = f.scene.semantic->timing.activeSeconds;
				const float bound = sinceRequest + maxFrameSeconds_ + 0.05f;
				const auto exitIt = exitActiveSeconds_.find(sceneId);
				const auto genIt = exitGeneration_.find(sceneId);
				const bool newEpoch = genIt == exitGeneration_.end() || f.scene.semantic->timing.generation > genIt->second;
				std::string detail = ofToString(active, 2) + " s <= " + ofToString(bound, 2) + " s since request; media ready after "
					+ ofToString(sinceRequest, 2) + " s";
				if (exitIt != exitActiveSeconds_.end()) {
					detail += "; previous visit left at " + ofToString(exitIt->second, 2) + " s, generation "
						+ ofToString(genIt->second) + " -> " + ofToString(f.scene.semantic->timing.generation);
				}
				check("[" + label + "] semantic time is activation-relative (fresh)", active >= 0.0f && active <= bound && newEpoch, detail);
			}
			check("[" + label + "] capabilities/effects belong to " + sceneId,
				f.effects.has_value() == (sceneId == kTemporalId)
				&& hasCommand(f.capabilities, SceneCommand::Regenerate) == (sceneId == kBlueprintId));
			if (pressSwitch) {
				note("[" + label + "] max frame since switch request: " + ofToString(maxFrameSeconds_ * 1000.0f, 1) + " ms");
			}
		}});
}

void ParitySoakHarness::addTemporalPatternTour() {
	auto& rt = runtime_;
	auto composition = [&rt]() -> TFComposition& { return rt.temporalSceneForTesting().coreForTesting().composition(); };
	for (int i = 0; i < 9; ++i) {
		script_.push_back({"pattern force " + ofToString(i),
			[this, composition] { return composition().getPhase() == TFComposition::CyclePhase::RUNNING && framesInStep_ >= 10; },
			[this, composition] {
				patternBeforeForce_ = static_cast<int>(composition().getActivePatternType());
				composition().forceNextPattern();
			}});
		// Sampled two frames after forcing: evolution can drift the transition
		// duration below 1 s, so a later sample could miss it.
		script_.push_back({"pattern transition " + ofToString(i), [this] { return framesInStep_ >= 2; },
			[this, composition, i] {
				// Evolution can make a transition a hard cut or ~0.1 s long, so
				// "still transitioning" is reported, not asserted.
				const bool inTransition = composition().getPhase() == TFComposition::CyclePhase::PATTERN_TRANSITION;
				const bool switched = static_cast<int>(composition().getActivePatternType()) != patternBeforeForce_;
				float mean = 0.0f;
				const bool notBlack = sceneFrameNotBlack(&mean);
				check("Temporal pattern " + tfPatternTypeName(composition().getActivePatternType())
						+ ": switched through TFComposition's transition, scene FBO not black",
					switched && notBlack, "mean " + ofToString(mean, 1) + (inTransition ? ", transition in progress" : ", transition already complete"));
				if (i == 0) capture("p03_temporal_fragment_transition_mid");
			}});
		script_.push_back({"pattern settled " + ofToString(i),
			[this, composition] { return composition().getPhase() == TFComposition::CyclePhase::RUNNING || secondsInStep_ > 8.0f; },
			[this, composition] {
				const TFPatternType type = composition().getActivePatternType();
				visitedPatterns_.insert(static_cast<int>(type));
				capture("p04_temporal_pattern_" + ofToString(visitedPatterns_.size()) + "_" + tfPatternTypeName(type));
			}});
	}
	script_.push_back({"pattern tour done", [this] { return framesInStep_ >= 1; }, [this] {
		check("all nine Temporal patterns reached and rendered through a real transition",
			visitedPatterns_.size() == 9, ofToString(visitedPatterns_.size()) + " distinct");
	}});
}

void ParitySoakHarness::buildScript() {
	auto& rt = runtime_;
	auto blueprint = [&rt]() -> const BEComposition& { return rt.blueprintSceneForTesting().coreForTesting().composition(); };

	// -- Leg 1: Blob ---------------------------------------------------------
	addSwitchLeg(kBlobId, "1_blob");
	script_.push_back({"blob media", [this] { return framesInStep_ >= 60; }, [this] {
		// Content-dependent: some clips open on (or pass through) black.
		float mean = 0.0f;
		const bool bright = sceneFrameNotBlack(&mean);
		note("Blob scene frame mean " + ofToString(mean, 1) + (bright ? "" : " (dark clip content)"));
		check("Blob renders a live scene frame", runtime_.currentHudFrameData().scene.semantic.has_value());
		capture("p01_blob");
		mediaBefore_ = mediaIdOf(runtime_.currentHudFrameData());
		runtime_.keyPressed('n'); // canonical NextMedia
	}});
	script_.push_back({"blob media changed", [this] { return secondsInStep_ >= 2.0f; }, [this] {
		const std::string now = mediaIdOf(runtime_.currentHudFrameData());
		check("NextMedia ('n') changed the canonical selection", now != mediaBefore_, mediaBefore_ + " -> " + now);
	}});

	// -- Leg 2: Temporal -------------------------------------------------------
	addSwitchLeg(kTemporalId, "2_temporal");
	script_.push_back({"temporal params", [this] { return framesInStep_ >= 30; }, [this] {
		const TFParameterPanel& p = runtime_.temporalSceneForTesting().coreForTesting().parameters();
		const auto bsp = p.getBSPParams();
		const auto blob = p.getBlobGridParams();
		note("Temporal live params: transitionDuration=" + ofToString(p.getTransitionDuration(), 2) +
			" weights=" + ofToString(p.getHardCutWeight(), 1) + "/" + ofToString(p.getCrossfadeWeight(), 1) + "/" +
			ofToString(p.getErosionWeight(), 1) + " bspReshuffle=" + ofToString(bsp.geometryReshuffleRate, 2) +
			" blobGridRes=" + ofToString(blob.gridResolution) + " blobGridDrift=" + ofToString(blob.driftSpeed, 3));
		check("Temporal runs the tuned TFSettings parameter model, not Params{} prototype defaults",
			p.getTransitionDuration() > 2.0f && blob.gridResolution <= 13 && blob.driftSpeed < 0.5f);
		check("Temporal effects snapshot present (canonical TFEffectPicker producer)",
			runtime_.currentHudFrameData().effects.has_value());
		capture("p02_temporal");
	}});
	addTemporalPatternTour();

	// -- Leg 3: Blueprint ---------------------------------------------------------
	addSwitchLeg(kBlueprintId, "3_blueprint");
	// (B) Each cycle draws its CycleMode at random: GHOST_LAYERS times its
	// PLACEMENT phase (progress published), PERPETUAL does not (progress
	// absent — also correct). Observe the cycle actually running; if it is
	// not timer-bounded, start a fresh random cycle with the product's own
	// Regenerate, up to kBoundedCycleAttempts. Require >= 1 s of strictly
	// advancing in-range progress that the HUD metric mirrors.
	script_.push_back({"blueprint timer-bounded phase", [this, blueprint] {
			const auto progress = blueprint().getPhaseProgress();
			const float dt = static_cast<float>(ofGetLastFrameTime());
			if (progress) {
				const HudFrameData& f = runtime_.currentHudFrameData();
				const SceneMetric* metric = f.scene.semantic ? findMetric(*f.scene.semantic, "scene.blueprint.metric.phase_progress") : nullptr;
				// The status was captured before this read; allow one frame of lag.
				if (!metric || !metric->value || std::abs(*metric->value - *progress) > 0.05f) boundedMetricMismatches_++;
				if (boundedFirstProgress_ < 0.0f) boundedFirstProgress_ = *progress;
				else if (*progress < boundedLastProgress_) boundedProgressMonotonic_ = false;
				boundedLastProgress_ = *progress;
				boundedSampleSeconds_ += dt;
				return boundedSampleSeconds_ >= kBoundedSampleSeconds && boundedLastProgress_ > boundedFirstProgress_;
			}
			boundedFirstProgress_ = boundedLastProgress_ = -1.0f;
			boundedSampleSeconds_ = 0.0f;
			unboundedCycleSeconds_ += dt;
			if (unboundedCycleSeconds_ >= kUnboundedCycleSeconds) {
				if (boundedAttempts_ >= kBoundedCycleAttempts) return true; // reported in act
				boundedAttempts_++;
				unboundedCycleSeconds_ = 0.0f;
				note("Blueprint cycle not timer-bounded (mode " + std::string(blueprint().getCurrentMode() == CycleMode::PERPETUAL
					? "PERPETUAL" : "GHOST_LAYERS") + "); Regenerate for a new random cycle, attempt " + ofToString(boundedAttempts_));
				runtime_.keyPressed('2');
			}
			return false;
		},
		[this, blueprint] {
			const bool observed = boundedSampleSeconds_ >= kBoundedSampleSeconds;
			if (!observed) {
				check("Blueprint timer-bounded phase observed", false,
					"legitimate randomized outcome: no GHOST_LAYERS cycle in " + ofToString(kBoundedCycleAttempts) + " Regenerates");
			} else {
				check("Blueprint phase_progress advances in [0,1] under the selected phase duration, mirrored by the HUD metric",
					boundedProgressMonotonic_ && boundedFirstProgress_ >= 0.0f && boundedLastProgress_ <= 1.0f
						&& boundedLastProgress_ > boundedFirstProgress_ && boundedMetricMismatches_ <= 1,
					ofToString(boundedFirstProgress_, 3) + " -> " + ofToString(boundedLastProgress_, 3) + " over "
						+ ofToString(boundedSampleSeconds_, 2) + " s; " + ofToString(boundedAttempts_) + " Regenerate(s); "
						+ ofToString(boundedMetricMismatches_) + " metric mismatches");
			}
			capture("p05_blueprint_progress");
		}});
	// (C) Regenerate runs synchronously inside the key path
	// (SceneManager::dispatchSceneCommand -> BlueprintSceneCore::restartCycle
	// -> CompositionBase::startCycle), so the reset is checked immediately,
	// before any update could advance it or end a cycle naturally.
	script_.push_back({"blueprint reseed", [this] { return framesInStep_ >= 2; }, [this, blueprint] {
		seedBefore_ = blueprint().getCycleSeed();
		cycleElapsedBeforeReseed_ = blueprint().getCycleElapsedSeconds();
		runtime_.keyPressed('2'); // real InputRouter -> SceneCommand::Regenerate
		seedAfterReseed_ = blueprint().getCycleSeed();
		check("Blueprint RESEED starts a new composition cycle (new seed, cycle clock reset)",
			seedAfterReseed_ != seedBefore_ && blueprint().getCycleElapsedSeconds() == 0.0f && cycleElapsedBeforeReseed_ > 0.0f,
			"cycle clock " + ofToString(cycleElapsedBeforeReseed_, 2) + " s -> " + ofToString(blueprint().getCycleElapsedSeconds(), 2) + " s");
	}});
	script_.push_back({"blueprint reseeded", [this] { return framesInStep_ >= 3; }, [this, blueprint] {
		check("Blueprint cycle runs on from the reset (same seed, clock advancing)",
			blueprint().getCycleSeed() == seedAfterReseed_ && blueprint().getCycleElapsedSeconds() > 0.0f,
			ofToString(blueprint().getCycleElapsedSeconds(), 3) + " s");
		runtime_.keyPressed(OF_KEY_RIGHT); // HUD focus engages
	}});
	script_.push_back({"blueprint artwork", [this, blueprint] { return blueprint().getFragments().size() >= 2 || secondsInStep_ > 25.0f; },
		[this, blueprint] {
			const auto& focus = runtime_.hudCompositorBridgeForTesting().focus().focused();
			check("HUD focus engaged on a visible control", focus.has_value(), focus.value_or("none"));
			check("Blueprint renders placed fragments", blueprint().getFragments().size() >= 2 && sceneFrameNotBlack());
			capture("p06_blueprint_after_reseed");
			blueprintExitSeed_ = blueprint().getCycleSeed();
		}});

	// -- Legs 4-6: second pass ------------------------------------------------------
	addSwitchLeg(kBlobId, "4_blob_again");
	script_.push_back({"blob again", [this] { return secondsInStep_ >= 4.0f; }, [this] {
		check("Blob renders after Blueprint (no GL/FBO carry-over)", sceneFrameNotBlack());
		capture("p07_blob_again");
	}});
	addSwitchLeg(kTemporalId, "5_temporal_again");
	script_.push_back({"temporal again", [this] { return secondsInStep_ >= 5.0f; }, [this] {
		check("Temporal renders after reactivation", sceneFrameNotBlack());
		capture("p08_temporal_again");
	}});
	addSwitchLeg(kBlueprintId, "6_blueprint_again");
	script_.push_back({"blueprint again", [this] { return framesInStep_ >= 2; }, [this, blueprint] {
		// Same media-latency-independent bound as (A): the cycle started at
		// this activation, so its clock cannot exceed the time since the request.
		const float bound = ofGetElapsedTimef() - switchRequestWall_ + maxFrameSeconds_ + 0.05f;
		check("Blueprint reactivation starts a fresh cycle (no stale imagery)",
			blueprint().getCycleSeed() != blueprintExitSeed_ && blueprint().getCycleElapsedSeconds() <= bound,
			ofToString(blueprint().getCycleElapsedSeconds(), 2) + " s <= " + ofToString(bound, 2) + " s since request");
	}});
	script_.push_back({"blueprint again settled", [this] { return secondsInStep_ >= 12.0f; }, [this] {
		check("Blueprint renders after reactivation", sceneFrameNotBlack());
		capture("p09_blueprint_again");
	}});
}

void ParitySoakHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	framesInStep_++;
	secondsInStep_ += dt;
	maxFrameSeconds_ = std::max(maxFrameSeconds_, dt);
	checkIdleInvariants();

	Step& s = script_[stepIndex_];
	if (s.ready()) {
		s.act();
		if (done_) return;
		stepIndex_++;
		framesInStep_ = 0;
		secondsInStep_ = 0.0f;
		if (stepIndex_ >= script_.size()) finish();
	} else if (secondsInStep_ > kStepTimeoutSeconds) {
		check("step '" + s.label + "' reached", false, "timed out");
		finish();
	}
}

void ParitySoakHarness::finish() {
	check("no stale status/effects/capabilities/semantics on any Idle frame", invariantFailures_ == 0,
		ofToString(idleFramesChecked_) + " frames, " + ofToString(invariantFailures_) + " violations");
	check("Blueprint phase_progress observed while timer-bounded, and HUD metric presence always matched",
		blueprintBoundedFrames_ > 0 && blueprintProgressMismatches_ <= 2,
		ofToString(blueprintBoundedFrames_) + " bounded frames, max " + ofToString(blueprintMaxProgress_, 2) + ", " +
		ofToString(blueprintProgressMismatches_) + " presence mismatches");
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
