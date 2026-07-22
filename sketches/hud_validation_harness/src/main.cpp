#include "ofMain.h"
#include "ofApp.h"

// Deterministic visual capture harness for shared/src/hud/. Not a sketch to
// be performed live — it renders a fixed matrix of (widget x bounds x
// options x time) scenarios once, saves each as a PNG under
// bin/data/captures/, and exits. Intended use: run once before a HUD
// library change, run again after, diff the two capture sets image-by-image.
int main() {
    ofGLWindowSettings settings;
    settings.setSize(700, 400);
    settings.windowMode = OF_WINDOW;

    auto window = ofCreateWindow(settings);
    ofRunApp(window, std::make_shared<ofApp>());
    ofRunMainLoop();
}
