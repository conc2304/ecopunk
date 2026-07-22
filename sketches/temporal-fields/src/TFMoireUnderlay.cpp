#include "TFMoireUnderlay.h"
#include "TFSettings.h"
#include "ofGraphics.h"
#include <cmath>

void TFMoireUnderlay::setup(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
	angleA = 0.0f;
	angleB = 0.0f;
}

void TFMoireUnderlay::update(float dt) {
	angleA += MOIRE_ANGULAR_SPEED_A * dt;
	angleB += MOIRE_ANGULAR_SPEED_B * dt;
}

void TFMoireUnderlay::drawGrid(float angleDeg, float spacing, float opacity) const {
	ofPushMatrix();
	ofTranslate(canvasW * 0.5f, canvasH * 0.5f);
	ofRotateDeg(angleDeg);

	// Half-diagonal so the rotated grid still fully covers the canvas at
	// any angle, not just axis-aligned.
	float halfDiag = 0.5f * std::sqrt(static_cast<float>(canvasW) * canvasW + static_cast<float>(canvasH) * canvasH);

	ofSetColor(255, 255, 255, static_cast<int>(255 * opacity));
	ofSetLineWidth(1.0f);
	for (float x = -halfDiag; x <= halfDiag; x += spacing) {
		ofDrawLine(x, -halfDiag, x, halfDiag);
	}

	ofPopMatrix();
}

void TFMoireUnderlay::draw() const {
	ofPushStyle();
	ofEnableAlphaBlending();
	drawGrid(angleA, MOIRE_LINE_SPACING, MOIRE_OPACITY);
	drawGrid(angleB, MOIRE_LINE_SPACING, MOIRE_OPACITY);
	ofPopStyle();
}
