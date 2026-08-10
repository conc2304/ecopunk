#include "ofMain.h"
#include "ofApp.h"

// experience_runtime — ExperienceRuntime host application.
//
// Proves the shared scene contract (shared/src/scene/SceneContract.h)
// exists as code and that ExperienceRuntime owns the scene output FBO, a
// real IEcopunkScene renders into it, a read-only SceneFrame is handed to a
// stand-in frame consumer, runtime/scene commands route separately, and
// defensive GL-state restoration runs after scene drawing.
//
// Blob First Complete Production Migration: on the normal (non-harness)
// path, ofApp::setup() installs BlobProductionScene — see
// ExperienceRuntime::installBlobProductionScene() and ofApp.cpp — as the
// first real production scene this runtime hosts. The opt-in
// GlRestorationHarness path (EXPERIENCE_RUNTIME_GL_HARNESS) still exercises
// FakeScene unmodified — see ofApp.cpp/ExperienceRuntime.h.
int main() {
	ofGLWindowSettings settings;
	settings.setSize(1280, 720);
	settings.windowMode = OF_WINDOW;

	auto window = ofCreateWindow(settings);
	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();
}
