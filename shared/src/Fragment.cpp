#include "Fragment.h"
#include "ofGraphics.h"
#include "ofUtils.h"
#include "ofMath.h"
#include "ofLog.h"
#include "ofAppRunner.h"

ofShader Fragment::fragmentShader;
bool Fragment::fragmentShaderReady = false;

void Fragment::loadFragmentShader(const std::string& vertPath, const std::string& fragPath){
	fragmentShaderReady = fragmentShader.load(vertPath, fragPath);
	if(!fragmentShaderReady){
		ofLogError("Fragment") << "failed to load fragmentEffects shader (" << vertPath << ", " << fragPath << ")";
	}
}

void Fragment::setup(const Params& p){
	bounds               = p.bounds;
	placeholderColor     = p.placeholderColor;
	phaseOffset          = p.phaseOffset;
	arrivalDuration      = p.arrivalDuration;
	driftAmp             = p.driftAmp;
	driftFreq            = p.driftFreq;
	desaturateRampDuration = p.desaturateRampDuration;
	desaturateMax        = p.desaturateMax;
	circularMask         = p.circularMask;
	maskRadius           = p.maskRadius;

	state        = State::ARRIVING;
	stateElapsed = 0;
	opacity      = 1.0f;
	driftOffset  = glm::vec2(0, 0);
}

void Fragment::setVideoSource(const ofTexture* tex, ofRectangle crop){
	videoTexture = tex;
	videoCrop    = crop;
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

void Fragment::drawMaskedFill(float radius) const{
	glm::vec2 pos = getDrawPosition();

	if(!hasTexture()){
		ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);
		return;
	}

	ofSetColor(255, 255 * opacity);

	float desaturateAmount = (state == State::DRIFTING)
		? ofClamp(stateElapsed / desaturateRampDuration, 0.0f, 1.0f) * desaturateMax
		: 0.0f;

	if(fragmentShaderReady){
		fragmentShader.begin();
		fragmentShader.setUniform1f("desaturateAmount", desaturateAmount);
		if(circularMask && radius > 0.0f){
			glm::vec2 center = pos + glm::vec2(bounds.width * 0.5f, bounds.height * 0.5f);
			float windowH = ofGetWindowHeight();
			fragmentShader.setUniform1f("circleMask", 1.0f);
			fragmentShader.setUniform2f("maskCenterPx", center.x, windowH - center.y);
			fragmentShader.setUniform1f("maskRadiusPx", radius);
		} else {
			fragmentShader.setUniform1f("circleMask", 0.0f);
			fragmentShader.setUniform2f("maskCenterPx", 0.0f, 0.0f);
			fragmentShader.setUniform1f("maskRadiusPx", 0.0f);
		}
		videoTexture->drawSubsection(pos.x, pos.y, bounds.width, bounds.height,
			videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
		fragmentShader.end();
	} else {
		videoTexture->drawSubsection(pos.x, pos.y, bounds.width, bounds.height,
			videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
	}
}

void Fragment::drawStable() const{
	drawMaskedFill(maskRadius);
}
