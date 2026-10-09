#include "BlueprintRosterHarness.h"

#include "ofAppRunner.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

const std::string kBlobId = BlobProductionScene::kSceneId;
const std::string kTemporalId = TemporalProductionScene::kSceneId;
const std::string kBlueprintId = BlueprintProductionScene::kSceneId;
constexpr const char* kCaptureDir = "captures/blueprint-roster";
constexpr float kSwitchTimeoutSeconds = 30.0f;

const char* phaseName(CompositionBase::CyclePhase p) {
	static const char* names[] = { "BLANK", "PLACEMENT", "DENSITY", "DISSOLVE", "RESET_HOLD" };
	return names[static_cast<int>(p)];
}

std::string semanticPrefixFor(const std::string& sceneId) {
	if (sceneId == kBlobId) return "scene.blob.";
	if (sceneId == kTemporalId) return "scene.temporal.";
	if (sceneId == kBlueprintId) return "scene.blueprint.";
	return "?";
}

bool hasCommand(const SceneCapabilities& caps, SceneCommand command) {
	return std::any_of(caps.commands.begin(), caps.commands.end(),
		[&](const SceneCommandDescriptor& d) { return d.command == command; });
}

std::string mediaIdOf(const HudFrameData& f) {
	return (f.video && f.video->mediaId) ? *f.video->mediaId : std::string("(none)");
}

} // namespace

bool BlueprintRosterHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_BLUEPRINT_HARNESS") != nullptr;
}

BlueprintRosterHarness::BlueprintRosterHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	legs_ = {
		{kBlueprintId, "1_blueprint"},
		{kBlobId, "2_blob"},
		{kTemporalId, "3_temporal"},
		{kBlueprintId, "4_blueprint_reactivated"},
	};
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	const auto ids = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
	check("registry is the full roster [Blob, Temporal, Blueprint]",
		ids.size() == 3 && ids[0] == kBlobId && ids[1] == kTemporalId && ids[2] == kBlueprintId,
		ofToString(ids.size()) + " entries");
}

void BlueprintRosterHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void BlueprintRosterHarness::note(const std::string& line) {
	ofLogNotice("BlueprintRosterHarness") << line;
	lines_.push_back(line);
}

void BlueprintRosterHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + "_runtime.png");
	runtime_.saveSceneFrameCaptureForTesting(ofToDataPath(std::string(kCaptureDir) + "/" + name + "_scene_only.png", true));
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + "_{runtime,scene_only}.png");
}

void BlueprintRosterHarness::checkIdleFrameInvariants() {
	const HudFrameData& f = runtime_.currentHudFrameData();
	const std::string& active = f.sceneManager.activeSceneId;
	idleFramesChecked_++;

	bool ok = true;
	std::string why;
	auto fail = [&](const std::string& reason) { ok = false; if (why.empty()) why = reason; };

	if (f.scene.sceneId != active) fail("status sceneId " + f.scene.sceneId + " != active " + active);
	if (f.effects.has_value() != (active == kTemporalId)) fail("effects presence does not match owner " + active);
	if (hasCommand(f.capabilities, SceneCommand::Regenerate) != (active == kBlueprintId)) fail("stale/foreign Regenerate capability");
	if (!hasCommand(f.capabilities, SceneCommand::NextMedia)) fail("NextMedia capability missing");
	if (f.scene.semantic && f.scene.semantic->state.primaryStateId.rfind(semanticPrefixFor(active), 0) != 0) {
		fail("semantic state " + f.scene.semantic->state.primaryStateId + " not owned by " + active);
	}
	if (!f.video || !f.video->mediaId) fail("canonical video status missing");
	if (!ok) {
		invariantFailures_++;
		if (invariantFailures_ <= 5) note("invariant violation [" + legs_[leg_].label + "]: " + why);
	}
}

void BlueprintRosterHarness::onFirstLiveFrameOfLeg() {
	const Leg& leg = legs_[leg_];
	const HudFrameData& f = runtime_.currentHudFrameData();
	check("[" + leg.label + "] switch landed on " + leg.expectedSceneId,
		f.sceneManager.activeSceneId == leg.expectedSceneId && f.scene.sceneId == leg.expectedSceneId);
	if (leg.expectedSceneId == kBlueprintId) {
		// A fresh cycle (new seed, cycle clock near zero) — fragments present
		// at this point were placed by the NEW cycle, never carried over.
		const BEComposition& comp = runtime_.blueprintSceneForTesting().coreForTesting().composition();
		check("[" + leg.label + "] fresh composition cycle on activation (no stale imagery)",
			comp.getCycleSeed() != seedAtLastBlueprintExit_ && comp.getCycleElapsedSeconds() < 1.0f,
			std::string("phase ") + phaseName(comp.getPhase()) + ", cycle t=" + ofToString(comp.getCycleElapsedSeconds(), 2) +
			" s, seed " + ofToString(seedAtLastBlueprintExit_) + " -> " + ofToString(comp.getCycleSeed()) +
			", " + ofToString(comp.getFragments().size()) + " fragments (new cycle)");
		check("[" + leg.label + "] HUD title is Blueprint Emergence", f.scene.displayName == "Blueprint Emergence");
	}
}

void BlueprintRosterHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();

	maxFrameSecondsThisLeg_ = std::max(maxFrameSecondsThisLeg_, dt);
	const HudFrameData& f = runtime_.currentHudFrameData();
	const Leg& leg = legs_[leg_];
	const bool idle = f.sceneManager.transitionPhase == SceneTransitionPhase::Idle;

	if (legFrame_ < 0) {
		framesSinceSwitch_++;
		if (idle && f.sceneManager.activeSceneId == leg.expectedSceneId && f.scene.sceneId == leg.expectedSceneId) {
			legFrame_ = 0;
			legSeconds_ = 0.0f;
			if (leg_ > 0) {
				note("[" + leg.label + "] switch request -> first live Idle frame: " +
					ofToString(ofGetElapsedTimef() - switchStartTime_, 2) + " s");
			}
			if (leg_ == 0) {
				// Leg 1 starts at app startup, not at a switch — report startup
				// separately so it doesn't count as a switch-in stall.
				note("startup (process launch -> first live Blueprint frame) max frame time: " +
					ofToString(maxFrameSecondsThisLeg_ * 1000.0f, 1) + " ms");
				maxFrameSecondsThisLeg_ = 0.0f;
			}
			onFirstLiveFrameOfLeg();
		} else if (ofGetElapsedTimef() - switchStartTime_ > kSwitchTimeoutSeconds) {
			check("[" + leg.label + "] switch completed", false, "timed out");
			finish();
		}
		return;
	}

	legFrame_++;
	legSeconds_ += dt;
	if (idle) checkIdleFrameInvariants();

	const bool blueprintLeg = leg.expectedSceneId == kBlueprintId;
	const bool firstBlueprintLeg = blueprintLeg && leg_ == 0;
	auto at = [&](float t) { return legSeconds_ >= t && legSeconds_ - dt < t; };

	if (firstBlueprintLeg && at(1.0f)) {
		cycleSeedBeforeRegenerate_ = runtime_.blueprintSceneForTesting().coreForTesting().composition().getCycleSeed();
		runtime_.keyPressed('2'); // real InputRouter -> SceneCommand::Regenerate
	}
	if (firstBlueprintLeg && at(1.5f)) {
		const BEComposition& comp = runtime_.blueprintSceneForTesting().coreForTesting().composition();
		check("[" + leg.label + "] Regenerate ('2') starts a new composition cycle with a new seed",
			comp.getCycleSeed() != cycleSeedBeforeRegenerate_ && comp.getCycleElapsedSeconds() < 0.75f,
			std::string("phase ") + phaseName(comp.getPhase()) + ", cycle t=" + ofToString(comp.getCycleElapsedSeconds(), 2) + " s");
		mediaBeforeNext_ = mediaIdOf(f);
		runtime_.keyPressed('n'); // real InputRouter -> SceneCommand::NextMedia -> canonical VideoPlaybackService
	}
	if (firstBlueprintLeg && at(4.0f)) {
		check("[" + leg.label + "] NextMedia ('n') changed the canonical selection",
			mediaIdOf(f) != mediaBeforeNext_, mediaBeforeNext_ + " -> " + mediaIdOf(f));
	}

	// Blueprint: 8 s BLANK, then PLACEMENT — capture once fragments exist.
	const float captureAt = blueprintLeg ? 20.0f : 5.0f;
	const float leaveAt = blueprintLeg ? 21.0f : 6.0f;
	if (blueprintLeg && leg_ > 0 && legFrame_ == 1) capture(leg.label + "_first_frame");
	if (at(captureAt)) {
		capture(leg.label);
		if (blueprintLeg) {
			const auto& sem = f.scene.semantic;
			note("[" + leg.label + "] semantic: " + (sem ? sem->state.primaryStateId + ", overall=" +
				ofToString(sem->activity.overall.value_or(-1.0f), 2) + ", metrics=" + ofToString(sem->metrics.size()) : "(none)") +
				", fragments=" + ofToString(runtime_.blueprintSceneForTesting().coreForTesting().composition().getFragments().size()));
			check("[" + leg.label + "] artwork is placing fragments from real media",
				!runtime_.blueprintSceneForTesting().coreForTesting().composition().getFragments().empty() && sem.has_value());
		}
	}
	if (at(leaveAt)) {
		note("[" + leg.label + "] max frame time incl. switch-in: " + ofToString(maxFrameSecondsThisLeg_ * 1000.0f, 1) + " ms");
		check("[" + leg.label + "] no multi-second stall (max frame < 1000 ms)", maxFrameSecondsThisLeg_ < 1.0f);
		if (leg_ + 1 >= legs_.size()) {
			finish();
			return;
		}
		if (blueprintLeg) seedAtLastBlueprintExit_ = runtime_.blueprintSceneForTesting().coreForTesting().composition().getCycleSeed();
		leg_++;
		legFrame_ = -1;
		switchStartTime_ = ofGetElapsedTimef();
		framesSinceSwitch_ = 0;
		maxFrameSecondsThisLeg_ = 0.0f;
		runtime_.keyPressed(']'); // real InputRouter -> RuntimeCommand::NextScene (ring wraps Blueprint -> Blob)
	}
}

void BlueprintRosterHarness::finish() {
	check("per-frame invariants held on every Idle frame", invariantFailures_ == 0,
		ofToString(idleFramesChecked_) + " frames checked, " + ofToString(invariantFailures_) + " violations");
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1); // runtime.exit() -> shutdown of every scene follows
}
