#include "ofApp.h"
#include "Settings.h"
#include "TFSettings.h"
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

	ambientTextures.setup(ofGetWidth(), ofGetHeight(), "backgrounds");
	paramPanel.registerBackgroundTextures(ambientTextures.getParamGroup());

	TimeOffsetVideoBuffer::Settings bufferSettings;
	bufferSettings.numQuantizeBands = paramPanel.getQuantizeBands();
	bufferSettings.maxHistorySeconds = paramPanel.getMaxHistorySeconds();
	bufferSettings.minPlaytimeSeconds = MEDIA_MIN_PLAYTIME_SECONDS;
	bufferSettings.minLoopCount = MEDIA_MIN_LOOP_COUNT;
	timeOffsetBuffer.setup("media", bufferSettings);

	backgroundLayer.setup(&timeOffsetBuffer, &shaderLib, "backgrounds", ofGetWidth(), ofGetHeight(),
		paramPanel.getBackgroundParams());

	bspPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getBSPParams());
	blobGridPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getBlobGridParams());
	bandsPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getBandsParams());
	columnGridPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getColumnGridParams());
	telescopingFramesPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getTelescopingFramesParams());
	particleFieldPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getParticleFieldParams());
	ecologicalSuccessionPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getEcologicalSuccessionParams());
	networkGrowthPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getNetworkGrowthParams());
	temporalTidesPattern.setup(&timeOffsetBuffer, ofGetWidth(), ofGetHeight(), paramPanel.getTemporalTidesParams());

	TFComposition::Timing timing;
	timing.cycleDuration = CYCLE_DURATION;

	composition.setup(timing, {
								  { TFPatternType::BSP, &bspPattern },
								  { TFPatternType::BLOB_GRID, &blobGridPattern },
								  { TFPatternType::BANDS, &bandsPattern },
								  { TFPatternType::COLUMN_GRID, &columnGridPattern },
								  { TFPatternType::TELESCOPING_FRAMES, &telescopingFramesPattern },
								  { TFPatternType::PARTICLE_FIELD, &particleFieldPattern },
								  { TFPatternType::ECOLOGICAL_SUCCESSION, &ecologicalSuccessionPattern },
								  { TFPatternType::NETWORK_GROWTH, &networkGrowthPattern },
								  { TFPatternType::TEMPORAL_TIDES, &temporalTidesPattern },
							  },
		ofGetWidth(), ofGetHeight());
	composition.setOnPatternChanged([this](TFPatternType type) {
		std::string patternName = tfPatternTypeName(type);
		ofLogNotice("ofApp") << "active pattern -> " << patternName;
		if (timeOffsetBuffer.isMediaAdvanceEligible()) {
			timeOffsetBuffer.advanceToNextMedia();
		}
		hudLayer.onPatternSwitch(patternName, composition.getCycleSeed(),
			composition.getPhase() == TFComposition::CyclePhase::PATTERN_TRANSITION);
	});
	composition.setOnFragmentReassigned([this](float nx, float ny) {
		hudLayer.onFragmentReassigned(nx, ny, composition.getActiveFragmentCenters());
	});
	composition.startCycle();

	motionEx.setup();
	hudLayer.setup(ofGetWidth(), ofGetHeight());
	paramPanel.registerHudVisibility(hudLayer.getVisibilityParamGroup());

	hudOverlay.setup(ofGetWidth(), ofGetHeight());
	hudOverlayPanel.setup();
}

