#include "ofApp.h"
#include <sstream>

namespace {
	const int kCanvasWidth = 1280;
	const int kCanvasHeight = 720;
}

void ofApp::setup() {
	ofSetFrameRate(60);
	ofBackground(0);
	// All of this sketch's custom shaders (reveal_mask.frag, color_reveal.frag)
	// use sampler2D/texture2D against normalized [0,1] texcoords. Without
	// this, oF's default ARB rectangle textures on desktop GL don't match
	// that sampler type and sample incorrectly — same reason
	// quadrant-crosshair/blueprint_emergence/temporal-fields all call this
	// before doing custom texture2D-based shader work.
	ofDisableArbTex();

	// Shared media collection, same convention as quadrant-crosshair/
	// blueprint_emergence: an absolute path on the Pi, the local (symlinked)
	// bin/data folder otherwise.
#ifdef PLATFORM_PI
	videoSampler.setup("/home/pi/blueprint/media");
#else
	videoSampler.setup(ofToDataPath("sharedMedia", true));
#endif

	revealMask.setup(kCanvasWidth, kCanvasHeight, 0.965f);
	revealMask.setMaxOpacity(1.0f);

	compositor.setup();
	compositor.setColorThreshold(0.55f);

	hud::HudTheme theme;
	pulseEmitter.setTheme(theme);
	pulseEmitter.setBounds(0, 0, kCanvasWidth, kCanvasHeight);
	pulseEmitter.setup();

	hud::PulseEmitterConfig cfg;
	cfg.frequencyMs = 900.0f;
	cfg.speed = 90.0f;
	cfg.range = 260.0f;
	cfg.concurrency = 4;
	cfg.bandWidth = 60.0f;
	pulseEmitter.setConfig(cfg);

	// Emitter placement: the engineering handoff leaves "placement strategy
	// with no pointer input on a headless kiosk" as an open question
	// (Section 6). This is a placeholder — a handful of fixed positions —
	// not a resolved answer; revisit once that question is settled.
	pulseEmitter.addEmitter(kCanvasWidth * 0.25f, kCanvasHeight * 0.3f);
	pulseEmitter.addEmitter(kCanvasWidth * 0.75f, kCanvasHeight * 0.3f);
	pulseEmitter.addEmitter(kCanvasWidth * 0.5f, kCanvasHeight * 0.75f);
}

void ofApp::addEmitterAtRandomPosition() {
	pulseEmitter.addEmitter(ofRandom(kCanvasWidth), ofRandom(kCanvasHeight));
}

void ofApp::update() {
	float dt = ofGetLastFrameTime();

	uint64_t t0 = ofGetElapsedTimeMicros();
	videoSampler.update();
	uint64_t t1 = ofGetElapsedTimeMicros();
	msVideoUpdate = (t1 - t0) / 1000.0f;

	pulseEmitter.update(dt);

	uint64_t t2 = ofGetElapsedTimeMicros();
	const hud::PulseEmitterConfig & cfg = pulseEmitter.getConfig();
	std::vector<RPPulseStamp> stamps;
	for (const auto & p : pulseEmitter.getActivePulses()) {
		RPPulseStamp stamp;
		stamp.x = p.x;
		stamp.y = p.y;
		stamp.radius = p.radius;
		stamp.bandWidth = cfg.bandWidth;
		stamp.freshness = 1.0f - ofClamp(p.radius / cfg.range, 0.0f, 1.0f);
		stamps.push_back(stamp);
	}

	revealMask.beginStamp();
	revealMask.stampPulses(stamps);
	revealMask.endStamp();
	revealMask.update();
	uint64_t t3 = ofGetElapsedTimeMicros();
	msMaskStampAndDecay = (t3 - t2) / 1000.0f;
}

void ofApp::draw() {
	uint64_t t0 = ofGetElapsedTimeMicros();
	if (videoSampler.isReady()) {
		compositor.draw(videoSampler.getTexture(), revealMask.getMaskTexture(), kCanvasWidth, kCanvasHeight);
	} else {
		// No .mp4 found via bin/data/sharedMedia/ — fall back to a raw view
		// of the mask itself so pulse timing/decay can still be checked
		// without a video asset in place.
		ofSetColor(255);
		revealMask.getMaskTexture().draw(0, 0, kCanvasWidth, kCanvasHeight);
	}
	uint64_t t1 = ofGetElapsedTimeMicros();
	msComposite = (t1 - t0) / 1000.0f;

	pulseEmitter.draw();

	if (showDebugOverlay) {
		std::stringstream ss;
		ss << "fps: " << ofGetFrameRate() << "\n";
		ss << "video update: " << msVideoUpdate << " ms\n";
		ss << "mask stamp+decay: " << msMaskStampAndDecay << " ms\n";
		ss << "composite draw: " << msComposite << " ms\n";
		ss << "active pulses: " << pulseEmitter.getActivePulses().size() << "\n";
		ss << "stamp blend: " << (revealMask.isUsingMaxBlend() ? "GL_MAX" : "sorted alpha-over fallback") << "\n";
		if (!videoSampler.isReady()) ss << "(no media/*.mp4 found via sharedMedia/ — showing mask+widget debug view)\n";
		else ss << "clip: " << videoSampler.getCurrentFilename() << "\n";
		ss << "[a] add emitter   [d] toggle this overlay";
		ofSetColor(255);
		ofDrawBitmapStringHighlight(ss.str(), 20, 20);
	}
}

void ofApp::keyPressed(int key) {
	if (key == 'a') addEmitterAtRandomPosition();
	if (key == 'd') showDebugOverlay = !showDebugOverlay;
}
