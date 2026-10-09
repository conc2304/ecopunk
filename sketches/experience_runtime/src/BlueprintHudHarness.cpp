#include "BlueprintHudHarness.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
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
constexpr const char* kCaptureDir = "captures/blueprint-hud";
constexpr const char* kPrev = "controls.media_previous";
constexpr const char* kNext = "controls.media_next";
constexpr const char* kReseed = "controls.reseed";
constexpr float kStepTimeoutSeconds = 45.0f;

bool hasCommand(const SceneCapabilities& caps, SceneCommand command) {
	return std::any_of(caps.commands.begin(), caps.commands.end(),
		[&](const SceneCommandDescriptor& d) { return d.command == command; });
}

} // namespace

bool BlueprintHudHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_BLUEPRINT_HUD_HARNESS") != nullptr;
}

BlueprintHudHarness::BlueprintHudHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	const auto ids = runtime_.sceneManagerForTesting().registeredSceneIdsForTesting();
	check("default roster [Blob, Temporal, Blueprint], startup Blob",
		ids.size() == 3 && ids[0] == kBlobId && ids[1] == kTemporalId && ids[2] == kBlueprintId);
	buildScript();
}

void BlueprintHudHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void BlueprintHudHarness::note(const std::string& line) {
	ofLogNotice("BlueprintHudHarness") << line;
	lines_.push_back(line);
}

void BlueprintHudHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png (" +
		ofToString(img.getWidth()) + "x" + ofToString(img.getHeight()) + ", focus=" + focused() + ")");
}

// Accent (244,255,106) corner brackets, 2 px lines, 3 px outside the region's
// top-left corner — same probe as FocusMenuHarness, read from the frame just drawn.
bool BlueprintHudHarness::bracketsAt(const std::string& regionId) {
	const ofRectangle b = runtime_.hudCompositorBridgeForTesting().renderer().pixelBoundsFor(regionId);
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	const ofPixels& px = img.getPixels();
	const int x = int(b.x - 3.0f) + 5;
	for (int y = int(b.y - 3.0f) - 2; y <= int(b.y - 3.0f) + 2; ++y) {
		const ofColor c = px.getColor(x, y);
		if (c.r > 200 && c.g > 200 && c.b < 170) return true;
	}
	return false;
}

bool BlueprintHudHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId
		&& f.scene.sceneId == sceneId;
}

std::string BlueprintHudHarness::focused() const {
	const auto& f = runtime_.hudCompositorBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}