void ofApp::update() {
	if (hudOverlayActive) {
		hudOverlayPanel.update(hudOverlayDials);
		hudOverlay.update(ofGetLastFrameTime(), hudOverlayDials);
		return;
	}

	float dt = ofGetLastFrameTime();

	// Push whatever the panel currently holds into both patterns every
	// frame — cheap POD struct copies, and the simplest way for a live
	// slider drag to take effect immediately without an event-listener
	// wire-up per dial.
	paramPanel.update(dt);

	bspPattern.setParams(paramPanel.getBSPParams());
	blobGridPattern.setParams(paramPanel.getBlobGridParams());
	bandsPattern.setParams(paramPanel.getBandsParams());
	columnGridPattern.setParams(paramPanel.getColumnGridParams());
	telescopingFramesPattern.setParams(paramPanel.getTelescopingFramesParams());
	particleFieldPattern.setParams(paramPanel.getParticleFieldParams());
	ecologicalSuccessionPattern.setParams(paramPanel.getEcologicalSuccessionParams());
	networkGrowthPattern.setParams(paramPanel.getNetworkGrowthParams());
	temporalTidesPattern.setParams(paramPanel.getTemporalTidesParams());
	timeOffsetBuffer.setNumQuantizeBands(paramPanel.getQuantizeBands());
	timeOffsetBuffer.setMaxHistorySeconds(paramPanel.getMaxHistorySeconds());
	composition.setTransitionParams(paramPanel.getTransitionDuration(), paramPanel.getHardCutWeight(),
		paramPanel.getCrossfadeWeight(), paramPanel.getErosionWeight());

	backgroundLayer.setParams(paramPanel.getBackgroundParams());
	backgroundLayer.update(dt);

	// A multi-state preset timeline is authored to hold one single pattern
	// for its own duration (often several minutes) — suspend
	// TFComposition's unrelated ~90s auto-cycle for as long as one is
	// running so it doesn't get yanked to a different pattern mid-timeline.
	composition.setAutoCycleSuspended(paramPanel.isTimelineActive());
	composition.update(dt);
	timeOffsetBuffer.update(dt);

	// Feed on the live, un-delayed video texture (not any time-offset
	// playhead) so the readout reflects what's actually happening in the
	// source right now, independent of which historical offset each
	// fragment happens to be displaying.
	if (timeOffsetBuffer.hasMedia()) {
		motionEx.update(timeOffsetBuffer.getRawVideoTexture(), 0.85f, 1.0f);
	}
	// Average of all active playheads' current time offset (0 = live, 1 = as
	// far back as maxHistorySeconds allows) — continuously fluctuates as
	// fragments reassign playheads via noise. Replaces the old history-
	// buffer-fill metric, which ramps once at startup then sits pinned at
	// 100% for the rest of the session.
	float avgPlayheadDepth01 = 0.0f;
	int numPlayheadsForDepth = timeOffsetBuffer.getNumPlayheads();
	if (numPlayheadsForDepth > 0) {
		float sum = 0.0f;
		for (int i = 0; i < numPlayheadsForDepth; i++) {
			sum += timeOffsetBuffer.getPlayheadOffset(i);
		}
		avgPlayheadDepth01 = sum / static_cast<float>(numPlayheadsForDepth);
	}

	// Whichever pattern is active right now, read its own characteristic
	// irregularity/variation dial (already 0..1, already live-wobbling via
	// TFParameterPanel) so the HUD's contour underlay can track it.
	float patternDrift01 = 0.0f;
	switch (composition.getActivePatternType()) {
	case TFPatternType::BSP:
		patternDrift01 = paramPanel.getBSPParams().irregularity;
		break;
	case TFPatternType::BLOB_GRID:
		patternDrift01 = paramPanel.getBlobGridParams().sizeVariation;
		break;
	case TFPatternType::BANDS:
		patternDrift01 = paramPanel.getBandsParams().widthVariation;
		break;
	case TFPatternType::COLUMN_GRID:
		patternDrift01 = paramPanel.getColumnGridParams().rowHeightVariation;
		break;
	case TFPatternType::TELESCOPING_FRAMES:
		patternDrift01 = paramPanel.getTelescopingFramesParams().thicknessVariation;
		break;
	case TFPatternType::PARTICLE_FIELD:
		patternDrift01 = ofClamp(paramPanel.getParticleFieldParams().driftSpeed / 5.0f, 0.0f, 1.0f);
		break;
	case TFPatternType::ECOLOGICAL_SUCCESSION:
		patternDrift01 = ofClamp(paramPanel.getEcologicalSuccessionParams().sproutThreshold * 2.0f, 0.0f, 1.0f);
		break;
	case TFPatternType::NETWORK_GROWTH:
		patternDrift01 = ofClamp(paramPanel.getNetworkGrowthParams().branchAngleJitterDeg / 90.0f, 0.0f, 1.0f);
		break;
	case TFPatternType::TEMPORAL_TIDES:
		patternDrift01 = ofClamp(paramPanel.getTemporalTidesParams().amplitude, 0.0f, 1.0f);
		break;
	}

	hudLayer.setEventCadence(paramPanel.getHudCadenceOnFragmentReassign(), paramPanel.getHudCadenceOnPatternSwitch());
	hudLayer.update(dt, motionEx.getMotionEnergy(), timeOffsetBuffer.hasMedia(),
		timeOffsetBuffer.getCurrentMediaFilename(), avgPlayheadDepth01, composition.getActiveFragmentCenters(),
		patternDrift01);

	if (paramPanel.consumeSaveRequest()) {
		paramPanel.savePreset(composition.getActivePatternType());
	}

	TFPatternType waypointPattern;
	if (paramPanel.consumeWaypointPatternSwitch(waypointPattern)) {
		composition.forcePattern(waypointPattern);
	}

	// Keeps the GUI panel decluttered -- only the pattern actually playing
	// (after every possible switch this frame: auto-cycle, TAB, waypoint)
	// has its dropdown open; the other eight stay collapsed.
	paramPanel.setActivePattern(composition.getActivePatternType());
}

