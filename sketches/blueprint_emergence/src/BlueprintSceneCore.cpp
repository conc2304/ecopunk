#include "BlueprintSceneCore.h"

#include "BELFOLanes.h"
#include "BESettings.h"
#include "BETriggers.h"
#include "VideoPlaybackService.h"
#include "hud/HudElements.h"

#include <cmath>
#include <optional>

void BlueprintSceneCore::setup(VideoPlaybackService* video, const Config& config) {
	config_ = config;
	video_ = video;

	const int cw = config_.canvasW;
	const int ch = config_.canvasH;
	const float dividerX = static_cast<float>(cw) * DIVIDER_X_FRACTION;
	grid_.setup(cw, ch, dividerX);
	annotations_.setup(&grid_, cw, ch);
	annotations_.loadCodeFont(CODE_FONT_PATH, SIZE_CODE);
	annotations_.setMeasurementLineTiming(MLINE_DRAW_SPEED, MLINE_FADE_DELAY, MLINE_FADE_OPACITY);
	annotations_.setCodeTextTiming(CODE_TEXT_INTERVAL_MIN, CODE_TEXT_INTERVAL_MAX, CODE_TEXT_OPACITY_MIN, CODE_TEXT_OPACITY_MAX);
	annotations_.setCodeFragments(loadCodeFragments());

	Fragment::loadFragmentShader("shaders/fragmentEffects.vert", "shaders/fragmentEffects.frag");

	// ── Vitality systems (LFOBank / TriggerBus / GridState / ErosionFBO) ──
	shaderLib_.setup();
	gridState_.setup(GRID_COLS, GRID_ROWS, GRIDSTATE_DECAY_RATE);
	erosionFBO_.setup(cw, ch, EROSION_DECAY_RATE);

	motionEx_.setup(cw, ch); // RT-007: canvas-sized, never the host window
	motionEx_.setOutputMode(0); // LUMA_GLOW
	motionEx_.setReferenceMode(MotionExtraction::REFERENCE_ACCUMULATION);

	lfoBank_.setup(static_cast<int>(BELFOLane::COUNT));
	lfoBank_.setFrequency(static_cast<int>(BELFOLane::GRID_OPACITY), LFO_GRID_OPACITY_HZ);
	lfoBank_.setFrequency(static_cast<int>(BELFOLane::DIVIDER_BRIGHTNESS), LFO_DIVIDER_BRIGHTNESS_HZ);
	lfoBank_.setFrequency(static_cast<int>(BELFOLane::PLACEMENT_BIAS), LFO_PLACEMENT_BIAS_HZ);
	lfoBank_.setFrequency(static_cast<int>(BELFOLane::FRAG_DESAT_OFFSET_A), LFO_FRAG_DESAT_A_HZ);
	lfoBank_.setFrequency(static_cast<int>(BELFOLane::FRAG_DESAT_OFFSET_B), LFO_FRAG_DESAT_B_HZ);
	lfoBank_.setFrequency(static_cast<int>(BELFOLane::CODE_TEXT_WEIGHT), LFO_CODE_TEXT_WEIGHT_HZ);
	for (int i = 0; i < static_cast<int>(BELFOLane::COUNT); i++) {
		lfoBank_.setPhaseOffset(i, ofRandom(TWO_PI));
	}

	triggerBus_.setup(static_cast<int>(BETrigger::COUNT));
	triggerBus_.setCooldown(static_cast<int>(BETrigger::ZONE_IMBALANCE), TRIGGER_ZONE_IMBALANCE_CD);
	triggerBus_.setCooldown(static_cast<int>(BETrigger::DENSITY_HIGH), TRIGGER_DENSITY_HIGH_CD);
	triggerBus_.setCooldown(static_cast<int>(BETrigger::DENSITY_CRITICAL), TRIGGER_DENSITY_CRIT_CD);
	triggerBus_.setCooldown(static_cast<int>(BETrigger::LONG_SILENCE), TRIGGER_LONG_SILENCE_CD);
	triggerBus_.setCooldown(static_cast<int>(BETrigger::PHASE_TRANSITION), TRIGGER_PHASE_TRANSITION_CD);
	triggerBus_.setCooldown(static_cast<int>(BETrigger::MEASUREMENT_HUB), TRIGGER_MEASUREMENT_HUB_CD);
	triggerBus_.setCooldown(static_cast<int>(BETrigger::CIRCLE_PLACED), 0.0f);
	triggerBus_.setCooldown(static_cast<int>(BETrigger::CYCLE_START), 0.0f);

	collage_.setup(cw, ch, CODE_FONT_PATH, 13);
	strata_g_.setup(cw, ch, CODE_FONT_PATH, 13);
	contact_g_.setup(cw, ch, CODE_FONT_PATH, 13);
	exploded_g_.setup(cw, ch, CODE_FONT_PATH, 13);
	section_g_.setup(cw, ch, CODE_FONT_PATH, 13);
	for (BEGrammar * g : { static_cast<BEGrammar *>(&strata_g_), static_cast<BEGrammar *>(&contact_g_), static_cast<BEGrammar *>(&exploded_g_), static_cast<BEGrammar *>(&section_g_) })
		g->setShaderLibrary(&shaderLib_);
	composition_.setDissolveGate([this]() { return !editorialCollage_ || collage_.dissolveFinished(); });

	composition_.setupBE(&grid_, video_, cw, ch);
	composition_.setLFOBank(&lfoBank_);
	composition_.setTriggerBus(&triggerBus_); // also registers BEComposition's own listeners
	composition_.setGridState(&gridState_);
	composition_.setShaderLibrary(&shaderLib_);
	composition_.setMotionExtraction(&motionEx_);

	wireTriggerResponses();

	composition_.setOnFragmentPlaced([this](Fragment* newFrag, Fragment* nearest) {
		annotations_.onFragmentPlaced(newFrag, nearest);
	});
	composition_.setOnFragmentRemoved([this](Fragment* frag) {
		annotations_.onFragmentRemoved(frag);
	});
	composition_.setOnCycleStart([this]() {
		// One canonical media advance per composition cycle (see
		// Config::advanceMediaPerCycle) — Shared Video still picks WHAT plays.
		cycleStartCount_++;
		const bool skipAdvance = skipNextCycleMediaAdvance_;
		skipNextCycleMediaAdvance_ = false;
		if (config_.advanceMediaPerCycle && !skipAdvance && video_ != nullptr && !video_->advanceAtBoundary()) {
			ofLogWarning("BlueprintSceneCore") << "onCycleStart: VideoPlaybackService::advanceAtBoundary() failed (media unavailable?)";
		}
		if (editorialCollage_) {
			// Fresh plate per cycle start (natural cycle end or ghost mini-cycle).
			collage_.beginLayout(static_cast<uint32_t>(composition_.getCycleSeed()) * 1664525u
				+ static_cast<uint32_t>(cycleStartCount_) * 1013904223u);
		}
		if (alt_ != nullptr) {
			alt_->beginLayout(static_cast<uint32_t>(composition_.getCycleSeed()) * 2654435761u
				+ static_cast<uint32_t>(cycleStartCount_) * 40503u);
		}
		annotations_.reset();
		erosionFBO_.clear();
		gridPulseBoost_ = 0.0f;
		for (int i = 0; i < static_cast<int>(BELFOLane::COUNT); i++) {
			lfoBank_.setPhaseOffset(i, ofRandom(TWO_PI));
		}
		triggerBus_.fireImmediate(static_cast<int>(BETrigger::CYCLE_START));
	});
	composition_.setOnPhaseChanged([this](CompositionBase::CyclePhase newPhase) {
		static const char* phaseNames[] = { "BLANK", "PLACEMENT", "DENSITY", "DISSOLVE", "RESET_HOLD" };
		ofLogNotice("BlueprintSceneCore") << "phase -> " << phaseNames[static_cast<int>(newPhase)]
										  << " at t=" << ofGetElapsedTimef();
		gridPulseBoost_ = 0.8f;
		if (editorialCollage_) {
			using P = CompositionBase::CyclePhase;
			if (newPhase == P::DENSITY) collage_.setPhase(CollagePhase::Density);
			else if (newPhase == P::DISSOLVE) collage_.setPhase(CollagePhase::Dissolve);
			else collage_.setPhase(CollagePhase::Placement);
		}
		triggerBus_.fireImmediate(static_cast<int>(BETrigger::PHASE_TRANSITION));
	});

	didSetup_ = true;
	if (config_.editorialCollage) setEditorialCollage(true); // not active yet: no cycle restart
}

