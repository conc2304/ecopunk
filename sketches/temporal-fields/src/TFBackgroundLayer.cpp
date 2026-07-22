#include "TFBackgroundLayer.h"
#include "TFRandom.h"
#include "ofMath.h"
#include <utility>
#include <vector>

void TFBackgroundLayer::setup(TimeOffsetVideoBuffer* videoBuffer_, ShaderLibrary* shaderLib,
	const std::string& imagesFolder, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;

	effectPicker.setup(shaderLib);
	effectPicker.setWeights(params.effectWeights);

	imageCycler.setup(imagesFolder);
	imageCycler.setParams(params.imageParams);

	pickNextMode();
}

void TFBackgroundLayer::setParams(const Params& p) {
	params = p;
	effectPicker.setWeights(params.effectWeights);
	imageCycler.setParams(params.imageParams);
}

void TFBackgroundLayer::update(float dt) {
	// Run both unconditionally regardless of currentMode — cheap (just
	// timers/weight bookkeeping when not being drawn), and means switching
	// back to a mode mid-cycle doesn't always land on a freshly-reset
	// state, which reads more organic.
	effectPicker.update(dt);
	imageCycler.update(dt);

	modeTimer += dt;
	if (modeTimer >= params.modeChangeInterval) {
		modeTimer = 0.0f;
		pickNextMode();
	}
}

void TFBackgroundLayer::pickNextMode() {
	std::vector<std::pair<Mode, float>> options = { { Mode::FULL_VIDEO, params.fullVideoWeight },
		{ Mode::FULL_IMAGE, params.fullImageWeight }, { Mode::SPLIT, params.splitWeight } };
	currentMode = tfWeightedPick(options);

	if (currentMode == Mode::SPLIT) {
		currentSplitIsVertical = (params.splitAxisChoice == 1) ? true
			: (params.splitAxisChoice == 2)                    ? false
															   : (tfRandRangeI(0, 1) == 0);
		videoOnFirstHalf = (tfRandRangeI(0, 1) == 0);
	}
}

void TFBackgroundLayer::draw() {
	switch (currentMode) {
		case Mode::FULL_VIDEO:
			drawFullVideo();
			break;
		case Mode::FULL_IMAGE:
			drawFullImage();
			break;
		case Mode::SPLIT:
			drawSplit();
			break;
	}
}

void TFBackgroundLayer::drawFullVideo() {
	if (videoBuffer == nullptr) {
		return;
	}
	effectPicker.drawCurrent(videoBuffer->getRawVideoTexture(), ofRectangle(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH)));
}

void TFBackgroundLayer::drawFullImage() {
	imageCycler.draw(ofRectangle(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH)));
}

void TFBackgroundLayer::drawSplit() {
	// Soft-bounded away from the extremes to avoid a degenerate
	// near-zero-width half — the "1:2" example from the request (~0.33) is
	// well within this range; 0.5 is the shipped default.
	float ratio = ofClamp(params.splitRatio, 0.2f, 0.8f);

	ofRectangle rectA;
	ofRectangle rectB;

	if (currentSplitIsVertical) {
		float splitX = canvasW * ratio;
		rectA = ofRectangle(0, 0, splitX, static_cast<float>(canvasH));
		rectB = ofRectangle(splitX, 0, canvasW - splitX, static_cast<float>(canvasH));
	} else {
		float splitY = canvasH * ratio;
		rectA = ofRectangle(0, 0, static_cast<float>(canvasW), splitY);
		rectB = ofRectangle(0, splitY, static_cast<float>(canvasW), canvasH - splitY);
	}

	const ofRectangle& videoRect = videoOnFirstHalf ? rectA : rectB;
	const ofRectangle& imageRect = videoOnFirstHalf ? rectB : rectA;

	if (videoBuffer != nullptr) {
		effectPicker.drawCurrent(videoBuffer->getRawVideoTexture(), videoRect);
	}
	imageCycler.draw(imageRect);
}
