#include "Fragment.h"
#include "ofGraphics.h"
#include "ofUtils.h"
#include "ofMath.h"

void Fragment::setup(const ofRectangle& bounds_, const ofColor& placeholderColor_, float phaseOffset_, float arrivalDuration_,
	glm::vec2 driftAmp_, glm::vec2 driftFreq_){

	bounds = bounds_;
	placeholderColor = placeholderColor_;
	phaseOffset = phaseOffset_;
	arrivalDuration = arrivalDuration_;
	driftAmp = driftAmp_;
	driftFreq = driftFreq_;

	state = State::ARRIVING;
	stateElapsed = 0;
	opacity = 1.0f;
}

void Fragment::enterDrifting(){
	driftingEnabled = true;
	if(state == State::STABLE){
		state = State::DRIFTING;
		stateElapsed = 0;
	}
}

void Fragment::startDissolve(float fadeDuration){
	if(state == State::DEAD){
		return;
	}
	state = State::DISSOLVING;
	stateElapsed = 0;
	dissolveDuration = fadeDuration;
}

void Fragment::update(float dt){
	stateElapsed += dt;

	switch(state){
		case State::ARRIVING:
			if(stateElapsed >= arrivalDuration){
				state = driftingEnabled ? State::DRIFTING : State::STABLE;
				stateElapsed = 0;
			}
			break;

		case State::STABLE:
			opacity = 1.0f;
			if(driftingEnabled){
				state = State::DRIFTING;
				stateElapsed = 0;
			}
			break;

		case State::DRIFTING:{
			opacity = 1.0f;
			float t = ofGetElapsedTimef();
			driftOffset.x = sin(t * driftFreq.x + phaseOffset) * driftAmp.x;
			driftOffset.y = cos(t * driftFreq.y + phaseOffset) * driftAmp.y;
			break;
		}

		case State::DISSOLVING:
			opacity = 1.0f - ofClamp(stateElapsed / dissolveDuration, 0.0f, 1.0f);
			if(stateElapsed >= dissolveDuration){
				state = State::DEAD;
			}
			break;

		case State::DEAD:
			break;
	}
}

void Fragment::draw() const{
	if(state == State::DEAD){
		return;
	}

	ofPushStyle();
	if(state == State::ARRIVING){
		drawArrival(ofClamp(stateElapsed / arrivalDuration, 0.0f, 1.0f));
	} else {
		ofSetColor(placeholderColor, 255 * opacity);
		drawStable();
	}
	ofPopStyle();
}

void Fragment::drawArrival(float t) const{
	ofSetColor(placeholderColor, 255 * t);
	drawStable();
}

void Fragment::drawStable() const{
	glm::vec2 pos = getDrawPosition();
	ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);
}
