#include "BEFragment.h"
#include "BESettings.h"
#include "ofGraphics.h"
#include "ofMath.h"
#include "glm/glm.hpp"

void BEFragment::setupBE(const ofRectangle& bounds, const ofColor& placeholderColor, float phaseOffset,
	GeometryType geometryType_, int canvasW, int canvasH, glm::vec2 driftAmp, glm::vec2 driftFreq){

	geometryType = geometryType_;

	float arrivalDuration = (geometryType == GeometryType::SLIVER)
		? SLIDE_IN_DURATION
		: (SCAN_REVEAL_DURATION + BORDER_DRAW_DURATION);

	Fragment::setup(bounds, placeholderColor, phaseOffset, arrivalDuration, driftAmp, driftFreq);

	if(geometryType == GeometryType::SLIVER){
		glm::vec2 center = glm::vec2(bounds.getCenter());
		float distLeft = center.x;
		float distRight = canvasW - center.x;
		float distTop = center.y;
		float distBottom = canvasH - center.y;
		float minDist = std::min({distLeft, distRight, distTop, distBottom});

		glm::vec2 finalPos = glm::vec2(bounds.getPosition());
		if(minDist == distLeft){
			slideStartPos = finalPos - glm::vec2(bounds.width, 0);
		} else if(minDist == distRight){
			slideStartPos = finalPos + glm::vec2(bounds.width, 0);
		} else if(minDist == distTop){
			slideStartPos = finalPos - glm::vec2(0, bounds.height);
		} else {
			slideStartPos = finalPos + glm::vec2(0, bounds.height);
		}
	}
}

void BEFragment::drawArrival(float /*t*/) const{
	float elapsed = getStateElapsedSeconds();
	if(geometryType == GeometryType::SLIVER){
		drawSlideIn(elapsed);
	} else {
		drawScanReveal(elapsed);
	}
}

void BEFragment::drawStable() const{
	// SLIVER leaves a 1px afterimage at its slide start point that fades over
	// AFTERIMAGE_FADE_DURATION once STABLE begins (stateElapsed resets to 0
	// exactly when ARRIVING -> STABLE happens, so it doubles as "time since
	// the slide finished").
	if(geometryType == GeometryType::SLIVER){
		float sinceArrival = getStateElapsedSeconds();
		if(sinceArrival <= AFTERIMAGE_FADE_DURATION){
			float alpha = 1.0f - (sinceArrival / AFTERIMAGE_FADE_DURATION);
			ofSetColor(255, 255, 255, 255 * alpha);
			ofNoFill();
			ofDrawRectangle(slideStartPos.x, slideStartPos.y, bounds.width, bounds.height);
			ofFill();
		}
	}

	Fragment::drawStable();
}

void BEFragment::drawScanReveal(float elapsed) const{
	glm::vec2 pos = getDrawPosition();

	if(elapsed <= SCAN_REVEAL_DURATION){
		float t = ofClamp(elapsed / SCAN_REVEAL_DURATION, 0.0f, 1.0f); // linear
		float revealedH = bounds.height * t;

		ofSetColor(placeholderColor);
		ofDrawRectangle(pos.x, pos.y, bounds.width, revealedH);

		ofSetColor(255, 255, 255, 200);
		ofDrawLine(pos.x, pos.y + revealedH, pos.x + bounds.width, pos.y + revealedH);
	} else {
		ofSetColor(placeholderColor);
		ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);

		float borderT = ofClamp((elapsed - SCAN_REVEAL_DURATION) / BORDER_DRAW_DURATION, 0.0f, 1.0f);
		drawClockwiseBorder(pos, bounds.width, bounds.height, borderT);
	}
}

void BEFragment::drawClockwiseBorder(glm::vec2 pos, float w, float h, float t) const{
	glm::vec2 corners[5] = {
		pos,
		pos + glm::vec2(w, 0),
		pos + glm::vec2(w, h),
		pos + glm::vec2(0, h),
		pos
	};

	float perimeter = 2.0f * (w + h);
	float target = perimeter * ofClamp(t, 0.0f, 1.0f);

	ofSetColor(255, 255, 255, 200);
	float drawn = 0;
	for(int i = 0; i < 4 && drawn < target; i++){
		glm::vec2 a = corners[i];
		glm::vec2 b = corners[i + 1];
		float segLen = glm::distance(a, b);
		float remain = target - drawn;
		if(remain >= segLen){
			ofDrawLine(a.x, a.y, b.x, b.y);
			drawn += segLen;
		} else {
			glm::vec2 p = a + (b - a) * (remain / segLen);
			ofDrawLine(a.x, a.y, p.x, p.y);
			drawn = target;
		}
	}
}

void BEFragment::drawSlideIn(float elapsed) const{
	float t = ofClamp(elapsed / SLIDE_IN_DURATION, 0.0f, 1.0f);
	float tEased = 1.0f - (1.0f - t) * (1.0f - t); // ease-out quad

	glm::vec2 finalPos = glm::vec2(bounds.getPosition());
	glm::vec2 pos = glm::mix(slideStartPos, finalPos, tEased);

	ofSetColor(placeholderColor);
	ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);
}
