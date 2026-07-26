#include "TFPatternEcologicalSuccession.h"
#include "TFRandom.h"
#include "TFPlayheadAssignment.h"
#include "TFQuarantineHatch.h"
#include "TFSettings.h"
#include "Settings.h"
#include "ofMath.h"
#include "ofGraphics.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
	constexpr float NOISE_SCALE = 2.2f;
	constexpr float NOISE_TIME_SPEED = 0.0167f; // matches Blob Grid's "airy and calm" pass
}

void TFPatternEcologicalSuccession::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternEcologicalSuccession::resizeCanvas(int canvasW_, int canvasH_) {
	canvasW = canvasW_;
	canvasH = canvasH_;
}

void TFPatternEcologicalSuccession::reset(int seed) {
	std::srand(seed);
	elapsedTime = tfRandRangeF(0.0f, 1000.0f);
	noiseTime = elapsedTime;
	disturbanceTimer = 0.0f;
	rebuildGrid();
}

void TFPatternEcologicalSuccession::rebuildGrid() {
	gridCols = std::max(1, params.gridResolution);
	gridRows = std::max(1, params.gridResolution);
	float cellW = static_cast<float>(canvasW) / gridCols;
	float cellH = static_cast<float>(canvasH) / gridRows;

	patches.clear();
	patches.reserve(gridCols * gridRows);

	for (int gy = 0; gy < gridRows; gy++) {
		for (int gx = 0; gx < gridCols; gx++) {
			Patch p;
			p.bounds = ofRectangle(gx * cellW, gy * cellH, cellW, cellH);
			// Randomized starting age/phase so every patch doesn't sprout and
			// churn in lockstep — same desync reasoning as Blob Grid's blob
			// phases and Particle Field's spawn jitter.
			p.age = tfRandRangeF(0.0f, std::max(0.01f, params.maturityTime));
			float maturity = ofClamp(p.age / std::max(0.01f, params.maturityTime), 0.0f, 1.0f);
			float interval = ofLerp(params.youngTurnoverInterval, params.climaxTurnoverInterval, maturity);
			p.reassignTimer = tfRandRangeF(0.0f, interval);

			float cx = (p.bounds.x + p.bounds.width * 0.5f) / canvasW;
			float cy = (p.bounds.y + p.bounds.height * 0.5f) / canvasH;
			float gray = ofNoise(cx * NOISE_SCALE, cy * NOISE_SCALE, noiseTime);
			p.committedOffset = videoBuffer->quantize(gray);

			patches.push_back(std::move(p));
		}
	}
}

void TFPatternEcologicalSuccession::update(float dt) {
	elapsedTime += dt;
	noiseTime += dt * NOISE_TIME_SPEED;

	disturbanceTimer += dt;
	if (disturbanceTimer >= params.disturbanceInterval) {
		disturbanceTimer -= params.disturbanceInterval;
		triggerDisturbance();
	}

	std::vector<ofVec2f> normalizedCenters(patches.size());
	std::vector<float> desiredOffsets(patches.size());

	for (size_t i = 0; i < patches.size(); i++) {
		Patch& p = patches[i];
		p.age += dt;

		float cx = (p.bounds.x + p.bounds.width * 0.5f) / canvasW;
		float cy = (p.bounds.y + p.bounds.height * 0.5f) / canvasH;
		normalizedCenters[i] = ofVec2f(cx, cy);

		float maturity = ofClamp(p.age / std::max(0.01f, params.maturityTime), 0.0f, 1.0f);
		float interval = ofLerp(params.youngTurnoverInterval, params.climaxTurnoverInterval, maturity);

		p.reassignTimer += dt;
		if (p.reassignTimer >= interval) {
			p.reassignTimer -= interval;
			float gray = ofNoise(cx * NOISE_SCALE, cy * NOISE_SCALE, noiseTime);
			p.committedOffset = videoBuffer->quantize(gray);
		}
		desiredOffsets[i] = p.committedOffset;

		if (p.flashElapsed >= 0.0f) {
			p.flashElapsed += dt;
			if (p.flashElapsed >= params.disturbanceFlashDuration) {
				p.flashElapsed = -1.0f;
			}
		}
	}

	std::vector<int> playheadIndices;
	tfAssignPlayheadsByDesiredOffsets(*videoBuffer, desiredOffsets, playheadIndices);

	float bufW = static_cast<float>(videoBuffer->getBufferWidth());
	float bufH = static_cast<float>(videoBuffer->getBufferHeight());

	for (size_t i = 0; i < patches.size(); i++) {
		Patch& p = patches[i];
		int newIndex = playheadIndices[i];

		if (p.lastPlayheadIndex >= 0 && p.lastPlayheadIndex != newIndex) {
			float maturity = ofClamp(p.age / std::max(0.01f, params.maturityTime), 0.0f, 1.0f);
			float hardCutW = ofLerp(params.youngHardCutWeight, 0.0f, maturity);
			float erosionW = ofLerp(0.0f, params.climaxErosionWeight, maturity);

			ofRectangle srcRect(
				(p.bounds.x / canvasW) * bufW,
				(p.bounds.y / canvasH) * bufH,
				(p.bounds.width / canvasW) * bufW,
				(p.bounds.height / canvasH) * bufH);

			if (!p.transition) {
				p.transition = std::make_unique<TFFragmentTransition>();
			}
			p.transition->begin(
				tfPickTransitionStyle(hardCutW, params.crossfadeWeight, erosionW),
				params.transitionDuration,
				p.bounds,
				videoBuffer->getPlayheadTexture(p.lastPlayheadIndex),
				srcRect);

			if (onFragmentReassignedCb) {
				onFragmentReassignedCb(normalizedCenters[i].x, normalizedCenters[i].y);
			}
		}

		p.playheadIndex = newIndex;
		p.lastPlayheadIndex = newIndex;

		if (p.transition) {
			p.transition->update(dt);
		}
	}
}

