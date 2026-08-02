#include "ofApp.h"

namespace {
// TriggerBus's HIGH_THRESH/LOW_THRESH are private (this class was copied
// verbatim from quadrant-crosshair per the implementation plan's reuse
// decision — not modified to expose them). Duplicated here rather than
// changing that class's encapsulation; keep in sync with TriggerBus.h if
// those values ever change.
constexpr float kSpeedHighThresh = 4.0f;
constexpr float kSpeedLowThresh = 0.8f;
}

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

	crosshair.setup();
	// Preset 2 ("HUNT") — highest freqA/amplitude combination of the 5
	// presets, i.e. the fastest sweep across the canvas. Default preset 0
	// ("DRIFT") was too slow to cross the 70px spawn-distance threshold at
	// a noticeable rate (user feedback: no fragments visibly appearing).
	crosshair.setPreset(2);
	triggerBus.setup();
	lfoBank.setup();
	shaderLib.setup();
	modeController.setup();
	paramPanel.setup();

	triggerBus.addListener([this](const TriggerEvent& e) {
		modeController.onTriggerEvent(e);
	});

	TimeOffsetVideoBuffer::Settings settings;
	settings.numPlayheads = kNumPlayheads;
	videoBuffer.setup("media", settings);

	// Full-canvas backdrop beneath the fragment pool — dimmed raw video split
	// across two sections (one alternates with a background image, when
	// bin/data/backgrounds/ has any — see FTImageCycler::hasImages()), plus a
	// fixed ambient hex-grid HUD widget. Same "underlay" role as
	// temporal-fields' TFBackgroundLayer/TFHudLayer::drawUnderlay().
	backgroundLayer.setup(&videoBuffer, "backgrounds", ofGetWidth(), ofGetHeight());

	pool.setup(paramPanel.getMaxFragments(), paramPanel.getSpawnMinDistance(), paramPanel.getSpawnMinInterval(),
		paramPanel.getIdlePulseEnabled(), paramPanel.getIdlePulseInterval(),
		paramPanel.getSustainSeconds(), paramPanel.getDecaySeconds(), paramPanel.getMinSize(), paramPanel.getMaxSize(),
		kNumPlayheads);

	hudOverlay.setup(ofGetWidth(), ofGetHeight());

	hudoverlay::HudOverlayDialState initialDials;
	initialDials.intensity = 0.18f;     // low -- ~one organism-tier event per 10-20s against a ~3.2s fragment lifespan
	initialDials.discipline = 0.85f;    // high (see FTOverlayDirector: not load-bearing here yet, kept high for if it ever is)
	initialDials.eventCoupling = 0.15f; // low -- reinforces the one-organism-at-a-time restraint
	hudOverlayPanel.setup(initialDials);

	hudOverlay.setSchedulerEnabled(false); // driven by real fragment events (FTOverlayDirector), not ambient random timing
	hudOverlay.setFaultCascadeHalftoneEnabled(false); // tear+flash only for the eviction gesture, per the design brief
	overlayDirector.setup(&pool, &hudOverlay);
}

void ofApp::update() {
	float dt = ofGetLastFrameTime();
	if (dt <= 0.f) return;

	lfoBank.update(dt);
	crosshair.applyLiveSettings(paramPanel.getCrosshairSpeed(), paramPanel.getCrosshairSmoothness(),
		paramPanel.getCrosshairSpeedVariance(), paramPanel.getCrosshairVarianceRate(),
		paramPanel.getCrosshairPattern(), paramPanel.getCrosshairAutoCycle(),
		paramPanel.getCrosshairAutoCycleInterval());
	crosshair.update(dt, lfoBank);
	triggerBus.update(crosshair.getState(), dt);
	videoBuffer.update(dt);
	backgroundLayer.update(dt);

	modeController.setDwellDuration(paramPanel.getModeDwellDuration());
	modeController.update(dt);

	pool.applyLiveSettings(paramPanel.getMaxFragments(), paramPanel.getSustainSeconds(), paramPanel.getDecaySeconds(),
		paramPanel.getMinSize(), paramPanel.getMaxSize(),
		paramPanel.getIdlePulseEnabled(), paramPanel.getIdlePulseInterval());

	// speedResponse (Section 3): normalize crosshair speed into a 0-1
	// "energy" value via TriggerBus's own high/low thresholds, then scale
	// spawn cadence — faster movement lowers the effective distance/
	// interval thresholds (denser trail while "searching"), slower raises
	// them (sparser, more deliberate emergence).
	float speed = crosshair.getState().speed;
	float energy01 = ofClamp(ofMap(speed, kSpeedLowThresh, kSpeedHighThresh, 0.f, 1.f, true), 0.f, 1.f);
	float responseAmount = paramPanel.getSpeedResponse();
	float energyScale = ofClamp(1.0f - energy01 * responseAmount, 0.2f, 1.0f);
	float effectiveMinDistance = paramPanel.getSpawnMinDistance() * energyScale;
	float effectiveMinInterval = paramPanel.getSpawnMinInterval() * energyScale;

	pool.update(dt, crosshair.getState(), videoBuffer, modeController.getCurrentMode(),
		modeController.getModeCEffectName(), effectiveMinDistance, effectiveMinInterval);
	// ^ spawn/evict callbacks fire synchronously above, into overlayDirector's gates.

	hudOverlayPanel.update(hudOverlayDials);
	overlayDirector.update(dt, hudOverlayDials);
	hudOverlay.update(dt, hudOverlayDials); // last, so anything triggered above gets its first tick correctly
}

void ofApp::draw() {
	ofBackground(8, 10, 9);
	backgroundLayer.draw();
	hudOverlay.drawAmbient(); // BELOW: scanline, radar station, breathing ticks, telemetry -- "the room"
	pool.draw(videoBuffer, shaderLib);
	hudOverlay.drawOrganisms(); // BETWEEN/ABOVE: tick-burst accents + the 4 organisms, anchored to live fragment bounds
	crosshair.draw();
	hudOverlayPanel.draw(); // self-gates on isVisible()
	paramPanel.draw();
}

void ofApp::keyPressed(int key) {
	modeController.onKeyPressed(key);
	if (key == 'h' || key == 'H') paramPanel.toggleVisible();
	if (key == 'p' || key == 'P') crosshair.nextPreset(); // cycle DRIFT/SCAN/HUNT/NERVOUS/ORBIT for live speed tuning
	// Unlike the other 3 sketches, 'o' does not swap the whole scene here --
	// the overlay always renders live-composited; this only shows/hides its
	// dial-tuning panel.
	if (key == 'o' || key == 'O') hudOverlayPanel.toggleVisible();
}

void ofApp::windowResized(int w, int h) {
	hudOverlay.windowResized(w, h);
}