void BlueprintSceneCore::setEditorialCollage(bool on) {
	if (on == editorialCollage_) return;
	setGrammar(on ? Grammar::Collage : Grammar::Classic);
}

const char* BlueprintSceneCore::grammarId(Grammar g) {
	switch (g) {
		case Grammar::Collage: return "editorial_collage";
		case Grammar::Strata: return "strata";
		case Grammar::ContactSheet: return "contact_sheet";
		case Grammar::ExplodedView: return "exploded_view";
		case Grammar::SectionCut: return "section_cut";
		default: return "ghost_layers";
	}
}

void BlueprintSceneCore::setGrammar(Grammar g) {
	if (g == grammar_) return;
	applyGrammar(g);
}

// Every non-classic grammar pins the composition to GHOST_LAYERS with
// spawning suspended. Returning to Classic lands deterministically on
// GHOST_LAYERS (first classic mode in the NEXT MODE order).
void BlueprintSceneCore::applyGrammar(Grammar g) {
	grammar_ = g;
	editorialCollage_ = (g == Grammar::Collage);
	switch (g) {
		case Grammar::Strata: alt_ = &strata_g_; break;
		case Grammar::ContactSheet: alt_ = &contact_g_; break;
		case Grammar::ExplodedView: alt_ = &exploded_g_; break;
		case Grammar::SectionCut: alt_ = &section_g_; break;
		default: alt_ = nullptr; break;
	}
	const bool on = (g != Grammar::Classic);
	collage_.lock(false);
	if (alt_ != nullptr) alt_->reset(); // rebuilt by the cycle-start callback
	composition_.setSpawningSuspended(on);
	composition_.setForcedCycleMode(on ? std::optional<CycleMode>(CycleMode::GHOST_LAYERS) : std::nullopt);
	if (didSetup_ && active_) {
		skipNextCycleMediaAdvance_ = true; // a grammar switch keeps the current media
		if (!on) {
			// Leaving the collage lands deterministically on GHOST_LAYERS (first classic
			// mode in the NEXT MODE order), then releases the pin.
			composition_.setForcedCycleMode(CycleMode::GHOST_LAYERS);
			restartCycle();
			composition_.setForcedCycleMode(std::nullopt);
		} else {
			restartCycle();
		}
	}
}

