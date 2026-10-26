// Hud006v2Harness — HUD-006 v2 six-button navigation, real-runtime check
// (EXPERIENCE_RUNTIME_HUD006V2_HARNESS). Wireframe / unskinned: a v1 Dark
// Moss shell is never used to judge v2 alignment (it is rejected anyway).
#include "Hud006v2Harness.h"

#include "HudReactiveControlArt.h"
#include "HudRealFrameResolver.h"

#include "ofAppRunner.h"
#include "ofEvents.h"
#include "ofImage.h"
#include "ofLog.h"
#include "ofUtils.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {
constexpr const char* kCaptureDir = "captures/hud006-v2";
constexpr float kTimeout = 45.0f;
const char* const kRing[] = {"controls.scene_previous", "controls.scene_next", "controls.mode",
	"controls.media_previous", "controls.media_next", "controls.fullscreen"};
} // namespace

bool Hud006v2Harness::isRequested() { return std::getenv("EXPERIENCE_RUNTIME_HUD006V2_HARNESS") != nullptr; }

Hud006v2Harness::Hud006v2Harness(ExperienceRuntime& runtime) : runtime_(runtime) {
	std::filesystem::create_directories(ofToDataPath(kCaptureDir, true));
	buildScript();
}

void Hud006v2Harness::check(const std::string& name, bool passed, const std::string& detail) {
	(passed ? passes_ : failures_)++;
	note(std::string(passed ? "PASS " : "FAIL ") + name + (detail.empty() ? "" : " — " + detail));
}
void Hud006v2Harness::note(const std::string& line) {
	const std::string s = "[t=" + ofToString(ofGetElapsedTimef(), 1) + "s] " + line;
	ofLogNotice("Hud006v2Harness") << s;
	lines_.push_back(s);
}
void Hud006v2Harness::capture(const std::string& name) {
	ofImage img;
	img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
	img.save(std::string(kCaptureDir) + "/" + name + ".png");
	note("capture: bin/data/" + std::string(kCaptureDir) + "/" + name + ".png (focus=" + focused() + ")");
}
std::string Hud006v2Harness::focused() const {
	const auto& f = runtime_.hudCompositorBridgeForTesting().focus().focused();
	return f ? *f : std::string("none");
}
bool Hud006v2Harness::idle() const {
	return runtime_.currentHudFrameData().sceneManager.transitionPhase == SceneTransitionPhase::Idle;
}
std::string Hud006v2Harness::resolved(const std::string& sourceId) const {
	static const hudpresent::HudRealFrameResolver resolver;
	const auto v = resolver.resolve(sourceId, runtime_.currentHudFrameData());
	if (!v || !v->present) return "(missing)";
	if (v->valueType == hudpresent::HudSourceValueType::Boolean) return v->boolValue ? "true" : "false";
	return v->textValue;
}
bool Hud006v2Harness::focusOn(const std::string& region) {
	if (focused() == region) return true;
	if (framesInStep_ % 2 == 0) runtime_.keyPressed(OF_KEY_RIGHT);
	return false;
}
int Hud006v2Harness::modeIndex() const {
	const auto& m = runtime_.currentHudFrameData().scene.mode;
	return m ? m->index : -1;
}

