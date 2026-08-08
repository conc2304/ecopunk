#include "ofMain.h"
#include "ofApp.h"

// experience_runtime — ExperienceRuntime scaffold, Increment 1.
//
// Proves the shared scene contract (shared/src/scene/SceneContract.h)
// exists as code and that ExperienceRuntime owns the scene output FBO, a
// fake IEcopunkScene renders into it, a read-only SceneFrame is handed to a
// stand-in frame consumer, runtime/scene commands route separately, and
// defensive GL-state restoration runs after scene drawing. No production
// scene is built or referenced here — see ofApp.h/ExperienceRuntime.h.
int main() {
	ofGLWindowSettings settings;
	settings.setSize(1280, 720);
	settings.windowMode = OF_WINDOW;

	auto window = ofCreateWindow(settings);
	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();
}
