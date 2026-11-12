#include "ofApp.h"
#include "BESettings.h"

int runCollageSelfTest(); // BECollageSelfTest.cpp (dev only)

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

	if (std::getenv("BE_COLLAGE_SELFTEST")) {
		const int failures = runCollageSelfTest();
		ofExit(failures == 0 ? 0 : 1);
		return;
	}

	// The artwork itself lives in BlueprintSceneCore (shared with
	// ExperienceRuntime's BlueprintProductionScene).
	BlueprintSceneCore::Config sceneConfig;
	sceneConfig.canvasW = ofGetWidth();
	sceneConfig.canvasH = ofGetHeight();
	sceneConfig.advanceMediaPerCycle = true;
	sceneConfig.editorialCollage = true; // standalone default; 'c' toggles back to classic
	scene.setup(&videoPlayback, sceneConfig);
	if (const char* prof = std::getenv("BE_COLLAGE_PROFILE")) scene.collage().setProfile(prof);
	scene.activate();

	if (const char* chart = std::getenv("BE_COLLAGE_UVCHART")) {
		// Asymmetric 512x512 test texture: gradient + a big "F" + coloured corners (TL red, TR green, BR blue, BL yellow).
		ofPixels px;
		px.allocate(512, 512, OF_PIXELS_RGB);
		for (int y = 0; y < 512; y++)
			for (int x = 0; x < 512; x++) {
				ofColor c(40 + x / 3, 40 + y / 3, 110);
				const bool bar = (x > 150 && x < 220 && y > 90 && y < 430) || (x >= 220 && x < 400 && y > 90 && y < 160) || (x >= 220 && x < 340 && y > 235 && y < 300);
				if (bar) c = ofColor(245);
				if (x < 60 && y < 60) c = ofColor(230, 40, 40);
				if (x > 451 && y < 60) c = ofColor(40, 200, 60);
				if (x > 451 && y > 451) c = ofColor(50, 90, 240);
				if (x < 60 && y > 451) c = ofColor(240, 220, 40);
				px.setColor(x, y, c);
			}
		uvChartTex.allocate(px);
		uvChartPath = chart;
	}

	if (const char* dir = std::getenv("BE_COLLAGE_CAPTURE_DIR")) {
		captureDir = dir;
		// 3 templates x 3 seeds; each seed also picks its own palette.
		const uint32_t seeds[3][3] = { { 11, 29, 47 }, { 5, 23, 61 }, { 8, 36, 73 } };
		for (int t = 0; t < BECollage::TEMPLATE_COUNT; t++)
			for (int k = 0; k < 3; k++)
				captureJobs.push_back({ seeds[t][k], t, true, "t" + ofToString(t) + "_seed" + ofToString(seeds[t][k]) });
		captureJobs.push_back({ 11, 0, false, "t0_seed11_placement_only" });
		captureWait = 90; // let the shared video decode its first frames
		scene.setEditorialCollage(true);
	}

	if (const char* dir = std::getenv("BE_COLLAGE_LIVE_DIR")) {
		liveDir = dir;
		if (const char* iv = std::getenv("BE_COLLAGE_LIVE_INTERVAL")) liveInterval = std::max(0.1f, static_cast<float>(std::atof(iv)));
		if (const char* st = std::getenv("BE_COLLAGE_LIVE_SECONDS")) liveStopAt = static_cast<float>(std::atof(st));
		scene.setEditorialCollage(true);
	}

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
	// Dev-only: BE_TIME_SCALE=<n> runs the scene clock n times faster (lifecycle checks).
	static const float timeScale = std::getenv("BE_TIME_SCALE") ? std::atof(std::getenv("BE_TIME_SCALE")) : 1.0f;
	dt *= timeScale;
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

	if (uvChartTex.isAllocated()) {
		scene.collage().debugDrawOrientationChart(&uvChartTex);
		if (++uvChartFrames == 3) {
			ofImage img;
			img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
			img.save(uvChartPath);
			ofLogNotice("uvchart") << "saved " << uvChartPath;
			ofExit(0);
		}
		return;
	}
	ofBackground(GROUND_DARK);
	scene.draw();
	if (!captureDir.empty()) runCaptureStep();
	if (!liveDir.empty()) {
		liveElapsed += ofGetLastFrameTime() * (std::getenv("BE_TIME_SCALE") ? std::atof(std::getenv("BE_TIME_SCALE")) : 1.0f);
		{
			const auto& C = scene.collage();
			static const char* act[] = { "QUIET", "ACTIVE", "SWELL", "SETTLE" };
			for (const auto& e : C.eventLog()) {
				if (e.t <= lastEventLogT) continue;
				ofLogNotice("collage-event") << "t=" << ofToString(e.t, 2) << " " << collageEventName(e.type) << " id=" << e.id
											 << " frags=" << e.fragments << " gfx=" << e.graphics << " activity=" << act[static_cast<int>(C.activity())]
											 << " level=" << ofToString(C.activityLevel(), 2) << " template=" << C.templateName();
				lastEventLogT = e.t;
			}
			static int fpsN = 0; static double fpsSum = 0, fpsMin = 1e9;
			if (liveElapsed > 5.0f) { fpsN++; fpsSum += ofGetFrameRate(); fpsMin = std::min(fpsMin, (double)ofGetFrameRate()); }
			if (liveStopAt > 0.0f && liveElapsed >= liveStopAt) {
				ofLogNotice("perf") << "desktop fps mean=" << (fpsN ? fpsSum / fpsN : 0) << " min=" << fpsMin << " frames=" << fpsN << " profile=" << C.tuning().name;
				ofExit(0);
			}
		}
		if (liveElapsed >= liveNextShot) {
			liveNextShot += liveInterval;
			ofImage img;
			img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
			img.save(liveDir + "/live_" + ofToString(liveShotIndex++, 3, '0') + "_t" + ofToString(static_cast<int>(liveElapsed)) + ".png");
		}
	}

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
			+ "  ('g' grid, 'r' restart, 'f' flip divider, 'c' collage)",
		12, 18);
}

