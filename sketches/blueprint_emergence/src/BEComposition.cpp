#include "BEComposition.h"
#include "BESettings.h"
#include "ofMath.h"
#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

void BEComposition::setupBE(GridSystem* grid_, int canvasW_, int canvasH_, int dividerCol_){
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

void BEComposition::onCycleStart(){
	zoneALight = randRangeF(0.0f, 1.0f) < 0.3f; // §06: Zone A uses GROUND_LIGHT in 30% of cycles
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

bool BEComposition::attemptPlacement(){
	GeometryType type = pickGeometryType();
	int w, h;
	pickSize(type, w, h);

	if(w > grid->getCols() || h > grid->getRows()){
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

	for(int attempt = 0; attempt < PLACEMENT_MAX_ATTEMPTS; attempt++){
		int col = randRangeI(0, grid->getCols() - w);
		int row = randRangeI(0, grid->getRows() - h);

		if(overlapFraction(col, row, w, h) > 0.30f){
			continue;
		}

		ofRectangle candidateBounds = boundsForCell(col, row, w, h);
		glm::vec2 center = glm::vec2(candidateBounds.getCenter());

		float centerDist = glm::distance(center, canvasCenter);
		float centerScore = (maxCenterDist > 0) ? centerDist / maxCenterDist : 0.0f;

		float nearestDist = std::numeric_limits<float>::max();
		Fragment* nearest = nullptr;
		for(const auto& f : fragments){
			glm::vec2 fc = glm::vec2(f->getBounds().getCenter());
			float d = glm::distance(center, fc);
			if(d < nearestDist){
				nearestDist = d;
				nearest = f.get();
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

		float score = PLACEMENT_SCORE_W_CENTER * centerScore
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

	ofRectangle bounds = boundsForCell(best.col, best.row, w, h);
	float phaseOffset = randRangeF(0.0f, glm::two_pi<float>());
	ofColor color = pickPlaceholderColor();

	auto frag = std::make_unique<BEFragment>();
	frag->setupBE(bounds, color, phaseOffset, type, canvasW, canvasH,
		glm::vec2(DRIFT_AMP_X, DRIFT_AMP_Y), glm::vec2(DRIFT_FREQ_X, DRIFT_FREQ_Y));

	Fragment* newFragPtr = frag.get();
	grid->reserve(best.col, best.row, w, h);
	fragments.push_back(std::move(frag));
	notifyFragmentPlaced(newFragPtr, best.nearest);

	return true;
}