void BlueprintHudHarness::buildScript() {
	auto& rt = runtime_;
	auto frame = [&rt]() -> const HudFrameData& { return rt.currentHudFrameData(); };
	auto renderer = [&rt]() -> const hudpresent::HudWireframeRenderer& { return rt.hudCompositorBridgeForTesting().renderer(); };
	auto composition = [&rt]() -> const BEComposition& { return rt.blueprintSceneForTesting().coreForTesting().composition(); };
	auto framesAtLeast = [this](int n) { return [this, n] { return framesInStep_ >= n; }; };

	script_ = {
		// -- Blob: baseline, engage focus, refused activation --------------
		{"blob live", [this] { return idleOn(kBlobId) && framesInStep_ >= 90; }, [this, frame] {
			check("Blob: no focus before any input", focused() == "none");
			check("Blob: RESEED hidden (Blob advertises no Regenerate)",
				!runtime_.hudCompositorBridgeForTesting().renderer().controlEnabled(kReseed).has_value()
				&& !hasCommand(frame().capabilities, SceneCommand::Regenerate));
			capture("01_blob_baseline");
			runtime_.keyPressed(OF_KEY_RIGHT);
		}},
		{"blob focus engaged", framesAtLeast(2), [this] {
			check("first step engages NEXT MEDIA (first enabled control)", focused() == kNext, focused());
			check("focus brackets drawn around NEXT MEDIA", bracketsAt(kNext));
			runtime_.keyPressed(OF_KEY_LEFT);
		}},
		{"blob prev focused", framesAtLeast(2), [this, renderer] {
			const auto prev = renderer().controlEnabled(kPrev);
			check("LEFT focuses PREV MEDIA, which is disabled at the start of media history",
				focused() == kPrev && prev.has_value() && !*prev, focused());
			activationSerialBefore_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"blob refused", framesAtLeast(1), [this] {
			note("refused flash captured " + ofToString(ofGetLastFrameTime() * 1000.0f, 1) + " ms after Enter (fps " +
				ofToString(runtime_.currentHudFrameData().runtime.fps, 1) + ")");
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("Enter on disabled PREV MEDIA is refused, nothing dispatched",
				rec.serial == activationSerialBefore_ + 1 && rec.refused && rec.command.empty() && rec.region == kPrev);
			capture("02_blob_prev_media_refused_flash");
			runtime_.keyPressed(OF_KEY_RIGHT); // back to NEXT MEDIA before switching scenes
		}},
		// -- Blob -> Temporal: focus stays on NEXT MEDIA ------------------------
		{"to temporal", framesAtLeast(2), [this] {
			check("focus back on NEXT MEDIA", focused() == kNext, focused());
			runtime_.keyPressed(']');
		}},
		{"temporal live", [this] { return idleOn(kTemporalId) && framesInStep_ >= 2; }, [this, frame] {
			// Allow the incoming scene's first published frames to settle.
			(void)frame;
		}},
		{"temporal settled", framesAtLeast(45), [this, frame] {
			check("focus stable across Blob -> Temporal (still NEXT MEDIA)", focused() == kNext, focused());
			check("brackets still on NEXT MEDIA in Temporal", bracketsAt(kNext));
			check("Temporal: RESEED hidden, no Regenerate capability leaked",
				!runtime_.hudCompositorBridgeForTesting().renderer().controlEnabled(kReseed).has_value()
				&& !hasCommand(frame().capabilities, SceneCommand::Regenerate));
			capture("03_temporal_focus_stable");
			runtime_.keyPressed(']');
		}},
		// -- Temporal -> Blueprint: RESEED appears, values are Blueprint's ------
		{"blueprint live", [this] { return idleOn(kBlueprintId) && framesInStep_ >= 2; }, [this, frame] {
			const auto& sem = frame().scene.semantic;
			if (sem) {
				note("first Blueprint frame semantic: " + sem->state.primaryStateId + ", active " +
					ofToString(sem->timing.activeSeconds, 2) + " s");
			} else {
				note("first Blueprint frame: no semantic payload yet (waiting for a video frame)");
			}
		}},
		{"blueprint settled", [this, frame] { return framesInStep_ >= 45 && frame().scene.semantic.has_value(); }, [this, frame, renderer] {
			const HudFrameData& f = frame();
			const auto reseed = renderer().controlEnabled(kReseed);
			check("Blueprint: RESEED visible and enabled", reseed.has_value() && *reseed);
			check("Blueprint advertises Regenerate; no Temporal effects snapshot leaked",
				hasCommand(f.capabilities, SceneCommand::Regenerate) && !f.effects.has_value());
			check("focus stable across Temporal -> Blueprint (still NEXT MEDIA)", focused() == kNext, focused());
			const auto& sem = *f.scene.semantic;
			check("Blueprint state ID is Blueprint's own", sem.state.primaryStateId.rfind("scene.blueprint.state.", 0) == 0,
				sem.state.primaryStateId);
			check("Blueprint active time restarted on activation (not Blob/Temporal's clock)",
				sem.timing.activeSeconds < 5.0f, ofToString(sem.timing.activeSeconds, 2) + " s");
			check("Blueprint activity = fragments / 4-fragment cap",
				sem.activity.overall.has_value(), ofToString(sem.activity.overall.value_or(-1.0f), 2));
			check("Blueprint HUD title", f.scene.displayName == "Blueprint Emergence");
			note("media title source: titleId=" + f.video->titleId.value_or("-") + ", fallbackDisplayTitle=" +
				f.video->fallbackDisplayTitle.value_or("-"));
		}},
		{"blueprint artwork", [composition, this] { return composition().getFragments().size() >= 2 || secondsInStep_ > 25.0f; },
			[this, frame, composition] {
			const auto& sem = frame().scene.semantic;
			note("Blueprint artwork: " + ofToString(composition().getFragments().size()) + " fragments, state " +
				(sem ? sem->state.primaryStateId : std::string("-")) + ", overall " +
				ofToString(sem ? sem->activity.overall.value_or(-1.0f) : -1.0f, 2));
			capture("04_blueprint_live_hud");
			runtime_.keyPressed(OF_KEY_RIGHT);
		}},
		{"reseed focused", framesAtLeast(2), [this, composition] {
			check("RIGHT moves focus NEXT MEDIA -> RESEED", focused() == kReseed, focused());
			check("focus brackets drawn around RESEED", bracketsAt(kReseed));
			capture("05_blueprint_reseed_focused");
		}},
		// Enter in its own step, a few normal frames after the screen grabs
		// above, so harness capture time never ages the 0.3 s flash.
		{"reseed enter", framesAtLeast(5), [this, composition] {
			seedBeforeReseed_ = composition().getCycleSeed();
			activationSerialBefore_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"reseed activated", framesAtLeast(1), [this, composition] {
			note("accepted flash captured " + ofToString(ofGetLastFrameTime() * 1000.0f, 1) + " ms after Enter (fps " +
				ofToString(runtime_.currentHudFrameData().runtime.fps, 1) + ")");
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("Enter on RESEED dispatches SceneCommand::Regenerate and is accepted",
				rec.serial == activationSerialBefore_ + 1 && rec.region == kReseed
				&& rec.command == "SceneCommand::Regenerate" && rec.accepted && !rec.refused, rec.command);
			check("Regenerate started a new composition cycle (new seed, cycle clock reset)",
				composition().getCycleSeed() != seedBeforeReseed_ && composition().getCycleElapsedSeconds() < 0.5f,
				"t=" + ofToString(composition().getCycleElapsedSeconds(), 2) + " s");
			capture("06_blueprint_reseed_activation_flash");
		}},
		{"after reseed", framesAtLeast(60), [this] {
			check("focus stays on RESEED after activation", focused() == kReseed, focused());
			capture("07_blueprint_after_reseed");
			runtime_.keyPressed(']'); // Blueprint -> Blob (ring wraps)
		}},
		// -- Blueprint -> Blob: RESEED hidden, focus must move ------------------
		{"blob again live", [this] { return idleOn(kBlobId) && framesInStep_ >= 2; }, [] {}},
		{"blob again settled", framesAtLeast(45), [this, frame] {
			const HudFrameData& f = frame();
			const auto& r = runtime_.hudCompositorBridgeForTesting().renderer();
			const std::string fc = focused();
			check("RESEED hidden again on Blob", !r.controlEnabled(kReseed).has_value());
			check("focus left the hidden RESEED for a visible control", fc != kReseed && fc != "none"
				&& r.controlEnabled(fc).has_value(), fc);
			check("Blob state/capabilities: no Blueprint values leaked",
				!hasCommand(f.capabilities, SceneCommand::Regenerate) && !f.effects.has_value()
				&& (!f.scene.semantic || f.scene.semantic->state.primaryStateId.rfind("scene.blob.", 0) == 0));
			capture("08_blob_after_blueprint_focus_moved");
		}},
	};
}

void BlueprintHudHarness::step(float dt) {
	if (done_) return;
	runtime_.draw();
	framesInStep_++;
	secondsInStep_ += dt;

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

void BlueprintHudHarness::finish() {
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
