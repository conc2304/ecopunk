#include "RidgelineRenderer.h"

void RidgelineRenderer::setup(int canvasWidth, int canvasHeight) {
	canvasW = canvasWidth;
	canvasH = canvasHeight;
	rebuildNeeded = true;
}

// Nearest-neighbour sample on purpose: cheap CPU reads, no filtering.
// u, v are normalised 0..1 into the source pixels.
float RidgelineRenderer::sampleLuminance(const ofPixels& px, float u, float v) const {
	int w = px.getWidth();
	int h = px.getHeight();
	if (w <= 0 || h <= 0) return 0.0f;

	int x = ofClamp((int)(u * (w - 1)), 0, w - 1);
	int y = ofClamp((int)(v * (h - 1)), 0, h - 1);

	ofColor c = px.getColor(x, y);
	return (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) / 255.0f;
}

void RidgelineRenderer::rebuildLines(const ofPixels& sourcePixels) {
	lines.clear();

	int numLines = std::max(2, params.numLines);
	int samples  = std::max(2, params.samplesPerLine);

	float marginX   = canvasW * params.marginXPct;
	float usableW   = canvasW - marginX * 2.0f;
	float spacingPx = params.spacingPct * canvasH;
	float stackH    = spacingPx * (numLines - 1);
	float centerY   = canvasH * params.centerYPct;
	float bottomY   = centerY + stackH * 0.5f;

	lines.resize(numLines);

	for (int row = 0; row < numLines; row++) {
		float restY = bottomY - row * spacingPx;
		float v     = (float)row / (float)(numLines - 1);

		Line& line = lines[row];
		line.stroke.clear();

		for (int s = 0; s <= samples; s++) {
			float u   = (float)s / (float)samples;
			float lum = sampleLuminance(sourcePixels, u, v);
			float x   = marginX + u * usableW;
			float y   = restY - lum * params.amplitude;
			line.stroke.addVertex(x, y, 0.0f);
		}
	}

	rebuildNeeded = false;
}

void RidgelineRenderer::update(const ofPixels& sourcePixels) {
	if (!sourcePixels.isAllocated()) return;
	rebuildLines(sourcePixels);
}

void RidgelineRenderer::draw(ofTexture* sourceTexForOverlay, float opacity) {
	ofPushStyle();
	auto a = [opacity](int base) { return (int)(base * opacity); };

	// Background drawn in normal orientation — never flipped.
	if (params.overlayMode && sourceTexForOverlay && sourceTexForOverlay->isAllocated()) {
		ofSetColor(255, 255, 255, a(255));
		sourceTexForOverlay->draw(0, 0, canvasW, canvasH);
		ofSetColor(0, 0, 0, a(90));
		ofDrawRectangle(0, 0, canvasW, canvasH);
	} else {
		ofColor gc = params.groundColor;
		ofSetColor(gc.r, gc.g, gc.b, a(gc.a));
		ofDrawRectangle(0, 0, canvasW, canvasH);
	}

	// Ridgeline strokes (and occlusion fills) are the only things that flip.
	ofPushMatrix();
	if (params.flipX || params.flipY) {
		ofTranslate(params.flipX ? (float)canvasW : 0.f,
		            params.flipY ? (float)canvasH : 0.f);
		ofScale(params.flipX ? -1.f : 1.f,
		        params.flipY ? -1.f : 1.f);
	}
	for (auto& line : lines) {
		ofColor lc = params.lineColor;
		ofSetColor(lc.r, lc.g, lc.b, a(lc.a));
		ofSetLineWidth(1.0f);
		line.stroke.draw();
	}
	ofPopMatrix();

	ofPopStyle();
}
