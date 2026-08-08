#include "ofMain.h"
#include "ofApp.h"

// hud_validation_studio — the interactive HUD Validation Studio foundation
// this task asks for. NOT merged into the installation runtime (no
// ExperienceRuntime/SceneManager dependency of any kind — see ofApp.h),
// and NOT a replacement for sketches/hud_validation_harness/ (which keeps
// validating shared/src/hud/'s widget library on its own, unrelated
// timeline — see docs/probes/hud-runtime-validation-studio-probe.md §10).
int main() {
	ofGLWindowSettings settings;
	settings.setSize(1280, 720);
	settings.windowMode = OF_WINDOW;

	// Architecture-Closure Session: this dev machine has three displays
	// (system_profiler SPDisplaysDataType: a 2880x1800 Retina internal
	// panel marked "Main Display: Yes", a 1920x1080 external, and a
	// 1080x1920 PORTRAIT external) — without an explicit position, the
	// window opened on whichever display GLFW enumerates first, which is
	// not necessarily the (Cocoa-designated) main display, and on the
	// 1080-wide portrait monitor specifically clamped a requested
	// 1280-wide window down to 1080, silently producing every prior
	// session's 1080x720 screenshots instead of the required 1280x720
	// (confirmed via `sips -g pixelWidth` on those files). Cocoa's global
	// coordinate space places the MAIN display's top-left corner at
	// (0,0) as a hard OS invariant, independent of physical monitor
	// arrangement — so (50,50) is guaranteed to land inside the main
	// display's bounds (2880x1800 here) regardless of how the other two
	// are physically positioned, without needing to know their offsets.
	settings.setPosition(glm::vec2(50, 50));

	auto window = ofCreateWindow(settings);
	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();
}