void ofApp::draw() {
	if (hudOverlayActive) {
		ofBackground(13, 13, 13);
		hudOverlay.draw();
		hudOverlayPanel.draw();
		return;
	}

	// Section 6a: "Solid Ground" mode clears to GROUND_DARK (shared/src's
	// house palette token), not literally black.
	ofBackground(GROUND_DARK);
	ofEnableAlphaBlending();

	backgroundLayer.draw();

	// Ambient ground textures (leaf vein, woodgrain, moss, ...) sit above
	// the background layer's video/image content but beneath everything
	// else -- same "underlay" slot as TFMoireUnderlay, drawn independently
	// of TFHudLayer's widget rotation so their opacity sliders are always
	// live rather than gated behind a rotation roll.
	ambientTextures.drawUnderlay();

	// Underlay first: sits beneath the composition, so it shows through
	// Blob Grid's transparent mask gaps and along BSP's rectangle seams.
	hudLayer.drawUnderlay();

	composition.draw();
	if (showDebugGui) {
		drawTimeOffsetDebugStrip();
	}

	hudLayer.drawOverlay();

	// Topmost ambient layer -- draws after the HUD overlay (which includes
	// the measurement-line/tick-stamp annotations), so overlay textures
	// (contour lines, solar cell grid ghost, ...) sit above everything.
	ambientTextures.drawOverlay();

	std::string bgModeStr = backgroundLayer.getCurrentMode() == TFBackgroundLayer::Mode::FULL_VIDEO ? "FULL_VIDEO"
		: backgroundLayer.getCurrentMode() == TFBackgroundLayer::Mode::FULL_IMAGE					? "FULL_IMAGE"
																									: "SPLIT";
	std::string bgEffectStr = backgroundLayer.getCurrentEffectName().empty() ? "raw" : backgroundLayer.getCurrentEffectName();

	if (showDebugGui) {
		ofSetColor(255);
		ofDrawBitmapString(
			"pattern: [" + tfPatternTypeName(composition.getActivePatternType()) + "]  phase "
				+ std::string(composition.getPhase() == TFComposition::CyclePhase::RUNNING ? "RUNNING" : "PATTERN_TRANSITION")
				+ "\n('r' restart, 't' force next pattern, 'G' gui, 'S' save preset, TAB load next preset, 'd' toggle debug,"
				+ " 'P' timeline pause/resume, ']' timeline next state)"
				+ "\nbg mode: [" + bgModeStr + "]  bg effect: [" + bgEffectStr + "]",
			12, ofGetHeight() - 40);

		std::string timelineStatus = paramPanel.getTimelineDebugStatus();
		if (!timelineStatus.empty()) {
			ofDrawBitmapString(timelineStatus, 12, 20);
		}
	}

	paramPanel.draw();
}

void ofApp::drawTimeOffsetDebugStrip() {
	if (!timeOffsetBuffer.hasMedia()) {
		return;
	}

	int numPlayheads = timeOffsetBuffer.getNumPlayheads();
	float tileW = 160.0f;
	float tileH = 110.0f;
	float y = ofGetHeight() - tileH - 50;

	ofSetColor(255);
	ofDrawBitmapString(
		"history " + ofToString(timeOffsetBuffer.getHistoryFrameCount()) + "/"
			+ ofToString(timeOffsetBuffer.getHistoryCapacityFrames()) + " frames  ["
			+ ofFilePath::getFileName(timeOffsetBuffer.getCurrentMediaFilename()) + "]",
		12, y - 8);

	for (int i = 0; i < numPlayheads; i++) {
		float x = 12 + i * (tileW + 8);
		const ofTexture & tex = timeOffsetBuffer.getPlayheadTexture(i);
		if (!tex.isAllocated()) {
			continue;
		}
		ofSetColor(255);
		tex.draw(x, y, tileW, tileH);
		ofDrawBitmapString(ofToString(timeOffsetBuffer.getPlayheadOffset(i), 2), x, y + tileH + 14);
	}
}

void ofApp::windowResized(int w, int h) {
	// Propagates the new canvas size to everything that draws full-canvas
	// content, so a resize (dragging the window edge, or launching on a
	// differently-sized display than the 1280x720 default) actually
	// reflows the composition instead of leaving it stuck at whatever size
	// setup() first saw. Each layer/pattern already derives its fill
	// geometry from its own cached canvasW/canvasH -- see
	// TFComposition::resizeCanvas()/TFHudLayer::resize() -- so this just
	// needs to reach every one of them.
	ambientTextures.resizeCanvas(w, h);
	backgroundLayer.resizeCanvas(w, h);
	composition.resizeCanvas(w, h);
	hudLayer.resize(w, h);
	hudOverlay.windowResized(w, h);
}

void ofApp::keyPressed(int key) {
	if (key == 'o' || key == 'O') {
		hudOverlayActive = !hudOverlayActive;
		return;
	} else if (key == 'G' || key == 'g') {
		paramPanel.toggleVisible();
	} else if (key == 'd') {
		showDebugGui = !showDebugGui;
	} else if (key == 'r') {
		composition.startCycle();
	} else if (key == 't') {
		composition.forceNextPattern();
	} else if (key == 'S' || key == 's') {
		paramPanel.savePreset(composition.getActivePatternType());
	} else if (key == OF_KEY_TAB) {
		TFPatternType presetPattern;
		if (paramPanel.loadNextPreset(presetPattern)) {
			composition.forcePattern(presetPattern);
		}
	} else if (key == 'P' || key == 'p') {
		if (paramPanel.isTimelinePaused()) {
			paramPanel.resumeTimeline();
		} else {
			paramPanel.pauseTimeline();
		}
	} else if (key == ']') {
		paramPanel.advanceTimelineState();
	}
}