//--------------------------------------------------------------
// Runs after scene.draw() each frame: grab what was just drawn for the current
// job, then queue the next one. The first frames only wait for video.
void ofApp::runCaptureStep() {
	if (captureWait > 0) {
		captureWait--;
		return;
	}
	if (captureIndex > 0 && captureIndex <= captureJobs.size()) {
		// The frame on screen belongs to job captureIndex-1 (set up last frame).
		const CaptureJob& job = captureJobs[captureIndex - 1];
		ofImage img;
		img.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
		const std::string path = captureDir + "/" + job.name + ".png";
		img.save(path);
		const auto& C = scene.collage();
		int nFrag = 0;
		for (const auto& f : C.elements()) nFrag += f.video ? 1 : 0;
		ofLogNotice("capture") << path << "  template=" << C.templateName() << " palette=" << C.layout().palette.name
							   << " fragments=" << nFrag << " elements=" << C.elements().size();
		for (const auto& f : C.elements()) {
			if (!f.video) continue;
			const glm::vec2 dc(f.rect.getCenter().x / ofGetWidth(), f.rect.getCenter().y / ofGetHeight());
			ofLogNotice("capture") << "   frag " << f.labelTag << " turn=" << static_cast<int>(f.crop.rot) * 90
								   << " flipX=" << f.crop.flipX << " flipY=" << f.crop.flipY << " destC=(" << dc.x << "," << dc.y
								   << ") srcC=(" << f.crop.center.x << "," << f.crop.center.y << ") dist=" << glm::length(dc - f.crop.center);
		}
	}
	if (captureIndex >= captureJobs.size()) {
		ofLogNotice("capture") << "done";
		ofExit(0);
		return;
	}
	const CaptureJob& next = captureJobs[captureIndex++];
	scene.debugShowCollage(next.seed, next.templateIndex, next.density);
	captureWait = 2; // draw once with the new layout, then grab
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
	} else if (key == 'c') {
		scene.setEditorialCollage(!scene.editorialCollage());
		ofLogNotice("ofApp") << "editorialCollage = " << scene.editorialCollage();
	} else if (key == 'p') {
		static int profileIdx = 1;
		static const char* names[] = { "quiet", "balanced", "kinetic" };
		profileIdx = (profileIdx + 1) % 3;
		scene.collage().setProfile(names[profileIdx]);
		ofLogNotice("ofApp") << "collage profile = " << scene.collage().tuning().name;
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