void BlueprintSceneCore::debugShowCollage(uint32_t seed, int templateIndex, bool densityReached) {
	if (!editorialCollage_) setEditorialCollage(true);
	collage_.forceTemplate(templateIndex);
	collage_.lock(false);
	collage_.beginLayout(seed, true);
	collage_.debugSetTimeline(60.0f, densityReached ? 30.0f : -1.0f);
}

void BlueprintSceneCore::activate() {
	if (!didSetup_) return;
	motionOverlayOpacity_ = 0.0f;
	hadHudWidget_ = false;
	skipNextCycleMediaAdvance_ = true; // the activation cycle keeps the current media
	restartCycle();
	active_ = true;
}

void BlueprintSceneCore::restartCycle() {
	composition_.startCycle();
}

bool BlueprintSceneCore::requestNextMode() {
	// Cycles through every grammar in enum order. Refused during DISSOLVE (the
	// composition's own transition); never advances the shared media.
	if (!didSetup_ || !active_) return false;
	if (composition_.getPhase() == CompositionBase::CyclePhase::DISSOLVE) return false;
	setGrammar(static_cast<Grammar>((static_cast<int>(grammar_) + 1) % GRAMMAR_COUNT));
	return true;
}

void BlueprintSceneCore::wireTriggerResponses() {
	triggerBus_.addListener(static_cast<int>(BETrigger::LONG_SILENCE), [this](int) {
		annotations_.triggerCodeTextSpawn();
	});
	triggerBus_.addListener(static_cast<int>(BETrigger::ZONE_IMBALANCE), [this](int) {
		annotations_.triggerCodeTextSpawn();
	});
	triggerBus_.addListener(static_cast<int>(BETrigger::CIRCLE_PLACED), [this](int) {
		annotations_.pulseMeasurementLines(0.5f, 3.0f);
	});
}

