#include "ofApp.h"
#include "BESettings.h"

//--------------------------------------------------------------
void ofApp::setup() {
	// !! DO NOT REMOVE — removing this call causes every fragment to go black
	// !! the moment its slide-in animation ends.
	//
	// Force GL_TEXTURE_2D globally so texture2D() in GLSL shaders receives
	// normalized [0,1] UV coords. oF defaults to GL_TEXTURE_RECTANGLE_ARB which
	// passes pixel-space coords; texture2D() then GL_REPEAT-wraps values like
	// (512,288) to (0,0), sampling the top-left (black) corner of every texture
	// instead of the video frame. drawSlideIn() avoids the issue because it binds
	// no custom shader, but Fragment::drawTexturedRect() does — so the bug only
	// surfaces once a fragment leaves ARRIVING state. Must precede ALL texture/FBO
	// allocations (motionEx.setup, erosionFBO.setup, videoSampler, etc.).
	ofDisableArbTex();

	ofSetFrameRate(TARGET_FPS);

	// Migrated from VideoSampler to the shared VideoPlaybackService —
	// Shared Video Playback, Engineering Session 2, Task G. mediaRoot
	// points at the canonical physical root (assets/shared/media/). automaticAdvance is
	// deliberately false: the scene core's composition-cycle hook drives
	// every media change in this standalone sketch (one per composition
	// cycle — BlueprintSceneCore::Config::advanceMediaPerCycle), exactly
	// matching the pre-migration VideoSampler::selectVideoForCycle() timing.
	VideoPlaybackService::Config videoConfig;
	videoConfig.mediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	videoConfig.automaticAdvance = false;
	videoPlayback.setup(videoConfig);

	// The artwork itself lives in BlueprintSceneCore (shared with
	// ExperienceRuntime's BlueprintProductionScene).
	BlueprintSceneCore::Config sceneConfig;
	sceneConfig.canvasW = ofGetWidth();
	sceneConfig.canvasH = ofGetHeight();
	sceneConfig.advanceMediaPerCycle = true;
	scene.setup(&videoPlayback, sceneConfig);
	scene.activate();

	hudOverlay.setup(sceneConfig.canvasW, sceneConfig.canvasH);
	hudOverlayPanel.setup();
}

//--------------------------------------------------------------
void ofApp::update() {
	if (hudOverlayActive) {
		hudOverlayPanel.update(hudOverlayDials);
		hudOverlay.update(ofGetLastFrameTime(), hudOverlayDials);
		return;
	}

	float dt = ofGetLastFrameTime();
	videoPlayback.update(dt);
	scene.update(dt);
}

//--------------------------------------------------------------
void ofApp::draw() {
	if (hudOverlayActive) {
		ofBackground(13, 13, 13);
		hudOverlay.draw();
		hudOverlayPanel.draw();
		return;
	}

	ofBackground(GROUND_DARK);
	scene.draw();

	if (showOccupancyDebug) {
		drawOccupancyDebug();
	}

	const BEComposition& composition = scene.composition();
	ofSetColor(TEXT_DIM);
	ofDrawBitmapString("fps " + ofToString(ofGetFrameRate(), 1)
			+ "  zone a: " + (composition.isZoneALight() ? "light" : "dark")
			+ "  div: " + (composition.isDividerHorizontal() ? "horiz" : "vert")
			+ "  fragments: " + ofToString(composition.getFragments().size())
			+ "  seed: " + ofToString(composition.getCycleSeed())
			+ "  ('g' grid, 'r' restart, 'f' flip divider)",
		12, 18);
}

//--------------------------------------------------------------
void ofApp::drawOccupancyDebug() const {
	ofSetColor(255, 0, 0, 90);
	for (const auto & rect : scene.grid().getOccupiedRects()) {
		ofDrawRectangle(rect);
	}
}

//--------------------------------------------------------------
void ofApp::exit() {
	// This standalone sketch has no RuntimeServices to own videoPlayback's
	// lifetime (that only exists inside the separate experience_runtime
	// harness — see Shared Video Playback Engineering Session 2's
	// implementation report) — ofApp is the only owner here, so ofApp is
	// responsible for an explicit shutdown() rather than relying on
	// destructor order alone.
	videoPlayback.shutdown();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == 'o' || key == 'O') {
		hudOverlayActive = !hudOverlayActive;
		return;
	}
	if (key == 'g') {
		showOccupancyDebug = !showOccupancyDebug;
	} else if (key == 'r') {
		scene.restartCycle();
	} else if (key == 'e') {
		scene.setBypassErosion(!scene.bypassErosion());
		ofLogNotice("ofApp") << "bypassErosion = " << scene.bypassErosion();
	} else if (key == 'f') {
		scene.forceAxisFlip();
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
	hudOverlay.windowResized(w, h);
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
}
