#include "BEComposition.h"
#include "BESettings.h"
#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"
#include "ofLog.h"
#include "ofMath.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

void BEComposition::setupBE(GridSystem * grid_, VideoSampler * videoSampler_, int canvasW_, int canvasH_, int dividerCol_) {
	videoSampler = videoSampler_;
	canvasW = canvasW_;
	canvasH = canvasH_;
	dividerCol = dividerCol_;

	CompositionBase::Timing timing;
	timing.cycleDurationMin = CYCLE_DURATION_MIN;
	timing.cycleDurationMax = CYCLE_DURATION_MAX;
	timing.blankDuration = BLANK_DURATION;
	timing.densityDuration = DENSITY_DURATION;
	timing.dissolveDuration = DISSOLVE_DURATION;
	timing.resetHoldDuration = RESET_HOLD_DURATION;
	timing.placementIntervalMin = PLACEMENT_INTERVAL_MIN;
	timing.placementIntervalMax = PLACEMENT_INTERVAL_MAX;
	timing.placementIntervalDense = PLACEMENT_INTERVAL_DENSE;
	timing.dissolveFadeMin = DISSOLVE_FADE_MIN;
	timing.dissolveFadeMax = DISSOLVE_FADE_MAX;
	timing.maxFragments = MAX_FRAGMENTS;

	CompositionBase::setup(grid_, timing);
}

void BEComposition::onCycleStart() {
	zoneALight = randRangeF(0.0f, 1.0f) < 0.3f; // §06: Zone A uses GROUND_LIGHT in 30% of cycles

	// Roll circle trigger threshold as an early/mid-cycle hero event.
	// The design plan's MAX_FRAGMENTS is 12, which puts the circle around
	// placement 6-8. If MAX_FRAGMENTS is temporarily higher during testing,
	// do not let that push the circle so late that it disappears into density.
	int triggerMin = static_cast<int>(std::round(CIRCLE_TRIGGER_MIN_FRACTION * MAX_FRAGMENTS));
	int triggerMax = static_cast<int>(std::round(CIRCLE_TRIGGER_MAX_FRACTION * MAX_FRAGMENTS));
	triggerMin = ofClamp(triggerMin, 3, 6);
	triggerMax = ofClamp(triggerMax, triggerMin, 8);
	circleTriggerCount = randRangeI(triggerMin, triggerMax);

	placementCount = 0;
	circleSpawnedThisCycle = false;

	ofLogNotice("BEComposition") << "circleTriggerCount=" << circleTriggerCount
								 << " maxFragments=" << MAX_FRAGMENTS;
}

GeometryType BEComposition::pickGeometryType() const {
	int roll = randRangeI(0, 89); // RECT 50 / SLIVER 20 / SQUARE 20, sums to 90
	if (roll < 50) return GeometryType::RECT;
	if (roll < 70) return GeometryType::SLIVER;
	return GeometryType::SQUARE;
}

void BEComposition::pickSize(GeometryType type, int & w, int & h) const {
	switch (type) {
	case GeometryType::RECT:
		w = randRangeI(1, 4);
		h = randRangeI(1, 3);
		break;
	case GeometryType::SQUARE:
		w = 2;
		h = 2;
		break;
	case GeometryType::SLIVER:
		if (randRangeI(0, 1) == 0) {
			w = 1;
			h = randRangeI(4, 6);
		} else {
			w = randRangeI(4, 6);
			h = 1;
		}
		break;
	case GeometryType::CIRCLE:
		break; // handled separately in placeCircleFragment()
	}
}

float BEComposition::overlapFraction(int col, int row, int w, int h) const {
	int occupiedCount = 0;
	for (int r = row; r < row + h; r++) {
		for (int c = col; c < col + w; c++) {
			if (grid->isOccupied(c, r)) {
				occupiedCount++;
			}
		}
	}
	return static_cast<float>(occupiedCount) / static_cast<float>(w * h);
}