std::vector<std::string> BlueprintSceneCore::loadCodeFragments() const {
	std::vector<std::string> lines;
	ofBuffer buffer = ofBufferFromFile("codefragments.txt");
	for (const auto& line : buffer.getLines()) {
		if (!line.empty()) {
			lines.push_back(line);
		}
	}
	return lines;
}

bool BlueprintSceneCore::hasVideoFrame() const {
	return video_ != nullptr && video_->currentTexture() != nullptr;
}

void BlueprintSceneCore::update(float dt) {
	if (!active_) return;

	lfoBank_.update(dt);
	composition_.update(dt); // drives GridState decay, trigger condition checks, LFO desat nudges
	triggerBus_.update(dt); // ticks cooldown timers
	if (editorialCollage_) {
		// The collage follows the composition's phase arc; the classic
		// annotation / motion layers are not drawn, so don't pay for them.
		collage_.update(dt);
		return;
	}
	if (alt_ != nullptr) {
		alt_->update(dt);
		return;
	}
	annotations_.update(dt);

	triggerBus_.setConditionActive(static_cast<int>(BETrigger::MEASUREMENT_HUB),
		annotations_.getMaxConnectionCount() >= TRIGGER_HUB_MIN_CONNECTIONS);
	annotations_.setCodeTextWeight(lfoBank_.getUnipolar(static_cast<int>(BELFOLane::CODE_TEXT_WEIGHT)));

	bool hasHudWidget = (composition_.getActiveHudWidget() != nullptr);
	if (hasHudWidget && !hadHudWidget_) {
		annotations_.clearCodeTextInRect(composition_.getActiveHudWidget()->getBounds().rect());
	}
	hadHudWidget_ = hasHudWidget;

	gridPulseBoost_ *= std::exp(-dt / 0.8f);

	// Motion overlay — driven by blueprint_emergence's own state (GridState's
	// average activity + composition phase) rather than a crosshair signal.
	const ofTexture* liveTexForMotion = video_ != nullptr ? video_->currentTexture() : nullptr;
	if (liveTexForMotion != nullptr) {
		float activity = gridState_.getAverageActivity();
		float decayWeight = ofMap(activity, 0.0f, 1.0f, MOTION_DECAY_MIN, MOTION_DECAY_MAX, true);
		float sensitivity = ofMap(activity, 0.0f, 1.0f, MOTION_SENSITIVITY_MIN, MOTION_SENSITIVITY_MAX, true);
		motionEx_.update(*liveTexForMotion, decayWeight, sensitivity);

		using Phase = CompositionBase::CyclePhase;
		Phase phase = composition_.getPhase();
		float phaseFactor = (phase == Phase::PLACEMENT || phase == Phase::DENSITY) ? 1.0f : 0.0f;
		float targetOpacity = phaseFactor * ofMap(activity, 0.0f, 1.0f, MOTION_OPACITY_MIN, MOTION_OPACITY_MAX, true);
		motionOverlayOpacity_ += (targetOpacity - motionOverlayOpacity_) * ofClamp(dt / MOTION_OPACITY_SMOOTH_SECONDS, 0.0f, 1.0f);
	}
}

