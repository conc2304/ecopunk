#include "ofApp.h"
#include "../../../shared/src/video-playback/SharedMediaRoot.h"

void ofApp::setup() {
	ofSetVerticalSync(true);
	// Must run before any texture allocation (video, FBOs, etc.). Without
	// this, ofVideoPlayer's texture defaults to GL_TEXTURE_RECTANGLE_ARB
	// (non-normalized [0,W]x[0,H] texcoords) on desktop, but every fragment
	// shader here declares `sampler2D`/`texture2D()`, which assumes
	// normalized [0,1] coords — sampling wildly out of range then clamps to
	// the texture edge for virtually the whole frame, rendering as a flat,
	// detail-free fill. Matches quadrant-crosshair/blueprint_emergence/
	// temporal-fields, which all call this for the same reason.
	ofDisableArbTex();

	shaderLib.setup();

	TimeOffsetVideoBuffer::Settings settings;
	settings.numPlayheads = kNumPlayheads;
	videoBuffer.setup(ecopunk::sharedMediaRoot(), settings);

	fragmenttrail::FragmentSceneCore::Config config;
	config.canvasW = ofGetWidth();
	config.canvasH = ofGetHeight();
	config.numPlayheads = kNumPlayheads;
	config.trackWindow = true;
	core.setup(&videoBuffer, &shaderLib, config);
	core.activate();

	hudOverlayPanel.setup(fragmenttrail::FragmentSceneCore::initialOverlayDials());
}

void ofApp::update() {
	float dt = ofGetLastFrameTime();
	if (dt <= 0.f) return;

	videoBuffer.update(dt);
	hudOverlayPanel.update(core.overlayDials());
	core.update(dt);
}

void ofApp::draw() {
	core.draw();
	hudOverlayPanel.draw(); // self-gates on isVisible()
	core.parameters().draw();
}

void ofApp::keyPressed(int key) {
	core.modeController().onKeyPressed(key);
	if (key == 'h' || key == 'H') core.parameters().toggleVisible();
	if (key == 'p' || key == 'P') core.crosshair().nextPreset(); // cycle DRIFT/SCAN/HUNT/NERVOUS/ORBIT for live speed tuning
	// Unlike the other 3 sketches, 'o' does not swap the whole scene here --
	// the overlay always renders live-composited; this only shows/hides its
	// dial-tuning panel.
	if (key == 'o' || key == 'O') hudOverlayPanel.toggleVisible();
}

void ofApp::windowResized(int w, int h) {
	core.windowResized(w, h);
}
