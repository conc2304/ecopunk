#include "ReactiveControlHarness.h"

#include "HudReactiveControlArt.h"
#include "HudSkinPng.h"

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
constexpr const char* kCaptureDir = "captures/reactive-control";
constexpr const char* kRegion = hudpresent::reactive::kSceneNextRegionId;
constexpr float kStepTimeoutSeconds = 45.0f;
} // namespace

bool ReactiveControlHarness::isRequested() {
	return std::getenv("EXPERIENCE_RUNTIME_REACTIVE_HARNESS") != nullptr;
}

ReactiveControlHarness::ReactiveControlHarness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	const char* root = std::getenv("EXPERIENCE_RUNTIME_REACTIVE_SKINS_ROOT");
	skinsRoot_ = root ? root : "";
	buildScript();
}

void ReactiveControlHarness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}

void ReactiveControlHarness::note(const std::string& line) {
	const std::string stamped = "[t=" + ofToString(ofGetElapsedTimef(), 2) + "s] " + line;
	ofLogNotice("ReactiveControlHarness") << stamped;
	lines_.push_back(stamped);
}

void ReactiveControlHarness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png (opacity "
		+ ofToString(runtime_.hudCompositorBridgeForTesting().renderer().pressedArtworkOpacity(), 2) + ", focus=" + focused() + ")");
}

// Pixels of the frame just drawn, in and around controls.scene_next.
ReactiveControlHarness::Probe ReactiveControlHarness::probe() const {
	const ofRectangle b = runtime_.hudCompositorBridgeForTesting().renderer().pixelBoundsFor(kRegion);
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	const ofPixels& px = img.getPixels();
	Probe p;
	// Rim band: 3..6 px inside each edge (where the test artwork's opaque rim
	// lies at 2x -> 1x scale), excluding the centre where the label is.
	double g = 0.0, r = 0.0;
	int n = 0;
	for (int y = int(b.y) + 3; y < int(b.getBottom()) - 3; ++y) {
		for (int x = int(b.x) + 3; x < int(b.getRight()) - 3; ++x) {
			const int m = std::min({x - int(b.x), y - int(b.y), int(b.getRight()) - 1 - x, int(b.getBottom()) - 1 - y});
			if (m > 6) continue;
			const ofColor c = px.getColor(x, y);
			g += c.g;
			r += c.r;
			++n;
		}
	}
	p.rimG = n ? float(g / n) : 0.0f;
	p.rimR = n ? float(r / n) : 0.0f;
	// Label: teal text pixels in the central band. The label is teal (blue-
	// rich); the shell's idle face and the test artwork's face are not
	// (artwork face b ~65 composited), so b > 105 with g > 100 isolates text
	// even when the label is DIMMED (0.35 opacity: the control is disabled
	// during the scene switch that its own activation starts).
	for (int y = int(b.y + b.height * 0.3f); y < int(b.y + b.height * 0.8f); ++y) {
		for (int x = int(b.x + 4); x < int(b.getRight() - 4); ++x) {
			const ofColor c = px.getColor(x, y);
			if (c.b > 105 && c.g > 100 && c.b > c.r + 30) p.labelPixels++;
		}
	}
	// Focus brackets: the four corners, 3 px outside the bounds (accent yellow-green).
	const float ox[] = {b.x - 3.0f, b.getRight() + 3.0f};
	const float oy[] = {b.y - 3.0f, b.getBottom() + 3.0f};
	for (float cx : ox) {
		for (float cy : oy) {
			for (int dy = -2; dy <= 2; ++dy) {
				for (int dx = -2; dx <= 2; ++dx) {
					const ofColor c = px.getColor(std::clamp(int(cx) + dx, 0, int(px.getWidth()) - 1), std::clamp(int(cy) + dy, 0, int(px.getHeight()) - 1));
					if (c.r > 180 && c.g > 200 && c.b < 140) p.bracketPixels++;
				}
			}
		}
	}
	return p;
}

