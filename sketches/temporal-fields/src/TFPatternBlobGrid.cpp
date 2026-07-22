#include "TFPatternBlobGrid.h"
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
	constexpr float NOISE_TIME_SPEED = 0.05f;
	// How far above the field's inside/outside threshold (1.0) a block's
	// corners must sit before it's safe to merge into one larger fragment
	// — keeps merged blocks well clear of the feathered boundary, which
	// must stay at the finest available size per the brief.
	constexpr float INTERIOR_MARGIN = 0.5f;
	// Cells at or below this coverage are skipped entirely when maskToBlob
	// is on — matches the brief's Section 6a "~0.02" threshold. Skipping
	// the draw call (not just drawing at alpha 0) is the actual point:
	// on the Pi, an alpha-0 draw still costs a fragment shader invocation
	// and overdraw.
	constexpr float MASK_COVERAGE_THRESHOLD = 0.02f;
}

void TFPatternBlobGrid::setup(TimeOffsetVideoBuffer* videoBuffer_, int canvasW_, int canvasH_, const Params& params_) {
	videoBuffer = videoBuffer_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	params = params_;
}

void TFPatternBlobGrid::reset(int seed) {
	std::srand(seed);
	elapsedTime = tfRandRangeF(0.0f, 1000.0f);

	blobs.clear();
	int n = ofClamp(params.blobCenters, 1, 8);
	float minEdge = static_cast<float>(std::min(canvasW, canvasH));

	for (int i = 0; i < n; i++) {
		Blob b;
		b.anchor = ofVec2f(tfRandRangeF(canvasW * 0.25f, canvasW * 0.75f), tfRandRangeF(canvasH * 0.25f, canvasH * 0.75f));
		b.orbitRadius = tfRandRangeF(minEdge * 0.05f, minEdge * 0.2f);
		b.angularSpeed = tfRandRangeF(0.1f, 0.4f) * (tfRandRangeI(0, 1) == 0 ? 1.0f : -1.0f);
		b.phase = tfRandRangeF(0.0f, TWO_PI);
		blobs.push_back(b);
	}

	refreshTimer = 0.0f;
	updateBlobPositions();
	rebuildQuadtree();
	updateFragmentAlphasAndPlayheads();
}

void TFPatternBlobGrid::update(float dt) {
	elapsedTime += dt;
	updateBlobPositions();

	refreshTimer += dt;
	if (refreshTimer >= params.fragmentRefreshRate) {
		refreshTimer -= params.fragmentRefreshRate;
		// Note: rebuildQuadtree() discards the whole fragments vector and
		// rebuilds fresh, so every fragment's lastPlayheadIndex resets to
		// -1 here — a shape-change tick never plays a transition itself
		// (the brief also names fragment shape/size changes as a trigger;
		// that specific case isn't covered yet, only per-fragment playhead
		// reassignment between rebuilds — see the class comment).
		rebuildQuadtree();
	}

	updateFragmentAlphasAndPlayheads();

	for (auto& f : fragments) {
		if (f.transition) {
			f.transition->update(dt);
		}
	}
}

