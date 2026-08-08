#include "ofApp.h"
#include "BELFOLanes.h"
#include "BESettings.h"
#include "BETriggers.h"
#include "hud/HudElements.h"
#include <cmath>

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

	int cw = ofGetWidth();
	int ch = ofGetHeight();
	float dividerX = static_cast<float>(cw) * DIVIDER_X_FRACTION;
	grid.setup(cw, ch, dividerX);
	annotations.setup(&grid, cw, ch);
	annotations.loadCodeFont(CODE_FONT_PATH, SIZE_CODE);
	annotations.setMeasurementLineTiming(MLINE_DRAW_SPEED, MLINE_FADE_DELAY, MLINE_FADE_OPACITY);
	annotations.setCodeTextTiming(CODE_TEXT_INTERVAL_MIN, CODE_TEXT_INTERVAL_MAX, CODE_TEXT_OPACITY_MIN, CODE_TEXT_OPACITY_MAX);
	annotations.setCodeFragments(loadCodeFragments());

	Fragment::loadFragmentShader("shaders/fragmentEffects.vert", "shaders/fragmentEffects.frag");

	// Migrated from VideoSampler to the shared VideoPlaybackService —
	// Shared Video Playback, Engineering Session 2, Task G. mediaRoot
	// points at the canonical physical root (assets/shared/media/,
	// established this session — see that directory and
	// docs/shared-video-playback-system-implementation-report.md /
	// docs/shared-video-playback-engineering-session-2-report.md), not
	// this sketch's own bin/data/media/ (which now holds only
	// compatibility symlinks into the canonical root — see this session's
	// report, "Remaining compatibility paths"). automaticAdvance is
	// deliberately false: BEComposition's onCycleStart callback below
	// drives every media change (one per composition cycle), exactly
	// matching the pre-migration VideoSampler::selectVideoForCycle()
	// timing — enabling the shared service's own hold-timer-driven
	// automatic advance in addition would double-advance media on an
	// unrelated cadence, a real behavior change this migration does not
	// make (see this session's report, "Composition-cycle behavior").
	VideoPlaybackService::Config videoConfig;
	videoConfig.mediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	videoConfig.automaticAdvance = false;
	videoPlayback.setup(videoConfig);

	// ── Vitality systems (LFOBank / TriggerBus / GridState / ErosionFBO) ──
	shaderLib.setup();
	gridState.setup(GRID_COLS, GRID_ROWS, GRIDSTATE_DECAY_RATE);
	erosionFBO.setup(cw, ch, EROSION_DECAY_RATE);

	motionEx.setup();
	motionEx.setOutputMode(0); // LUMA_GLOW
	motionEx.setReferenceMode(MotionExtraction::REFERENCE_ACCUMULATION);

	lfoBank.setup(static_cast<int>(BELFOLane::COUNT));
	lfoBank.setFrequency(static_cast<int>(BELFOLane::GRID_OPACITY), LFO_GRID_OPACITY_HZ);
	lfoBank.setFrequency(static_cast<int>(BELFOLane::DIVIDER_BRIGHTNESS), LFO_DIVIDER_BRIGHTNESS_HZ);
	lfoBank.setFrequency(static_cast<int>(BELFOLane::PLACEMENT_BIAS), LFO_PLACEMENT_BIAS_HZ);
	lfoBank.setFrequency(static_cast<int>(BELFOLane::FRAG_DESAT_OFFSET_A), LFO_FRAG_DESAT_A_HZ);
	lfoBank.setFrequency(static_cast<int>(BELFOLane::FRAG_DESAT_OFFSET_B), LFO_FRAG_DESAT_B_HZ);
	lfoBank.setFrequency(static_cast<int>(BELFOLane::CODE_TEXT_WEIGHT), LFO_CODE_TEXT_WEIGHT_HZ);
	for (int i = 0; i < static_cast<int>(BELFOLane::COUNT); i++) {
		lfoBank.setPhaseOffset(i, ofRandom(TWO_PI));
	}

	triggerBus.setup(static_cast<int>(BETrigger::COUNT));
	triggerBus.setCooldown(static_cast<int>(BETrigger::ZONE_IMBALANCE), TRIGGER_ZONE_IMBALANCE_CD);
	triggerBus.setCooldown(static_cast<int>(BETrigger::DENSITY_HIGH), TRIGGER_DENSITY_HIGH_CD);
	triggerBus.setCooldown(static_cast<int>(BETrigger::DENSITY_CRITICAL), TRIGGER_DENSITY_CRIT_CD);
	triggerBus.setCooldown(static_cast<int>(BETrigger::LONG_SILENCE), TRIGGER_LONG_SILENCE_CD);
	triggerBus.setCooldown(static_cast<int>(BETrigger::PHASE_TRANSITION), TRIGGER_PHASE_TRANSITION_CD);
	triggerBus.setCooldown(static_cast<int>(BETrigger::MEASUREMENT_HUB), TRIGGER_MEASUREMENT_HUB_CD);
	triggerBus.setCooldown(static_cast<int>(BETrigger::CIRCLE_PLACED), 0.0f);
	triggerBus.setCooldown(static_cast<int>(BETrigger::CYCLE_START), 0.0f);

	composition.setupBE(&grid, &videoPlayback, cw, ch);
	composition.setLFOBank(&lfoBank);
	composition.setTriggerBus(&triggerBus); // also registers BEComposition's own listeners
	composition.setGridState(&gridState);
	composition.setShaderLibrary(&shaderLib);
	composition.setMotionExtraction(&motionEx);

	wireTriggerResponses();

	composition.setOnFragmentPlaced([this](Fragment * newFrag, Fragment * nearest) {
		annotations.onFragmentPlaced(newFrag, nearest);
	});
	composition.setOnFragmentRemoved([this](Fragment * frag) {
		annotations.onFragmentRemoved(frag);
	});
	composition.setOnCycleStart([this]() {
		// Pre-migration this called videoSampler.cancelPending() (defensive
		// guard against an in-flight seek-and-freeze still-frame capture,
		// see VideoSampler::requestCapture()) before
		// videoSampler.selectVideoForCycle(). Dropped here, not silently:
		// this scene never actually called requestCapture() anywhere (only
		// getTexture()/getPixels()/getVideoWidth()/getVideoHeight() — the
		// live-frame accessors — see this session's report, "Still-frame
		// capture behavior before and after"), so there was never anything
		// for cancelPending() to cancel. VideoPlaybackService has no
		// equivalent pending-capture concept to guard here for the same
		// reason. One media advance per composition cycle is preserved via
		// next() — see this scene's videoConfig.automaticAdvance = false
		// comment in setup() for why this is the only trigger.
		if (!videoPlayback.next()) {
			ofLogWarning("ofApp") << "onCycleStart: VideoPlaybackService::next() failed (media unavailable?)";
		}
		annotations.reset();
		erosionFBO.clear();
		gridPulseBoost = 0.0f;
		for (int i = 0; i < static_cast<int>(BELFOLane::COUNT); i++) {
			lfoBank.setPhaseOffset(i, ofRandom(TWO_PI));
		}
		triggerBus.fireImmediate(static_cast<int>(BETrigger::CYCLE_START));
	});
	composition.setOnPhaseChanged([this](CompositionBase::CyclePhase newPhase) {
		static const char * phaseNames[] = { "BLANK", "PLACEMENT", "DENSITY", "DISSOLVE", "RESET_HOLD" };
		ofLogNotice("ofApp") << "phase -> " << phaseNames[static_cast<int>(newPhase)]
							 << " at t=" << ofGetElapsedTimef();
		gridPulseBoost = 0.8f;
		triggerBus.fireImmediate(static_cast<int>(BETrigger::PHASE_TRANSITION));
	});

	composition.startCycle();

	hudOverlay.setup(cw, ch);
	hudOverlayPanel.setup();
}