int BEComposition::countZoneFragments(bool zoneA) const {
	float dividerX = dividerCol * grid->getCellWidth();
	int count = 0;
	for (const auto & f : fragments) {
		glm::vec2 center = glm::vec2(f->getBounds().getCenter());
		bool inZoneA = center.x < dividerX;
		if (inZoneA == zoneA) {
			count++;
		}
	}
	return count;
}

ofColor BEComposition::pickPlaceholderColor() const {
	return ofColor::fromHsb(randRangeI(0, 255), 120, 180);
}

ofRectangle BEComposition::boundsForCell(int col, int row, int w, int h) const {
	ofRectangle topLeft = grid->cellRect(col, row);
	return ofRectangle(topLeft.x, topLeft.y, w * grid->getCellWidth(), h * grid->getCellHeight());
}

void BEComposition::requestVideoTexture(BEFragment * fragment, int pxW, int pxH) const {
	if (videoSampler == nullptr || !videoSampler->hasMedia()) return;

	int videoW = videoSampler->getVideoWidth();
	int videoH = videoSampler->getVideoHeight();
	if (videoW <= 0 || videoH <= 0) return;

	// Pick a crop whose aspect ratio matches the fragment so no stretching occurs.
	float fragAspect = static_cast<float>(pxW) / static_cast<float>(pxH);
	float videoAspect = static_cast<float>(videoW) / static_cast<float>(videoH);

	float cropW, cropH;
	if (fragAspect >= videoAspect) {
		cropW = static_cast<float>(videoW);
		cropH = cropW / fragAspect;
	} else {
		cropH = static_cast<float>(videoH);
		cropW = cropH * fragAspect;
	}

	int maxCropX = videoW - static_cast<int>(cropW);
	int maxCropY = videoH - static_cast<int>(cropH);
	int cropX = (maxCropX > 0) ? (rand() % maxCropX) : 0;
	int cropY = (maxCropY > 0) ? (rand() % maxCropY) : 0;

	fragment->setVideoSource(&videoSampler->getTexture(), ofRectangle(cropX, cropY, cropW, cropH));
}

