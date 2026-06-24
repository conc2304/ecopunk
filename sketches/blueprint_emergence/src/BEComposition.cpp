#include "BEComposition.h"
#include "BESettings.h"
#include "ofMath.h"
#include "ofLog.h"
#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

void BEComposition::setupBE(GridSystem* grid_, VideoSampler* videoSampler_, int canvasW_, int canvasH_, int dividerCol_){
	videoSampler = videoSampler_;
	canvasW      = canvasW_;
	canvasH      = canvasH_;
	dividerCol   = dividerCol_;

	CompositionBase::Timing timing;
	timing.cycleDurationMin      = CYCLE_DURATION_MIN;
	timing.cycleDurationMax      = CYCLE_DURATION_MAX;
	timing.blankDuration         = BLANK_DURATION;
	timing.densityDuration       = DENSITY_DURATION;
	timing.dissolveDuration      = DISSOLVE_DURATION;
	timing.resetHoldDuration     = RESET_HOLD_DURATION;
	timing.placementIntervalMin  = PLACEMENT_INTERVAL_MIN;
	timing.placementIntervalMax  = PLACEMENT_INTERVAL_MAX;
	timing.placementIntervalDense = PLACEMENT_INTERVAL_DENSE;
	timing.dissolveFadeMin       = DISSOLVE_FADE_MIN;
	timing.dissolveFadeMax       = DISSOLVE_FADE_MAX;
	timing.maxFragments          = MAX_FRAGMENTS;

	CompositionBase::setup(grid_, timing);
}

void BEComposition::onCycleStart(){
	zoneALight = randRangeF(0.0f, 1.0f) < 0.3f; // §06: Zone A uses GROUND_LIGHT in 30% of cycles

	// Roll circle trigger threshold — 50–65% of MAX_FRAGMENTS placements
	circleTriggerCount = randRangeI(
		static_cast<int>(std::round(CIRCLE_TRIGGER_MIN_FRACTION * MAX_FRAGMENTS)),
		static_cast<int>(std::round(CIRCLE_TRIGGER_MAX_FRACTION * MAX_FRAGMENTS)));
	placementCount        = 0;
	circleSpawnedThisCycle = false;
}

GeometryType BEComposition::pickGeometryType() const{
	int roll = randRangeI(0, 89); // RECT 50 / SLIVER 20 / SQUARE 20, sums to 90
	if(roll < 50) return GeometryType::RECT;
	if(roll < 70) return GeometryType::SLIVER;
	return GeometryType::SQUARE;
}