void TFPatternEcologicalSuccession::triggerDisturbance() {
	float shorterEdge = static_cast<float>(std::min(canvasW, canvasH));
	float radiusPx = params.disturbanceRadius * shorterEdge;
	float radiusSq = radiusPx * radiusPx;

	ofVec2f center(tfRandRangeF(0.0f, static_cast<float>(canvasW)), tfRandRangeF(0.0f, static_cast<float>(canvasH)));

	for (auto& p : patches) {
		float cx = p.bounds.x + p.bounds.width * 0.5f;
		float cy = p.bounds.y + p.bounds.height * 0.5f;
		float dx = cx - center.x;
		float dy = cy - center.y;
		if (dx * dx + dy * dy <= radiusSq) {
			p.age = 0.0f;
			p.reassignTimer = 0.0f;
			p.flashElapsed = 0.0f;
		}
	}
}

void TFPatternEcologicalSuccession::draw() {
	float bufW = static_cast<float>(videoBuffer->getBufferWidth());
	float bufH = static_cast<float>(videoBuffer->getBufferHeight());

	ofEnableAlphaBlending();

	for (auto& p : patches) {
		float maturity = ofClamp(p.age / std::max(0.01f, params.maturityTime), 0.0f, 1.0f);

		if (maturity >= params.sproutThreshold && p.playheadIndex >= 0) {
			const ofTexture& tex = videoBuffer->getPlayheadTexture(p.playheadIndex);
			if (tex.isAllocated()) {
				float renderScale = ofLerp(params.minRenderScale, 1.0f, maturity);
				float insetW = p.bounds.width * renderScale;
				float insetH = p.bounds.height * renderScale;
				ofRectangle insetBounds(
					p.bounds.x + (p.bounds.width - insetW) * 0.5f,
					p.bounds.y + (p.bounds.height - insetH) * 0.5f,
					insetW, insetH);

				// Cropped proportionally to the patch's full bounds (not the
				// shrunk inset rect), so the video content itself visibly
				// grows into the cell as maturity increases rather than just
				// showing a differently-cropped slice at a fixed size.
				ofRectangle srcRect(
					(p.bounds.x / canvasW) * bufW,
					(p.bounds.y / canvasH) * bufH,
					(p.bounds.width / canvasW) * bufW,
					(p.bounds.height / canvasH) * bufH);

				if (p.transition) {
					p.transition->draw(insetBounds, tex, srcRect);
				} else {
					ofSetColor(255);
					tex.drawSubsection(insetBounds.x, insetBounds.y, insetBounds.width, insetBounds.height,
						srcRect.x, srcRect.y, srcRect.width, srcRect.height);
				}
			}
		}
		// Below Sprout Threshold: skip the draw call entirely (Section 0) —
		// bare ground shows through via whatever's already drawn beneath
		// composition.draw() in ofApp::draw().

		if (p.flashElapsed >= 0.0f) {
			float opacity = 1.0f - ofClamp(p.flashElapsed / params.disturbanceFlashDuration, 0.0f, 1.0f);
			tfDrawQuarantineHatch(p.bounds, HATCH_WARNING, opacity);
		}
	}
}

std::vector<ofVec2f> TFPatternEcologicalSuccession::getActiveFragmentCenters() const {
	std::vector<ofVec2f> centers;
	centers.reserve(patches.size());
	for (const auto& p : patches) {
		float maturity = ofClamp(p.age / std::max(0.01f, params.maturityTime), 0.0f, 1.0f);
		if (maturity < params.sproutThreshold || p.playheadIndex < 0) {
			continue; // matches draw()'s skip — not actually visible
		}
		centers.push_back(ofVec2f(
			(p.bounds.x + p.bounds.width * 0.5f) / canvasW,
			(p.bounds.y + p.bounds.height * 0.5f) / canvasH));
	}
	return centers;
}