void BEComposition::placeCircleFragment() {
	float cellW = grid->getCellWidth();
	float cellH = grid->getCellHeight();
	float avgCell = (cellW + cellH) * 0.5f;
	float regionHalf = CIRCLE_REGION_SIZE * 0.5f;

	// The circle is a hero/compositional event, not a normal occupancy-constrained
	// fragment. It is allowed to overlap existing material heavily, matching the
	// reference image where the circular crop dominates and cuts across the grid.
	for (int attempt = 0; attempt < CIRCLE_PLACEMENT_MAX_ATTEMPTS; attempt++) {
		// Keep the hero circle intentional and readable. The previous version used
		// the design center directly at x=320 plus a very large diameter, which made
		// the circle hang off the left edge and look like a stalled outline. Bias the
		// center left-of-middle, but keep the whole form mostly on-screen.
		float diameterFromCells = randRangeF(CIRCLE_DIAMETER_MIN_CELLS, CIRCLE_DIAMETER_MAX_CELLS) * avgCell;
		float minHeroDiameter = static_cast<float>(canvasW) * 0.36f;
		float maxHeroDiameter = static_cast<float>(canvasW) * 0.50f;
		float diameter = ofClamp(diameterFromCells, minHeroDiameter, maxHeroDiameter);
		float radius = diameter * 0.5f;

		float minCx = radius + cellW * 0.25f;
		float maxCx = std::min(static_cast<float>(canvasW) - radius - cellW * 0.25f, static_cast<float>(canvasW) * 0.48f);
		if (maxCx < minCx) maxCx = minCx;

		float minCy = radius + cellH * 0.25f;
		float maxCy = static_cast<float>(canvasH) - radius - cellH * 0.25f;
		if (maxCy < minCy) maxCy = minCy;

		float cx = randRangeF(minCx, maxCx);
		float cy = ofClamp(CIRCLE_REGION_CENTER_Y + randRangeF(-regionHalf, regionHalf), minCy, maxCy);

		ofRectangle pixelBounds(cx - radius, cy - radius, diameter, diameter);

		// In v2 the generated center keeps the circle mostly on-screen. This check is
		// left as a safety guard only; it should almost never fail.
		float visibleLeft = std::max(pixelBounds.getLeft(), 0.0f);
		float visibleTop = std::max(pixelBounds.getTop(), 0.0f);
		float visibleRight = std::min(pixelBounds.getRight(), static_cast<float>(canvasW));
		float visibleBottom = std::min(pixelBounds.getBottom(), static_cast<float>(canvasH));
		float visibleW = visibleRight - visibleLeft;
		float visibleH = visibleBottom - visibleTop;
		if (visibleW <= 0.0f || visibleH <= 0.0f) continue;
		float visibleFraction = (visibleW * visibleH) / (diameter * diameter);
		if (visibleFraction < 0.70f) continue;

		// Reserve only a small core near the circle center. Reserving the full
		// bounding box would make this hero circle block too much of the remaining
		// grid, especially because it is intentionally large and overlapping.
		int centerCol = static_cast<int>(std::floor(cx / cellW));
		int centerRow = static_cast<int>(std::floor(cy / cellH));
		centerCol = std::max(0, std::min(grid->getCols() - 1, centerCol));
		centerRow = std::max(0, std::min(grid->getRows() - 1, centerRow));

		int startCol = std::max(0, centerCol - 1);
		int startRow = std::max(0, centerRow - 1);
		int endCol = std::min(grid->getCols() - 1, centerCol + 1);
		int endRow = std::min(grid->getRows() - 1, centerRow + 1);

		int gridW = endCol - startCol + 1;
		int gridH = endRow - startRow + 1;
		if (gridW <= 0 || gridH <= 0) continue;

		float phaseOffset = randRangeF(0.0f, glm::two_pi<float>());
		ofColor color = pickPlaceholderColor();

		Fragment::Params params;
		params.bounds = pixelBounds;
		params.placeholderColor = color;
		params.phaseOffset = phaseOffset;
		params.driftAmp = glm::vec2(DRIFT_AMP_X, DRIFT_AMP_Y);
		params.driftFreq = glm::vec2(DRIFT_FREQ_X, DRIFT_FREQ_Y);
		params.desaturateRampDuration = DESATURATE_RAMP_DURATION;
		params.desaturateMax = DESATURATE_MAX;
		params.circularMask = true;
		params.maskRadius = radius;

		auto frag = std::make_unique<BEFragment>();
		frag->setupBE(params, GeometryType::CIRCLE, canvasW, canvasH);
		requestVideoTexture(frag.get(), static_cast<int>(diameter), static_cast<int>(diameter));

		grid->reserve(startCol, startRow, gridW, gridH);
		fragments.push_back(std::move(frag));
		// No notifyFragmentPlaced() — circle skips the measurement-line step (§06).

		circleSpawnedThisCycle = true;
		ofLogNotice("BEComposition") << "circle placed at (" << cx << "," << cy
									 << ") radius=" << radius << "px visibleFraction=" << visibleFraction;
		return;
	}

	// Do not mark the circle as spawned on failure. The circle is a required
	// mid-cycle visual event, so later successful normal placements should retry it.
	ofLogNotice("BEComposition") << "circle placement failed; will retry after next placement";
}