void TFPatternBlobGrid::draw() {
	if (!params.transparentBackground) {
		drawFragments();
		return;
	}

	// Transparent mode: composite into an alpha-capable off-screen FBO —
	// cleared to (0,0,0,0), not an opaque fill — then draw that FBO's
	// texture over whatever's already on screen with normal alpha
	// blending. Note this isn't OS-level window transparency (the
	// deployed sketch runs fullscreen/headless with nothing else to
	// composite against) — it's a real alpha hole ready for an in-app
	// background layer (a separate texture, a second video layer, etc.)
	// to eventually sit behind, once one exists. See Section 6a.
	if (!compositeFbo.isAllocated() || static_cast<int>(compositeFbo.getWidth()) != canvasW
		|| static_cast<int>(compositeFbo.getHeight()) != canvasH) {
		ofFbo::Settings s;
		s.width = canvasW;
		s.height = canvasH;
		s.internalformat = GL_RGBA;
		s.useDepth = false;
		compositeFbo.allocate(s);
	}

	compositeFbo.begin();
	ofClear(0, 0, 0, 0);
	drawFragments();
	compositeFbo.end();

	ofEnableAlphaBlending();
	ofSetColor(255);
	compositeFbo.draw(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
}

void TFPatternBlobGrid::drawFragments() {
	ofEnableAlphaBlending();

	float bufW = static_cast<float>(videoBuffer->getBufferWidth());
	float bufH = static_cast<float>(videoBuffer->getBufferHeight());

	for (auto& f : fragments) {
		if (f.playheadIndex < 0) {
			continue;
		}

		// maskToBlob off is a debug/tuning view of the raw quadtree
		// structure — draw every cell at full opacity regardless of the
		// blob field, ignoring the coverage-based skip below entirely.
		float effectiveAlpha = params.maskToBlob ? f.alpha : 1.0f;

		if (params.maskToBlob && effectiveAlpha <= MASK_COVERAGE_THRESHOLD) {
			// Skip the draw call outright — not an equivalent alpha-0
			// draw. See the MASK_COVERAGE_THRESHOLD comment.
			continue;
		}

		ofRectangle srcRect(
			(f.bounds.x / canvasW) * bufW,
			(f.bounds.y / canvasH) * bufH,
			(f.bounds.width / canvasW) * bufW,
			(f.bounds.height / canvasH) * bufH);

		const ofTexture& tex = videoBuffer->getPlayheadTexture(f.playheadIndex);
		if (!tex.isAllocated()) {
			// History is still empty (first frame or two after launch) —
			// nothing to draw yet.
			continue;
		}

		if (f.transition) {
			f.transition->draw(f.bounds, tex, srcRect, effectiveAlpha);
		} else {
			ofSetColor(255, 255, 255, static_cast<int>(255 * effectiveAlpha));
			tex.drawSubsection(f.bounds.x, f.bounds.y, f.bounds.width, f.bounds.height,
				srcRect.x, srcRect.y, srcRect.width, srcRect.height);
		}

		// Quarantine hatch: continuous, computed straight from this frame's
		// coverage (f.alpha) — no held/quarantined state needed here, unlike
		// BSP's discrete hold-then-split. Opacity scales up as coverage
		// drops toward the mask-skip threshold, i.e. as the cell gets
		// closer to actually disappearing.
		if (params.maskToBlob && f.alpha < BLOBGRID_HATCH_COVERAGE_THRESHOLD) {
			float hatchOpacity = 1.0f - ofClamp(f.alpha / BLOBGRID_HATCH_COVERAGE_THRESHOLD, 0.0f, 1.0f);
			tfDrawQuarantineHatch(f.bounds, HATCH_WARNING, hatchOpacity);
		}
	}
}

void TFPatternBlobGrid::updateBlobPositions() {
	for (auto& b : blobs) {
		float t = elapsedTime * params.driftSpeed * b.angularSpeed + b.phase;
		b.pos.x = b.anchor.x + b.orbitRadius * std::cos(t);
		b.pos.y = b.anchor.y + b.orbitRadius * std::sin(t);
	}
}

float TFPatternBlobGrid::fieldAt(float px, float py) const {
	float rPix = params.blobRadius * static_cast<float>(std::min(canvasW, canvasH));
	float rPixSq = rPix * rPix;

	float field = 0.0f;
	for (const auto& b : blobs) {
		float dx = px - b.pos.x;
		float dy = py - b.pos.y;
		float distSq = std::max(dx * dx + dy * dy, 1.0f);
		field += rPixSq / distSq;
	}
	return field;
}

bool TFPatternBlobGrid::isBlockSafelyInterior(int gx, int gy, int k) const {
	float x0 = gx * baseCellW;
	float y0 = gy * baseCellH;
	float x1 = (gx + k) * baseCellW;
	float y1 = (gy + k) * baseCellH;

	float corners[4][2] = { { x0, y0 }, { x1, y0 }, { x0, y1 }, { x1, y1 } };
	for (auto& c : corners) {
		if (fieldAt(c[0], c[1]) < 1.0f + INTERIOR_MARGIN) {
			return false;
		}
	}
	return true;
}

void TFPatternBlobGrid::rebuildQuadtree() {
	gridCols = std::max(1, params.gridResolution);
	gridRows = std::max(1, params.gridResolution);
	baseCellW = static_cast<float>(canvasW) / gridCols;
	baseCellH = static_cast<float>(canvasH) / gridRows;

	int maxMergeK = std::max(1, static_cast<int>(std::round(ofLerp(1.0f, 7.0f, ofClamp(params.sizeVariation, 0.0f, 1.0f)))));

	std::vector<std::vector<bool>> covered(gridRows, std::vector<bool>(gridCols, false));
	fragments.clear();

	for (int gy = 0; gy < gridRows; gy++) {
		for (int gx = 0; gx < gridCols; gx++) {
			if (covered[gy][gx]) {
				continue;
			}

			int chosenK = 1;
			for (int k = maxMergeK; k >= 1; k--) {
				if (gx + k > gridCols || gy + k > gridRows) {
					continue;
				}
				if (k == 1 || isBlockSafelyInterior(gx, gy, k)) {
					chosenK = k;
					break;
				}
			}

			for (int dy = 0; dy < chosenK; dy++) {
				for (int dx = 0; dx < chosenK; dx++) {
					covered[gy + dy][gx + dx] = true;
				}
			}

			Fragment f;
			f.bounds = ofRectangle(gx * baseCellW, gy * baseCellH, chosenK * baseCellW, chosenK * baseCellH);
			fragments.push_back(std::move(f));
		}
	}
}

void TFPatternBlobGrid::updateFragmentAlphasAndPlayheads() {
	std::vector<ofVec2f> normalizedCenters(fragments.size());

	float featherLow = 1.0f - ofClamp(params.edgeSoftness, 0.01f, 1.0f);

	for (size_t i = 0; i < fragments.size(); i++) {
		Fragment& f = fragments[i];
		float cx = f.bounds.x + f.bounds.width * 0.5f;
		float cy = f.bounds.y + f.bounds.height * 0.5f;

		float field = fieldAt(cx, cy);
		f.alpha = ofClamp((field - featherLow) / (1.0f - featherLow), 0.0f, 1.0f);

		normalizedCenters[i] = ofVec2f(cx / canvasW, cy / canvasH);
	}

	std::vector<int> playheadIndices;
	tfAssignPlayheadsByNoise(*videoBuffer, elapsedTime * NOISE_TIME_SPEED, NOISE_SCALE, normalizedCenters, playheadIndices);

	float bufW = static_cast<float>(videoBuffer->getBufferWidth());
	float bufH = static_cast<float>(videoBuffer->getBufferHeight());

	for (size_t i = 0; i < fragments.size(); i++) {
		Fragment& f = fragments[i];
		int newIndex = playheadIndices[i];

		if (f.lastPlayheadIndex >= 0 && f.lastPlayheadIndex != newIndex) {
			ofRectangle srcRect(
				(f.bounds.x / canvasW) * bufW,
				(f.bounds.y / canvasH) * bufH,
				(f.bounds.width / canvasW) * bufW,
				(f.bounds.height / canvasH) * bufH);

			if (!f.transition) {
				f.transition = std::make_unique<TFFragmentTransition>();
			}
			f.transition->begin(
				tfPickTransitionStyle(params.hardCutWeight, params.crossfadeWeight, params.erosionWeight),
				params.transitionDuration,
				f.bounds,
				videoBuffer->getPlayheadTexture(f.lastPlayheadIndex),
				srcRect);

			if (onFragmentReassignedCb) {
				onFragmentReassignedCb(normalizedCenters[i].x, normalizedCenters[i].y);
			}
		}

		f.playheadIndex = newIndex;
		f.lastPlayheadIndex = newIndex;
	}
}

std::vector<ofVec2f> TFPatternBlobGrid::getActiveFragmentCenters() const {
	std::vector<ofVec2f> centers;
	centers.reserve(fragments.size());
	for (const auto& f : fragments) {
		if (f.playheadIndex < 0) {
			continue;
		}
		if (params.maskToBlob && f.alpha <= MASK_COVERAGE_THRESHOLD) {
			continue; // matches drawFragments()'s skip — not actually visible
		}
		centers.push_back(ofVec2f(
			(f.bounds.x + f.bounds.width * 0.5f) / canvasW,
			(f.bounds.y + f.bounds.height * 0.5f) / canvasH));
	}
	return centers;
}