std::string ReactiveControlHarness::describe(const Probe& p) const {
	return "rim RGB-mean (r " + ofToString(p.rimR, 1) + ", g " + ofToString(p.rimG, 1) + "), label px " + ofToString(p.labelPixels)
		+ ", bracket px " + ofToString(p.bracketPixels);
}

bool ReactiveControlHarness::idleOn(const std::string& sceneId) const {
	const HudFrameData& f = runtime_.currentHudFrameData();
	return f.sceneManager.transitionPhase == SceneTransitionPhase::Idle && f.sceneManager.activeSceneId == sceneId;
}

bool ReactiveControlHarness::sceneIdle() const {
	return runtime_.currentHudFrameData().sceneManager.transitionPhase == SceneTransitionPhase::Idle;
}

std::string ReactiveControlHarness::focused() const {
	const auto& f = runtime_.hudCompositorBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}

bool ReactiveControlHarness::focusSceneNext() {
	if (focused() == kRegion) return true;
	if (framesInStep_ % 2 == 0) runtime_.keyPressed(OF_KEY_RIGHT);
	return false;
}

void ReactiveControlHarness::buildScript() {
	auto framesAtLeast = [this](int n) { return [this, n] { return framesInStep_ >= n; }; };
	auto bridge = [this]() -> HudCompositorBridge& { return runtime_.hudCompositorBridgeMutableForTesting(); };
	auto renderer = [this]() -> const hudpresent::HudWireframeRenderer& { return runtime_.hudCompositorBridgeForTesting().renderer(); };
	auto readyToPress = [this] { return sceneIdle() && framesInStep_ >= 60 && focusSceneNext(); };

	script_ = {
		{"load skin with artwork", [this] { return idleOn(BlobProductionScene::kSceneId) && framesInStep_ >= 60; }, [this, bridge, renderer] {
			check("scratch skins root provided", !skinsRoot_.empty(), skinsRoot_);
			bridge().setSkinsRootForTesting(skinsRoot_);
			const auto o = bridge().requestSkin("dark-moss-experimental");
			check("Dark Moss + controls/scene_next/pressed.png: skin activated, artwork loaded once",
				o.activated && o.sceneNextPressedLoaded && o.optionalDiagnostics.empty(), o.code + " " + o.detail);
			const ofRectangle b = renderer().pixelBoundsFor(kRegion);
			check("artwork placement = canonical controls.scene_next bounds (153.6, 648, 102.4 x 43.2)",
				std::fabs(b.x - 153.6f) < 0.01f && std::fabs(b.y - 648.0f) < 0.01f && std::fabs(b.width - 102.4f) < 0.01f
					&& std::fabs(b.height - 43.2f) < 0.01f,
				ofToString(b.x) + "," + ofToString(b.y) + " " + ofToString(b.width) + "x" + ofToString(b.height));
			const auto& active = bridge().skinLoader().active();
			check("two live skin textures (shell + pressed), renderer holds the pressed texture",
				bridge().skinBackend().liveTextureCount() == 2 && renderer().pressedArtwork() != nullptr && active
					&& active->sceneNextPressed && active->sceneNextPressed->width == 112 && active->sceneNextPressed->height == 52,
				ofToString(bridge().skinBackend().liveTextureCount()) + " textures");
			stageCallsAfterLoad_ = bridge().skinBackend().stageCallCount();
		}},
		{"idle", framesAtLeast(10), [this, renderer] {
			idle_ = probe();
			check("idle: artwork not drawn", renderer().pressedArtworkOpacity() == 0.0f);
			note("idle probe: " + describe(idle_));
			capture("c01_idle");
		}},
		{"focus NEXT SCENE", [this] { return focusSceneNext(); }, [] {}},
		{"focused", framesAtLeast(10), [this, renderer] {
			focused_ = probe();
			check("focused: artwork not drawn, brackets + edge illumination", renderer().pressedArtworkOpacity() == 0.0f
				&& focused_.bracketPixels > 0, describe(focused_));
			check("focused-edge illumination raises the rim band (inside the bounds) vs idle", focused_.rimG > idle_.rimG + 2.0f,
				"rim g " + ofToString(idle_.rimG, 1) + " -> " + ofToString(focused_.rimG, 1));
			capture("c02_focused");
			activationSerial_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN); // accepted: NEXT SCENE
		}},
		{"pressed", framesAtLeast(1), [this, renderer] {
			pressed_ = probe();
			const auto& rec = runtime_.lastFocusActivationForTesting();
			const float opacity = renderer().pressedArtworkOpacity();
			check("Enter on NEXT SCENE accepted (existing command path)", rec.serial == activationSerial_ + 1 && rec.accepted
				&& rec.region == kRegion, rec.command);
			{
				const auto en = renderer().controlEnabled(kRegion);
				const auto& fc = renderer().focusedControl();
				note("pressed-frame state: controlEnabled=" + std::string(!en ? "hidden" : (*en ? "enabled" : "disabled"))
					+ ", renderer focus=" + (fc ? *fc : std::string("none")) + ", transition phase="
					+ ofToString(int(runtime_.currentHudFrameData().sceneManager.transitionPhase)) + ", feedback age "
					+ ofToString(renderer().activationFeedbackAgeSeconds(), 3) + " s, last frame " + ofToString(ofGetLastFrameTime(), 3) + " s");
			}
			check("accepted activation shows the pressed artwork (existing 0.30 s feedback interval)", opacity > 0.8f,
				"opacity " + ofToString(opacity, 3));
			check("pressed artwork visible in the control (rim turns artwork-green)", pressed_.rimG > focused_.rimG + 20.0f,
				describe(pressed_));
			check("label still drawn ABOVE the artwork (dimmed: the control is disabled during the switch it started)",
				pressed_.labelPixels > 0 && pressed_.labelPixels >= focused_.labelPixels / 3,
				ofToString(focused_.labelPixels) + " -> " + ofToString(pressed_.labelPixels) + " label px");
			check("focus brackets still drawn above/around the artwork", pressed_.bracketPixels > 0, ofToString(pressed_.bracketPixels));
			capture("c03_pressed");
			activationSerial_ = rec.serial;
			runtime_.keyPressed(OF_KEY_RETURN); // during the transition NEXT SCENE is disabled -> refused
		}},
		{"refused", framesAtLeast(2), [this, renderer] {
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("Enter on the disabled control (scene transition) is refused", rec.serial == activationSerial_ + 1 && rec.refused,
				rec.region);
			check("refused/disabled activation never shows the pressed artwork", renderer().pressedArtworkOpacity() == 0.0f,
				"opacity " + ofToString(renderer().pressedArtworkOpacity(), 3));
			capture("c03b_refused_while_disabled");
		}},
		{"released", [this] { return secondsInStep_ >= 0.6f && sceneIdle() && framesInStep_ >= 30; }, [this, renderer, bridge] {
			const Probe p = probe();
			check("after the feedback interval the artwork is gone (release)", renderer().pressedArtworkOpacity() == 0.0f
				&& p.rimG < pressed_.rimG - 15.0f, describe(p));
			check("no asset loading during rendering (stage calls unchanged since skin load)",
				bridge().skinBackend().stageCallCount() == stageCallsAfterLoad_,
				ofToString(stageCallsAfterLoad_) + " -> " + ofToString(bridge().skinBackend().stageCallCount()));
			capture("c04_released");
		}},
		// -- Replacement without artwork --------------------------------------
		{"replace: no artwork", framesAtLeast(5), [this, bridge, renderer] {
			const auto o = bridge().requestSkin("dark-moss-no-pressed");
			check("replacement skin without artwork: activated, previous pressed texture released, none drawn",
				o.activated && o.releasedPrevious && !o.sceneNextPressedLoaded && o.optionalDiagnostics.empty()
					&& renderer().pressedArtwork() == nullptr && bridge().skinBackend().liveTextureCount() == 1,
				ofToString(bridge().skinBackend().liveTextureCount()) + " live textures");
		}},
		{"press without artwork", readyToPress, [this] {
			activationSerial_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"missing-asset pressed", framesAtLeast(1), [this, renderer] {
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("missing asset: NEXT SCENE still works, existing accepted flash instead of artwork",
				rec.accepted && rec.serial == activationSerial_ + 1 && renderer().pressedArtworkOpacity() == 0.0f, rec.command);
			capture("c05_missing_asset_pressed");
		}},
		// -- Invalid artwork ----------------------------------------------------
		{"invalid asset", [this] { return sceneIdle() && framesInStep_ >= 30; }, [this, bridge, renderer] {
			const auto o = bridge().requestSkin("dark-moss-bad-pressed");
			check("invalid (RGB) artwork: skin still activates, one nonfatal diagnostic, artwork not used",
				o.activated && !o.sceneNextPressedLoaded && o.optionalDiagnostics.size() == 1
					&& o.optionalDiagnostics[0].code == hudskin::pngcodes::NotRgba8 && renderer().pressedArtwork() == nullptr,
				o.optionalDiagnostics.empty() ? std::string("(no diagnostic)") : o.optionalDiagnostics[0].code);
		}},
		{"press with invalid artwork", readyToPress, [this] {
			activationSerial_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"invalid-asset pressed", framesAtLeast(1), [this, renderer] {
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("invalid asset: NEXT SCENE still works, no artwork", rec.accepted && renderer().pressedArtworkOpacity() == 0.0f,
				rec.command);
			capture("c06_invalid_asset_pressed");
		}},
		// -- Upload failure, reload, unskinned -------------------------------------
		{"upload failure", [this] { return sceneIdle() && framesInStep_ >= 30; }, [this, bridge, renderer] {
			bridge().skinBackendForTesting().failUploadAfterForTesting(1); // shell succeeds, artwork upload fails
			const auto o = bridge().requestSkin("dark-moss-experimental");
			check("artwork upload failure: skin activates without it (nonfatal), nothing leaked",
				o.activated && !o.sceneNextPressedLoaded && o.optionalDiagnostics.size() == 1
					&& o.optionalDiagnostics[0].code == hudskin::loadcodes::UploadFailed && renderer().pressedArtwork() == nullptr
					&& bridge().skinBackend().liveTextureCount() == 1,
				o.optionalDiagnostics.empty() ? std::string("(no diagnostic)") : o.optionalDiagnostics[0].detail);
			const auto again = bridge().requestSkin("dark-moss-experimental");
			check("reload restores the artwork", again.activated && again.sceneNextPressedLoaded && renderer().pressedArtwork() != nullptr
				&& bridge().skinBackend().liveTextureCount() == 2);
		}},
		{"unskinned", framesAtLeast(5), [this, bridge, renderer] {
			bridge().shutdownSkins();
			check("unload: all skin textures released, renderer holds no shell/artwork",
				bridge().skinBackend().liveTextureCount() == 0 && renderer().pressedArtwork() == nullptr
					&& renderer().shellTexture() == nullptr);
		}},
		{"press unskinned", readyToPress, [this] {
			activationSerial_ = runtime_.lastFocusActivationForTesting().serial;
			runtime_.keyPressed(OF_KEY_RETURN);
		}},
		{"unskinned pressed", framesAtLeast(1), [this, renderer] {
			const auto& rec = runtime_.lastFocusActivationForTesting();
			check("unskinned: NEXT SCENE works with the unchanged wireframe flash", rec.accepted
				&& renderer().pressedArtworkOpacity() == 0.0f, rec.command);
			capture("c07_unskinned_pressed");
		}},
		{"done", framesAtLeast(5), [this] { finish(); }},
	};
}

void ReactiveControlHarness::step(float dt) {
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

void ReactiveControlHarness::finish() {
	if (done_) return;
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
