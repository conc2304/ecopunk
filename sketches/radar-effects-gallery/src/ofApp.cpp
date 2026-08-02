#include "ofApp.h"
#include <sstream>
#include <algorithm>

namespace {
	const int kCanvasWidth = 1280;
	const int kCanvasHeight = 720;
}

void ofApp::setup() {
	// 24fps per the handoff's target — on this desktop GPU this will
	// trivially hold; the number here is only useful as a sanity check that
	// the harness itself isn't pathologically expensive, not as a stand-in
	// for the Pi 3B numbers the doc actually needs.
	ofSetFrameRate(24);
	ofBackground(0);
	// All of this sketch's shaders use sampler2D/texture2D against
	// normalized [0,1] texcoords — see radar-pulse/src/ofApp.cpp's identical
	// comment for why this has to be called first.
	ofDisableArbTex();

#ifdef PLATFORM_PI
	videoSampler.setup("/home/pi/blueprint/media");
#else
	videoSampler.setup(ofToDataPath("sharedMedia", true));
#endif

	revealMask.setup(kCanvasWidth, kCanvasHeight, 0.965f);
	revealMask.setMaxOpacity(1.0f);

	compositor.setup(kCanvasWidth, kCanvasHeight);

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

	// A few fixed emitters, same placeholder-placement caveat as radar-pulse
	// (Section 6 of Handoff v2 — no resolved kiosk placement strategy yet).
	// Three-plus emitters also gives Mode 2 (per-emitter hue) more than one
	// color to show at once.
	pulseEmitter.addEmitter(kCanvasWidth * 0.25f, kCanvasHeight * 0.3f);
	pulseEmitter.addEmitter(kCanvasWidth * 0.75f, kCanvasHeight * 0.3f);
	pulseEmitter.addEmitter(kCanvasWidth * 0.5f, kCanvasHeight * 0.75f);
}

void ofApp::update() {
	float dt = ofGetLastFrameTime();

	videoSampler.update();
	pulseEmitter.update(dt);

	const hud::PulseEmitterConfig & cfg = pulseEmitter.getConfig();
	std::vector<RPPulseStamp> stamps;
	std::vector<GalleryPulseInfo> pulses;
	for (const auto & p : pulseEmitter.getActivePulses()) {
		RPPulseStamp stamp;
		stamp.x = p.x;
		stamp.y = p.y;
		stamp.radius = p.radius;
		stamp.bandWidth = cfg.bandWidth;
		stamp.freshness = 1.0f - ofClamp(p.radius / cfg.range, 0.0f, 1.0f);
		// Always tinted, regardless of the active mode — cheap, and only
		// Mode 2's shader actually reads maskTex.rgb, so this can't affect
		// any other mode's look.
		stamp.tint = compositor.getEmitterTint(p.emitterId);
		stamps.push_back(stamp);

		GalleryPulseInfo info;
		info.x = p.x;
		info.y = p.y;
		info.radius = p.radius;
		info.ageMs = p.ageMs;
		info.emitterId = p.emitterId;
		pulses.push_back(info);
	}

	revealMask.beginStamp();
	revealMask.stampPulses(stamps);
	revealMask.endStamp();
	revealMask.update();

	compositor.update(dt, pulses);
}

void ofApp::draw() {
	std::vector<GalleryPulseInfo> pulses;
	for (const auto & p : pulseEmitter.getActivePulses()) {
		pulses.push_back({ p.x, p.y, p.radius, p.ageMs, p.emitterId });
	}

	if (videoSampler.isReady()) {
		compositor.draw(videoSampler.getTexture(), revealMask.getMaskTexture(), pulses);
	} else {
		ofSetColor(255);
		revealMask.getMaskTexture().draw(0, 0, kCanvasWidth, kCanvasHeight);
	}

	pulseEmitter.draw();

	if (showOverlay) {
		std::stringstream ss;
		std::string name = compositor.getModeName();
		std::transform(name.begin(), name.end(), name.begin(), ::toupper);
		ss << "MODE " << compositor.getModeIndex() << "/" << (GalleryCompositor::NUM_MODES - 1) << " -- " << name << "\n";
		ss << "fps: " << ofGetFrameRate() << "\n";
		ss << "active pulses: " << pulses.size() << "\n";
		ss << "stamp blend: " << (revealMask.isUsingMaxBlend() ? "GL_MAX" : "sorted alpha-over fallback") << "\n";
		if (videoSampler.isReady()) ss << "clip: " << videoSampler.getCurrentFilename() << "\n";
		else ss << "(no media/*.mp4 found via sharedMedia/ -- showing mask+widget debug view)\n";
		ss << "[n] next mode   [p] prev mode   [d] toggle this overlay";
		ofSetColor(255);
		ofDrawBitmapStringHighlight(ss.str(), 20, 20);
	}
}

void ofApp::keyPressed(int key) {
	if (key == 'n') compositor.nextMode();
	if (key == 'p') compositor.prevMode();
	if (key == 'd') showOverlay = !showOverlay;
}