bool BEComposition::attemptPlacement() {
	GeometryType type = pickGeometryType();
	int w, h;
	pickSize(type, w, h);

	if (w > grid->getCols() || h > grid->getRows()) {
		return false;
	}

	std::vector<Candidate> candidates;
	float dividerX = dividerCol * grid->getCellWidth();
	glm::vec2 canvasCenter(canvasW / 2.0f, canvasH / 2.0f);
	float maxCenterDist = glm::distance(glm::vec2(0, 0), canvasCenter);
	float canvasDiag = glm::distance(glm::vec2(0, 0), glm::vec2(canvasW, canvasH));
	float idealGap = 1.5f * (grid->getCellWidth() + grid->getCellHeight()) / 2.0f;
	int zoneACount = countZoneFragments(true);
	int zoneBCount = countZoneFragments(false);

	for (int attempt = 0; attempt < PLACEMENT_MAX_ATTEMPTS; attempt++) {
		int col = randRangeI(0, grid->getCols() - w);
		int row = randRangeI(0, grid->getRows() - h);

		if (overlapFraction(col, row, w, h) > 0.30f) {
			continue;
		}

		ofRectangle candidateBounds = boundsForCell(col, row, w, h);
		glm::vec2 center = glm::vec2(candidateBounds.getCenter());

		float centerDist = glm::distance(center, canvasCenter);
		float centerScore = (maxCenterDist > 0) ? centerDist / maxCenterDist : 0.0f;

		float nearestDist = std::numeric_limits<float>::max();
		Fragment * nearest = nullptr;
		for (const auto & f : fragments) {
			glm::vec2 fc = glm::vec2(f->getBounds().getCenter());
			float d = glm::distance(center, fc);
			if (d < nearestDist) {
				nearestDist = d;
				nearest = f.get();
			}
		}

		float proximityScore = 0.0f;
		if (nearest != nullptr) {
			proximityScore = 1.0f - ofClamp(fabs(nearestDist - idealGap) / canvasDiag, 0.0f, 1.0f);
		}

		bool inZoneA = center.x < dividerX;
		float zoneScore = 0.0f;
		if (inZoneA && zoneACount < zoneBCount) {
			zoneScore = 1.0f;
		} else if (!inZoneA && zoneBCount < zoneACount) {
			zoneScore = 1.0f;
		}

		float jitter = randRangeF(-PLACEMENT_SCORE_JITTER, PLACEMENT_SCORE_JITTER);
		float score = PLACEMENT_SCORE_W_CENTER * centerScore
			+ PLACEMENT_SCORE_W_PROXIMITY * proximityScore
			+ PLACEMENT_SCORE_W_ZONE * zoneScore
			+ jitter;

		candidates.push_back({ col, row, score, nearest });
	}

	if (candidates.empty()) {
		return false;
	}

	const Candidate & best = *std::max_element(candidates.begin(), candidates.end(),
		[](const Candidate & a, const Candidate & b) { return a.score < b.score; });

	ofRectangle placedBounds = boundsForCell(best.col, best.row, w, h);
	float phaseOffset = randRangeF(0.0f, glm::two_pi<float>());
	ofColor color = pickPlaceholderColor();

	Fragment::Params params;
	params.bounds = placedBounds;
	params.placeholderColor = color;
	params.phaseOffset = phaseOffset;
	params.driftAmp = glm::vec2(DRIFT_AMP_X, DRIFT_AMP_Y);
	params.driftFreq = glm::vec2(DRIFT_FREQ_X, DRIFT_FREQ_Y);
	params.desaturateRampDuration = DESATURATE_RAMP_DURATION;
	params.desaturateMax = DESATURATE_MAX;
	// circularMask / maskRadius stay false/0 for RECT/SLIVER/SQUARE

	auto frag = std::make_unique<BEFragment>();
	frag->setupBE(params, type, canvasW, canvasH);
	requestVideoTexture(frag.get(),
		static_cast<int>(placedBounds.width), static_cast<int>(placedBounds.height));

	Fragment * newFragPtr = frag.get();
	grid->reserve(best.col, best.row, w, h);
	fragments.push_back(std::move(frag));
	notifyFragmentPlaced(newFragPtr, best.nearest);

	// Check circle trigger. The circle is a special hero event; if placement
	// fails because the generated bounds are too far offscreen, it remains pending
	// and will retry after the next successful normal placement.
	placementCount++;
	if (!circleSpawnedThisCycle && placementCount >= circleTriggerCount) {
		ofLogNotice("BEComposition") << "circle trigger reached at placementCount="
									 << placementCount << " trigger=" << circleTriggerCount;
		placeCircleFragment();
	}

	return true;
}