void BlueprintSceneCore::draw() {
	ofPushStyle();

	if (editorialCollage_) {
		collage_.draw(video_ != nullptr ? video_->currentTexture() : nullptr);
		ofPopStyle();
		return;
	}
	if (alt_ != nullptr) {
		alt_->draw(video_ != nullptr ? video_->currentTexture() : nullptr);
		ofPopStyle();
		return;
	}

	// The whole scene draws into ErosionFBO's capture buffer, which is then
	// blended against its own decaying history and presented — recent draws
	// leave a fading trace rather than vanishing the instant they stop being
	// drawn. See shared/src/ErosionFBO.h.
	if (bypassErosion_) {
		drawScene();
	} else {
		erosionFBO_.beginCapture();
		drawScene();
		erosionFBO_.endCapture();
		erosionFBO_.update();
		erosionFBO_.draw(0, 0, config_.canvasW, config_.canvasH);
	}

	// Fragment shader-effect overlays draw in their own pass, after the
	// erosion capture is closed. They use their own FBOs internally (to get
	// clean local UVs for crop-sampled effects); never nested inside the
	// erosion capture.
	for (const auto& fragment : composition_.getFragments()) {
		fragment->drawOverlay();
	}

	// Divider drawn after all overlays so it is always the topmost layer —
	// video content and shader effects cannot bleed over it.
	ofEnableAlphaBlending();
	float divBrightness = ofMap(lfoBank_.getUnipolar(static_cast<int>(BELFOLane::DIVIDER_BRIGHTNESS)),
		0.0f, 1.0f, LFO_DIVIDER_MIN, LFO_DIVIDER_MAX);
	auto divEndpoints = composition_.getDividerEndpoints();
	annotations_.drawDivider(divEndpoints.first, divEndpoints.second, divBrightness);

	ofPopStyle();
}

void BlueprintSceneCore::drawScene() {
	const float cw = static_cast<float>(config_.canvasW);
	const float ch = static_cast<float>(config_.canvasH);

	// LFO-driven grid opacity.
	// RULE_WHITE already carries ~30% baseline alpha; normalize the LFO's
	// 0.20–0.45 opacity range against that baseline since drawGrid(alpha)
	// is a multiplier on top of it, not a raw alpha value.
	float lfoFrac = ofMap(lfoBank_.getUnipolar(static_cast<int>(BELFOLane::GRID_OPACITY)),
		0.0f, 1.0f, LFO_GRID_OPACITY_MIN, LFO_GRID_OPACITY_MAX);
	float gridAlpha = lfoFrac / 0.30f;
	gridAlpha *= (1.0f - composition_.getGridDimAmount()); // DENSITY_CRITICAL response
	gridAlpha += gridPulseBoost_; // PHASE_TRANSITION response

	// Ground fill over the whole canvas (equivalent of the standalone's
	// ofBackground(GROUND_DARK), but scoped to the bound target).
	ofSetColor(GROUND_DARK);
	ofFill();
	ofDrawRectangle(0, 0, cw, ch);

	// Alpha blending stays on for the whole scene — fragment arrival/dissolve
	// fades, the desaturate shader's alpha output, HUD widget panels, and
	// annotation fades all depend on it.
	ofEnableAlphaBlending();

	if (composition_.isDividerHorizontal()) {
		float dividerY = composition_.getDividerY();
		ofSetColor(composition_.isZoneALight() ? GROUND_LIGHT : GROUND_DARK);
		ofDrawRectangle(0, 0, cw, dividerY);
		ofSetColor(GROUND_DARK);
		ofDrawRectangle(0, dividerY, cw, ch - dividerY);
	} else {
		float dividerX = grid_.getDividerX();
		ofSetColor(composition_.isZoneALight() ? GROUND_LIGHT : GROUND_DARK);
		ofDrawRectangle(0, 0, dividerX, ch);
		ofSetColor(GROUND_DARK);
		ofDrawRectangle(dividerX, 0, cw - dividerX, ch);
	}

	// Motion overlay sits behind the grid/fragments — a soft glow of recent
	// motion in the source video, intensity tracking composition activity.
	if (motionOverlayOpacity_ > 0.001f) {
		ofSetColor(255, 255, 255, static_cast<int>(255 * motionOverlayOpacity_));
		motionEx_.getMotionTexture().draw(0, 0, cw, ch);
	}

	for (const auto& fragment : composition_.getFragments()) {
		fragment->draw();
	}

	if (hud::HudWidget* widget = composition_.getActiveHudWidget()) {
		widget->draw();
	}

	if (hud::HudWidget* scanner = composition_.getCircleScannerWidget()) {
		scanner->draw();
	}

	annotations_.drawMeasurementLines();
	annotations_.drawCodeText();
	annotations_.drawHubHighlight();

	annotations_.drawGrid(gridAlpha);
}