//--------------------------------------------------------------
void ofApp::wireTriggerResponses() {
	triggerBus.addListener(static_cast<int>(BETrigger::LONG_SILENCE), [this](int) {
		annotations.triggerCodeTextSpawn();
	});
	triggerBus.addListener(static_cast<int>(BETrigger::ZONE_IMBALANCE), [this](int) {
		annotations.triggerCodeTextSpawn();
	});
	triggerBus.addListener(static_cast<int>(BETrigger::CIRCLE_PLACED), [this](int) {
		annotations.pulseMeasurementLines(0.5f, 3.0f);
	});
}

//--------------------------------------------------------------
std::vector<std::string> ofApp::loadCodeFragments() const {
	std::vector<std::string> lines;
	ofBuffer buffer = ofBufferFromFile("codefragments.txt");
	for (const auto & line : buffer.getLines()) {
		if (!line.empty()) {
			lines.push_back(line);
		}
	}
	return lines;
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
	lfoBank.update(dt);
	composition.update(dt); // drives GridState decay, trigger condition checks, LFO desat nudges
	triggerBus.update(dt); // ticks cooldown timers
	annotations.update(dt);

	triggerBus.setConditionActive(static_cast<int>(BETrigger::MEASUREMENT_HUB),
		annotations.getMaxConnectionCount() >= TRIGGER_HUB_MIN_CONNECTIONS);
	annotations.setCodeTextWeight(lfoBank.getUnipolar(static_cast<int>(BELFOLane::CODE_TEXT_WEIGHT)));

	bool hasHudWidget = (composition.getActiveHudWidget() != nullptr);
	if (hasHudWidget && !hadHudWidget) {
		annotations.clearCodeTextInRect(composition.getActiveHudWidget()->getBounds().rect());
	}
	hadHudWidget = hasHudWidget;

	gridPulseBoost *= std::exp(-dt / 0.8f);

	// Motion overlay — driven by blueprint_emergence's own state (GridState's
	// average activity + composition phase) rather than a crosshair signal.
	const ofTexture* liveTexForMotion = videoPlayback.currentTexture();
	if (liveTexForMotion != nullptr) {
		float activity = gridState.getAverageActivity();
		float decayWeight = ofMap(activity, 0.0f, 1.0f, MOTION_DECAY_MIN, MOTION_DECAY_MAX, true);
		float sensitivity = ofMap(activity, 0.0f, 1.0f, MOTION_SENSITIVITY_MIN, MOTION_SENSITIVITY_MAX, true);
		motionEx.update(*liveTexForMotion, decayWeight, sensitivity);

		using Phase = CompositionBase::CyclePhase;
		Phase phase = composition.getPhase();
		float phaseFactor = (phase == Phase::PLACEMENT || phase == Phase::DENSITY) ? 1.0f : 0.0f;
		float targetOpacity = phaseFactor * ofMap(activity, 0.0f, 1.0f, MOTION_OPACITY_MIN, MOTION_OPACITY_MAX, true);
		motionOverlayOpacity += (targetOpacity - motionOverlayOpacity) * ofClamp(dt / MOTION_OPACITY_SMOOTH_SECONDS, 0.0f, 1.0f);
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	if (hudOverlayActive) {
		ofBackground(13, 13, 13);
		hudOverlay.draw();
		hudOverlayPanel.draw();
		return;
	}

	// The whole scene draws into ErosionFBO's capture buffer, which is then
	// blended against its own decaying history and presented — recent draws
	// leave a fading trace rather than vanishing the instant they stop being
	// drawn. See shared/src/ErosionFBO.h.
	// TEMP DIAGNOSTIC: press 'e' to bypass erosion entirely and draw the
	// scene straight to screen, to isolate whether erosion compositing is
	// still the problem or whether drawScene() itself is broken.
	if (bypassErosion) {
		drawScene();
	} else {
		erosionFBO.beginCapture();
		drawScene();
		erosionFBO.endCapture();
		erosionFBO.update();
		erosionFBO.draw(0, 0, ofGetWidth(), ofGetHeight());
	}

	// Fragment shader-effect overlays draw in their own pass, after the
	// erosion capture is closed. They use their own FBOs internally (to get
	// clean local UVs for crop-sampled effects), and ofFbo::end() resets the
	// matrix/viewport to the window unconditionally rather than back to
	// whatever FBO was bound before — nesting them inside ErosionFBO's
	// capture would silently corrupt everything drawn afterward in that FBO.
	for (const auto & fragment : composition.getFragments()) {
		fragment->drawOverlay();
	}

	// Divider drawn outside the FBO and after all overlays so it is always
	// the topmost layer — video content and shader effects cannot bleed over it.
	{
		ofEnableAlphaBlending();
		float divBrightness = ofMap(lfoBank.getUnipolar(static_cast<int>(BELFOLane::DIVIDER_BRIGHTNESS)),
			0.0f, 1.0f, LFO_DIVIDER_MIN, LFO_DIVIDER_MAX);
		auto divEndpoints = composition.getDividerEndpoints();
		annotations.drawDivider(divEndpoints.first, divEndpoints.second, divBrightness);
	}

	if (showOccupancyDebug) {
		drawOccupancyDebug();
	}

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
void ofApp::drawScene() {
	// LFO-driven grid opacity.
	// RULE_WHITE already carries ~30% baseline alpha; normalize the LFO's
	// 0.20–0.45 opacity range against that baseline since drawGrid(alpha)
	// is a multiplier on top of it, not a raw alpha value.
	float lfoFrac = ofMap(lfoBank.getUnipolar(static_cast<int>(BELFOLane::GRID_OPACITY)),
		0.0f, 1.0f, LFO_GRID_OPACITY_MIN, LFO_GRID_OPACITY_MAX);
	float gridAlpha = lfoFrac / 0.30f;
	gridAlpha *= (1.0f - composition.getGridDimAmount()); // DENSITY_CRITICAL response
	gridAlpha += gridPulseBoost; // PHASE_TRANSITION response

	ofBackground(GROUND_DARK);

	// Alpha blending stays on for the whole scene — fragment arrival/dissolve
	// fades, the desaturate shader's alpha output, HUD widget panels, and
	// annotation fades all depend on it. (Previously this was only toggled
	// on/off narrowly around the motion overlay draw below, which left
	// blending OFF for every draw call after it — including the fragments
	// loop — once motion overlay opacity ramped above zero.)
	ofEnableAlphaBlending();

	if (composition.isDividerHorizontal()) {
		float dividerY = composition.getDividerY();
		ofSetColor(composition.isZoneALight() ? GROUND_LIGHT : GROUND_DARK);
		ofDrawRectangle(0, 0, ofGetWidth(), dividerY);
		ofSetColor(GROUND_DARK);
		ofDrawRectangle(0, dividerY, ofGetWidth(), ofGetHeight() - dividerY);
	} else {
		float dividerX = grid.getDividerX();
		ofSetColor(composition.isZoneALight() ? GROUND_LIGHT : GROUND_DARK);
		ofDrawRectangle(0, 0, dividerX, ofGetHeight());
		ofSetColor(GROUND_DARK);
		ofDrawRectangle(dividerX, 0, ofGetWidth() - dividerX, ofGetHeight());
	}

	// Motion overlay sits behind the grid/fragments — a soft glow of recent
	// motion in the source video, intensity tracking composition activity.
	if (motionOverlayOpacity > 0.001f) {
		ofSetColor(255, 255, 255, static_cast<int>(255 * motionOverlayOpacity));
		motionEx.getMotionTexture().draw(0, 0, ofGetWidth(), ofGetHeight());
	}

	for (const auto & fragment : composition.getFragments()) {
		fragment->draw();
	}

	if (hud::HudWidget * widget = composition.getActiveHudWidget()) {
		widget->draw();
	}

	if (hud::HudWidget * scanner = composition.getCircleScannerWidget()) {
		scanner->draw();
	}

	annotations.drawMeasurementLines();
	annotations.drawCodeText();
	annotations.drawHubHighlight();

	annotations.drawGrid(gridAlpha);
}

//--------------------------------------------------------------
void ofApp::drawOccupancyDebug() const {
	ofSetColor(255, 0, 0, 90);
	for (const auto & rect : grid.getOccupiedRects()) {
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
		composition.startCycle();
	} else if (key == 'e') {
		bypassErosion = !bypassErosion;
		ofLogNotice("ofApp") << "bypassErosion = " << bypassErosion;
	} else if (key == 'f') {
		composition.forceAxisFlip();
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
