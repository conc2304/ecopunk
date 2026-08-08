#include "ofApp.h"

void ofApp::setup() {
	runtime.setup();
	if (GlRestorationHarness::isRequested()) {
		harness = std::make_unique<GlRestorationHarness>(runtime);
	}
}

void ofApp::update() {
	runtime.update(ofGetLastFrameTime());
	if (harness) {
		harness->step(ofGetLastFrameTime());
	}
}

void ofApp::draw() {
	if (!harness) {
		runtime.draw();
	}
	// When the harness is active, it drives runtime.draw() itself (see
	// GlRestorationHarness::step()) so it can inspect GL state and grab
	// screenshots between specific, scripted draw calls.
}

void ofApp::exit() {
	runtime.exit();
}

void ofApp::keyPressed(int key) {
	runtime.keyPressed(key);
}
