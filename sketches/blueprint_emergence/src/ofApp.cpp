#include "ofApp.h"
#include "BELFOLanes.h"
#include "BESettings.h"
#include "BETriggers.h"
#include "hud/HudWidget.h"
#include <cmath>

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetFrameRate(TARGET_FPS);

	grid.setup(CANVAS_W, CANVAS_H, DIVIDER_X);
	annotations.setup(&grid, CANVAS_W, CANVAS_H);
	annotations.loadCodeFont(CODE_FONT_PATH, SIZE_CODE);
	annotations.setMeasurementLineTiming(MLINE_DRAW_SPEED, MLINE_FADE_DELAY, MLINE_FADE_OPACITY);
	annotations.setCodeTextTiming(CODE_TEXT_INTERVAL_MIN, CODE_TEXT_INTERVAL_MAX, CODE_TEXT_OPACITY_MIN, CODE_TEXT_OPACITY_MAX);
	annotations.setCodeFragments(loadCodeFragments());

	Fragment::loadFragmentShader("shaders/fragmentEffects.vert", "shaders/fragmentEffects.frag");
	videoSampler.setup(MEDIA_PATH);

	// ── Vitality systems (LFOBank / TriggerBus / GridState / ErosionFBO) ──
	shaderLib.setup();
	gridState.setup(GRID_COLS, GRID_ROWS, GRIDSTATE_DECAY_RATE);
	erosionFBO.setup(CANVAS_W, CANVAS_H, EROSION_DECAY_RATE);

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

	composition.setupBE(&grid, &videoSampler, CANVAS_W, CANVAS_H);
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
		videoSampler.cancelPending(); // drop in-flight captures before fragments.clear() destroys their targets
		videoSampler.selectVideoForCycle();
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
	float dt = ofGetLastFrameTime();
	videoSampler.update();
	lfoBank.update(dt);
	composition.update(dt); // drives GridState decay, trigger condition checks, LFO desat nudges
	triggerBus.update(dt); // ticks cooldown timers
	annotations.update(dt);

	triggerBus.setConditionActive(static_cast<int>(BETrigger::MEASUREMENT_HUB),
		annotations.getMaxConnectionCount() >= TRIGGER_HUB_MIN_CONNECTIONS);
	annotations.setCodeTextWeight(lfoBank.getUnipolar(static_cast<int>(BELFOLane::CODE_TEXT_WEIGHT)));

	gridPulseBoost *= std::exp(-dt / 0.8f);

	// Motion overlay — driven by blueprint_emergence's own state (GridState's
	// average activity + composition phase) rather than a crosshair signal.
	if (videoSampler.hasMedia()) {
		float activity = gridState.getAverageActivity();
		float decayWeight = ofMap(activity, 0.0f, 1.0f, MOTION_DECAY_MIN, MOTION_DECAY_MAX, true);
		float sensitivity = ofMap(activity, 0.0f, 1.0f, MOTION_SENSITIVITY_MIN, MOTION_SENSITIVITY_MAX, true);
		motionEx.update(videoSampler.getTexture(), decayWeight, sensitivity);

		using Phase = CompositionBase::CyclePhase;
		Phase phase = composition.getPhase();
		float phaseFactor = (phase == Phase::PLACEMENT || phase == Phase::DENSITY) ? 1.0f : 0.0f;
		float targetOpacity = phaseFactor * ofMap(activity, 0.0f, 1.0f, MOTION_OPACITY_MIN, MOTION_OPACITY_MAX, true);
		motionOverlayOpacity += (targetOpacity - motionOverlayOpacity) * ofClamp(dt / MOTION_OPACITY_SMOOTH_SECONDS, 0.0f, 1.0f);
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	using Phase = CompositionBase::CyclePhase;
	Phase phase = composition.getPhase();

	if (phase == Phase::RESET_HOLD) {
		ofBackground(ofColor::black);
		ofSetColor(TEXT_DIM);
		ofDrawBitmapString("fps " + ofToString(ofGetFrameRate(), 1) + "  (reset hold)", 12, 18);
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
		erosionFBO.draw(0, 0, CANVAS_W, CANVAS_H);
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

	if (showOccupancyDebug) {
		drawOccupancyDebug();
	}

	ofSetColor(TEXT_DIM);
	ofDrawBitmapString("fps " + ofToString(ofGetFrameRate(), 1)
			+ "  zone a: " + (composition.isZoneALight() ? "light" : "dark")
			+ "  fragments: " + ofToString(composition.getFragments().size())
			+ "  seed: " + ofToString(composition.getCycleSeed())
			+ "  ('g' grid debug, 'r' restart cycle)",
		12, 18);
}

//--------------------------------------------------------------
void ofApp::drawScene() {
	using Phase = CompositionBase::CyclePhase;
	Phase phase = composition.getPhase();

	float dividerProgress = 1.0f;
	float gridAlpha = 1.0f;
	bool labelShown = true;

	if (phase == Phase::BLANK) {
		float elapsed = composition.getPhaseElapsed();
		dividerProgress = ofClamp(elapsed / DIVIDER_DRAW_DURATION, 0.0f, 1.0f);
		float gridElapsed = elapsed - DIVIDER_DRAW_DURATION;
		gridAlpha = ofClamp(gridElapsed / GRID_FADEIN_DURATION, 0.0f, 1.0f);
		labelShown = elapsed >= (DIVIDER_DRAW_DURATION + GRID_FADEIN_DURATION);
	} else {
		// RULE_WHITE already carries ~30% baseline alpha; normalize the LFO's
		// 0.20-0.45 opacity range against that baseline since drawGrid(alpha)
		// is a multiplier on top of it, not a raw alpha value.
		float lfoFrac = ofMap(lfoBank.getUnipolar(static_cast<int>(BELFOLane::GRID_OPACITY)),
			0.0f, 1.0f, LFO_GRID_OPACITY_MIN, LFO_GRID_OPACITY_MAX);
		gridAlpha = lfoFrac / 0.30f;
	}
	gridAlpha *= (1.0f - composition.getGridDimAmount()); // DENSITY_CRITICAL response
	gridAlpha += gridPulseBoost; // PHASE_TRANSITION response

	float dividerBrightness = ofMap(lfoBank.getUnipolar(static_cast<int>(BELFOLane::DIVIDER_BRIGHTNESS)),
		0.0f, 1.0f, LFO_DIVIDER_MIN, LFO_DIVIDER_MAX);

	ofBackground(GROUND_DARK);

	// Alpha blending stays on for the whole scene — fragment arrival/dissolve
	// fades, the desaturate shader's alpha output, HUD widget panels, and
	// annotation fades all depend on it. (Previously this was only toggled
	// on/off narrowly around the motion overlay draw below, which left
	// blending OFF for every draw call after it — including the fragments
	// loop — once motion overlay opacity ramped above zero.)
	ofEnableAlphaBlending();

	float dividerX = grid.getDividerX();

	ofSetColor(composition.isZoneALight() ? GROUND_LIGHT : GROUND_DARK);
	ofDrawRectangle(0, 0, dividerX, CANVAS_H);

	ofSetColor(GROUND_DARK);
	ofDrawRectangle(dividerX, 0, CANVAS_W - dividerX, CANVAS_H);

	// Motion overlay sits behind the grid/fragments — a soft glow of recent
	// motion in the source video, intensity tracking composition activity.
	if (motionOverlayOpacity > 0.001f) {
		ofSetColor(255, 255, 255, static_cast<int>(255 * motionOverlayOpacity));
		motionEx.getMotionTexture().draw(0, 0, CANVAS_W, CANVAS_H);
	}

	for (const auto & fragment : composition.getFragments()) {
		fragment->draw();
	}

	if (hud::HudWidget * widget = composition.getActiveHudWidget()) {
		widget->draw();
	}

	annotations.drawMeasurementLines();
	annotations.drawCodeText();
	annotations.drawHubHighlight();

	annotations.drawGrid(gridAlpha);
	annotations.drawDivider(dividerProgress, dividerBrightness);

	if (labelShown) {
		annotations.drawCornerLabel("[0,0]");
	}
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
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == 'g') {
		showOccupancyDebug = !showOccupancyDebug;
	} else if (key == 'r') {
		composition.startCycle();
	} else if (key == 'e') {
		bypassErosion = !bypassErosion;
		ofLogNotice("ofApp") << "bypassErosion = " << bypassErosion;
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
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
}
