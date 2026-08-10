#include "ofApp.h"
#include "Settings.h"
#include "TFSettings.h"
#include "TFActivityStatusSelfTest.h"
#include "TFEligibilitySelfTest.h"
#include "TFVideoAdapterSelfTest.h"
#include <algorithm>

void ofApp::setup() {
	// Must precede ALL texture/FBO allocations (temporalVideoAdapter's
	// buffer's playhead textures, TFFragmentTransition's snapshot FBOs) — oF defaults to
	// GL_TEXTURE_RECTANGLE_ARB, whose pixel-space texcoords break the
	// fragmentDissolve shader's normalized texture2D() sampling. See
	// blueprint_emergence/src/ofApp.cpp's identical setup() comment, which
	// documents this exact gotcha.
	ofDisableArbTex();

	ofSetFrameRate(TARGET_FPS);

	shaderLib.setup();

	// Shared Effects "Final Canonical Activity Producer Seam Patch" --
	// proves TFEffectPicker::activityStatus() (the real, existing
	// production DEC-015 accessor) is side-effect-free, stable, and
	// honestly derives health/canonical-IDs/phase/progress/prominence from
	// real owned state. Runs unconditionally at startup, using its own
	// scratch TFEffectPicker instances against the real, just-loaded
	// shaderLib -- does not touch this scene's own backgroundLayer/
	// effectPicker. See TFActivityStatusSelfTest.h.
	videoeffects::runActivityStatusSelfTest(shaderLib);

	// Shared Effects "Production Selector / Eligibility Adoption" -- proves
	// TFEffectPicker::applyEligibleCanonicalPreset() genuinely routes
	// through the real, unmodified EffectKnowledgePrecedence.h API
	// (resolveCompatibility(), isEligibleForAutomaticProductionSelection(),
	// EffectPresetId.h's isReusableAuthoredPreset()) against deterministic,
	// hand-seeded knowledge -- not against whatever the real canonical pack
	// happens to contain on disk. Runs unconditionally at startup, using
	// its own scratch TFEffectPicker instances. See TFEligibilitySelfTest.h.
	videoeffects::runEligibilitySelfTest(shaderLib);

	// Shared Video — Temporal Fields Specialized Adapter Seam session's
	// required real-decoder proof (DEC-013/DEC-014): proves the real
	// VideoPlaybackService + TimeOffsetPlaybackAdapter classes against
	// real files under the canonical assets/shared/media/ root. Runs
	// unconditionally at startup, using its own scratch instances --
	// does not touch this scene's own videoPlaybackService/
	// temporalVideoAdapter. Real frame arrival needs the actual oF run
	// loop pumping (see TFVideoAdapterSelfTest.h's own header comment for
	// why a synchronous setup()-time version of this test does not work in
	// this environment) — begin() only does the synchronous part; the
	// remaining real-decode-dependent checks advance once per frame via
	// tickTemporalVideoAdapterSelfTest() in update() below.
	beginTemporalVideoAdapterSelfTest();

	paramPanel.setup();

	ambientTextures.setup(ofGetWidth(), ofGetHeight(), "backgrounds");
	paramPanel.registerBackgroundTextures(ambientTextures.getParamGroup());

	// Shared Video — Temporal Fields Specialized Adapter Seam (DEC-013/
	// DEC-014). videoPlaybackService is the sole canonical media catalog/
	// selection/session-history/Previous-Next/hold-timing authority,
	// pointed at the canonical physical media root (DEC-011) via the same
	// bin/data-relative traversal every other migrated sketch uses.
	// temporalVideoAdapter.buffer() (a TimeOffsetVideoBuffer) never scans,
	// shuffles, or selects media itself from here on — see
	// synchronizeSelectedMedia() below and TimeOffsetVideoBuffer.h's own
	// "LEGACY PATH" comments on setup()/advanceToNextMedia(), neither of
	// which this file calls anymore.
	VideoPlaybackService::Config videoConfig;
	videoConfig.mediaRoot = ofToDataPath("../../../../assets/shared/media", true);
	// Reuses the same pacing constant the legacy isMediaAdvanceEligible()
	// gate used, so canonical-service-driven automatic advance keeps a
	// similar minimum-time-on-screen feel — MEDIA_MIN_LOOP_COUNT has no
	// analog here (VideoPlaybackService's hold timer is real-time-based
	// only; see this session's implementation report, "Deviations").
	videoConfig.holdDurationSeconds = MEDIA_MIN_PLAYTIME_SECONDS;
	videoConfig.automaticAdvance = true;
	videoPlaybackService.setup(videoConfig);

	TimeOffsetVideoBuffer::Settings bufferSettings;
	bufferSettings.numQuantizeBands = paramPanel.getQuantizeBands();
	bufferSettings.maxHistorySeconds = paramPanel.getMaxHistorySeconds();
	bufferSettings.minPlaytimeSeconds = MEDIA_MIN_PLAYTIME_SECONDS;
	bufferSettings.minLoopCount = MEDIA_MIN_LOOP_COUNT;
	temporalVideoAdapter.setup(bufferSettings);
	syncTemporalVideoAdapter();

	backgroundLayer.setup(&temporalVideoAdapter.buffer(), &shaderLib, "backgrounds", ofGetWidth(), ofGetHeight(),
		paramPanel.getBackgroundParams());

	bspPattern.setup(&temporalVideoAdapter.buffer(), ofGetWidth(), ofGetHeight(), paramPanel.getBSPParams());
	blobGridPattern.setup(&temporalVideoAdapter.buffer(), ofGetWidth(), ofGetHeight(), paramPanel.getBlobGridParams());
	bandsPattern.setup(&temporalVideoAdapter.buffer(), ofGetWidth(), ofGetHeight(), paramPanel.getBandsParams());
	columnGridPattern.setup(&temporalVideoAdapter.buffer(), ofGetWidth(), ofGetHeight(), paramPanel.getColumnGridParams());
	telescopingFramesPattern.setup(&temporalVideoAdapter.buffer(), ofGetWidth(), ofGetHeight(), paramPanel.getTelescopingFramesParams());
	particleFieldPattern.setup(&temporalVideoAdapter.buffer(), ofGetWidth(), ofGetHeight(), paramPanel.getParticleFieldParams());
	ecologicalSuccessionPattern.setup(&temporalVideoAdapter.buffer(), ofGetWidth(), ofGetHeight(), paramPanel.getEcologicalSuccessionParams());
	networkGrowthPattern.setup(&temporalVideoAdapter.buffer(), ofGetWidth(), ofGetHeight(), paramPanel.getNetworkGrowthParams());
	temporalTidesPattern.setup(&temporalVideoAdapter.buffer(), ofGetWidth(), ofGetHeight(), paramPanel.getTemporalTidesParams());

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
		// A pattern switch no longer triggers a media change directly —
		// DEC-013/DEC-014 make videoPlaybackService's own hold timer the
		// sole automatic-advance cadence (see this session's
		// implementation report, "Deviations," for the resulting pacing
		// change vs. the old isMediaAdvanceEligible()-gated
		// advanceToNextMedia() call this replaces).
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

void ofApp::syncTemporalVideoAdapter() {
	// videoPlaybackService.status()/currentAbsolutePath() are the ONLY
	// source of the media identity handed to the adapter here — no local
	// scan, shuffle, or selection decision happens in this function or
	// anywhere else in this file anymore (DEC-013/DEC-014).
	VideoPlaybackStatus status = videoPlaybackService.status();
	if (!status.mediaId.has_value()) {
		return; // nothing selected yet (e.g. empty/missing canonical media root)
	}
	std::optional<std::string> absolutePath = videoPlaybackService.currentAbsolutePath();
	if (!absolutePath.has_value()) {
		return; // should be impossible whenever mediaId is set, but never assume
	}
	temporalVideoAdapter.synchronizeSelectedMedia(*status.mediaId, *absolutePath);
}

void ofApp::update() {
	if (hudOverlayActive) {
		hudOverlayPanel.update(hudOverlayDials);
		hudOverlay.update(ofGetLastFrameTime(), hudOverlayDials);
		return;
	}

	float dt = ofGetLastFrameTime();

	// No-ops once done (see tickTemporalVideoAdapterSelfTest()'s own
	// isDone() guard) — advances the real-decoder self-test by one real
	// frame each call until it finishes and logs its PASS/FAIL summary.
	tickTemporalVideoAdapterSelfTest(dt);

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
	temporalVideoAdapter.buffer().setNumQuantizeBands(paramPanel.getQuantizeBands());
	temporalVideoAdapter.buffer().setMaxHistorySeconds(paramPanel.getMaxHistorySeconds());
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

	// videoPlaybackService.update(dt) drives its own hold timer/automatic
	// advance (DEC-013) — must run before syncTemporalVideoAdapter() below
	// so a hold-timer-triggered media change is observed and forwarded to
	// the adapter the same frame it happens, not one frame late.
	videoPlaybackService.update(dt);
	syncTemporalVideoAdapter();
	temporalVideoAdapter.update(dt);

	// Feed on the live, un-delayed video texture (not any time-offset
	// playhead) so the readout reflects what's actually happening in the
	// source right now, independent of which historical offset each
	// fragment happens to be displaying.
	if (temporalVideoAdapter.buffer().hasMedia()) {
		motionEx.update(temporalVideoAdapter.buffer().getRawVideoTexture(), 0.85f, 1.0f);
	}
	// Average of all active playheads' current time offset (0 = live, 1 = as
	// far back as maxHistorySeconds allows) — continuously fluctuates as
	// fragments reassign playheads via noise. Replaces the old history-
	// buffer-fill metric, which ramps once at startup then sits pinned at
	// 100% for the rest of the session.
	float avgPlayheadDepth01 = 0.0f;
	int numPlayheadsForDepth = temporalVideoAdapter.buffer().getNumPlayheads();
	if (numPlayheadsForDepth > 0) {
		float sum = 0.0f;
		for (int i = 0; i < numPlayheadsForDepth; i++) {
			sum += temporalVideoAdapter.buffer().getPlayheadOffset(i);
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
	hudLayer.update(dt, motionEx.getMotionEnergy(), temporalVideoAdapter.buffer().hasMedia(),
		temporalVideoAdapter.buffer().getCurrentMediaFilename(), avgPlayheadDepth01, composition.getActiveFragmentCenters(),
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
	if (!temporalVideoAdapter.buffer().hasMedia()) {
		return;
	}

	int numPlayheads = temporalVideoAdapter.buffer().getNumPlayheads();
	float tileW = 160.0f;
	float tileH = 110.0f;
	float y = ofGetHeight() - tileH - 50;

	ofSetColor(255);
	ofDrawBitmapString(
		"history " + ofToString(temporalVideoAdapter.buffer().getHistoryFrameCount()) + "/"
			+ ofToString(temporalVideoAdapter.buffer().getHistoryCapacityFrames()) + " frames  ["
			+ ofFilePath::getFileName(temporalVideoAdapter.buffer().getCurrentMediaFilename()) + "]",
		12, y - 8);

	for (int i = 0; i < numPlayheads; i++) {
		float x = 12 + i * (tileW + 8);
		const ofTexture & tex = temporalVideoAdapter.buffer().getPlayheadTexture(i);
		if (!tex.isAllocated()) {
			continue;
		}
		ofSetColor(255);
		tex.draw(x, y, tileW, tileH);
		ofDrawBitmapString(ofToString(temporalVideoAdapter.buffer().getPlayheadOffset(i), 2), x, y + tileH + 14);
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
