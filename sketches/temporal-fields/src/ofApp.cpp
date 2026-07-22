#include "ofApp.h"
#include "TFSettings.h"
#include "Settings.h"
#include <algorithm>

void ofApp::setup() {
	// Must precede ALL texture/FBO allocations (timeOffsetBuffer's playhead
	// textures, TFFragmentTransition's snapshot FBOs) — oF defaults to
	// GL_TEXTURE_RECTANGLE_ARB, whose pixel-space texcoords break the
	// fragmentDissolve shader's normalized texture2D() sampling. See
	// blueprint_emergence/src/ofApp.cpp's identical setup() comment, which
	// documents this exact gotcha.
	ofDisableArbTex();

	ofSetFrameRate(TARGET_FPS);

	shaderLib.setup();
	paramPanel.setup();

	TimeOffsetVideoBuffer::Settings bufferSettings;
	bufferSettings.numQuantizeBands = paramPanel.getQuantizeBands();
	bufferSettings.minPlaytimeSeconds = MEDIA_MIN_PLAYTIME_SECONDS;
	bufferSettings.minLoopCount = MEDIA_MIN_LOOP_COUNT;
	timeOffsetBuffer.setup("media", bufferSettings);

	backgroundLayer.setup(&timeOffsetBuffer, &shaderLib, "backgrounds", ofGetWidth(), ofGetHeight(),
		paramPanel.getBackgroundParams());

	bspPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getBSPParams());
	blobGridPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getBlobGridParams());

	TFComposition::Timing timing;
	timing.cycleDuration = CYCLE_DURATION;

	composition.setup(timing, &bspPattern, &blobGridPattern, ofGetWidth(), ofGetHeight());
	composition.setOnPatternChanged([this](TFPatternType type) {
		std::string patternName = (type == TFPatternType::BSP) ? "BSP" : "BLOB_GRID";
		ofLogNotice("ofApp") << "active pattern -> " << patternName;
		if (timeOffsetBuffer.isMediaAdvanceEligible()) {
			timeOffsetBuffer.advanceToNextMedia();
		}
		hudLayer.onPatternSwitch(patternName);
	});
	composition.setOnFragmentReassigned([this](float nx, float ny) {
		hudLayer.onFragmentReassigned(nx, ny, composition.getActiveFragmentCenters());
	});
	composition.startCycle();

	motionEx.setup();
	hudLayer.setup(ofGetWidth(), ofGetHeight());
}

void ofApp::update() {
	float dt = ofGetLastFrameTime();

	// Push whatever the panel currently holds into both patterns every
	// frame — cheap POD struct copies, and the simplest way for a live
	// slider drag to take effect immediately without an event-listener
	// wire-up per dial.
	paramPanel.update(dt);

	bspPattern.setParams(paramPanel.getBSPParams());
	blobGridPattern.setParams(paramPanel.getBlobGridParams());
	timeOffsetBuffer.setNumQuantizeBands(paramPanel.getQuantizeBands());
	composition.setTransitionParams(paramPanel.getTransitionDuration(), paramPanel.getHardCutWeight(),
		paramPanel.getCrossfadeWeight(), paramPanel.getErosionWeight());

	backgroundLayer.setParams(paramPanel.getBackgroundParams());
	backgroundLayer.update(dt);

	composition.update(dt);
	timeOffsetBuffer.update(dt);

	// Feed on the live, un-delayed video texture (not any time-offset
	// playhead) so the readout reflects what's actually happening in the
	// source right now, independent of which historical offset each
	// fragment happens to be displaying.
	if (timeOffsetBuffer.hasMedia()) {
		motionEx.update(timeOffsetBuffer.getRawVideoTexture(), 0.85f, 1.0f);
	}
	float historyBufferFill01 = static_cast<float>(timeOffsetBuffer.getHistoryFrameCount())
		/ static_cast<float>(std::max(1, timeOffsetBuffer.getHistoryCapacityFrames()));
	hudLayer.setEventCadence(paramPanel.getHudCadenceOnFragmentReassign(), paramPanel.getHudCadenceOnPatternSwitch());
	hudLayer.update(dt, motionEx.getMotionEnergy(), timeOffsetBuffer.hasMedia(),
		timeOffsetBuffer.getCurrentMediaFilename(), historyBufferFill01);

	if (paramPanel.consumeSaveRequest()) {
		paramPanel.savePreset(composition.getActivePatternType());
	}

	TFPatternType waypointPattern;
	if (paramPanel.consumeWaypointPatternSwitch(waypointPattern)) {
		composition.forcePattern(waypointPattern);
	}
}