void BEComposition::pickSize(GeometryType type, int& w, int& h) const{
	switch(type){
		case GeometryType::RECT:
			w = randRangeI(1, 4);
			h = randRangeI(1, 3);
			break;
		case GeometryType::SQUARE:
			w = 2;
			h = 2;
			break;
		case GeometryType::SLIVER:
			if(randRangeI(0, 1) == 0){
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

float BEComposition::overlapFraction(int col, int row, int w, int h) const{
	int occupiedCount = 0;
	for(int r = row; r < row + h; r++){
		for(int c = col; c < col + w; c++){
			if(grid->isOccupied(c, r)){
				occupiedCount++;
			}
		}
	}
	return static_cast<float>(occupiedCount) / static_cast<float>(w * h);
}

int BEComposition::countZoneFragments(bool zoneA) const{
	float dividerX = dividerCol * grid->getCellWidth();
	int count = 0;
	for(const auto& f : fragments){
		glm::vec2 center = glm::vec2(f->getBounds().getCenter());
		bool inZoneA = center.x < dividerX;
		if(inZoneA == zoneA){
			count++;
		}
	}
	return count;
}

ofColor BEComposition::pickPlaceholderColor() const{
	return ofColor::fromHsb(randRangeI(0, 255), 120, 180);
}

ofRectangle BEComposition::boundsForCell(int col, int row, int w, int h) const{
	ofRectangle topLeft = grid->cellRect(col, row);
	return ofRectangle(topLeft.x, topLeft.y, w * grid->getCellWidth(), h * grid->getCellHeight());
}

void BEComposition::requestVideoTexture(BEFragment* fragment, int pxW, int pxH) const{
	if(videoSampler == nullptr || !videoSampler->hasMedia()) return;

	int videoW = videoSampler->getVideoWidth();
	int videoH = videoSampler->getVideoHeight();
	if(videoW <= 0 || videoH <= 0) return;

	// Pick a crop whose aspect ratio matches the fragment so no stretching occurs.
	float fragAspect  = static_cast<float>(pxW) / static_cast<float>(pxH);
	float videoAspect = static_cast<float>(videoW) / static_cast<float>(videoH);

	float cropW, cropH;
	if(fragAspect >= videoAspect){
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

void BEComposition::placeCircleFragment(){
	float cellW  = grid->getCellWidth();
	float cellH  = grid->getCellHeight();
	float avgCell = (cellW + cellH) * 0.5f;
	float regionHalf = CIRCLE_REGION_SIZE * 0.5f;

	for(int attempt = 0; attempt < CIRCLE_PLACEMENT_MAX_ATTEMPTS; attempt++){
		float cx       = CIRCLE_REGION_CENTER_X + randRangeF(-regionHalf, regionHalf);
		float cy       = CIRCLE_REGION_CENTER_Y + randRangeF(-regionHalf, regionHalf);
		float diameter = randRangeF(CIRCLE_DIAMETER_MIN_CELLS, CIRCLE_DIAMETER_MAX_CELLS) * avgCell;
		float radius   = diameter * 0.5f;

		ofRectangle pixelBounds(cx - radius, cy - radius, diameter, diameter);

		// Covering grid cells (used for occupancy reservation)
		int startCol = std::max(0, static_cast<int>(std::floor(pixelBounds.x / cellW)));
		int startRow = std::max(0, static_cast<int>(std::floor(pixelBounds.y / cellH)));
		int endCol   = std::min(grid->getCols() - 1,
			static_cast<int>(std::ceil((pixelBounds.x + pixelBounds.width) / cellW)) - 1);
		int endRow   = std::min(grid->getRows() - 1,
			static_cast<int>(std::ceil((pixelBounds.y + pixelBounds.height) / cellH)) - 1);

		int gridW = endCol - startCol + 1;
		int gridH = endRow - startRow + 1;
		if(gridW <= 0 || gridH <= 0) continue;

		// Circle's bounding box spans many cells, so use a looser threshold —
		// the radial-discard shader handles visual overlap, the grid reservation
		// just keeps other fragments from smothering the center.
		if(overlapFraction(startCol, startRow, gridW, gridH) > 0.70f) continue;

		float   phaseOffset = randRangeF(0.0f, glm::two_pi<float>());
		ofColor color       = pickPlaceholderColor();

		Fragment::Params params;
		params.bounds                = pixelBounds;
		params.placeholderColor      = color;
		params.phaseOffset           = phaseOffset;
		params.driftAmp              = glm::vec2(DRIFT_AMP_X, DRIFT_AMP_Y);
		params.driftFreq             = glm::vec2(DRIFT_FREQ_X, DRIFT_FREQ_Y);
		params.desaturateRampDuration = DESATURATE_RAMP_DURATION;
		params.desaturateMax         = DESATURATE_MAX;
		params.circularMask          = true;
		params.maskRadius            = radius;

		auto frag = std::make_unique<BEFragment>();
		frag->setupBE(params, GeometryType::CIRCLE, canvasW, canvasH);
		requestVideoTexture(frag.get(), static_cast<int>(diameter), static_cast<int>(diameter));

		grid->reserve(startCol, startRow, gridW, gridH);
		fragments.push_back(std::move(frag));
		// No notifyFragmentPlaced() — circle skips the measurement-line step (§06)

		circleSpawnedThisCycle = true;
		ofLogNotice("BEComposition") << "circle placed at (" << cx << "," << cy
			<< ") radius=" << radius << "px";
		return;
	}

	// All attempts failed — mark done so we don't retry on every subsequent placement
	circleSpawnedThisCycle = true;
	ofLogNotice("BEComposition") << "circle placement: no room after "
		<< CIRCLE_PLACEMENT_MAX_ATTEMPTS << " attempts";
}

bool BEComposition::attemptPlacement(){
	GeometryType type = pickGeometryType();
	int w, h;
	pickSize(type, w, h);

	if(w > grid->getCols() || h > grid->getRows()){
		return false;
	}

	std::vector<Candidate> candidates;
	float dividerX    = dividerCol * grid->getCellWidth();
	glm::vec2 canvasCenter(canvasW / 2.0f, canvasH / 2.0f);
	float maxCenterDist = glm::distance(glm::vec2(0, 0), canvasCenter);
	float canvasDiag    = glm::distance(glm::vec2(0, 0), glm::vec2(canvasW, canvasH));
	float idealGap      = 1.5f * (grid->getCellWidth() + grid->getCellHeight()) / 2.0f;
	int zoneACount      = countZoneFragments(true);
	int zoneBCount      = countZoneFragments(false);

	for(int attempt = 0; attempt < PLACEMENT_MAX_ATTEMPTS; attempt++){
		int col = randRangeI(0, grid->getCols() - w);
		int row = randRangeI(0, grid->getRows() - h);

		if(overlapFraction(col, row, w, h) > 0.30f){
			continue;
		}

		ofRectangle candidateBounds = boundsForCell(col, row, w, h);
		glm::vec2 center = glm::vec2(candidateBounds.getCenter());

		float centerDist  = glm::distance(center, canvasCenter);
		float centerScore = (maxCenterDist > 0) ? centerDist / maxCenterDist : 0.0f;

		float nearestDist = std::numeric_limits<float>::max();
		Fragment* nearest = nullptr;
		for(const auto& f : fragments){
			glm::vec2 fc = glm::vec2(f->getBounds().getCenter());
			float d = glm::distance(center, fc);
			if(d < nearestDist){
				nearestDist = d;
				nearest     = f.get();
			}
		}

		float proximityScore = 0.0f;
		if(nearest != nullptr){
			proximityScore = 1.0f - ofClamp(fabs(nearestDist - idealGap) / canvasDiag, 0.0f, 1.0f);
		}

		bool inZoneA = center.x < dividerX;
		float zoneScore = 0.0f;
		if(inZoneA && zoneACount < zoneBCount){
			zoneScore = 1.0f;
		} else if(!inZoneA && zoneBCount < zoneACount){
			zoneScore = 1.0f;
		}

		float jitter = randRangeF(-PLACEMENT_SCORE_JITTER, PLACEMENT_SCORE_JITTER);
		float score  = PLACEMENT_SCORE_W_CENTER * centerScore
			+ PLACEMENT_SCORE_W_PROXIMITY * proximityScore
			+ PLACEMENT_SCORE_W_ZONE * zoneScore
			+ jitter;

		candidates.push_back({col, row, score, nearest});
	}

	if(candidates.empty()){
		return false;
	}

	const Candidate& best = *std::max_element(candidates.begin(), candidates.end(),
		[](const Candidate& a, const Candidate& b){ return a.score < b.score; });

	ofRectangle placedBounds = boundsForCell(best.col, best.row, w, h);
	float   phaseOffset = randRangeF(0.0f, glm::two_pi<float>());
	ofColor color       = pickPlaceholderColor();

	Fragment::Params params;
	params.bounds                = placedBounds;
	params.placeholderColor      = color;
	params.phaseOffset           = phaseOffset;
	params.driftAmp              = glm::vec2(DRIFT_AMP_X, DRIFT_AMP_Y);
	params.driftFreq             = glm::vec2(DRIFT_FREQ_X, DRIFT_FREQ_Y);
	params.desaturateRampDuration = DESATURATE_RAMP_DURATION;
	params.desaturateMax         = DESATURATE_MAX;
	// circularMask / maskRadius stay false/0 for RECT/SLIVER/SQUARE

	auto frag = std::make_unique<BEFragment>();
	frag->setupBE(params, type, canvasW, canvasH);
	requestVideoTexture(frag.get(),
		static_cast<int>(placedBounds.width), static_cast<int>(placedBounds.height));

	Fragment* newFragPtr = frag.get();
	grid->reserve(best.col, best.row, w, h);
	fragments.push_back(std::move(frag));
	notifyFragmentPlaced(newFragPtr, best.nearest);

	// Check circle trigger — fires once when we hit the threshold
	placementCount++;
	if(!circleSpawnedThisCycle && placementCount >= circleTriggerCount){
		placeCircleFragment();
	}

	return true;
}