void Hud006v2Harness::buildScript() {
	auto framesAtLeast = [this](int n) { return [this, n] { return framesInStep_ >= n; }; };
	auto& rt = runtime_;
	script_ = {
		{"start", [this] { return idle() && framesInStep_ >= 90; }, [this, &rt] {
			const auto& r = rt.hudCompositorBridgeForTesting().renderer();
			const auto& cat = r.regionCatalog();
			check("HUD-006 v2 catalog: 20 regions, controls.reseed retired", cat.all().size() == 20 && cat.find("controls.reseed") == nullptr,
				ofToString(cat.all().size()));
			const float xs[] = {25.6f, 110.4f, 195.2f, 299.2f, 384.0f, 488.0f};
			bool geom = true;
			std::string bad;
			for (int i = 0; i < 6; ++i) {
				const ofRectangle b = r.pixelBoundsFor(kRing[i]);
				if (std::fabs(b.x - xs[i]) > 0.05f || std::fabs(b.y - 633.6f) > 0.05f || std::fabs(b.width - 72.0f) > 0.05f
					|| std::fabs(b.height - 72.0f) > 0.05f) { geom = false; bad += std::string(" ") + kRing[i]; }
			}
			check("six 72x72 squares at the approved coordinates (y 633.6)", geom, bad.empty() ? "all six" : bad);
			check("v1 Dark Moss skin rejected under Blueprint v2 (wireframe kept)", !rt.hudCompositorBridgeForTesting().skinLoader().active(),
				"no active skin");
			check("PREV MEDIA enabled at the start of media history (wraps)", r.controlEnabled("controls.media_previous").value_or(false));
			check("MODE visible and enabled (scene advertises modes)", r.controlEnabled("controls.mode").value_or(false));
			check("FULLSCREEN visible and enabled", r.controlEnabled("controls.fullscreen").value_or(false));
			check("mode panel is informational (not a control)", !r.controlEnabled("flexible.secondary_a").has_value());
			capture("v2_01_six_button_row_idle");
			rt.keyPressed(OF_KEY_RIGHT); // engage focus
		}},
		{"ring walk", framesAtLeast(2), [this, &rt] {
			std::string order = focused();
			bool modePanel = false;
			for (int i = 0; i < 6; ++i) {
				rt.keyPressed(OF_KEY_RIGHT);
				order += " > " + focused();
				modePanel = modePanel || focused() == "flexible.secondary_a";
			}
			check("focus ring: scene_previous > scene_next > mode > media_previous > media_next > fullscreen (wraps)",
				order == "controls.scene_previous > controls.scene_next > controls.mode > controls.media_previous > controls.media_next > controls.fullscreen > controls.scene_previous",
				order);
			check("mode panel is never a focus stop", !modePanel);
		}},
		{"focus mode", [this] { return focusOn("controls.mode"); }, [this, &rt] {
			capture("v2_02_mode_focused");
			modeBefore_ = modeIndex();
			serial_ = rt.lastFocusActivationForTesting().serial;
			rt.keyPressed(OF_KEY_RETURN);
		}},
		{"mode activated", framesAtLeast(3), [this, &rt] {
			const auto& rec = rt.lastFocusActivationForTesting();
			check("Enter on MODE dispatches SceneCommand::NextMode, accepted, mode advanced",
				rec.serial == serial_ + 1 && rec.command == "SceneCommand::NextMode" && rec.accepted && modeIndex() != modeBefore_,
				rec.command + " mode " + ofToString(modeBefore_) + " -> " + ofToString(modeIndex()));
		}},
		{"focus prev media", [this] { return focusOn("controls.media_previous"); }, [this, &rt] {
			mediaBefore_ = rt.currentHudFrameData().video && rt.currentHudFrameData().video->mediaId ? *rt.currentHudFrameData().video->mediaId : "";
			serial_ = rt.lastFocusActivationForTesting().serial;
			rt.keyPressed(OF_KEY_RETURN);
		}},
		{"prev media wrapped", framesAtLeast(30), [this, &rt] {
			const auto& rec = rt.lastFocusActivationForTesting();
			const std::string now = rt.currentHudFrameData().video && rt.currentHudFrameData().video->mediaId ? *rt.currentHudFrameData().video->mediaId : "";
			check("PREV MEDIA at history start is accepted and wraps to another clip", rec.accepted && !rec.refused && now != mediaBefore_,
				mediaBefore_ + " -> " + now);
		}},
		{"focus fullscreen", [this] { return focusOn("controls.fullscreen"); }, [this, &rt] {
			capture("v2_03_fullscreen_focused");
			serial_ = rt.lastFocusActivationForTesting().serial;
			rt.keyPressed(OF_KEY_RETURN);
		}},
		{"fullscreen on", framesAtLeast(20), [this, &rt] {
			const auto& rec = rt.lastFocusActivationForTesting();
			check("Enter on FULLSCREEN enters IMMERSIVE (scene fills the window); window mode unchanged",
				rec.serial == serial_ + 1 && rec.command == "Runtime::EnterImmersive" && rec.accepted
					&& rt.immersiveForTesting() && ofGetWindowMode() == OF_WINDOW, rec.command);
			capture("v2_04_fullscreen_immersive");
			rt.keyPressed(OF_KEY_RETURN); // any key exits (and is consumed)
		}},
		{"fullscreen off", framesAtLeast(10), [this, &rt] {
			check("key exits IMMERSIVE; focus stays on FULLSCREEN",
				!rt.immersiveForTesting() && focused() == "controls.fullscreen", focused());
		}},
		{"immersive key", framesAtLeast(10), [this, &rt] {
			rt.keyPressed('i');
			check("'i' enters IMMERSIVE (scene-only), window mode unchanged", rt.immersiveForTesting() && ofGetWindowMode() == OF_WINDOW);
		}},
		{"immersive exit", framesAtLeast(20), [this, &rt] {
			capture("v2_05_immersive_via_i");
			rt.keyPressed(' ');
			check("any key exits IMMERSIVE", !rt.immersiveForTesting());
		}},
		{"focus next scene", [this] { return idle() && framesInStep_ >= 10 && focusOn("controls.scene_next"); }, [this, &rt] {
			rt.hudCompositorBridgeMutableForTesting().setSkinsRootForTesting(ofToDataPath("../../test/skin_fixtures/skins", true));
			const auto o = rt.hudCompositorBridgeMutableForTesting().requestSkin("pressed-valid");
			const auto& active = rt.hudCompositorBridgeForTesting().skinLoader().active();
			check("v2 fixture skin + derived 81x81 pressed sprite loads", o.activated && o.sceneNextPressedLoaded && active
				&& active->sceneNextPressed && active->sceneNextPressed->width == 81 && active->sceneNextPressed->height == 81, o.code);
			const auto o1 = rt.hudCompositorBridgeMutableForTesting().requestSkin("pressed-legacy-v1-112x52");
			check("v1 112x52 sprite: nonfatal wrong-size fallback", o1.activated && !o1.sceneNextPressedLoaded
				&& !o1.optionalDiagnostics.empty() && o1.optionalDiagnostics[0].code == hudskin::loadcodes::OptionalWrongSize,
				o1.optionalDiagnostics.empty() ? std::string() : o1.optionalDiagnostics[0].detail);
			rt.hudCompositorBridgeMutableForTesting().requestSkin("pressed-valid");
			const ofRectangle b = rt.hudCompositorBridgeForTesting().renderer().pixelBoundsFor("controls.scene_next");
			const auto sp = hudpresent::reactive::pressedSpriteRect(b.x, b.y, b.width, b.height);
			check("sprite rect derived from the catalog: 81x81 at (106,629); focus bounds stay 72x72",
				sp.x == 106 && sp.y == 629 && sp.width == 81 && sp.height == 81 && b.width == 72.0f);
			serial_ = rt.lastFocusActivationForTesting().serial;
			rt.keyPressed(OF_KEY_RETURN);
		}},
		{"pressed + disabled", framesAtLeast(1), [this, &rt] {
			check("NEXT SCENE accepted; pressed sprite showing", rt.lastFocusActivationForTesting().accepted
				&& rt.hudCompositorBridgeForTesting().renderer().pressedArtworkOpacity() > 0.5f);
			capture("v2_06_pressed_sprite_and_disabled_scene_buttons");
			const auto& r = rt.hudCompositorBridgeForTesting().renderer();
			check("during the switch PREV/NEXT SCENE are disabled (visible, dimmed)",
				r.controlEnabled("controls.scene_next") == false && r.controlEnabled("controls.scene_previous") == false);
			serial_ = rt.lastFocusActivationForTesting().serial;
			rt.keyPressed(OF_KEY_RETURN); // Enter on the now-disabled NEXT SCENE
		}},
		{"refused", framesAtLeast(1), [this, &rt] {
			const auto& rec = rt.lastFocusActivationForTesting();
			check("Enter on a disabled control is refused, nothing dispatched, no pressed sprite",
				rec.serial == serial_ + 1 && rec.refused && rec.command.empty()
					&& rt.hudCompositorBridgeForTesting().renderer().pressedArtworkOpacity() == 0.0f, rec.region);
			capture("v2_06b_refused_while_disabled");
		}},
		{"restore", [this] { return idle() && framesInStep_ >= 30; }, [this, &rt] {
			check("focus restored on NEXT SCENE after the switch", focused() == "controls.scene_next", focused());
			rt.hudCompositorBridgeMutableForTesting().shutdownSkins();
			capture("v2_07_wireframe_after_switch");
		}},
		{"done", framesAtLeast(3), [this] { finish(); }},
	};
}

void Hud006v2Harness::step(float dt) {
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
	} else if (secondsInStep_ > kTimeout) {
		check("step '" + s.label + "' reached", false, "timed out");
		finish();
	}
}

void Hud006v2Harness::finish() {
	if (done_) return;
	note("RESULT: " + ofToString(passes_) + " passed, " + ofToString(failures_) + " failed");
	std::ofstream out(ofToDataPath(std::string(kCaptureDir) + "/results.txt", true));
	for (const auto& l : lines_) out << l << "\n";
	done_ = true;
	ofExit(failures_ == 0 ? 0 : 1);
}