void ofApp::draw() {
	// Section 6a: "Solid Ground" mode clears to GROUND_DARK (shared/src's
	// house palette token), not literally black.
	ofBackground(GROUND_DARK);
	ofEnableAlphaBlending();

	backgroundLayer.draw();

	// Underlay first: sits beneath the composition, so it shows through
	// Blob Grid's transparent mask gaps and along BSP's rectangle seams.
	hudLayer.drawUnderlay();

	composition.draw();
	drawTimeOffsetDebugStrip();

	hudLayer.drawOverlay();

	std::string bgModeStr = backgroundLayer.getCurrentMode() == TFBackgroundLayer::Mode::FULL_VIDEO ? "FULL_VIDEO"
		: backgroundLayer.getCurrentMode() == TFBackgroundLayer::Mode::FULL_IMAGE ? "FULL_IMAGE"
																				  : "SPLIT";
	std::string bgEffectStr = backgroundLayer.getCurrentEffectName().empty() ? "raw" : backgroundLayer.getCurrentEffectName();

	ofSetColor(255);
	ofDrawBitmapString(
		"phase " + std::string(composition.getPhase() == TFComposition::CyclePhase::RUNNING ? "RUNNING" : "PATTERN_TRANSITION")
			+ "  ('r' restart, 't' force next pattern, 'G' gui, 'S' save preset, TAB load next preset)"
			+ "\nbg mode: [" + bgModeStr + "]  bg effect: [" + bgEffectStr + "]",
		12, ofGetHeight() - 20);

	paramPanel.draw();
}

void ofApp::drawTimeOffsetDebugStrip() {
	if (!timeOffsetBuffer.hasMedia()) {
		return;
	}

	int numPlayheads = timeOffsetBuffer.getNumPlayheads();
	float tileW = 160.0f;
	float tileH = 90.0f;
	float y = ofGetHeight() - tileH - 40;

	ofSetColor(255);
	ofDrawBitmapString(
		"history " + ofToString(timeOffsetBuffer.getHistoryFrameCount()) + "/"
			+ ofToString(timeOffsetBuffer.getHistoryCapacityFrames()) + " frames  ["
			+ ofFilePath::getFileName(timeOffsetBuffer.getCurrentMediaFilename()) + "]",
		12, y - 8);

	for (int i = 0; i < numPlayheads; i++) {
		float x = 12 + i * (tileW + 8);
		const ofTexture& tex = timeOffsetBuffer.getPlayheadTexture(i);
		if (!tex.isAllocated()) {
			continue;
		}
		ofSetColor(255);
		tex.draw(x, y, tileW, tileH);
		ofDrawBitmapString(ofToString(timeOffsetBuffer.getPlayheadOffset(i), 2), x, y + tileH + 14);
	}
}

void ofApp::keyPressed(int key) {
	if (key == 'G') {
		paramPanel.toggleVisible();
	} else if (key == 'r') {
		composition.startCycle();
	} else if (key == 't') {
		composition.forceNextPattern();
	} else if (key == 'S') {
		paramPanel.savePreset(composition.getActivePatternType());
	} else if (key == OF_KEY_TAB) {
		TFPatternType presetPattern;
		if (paramPanel.loadNextPreset(presetPattern)) {
			composition.forcePattern(presetPattern);
		}
	}
}
