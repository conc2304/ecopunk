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

	// Back-to-front: row (numLines-1) is farthest (drawn first),
	// row 0 is nearest (drawn last, on top).
	for (int row = numLines - 1; row >= 0; row--) {
		float restY = bottomY - row * spacingPx;
		float v     = (float)row / (float)(numLines - 1);

		Line& line = lines[row];
		line.stroke.clear();

		std::vector<glm::vec3> pts;
		pts.reserve(samples + 1);

		for (int s = 0; s <= samples; s++) {
			float u   = (float)s / (float)samples;
			float lum = sampleLuminance(sourcePixels, u, v);
			float x   = marginX + u * usableW;
			float y   = restY - lum * params.amplitude;
			pts.emplace_back(x, y, 0.0f);
			line.stroke.addVertex(x, y, 0.0f);
		}

		if (params.occlude) {
			// Triangle strip: (curve point, base point) pairs covering the
			// region between the ridge and a flat baseline below it.
			float baseY = restY + spacingPx * 0.6f + 2.0f;

			line.occlusionFill.clear();
			line.occlusionFill.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);

			for (auto& p : pts) {
				line.occlusionFill.addVertex(glm::vec3(p.x, p.y, 0.0f));
				line.occlusionFill.addVertex(glm::vec3(p.x, baseY, 0.0f));
			}
		}
	}

	rebuildNeeded = false;
}

void RidgelineRenderer::update(const ofPixels& sourcePixels) {
	if (!sourcePixels.isAllocated()) return;
	rebuildLines(sourcePixels);
}

void RidgelineRenderer::draw(ofTexture* sourceTexForOverlay) {
	ofPushStyle();

	if (params.overlayMode && sourceTexForOverlay && sourceTexForOverlay->isAllocated()) {
		ofSetColor(255);
		sourceTexForOverlay->draw(0, 0, canvasW, canvasH);
		ofSetColor(0, 0, 0, 90);
		ofDrawRectangle(0, 0, canvasW, canvasH);
	} else {
		ofSetColor(params.groundColor);
		ofDrawRectangle(0, 0, canvasW, canvasH);
	}

	for (auto& line : lines) {
		if (params.occlude && line.occlusionFill.getNumVertices() > 0) {
			ofSetColor(params.overlayMode ? ofColor(0, 0, 0) : params.groundColor);
			line.occlusionFill.draw();
		}
		ofSetColor(params.lineColor);
		ofSetLineWidth(1.0f);
		line.stroke.draw();
	}

	ofPopStyle();
}
