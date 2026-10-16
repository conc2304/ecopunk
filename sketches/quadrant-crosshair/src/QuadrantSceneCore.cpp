#include "QuadrantSceneCore.h"

#include "QCCanvas.h"

namespace quadrantcrosshair {

void QuadrantSceneCore::applyCanvasSize() {
	if (config_.trackWindow) setCanvasSize(0.0f, 0.0f); // 0 = follow the live window
	else setCanvasSize(static_cast<float>(config_.canvasW), static_cast<float>(config_.canvasH));
}

void QuadrantSceneCore::setup(const Config& config) {
	config_ = config;
	applyCanvasSize();

	shaders_.setup(config_.shaderLayout);
	motionEx_.setShaderPaths(config_.motionVertShader, config_.motionAccumShader, config_.motionExtractShader);
	// Allocation (quadrant erosion FBOs, motion FBOs, grid texture) happens
	// once here; activations reuse them (resetComposition()).
	quadrants_.setup(&shaders_);
	motionEx_.setup();
	grid_.setup();

	resetComposition();
	didSetup_ = true;
}

void QuadrantSceneCore::resetComposition() {
	lfo_ = LFOBank{};
	lfo_.setup();
	crosshair_ = CrosshairSystem{};
	crosshair_.setup();
	triggerBus_ = TriggerBus{};
	triggerBus_.setup();
	expansionDirector_ = ExpansionDirector{};
	expansionDirector_.setup();
	prevExpansionState_ = ExpansionState::IDLE;

	quadrants_.resetComposition();
	motionEx_.reset();
	grid_.setup(); // clears the grid field (re-allocating the tiny grid texture)

	// TriggerBus -> QuadrantManager
	triggerBus_.addListener([this](const TriggerEvent& e) {
		quadrants_.onTrigger(e);
	});
	// TriggerBus -> CrosshairSystem bloom
	triggerBus_.addListener([this](const TriggerEvent& e) {
		if (e.id == TriggerID::VELOCITY_HIGH && e.active)
			crosshair_.triggerBloom(24.f, 0.4f);
		if (e.id == TriggerID::DWELL)
			crosshair_.setBloomFill(e.active ? 1.f : 0.f, 2.0f);
	});

	hud_ = HudManager{};
	hud_.setup(triggerBus_, crosshair_, lfo_, quadrants_, motionEx_);
	hud_.showInformationLayer = config_.showHudInformationLayer;

	steeringBrightness_ = 0.5f;
	motionOverlayAlpha_ = 55;
	motionStillTimer_ = 0.f;
	motionRampAlpha_ = 55.f;
}

void QuadrantSceneCore::activate() {
	if (!didSetup_) return;
	applyCanvasSize();
	resetComposition();
	active_ = true;
}

void QuadrantSceneCore::onMediaChanged(const std::string& mediaName) {
	quadrants_.resetErosion();
	hud_.onVideoFileChanged(mediaName);
}

void QuadrantSceneCore::update(float dt, const VideoFrame& frame) {
	if (!active_) return;

	// 1. Source systems (media itself is advanced by the caller)
	lfo_.update(dt);

	// 2. Video CPU sampling + motion extraction
	if (frame.isFrameNew && frame.pixels != nullptr && frame.pixels->isAllocated() && frame.texture != nullptr) {
		const ofPixels& px = *frame.pixels;
		CrosshairState cs = crosshair_.getState();
		crosshair_.sampleColor(px, cs.cx, cs.cy);
		steeringBrightness_ = sampleBrightness(px);

		// Decay weight: long memory default, slowly swept by LFO
		float decayWeight = ofMap(lfo_.get(LFO_RD_FEED), -1, 1, 0.92f, 0.995f);
		float energy = motionEx_.getMotionEnergy();
		// Sensitivity: auto-adjusts inversely with energy to keep output readable
		float sensitivity = ofMap(energy, 0.f, 0.3f, 6.0f, 2.0f, true);

		motionEx_.update(*frame.texture, decayWeight, sensitivity);

		// Feed centroid to crosshair as soft attractor
		crosshair_.setMotionAttractor(
			motionEx_.getMotionCentroidX() * canvasWidth(),
			motionEx_.getMotionCentroidY() * canvasHeight(),
			energy);

		// Output mode switching based on energy thresholds
		if (energy > 0.25f) {
			motionStillTimer_ = 0.f;
			if (motionEx_.getOutputMode() != 1) motionEx_.setOutputMode(1); // CHROMA_PRESERVE
		} else if (energy < 0.02f) {
			motionStillTimer_ += dt;
			if (motionStillTimer_ > 5.f && motionEx_.getOutputMode() != 2)
				motionEx_.setOutputMode(2); // SIGNED_FIELD
		} else {
			motionStillTimer_ = 0.f;
			if (motionEx_.getOutputMode() != 0) motionEx_.setOutputMode(0); // LUMA_GLOW
		}

		// Ramp overlay alpha up on motion onset (energy crossing 0.1 rising)
		float targetAlpha = (energy > 0.1f) ? 80.f : 55.f;
		motionRampAlpha_ += (targetAlpha - motionRampAlpha_) * 0.05f;
		motionOverlayAlpha_ = (int)motionRampAlpha_;
	}

	// 3. Crosshair motion
	crosshair_.update(dt, lfo_);

	// 4. Expansion director — must run after crosshair so it reads current position
	{
		expansionDirector_.update(dt,
			{ crosshair_.getState().cx, crosshair_.getState().cy },
			crosshair_.getState().speed);
		ExpansionState curExpState = expansionDirector_.getState();

		// Hand crosshair position control to director while sequence is active
		if (expansionDirector_.isActive()) {
			crosshair_.setExpansionControl(true, expansionDirector_.getCrosshairTarget());
		} else {
			crosshair_.setExpansionControl(false, {});
		}

		// IDLE -> TRAVEL_OUT: clear stale trigger state so QuadrantManager is not left dirty
		if (prevExpansionState_ == ExpansionState::IDLE && curExpState == ExpansionState::TRAVEL_OUT)
			triggerBus_.clearAll();

		// HOLD -> TRAVEL_BACK: begin independent quadrant contraction crossfade
		if (prevExpansionState_ == ExpansionState::HOLD && curExpState == ExpansionState::TRAVEL_BACK)
			quadrants_.beginContraction(expansionDirector_.getTargetQuadrant());

		// TRAVEL_BACK -> IDLE: hand crosshair back to noise via 2-second blend
		if (prevExpansionState_ == ExpansionState::TRAVEL_BACK && curExpState == ExpansionState::IDLE) {
			crosshair_.beginResume();
			triggerBus_.clearAll();
		}

		prevExpansionState_ = curExpState;
	}

	// 5. Triggers — suppressed while expansion holds the crosshair
	if (!expansionDirector_.isActive())
		triggerBus_.update(crosshair_.getState(), dt);

	// 6. Grid
	float decayRate = ofMap(lfo_.get(LFO_GRID_DECAY), -1, 1, 0.96f, 0.995f);
	CrosshairState cs = crosshair_.getState();
	grid_.update(cs.cx, cs.cy, dt, decayRate);
	grid_.uploadTexture();

	// 7. Quadrants
	quadrants_.setVideoBrightness(steeringBrightness_);
	quadrants_.setVideoPixels(frame.pixels != nullptr && frame.pixels->isAllocated() ? frame.pixels : nullptr);
	quadrants_.update(dt, cs, lfo_, grid_.getTexture(),
		motionEx_.getMotionTexture(), motionEx_.getDelayedMotionTexture());
	if (frame.texture != nullptr && frame.texture->isAllocated()) {
		quadrants_.accumulateErosion(*frame.texture); // off-screen; never nested in the draw target
	}

	// 8. HUD layer (in-artwork)
	hud_.update(dt);
}

void QuadrantSceneCore::draw(const VideoFrame& frame) {
	ofPushStyle();
	const float W = canvasWidth();
	const float H = canvasHeight();
	float uiAlpha = expansionDirector_.getUIFadeAlpha();

	// Scoped to the bound target (the standalone window, or the runtime's
	// scene FBO) — ofClear does not change OF's global background state.
	ofClear(13, 13, 13, 255);

	// Layer 1: background fullscreen motion effect video (always bottom)
	if (motionOverlayAlpha_ > 0) {
		ofEnableAlphaBlending();
		ofSetColor(255, 255, 255, (int)(motionOverlayAlpha_ * uiAlpha));
		motionEx_.getMotionTexture().draw(0, 0, W, H);
		ofDisableAlphaBlending();
	}

	// Layer 2: fragment video / circle layer
	if (frame.texture != nullptr && frame.texture->isAllocated()) {
		quadrants_.draw(*frame.texture, glm::vec2(W, H));
	}

	// Layer 3: grid lines / in-artwork HUD layer
	if (showHud_) hud_.draw(uiAlpha);

	// Layer 4: orange starter line
	crosshair_.draw(uiAlpha);
	ofPopStyle();
}

float QuadrantSceneCore::sampleBrightness(const ofPixels& px) {
	int W = px.getWidth(), H = px.getHeight();
	float sum = 0.f;
	int n = 0;
#ifdef PLATFORM_PI
	const int STEP = 64;
#else
	const int STEP = 16;
#endif
	for (int y = 0; y < H; y += STEP) {
		for (int x = 0; x < W; x += STEP) {
			auto c = px.getColor(x, y);
			sum += 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
			n++;
		}
	}
	return n > 0 ? sum / (n * 255.f) : 0.5f;
}

} // namespace quadrantcrosshair
