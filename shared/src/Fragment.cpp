#include "Fragment.h"
#include "ofAppRunner.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofMath.h"
#include "ofMesh.h"
#include "ofUtils.h"
#include <cmath>

ofShader Fragment::fragmentShader;
bool Fragment::fragmentShaderReady = false;

void Fragment::loadFragmentShader(const std::string & vertPath, const std::string & fragPath) {
	fragmentShaderReady = fragmentShader.load(vertPath, fragPath);
	if (!fragmentShaderReady) {
		ofLogError("Fragment") << "failed to load fragmentEffects shader (" << vertPath << ", " << fragPath << ")";
	}
}

void Fragment::setup(const Params & p) {
	bounds = p.bounds;
	placeholderColor = p.placeholderColor;
	phaseOffset = p.phaseOffset;
	arrivalDuration = p.arrivalDuration;
	driftAmp = p.driftAmp;
	driftFreq = p.driftFreq;
	desaturateRampDuration = p.desaturateRampDuration;
	desaturateMax = p.desaturateMax;
	circularMask = p.circularMask;
	maskRadius = p.maskRadius;

	state = State::ARRIVING;
	stateElapsed = 0.0f;
	opacity = 1.0f;
	driftOffset = glm::vec2(0, 0);
}

void Fragment::setVideoSource(const ofTexture * tex, ofRectangle crop) {
	videoTexture = tex;
	videoCrop = crop;
}

void Fragment::enterDrifting() {
	driftingEnabled = true;
	if (state == State::STABLE) {
		state = State::DRIFTING;
		stateElapsed = 0.0f;
	}
}

void Fragment::startDissolve(float fadeDuration, float opacityFloor) {
	if (state == State::DEAD) {
		return;
	}
	state = State::DISSOLVING;
	stateElapsed = 0.0f;
	dissolveDuration = fadeDuration;
	dissolveFloor = opacityFloor;
}

void Fragment::enterGhost(float opacityFloor) {
	if (state == State::DEAD) {
		return;
	}
	driftingEnabled = false;
	driftOffset = glm::vec2(0, 0);
	state = State::GHOST;
	stateElapsed = 0.0f;
	ghostOpacity = opacityFloor;
	opacity = ghostOpacity;
}

void Fragment::update(float dt) {
	stateElapsed += dt;

	switch (state) {
	case State::ARRIVING:
		if (stateElapsed >= arrivalDuration) {
			state = driftingEnabled ? State::DRIFTING : State::STABLE;
			stateElapsed = 0.0f;
		}
		break;

	case State::STABLE:
		opacity = 1.0f;
		if (driftingEnabled) {
			state = State::DRIFTING;
			stateElapsed = 0.0f;
		}
		break;

	case State::DRIFTING: {
		opacity = 1.0f;
		float t = ofGetElapsedTimef();
		driftOffset.x = sin(t * driftFreq.x + phaseOffset) * driftAmp.x;
		driftOffset.y = cos(t * driftFreq.y + phaseOffset) * driftAmp.y;
		break;
	}

	case State::DISSOLVING:
		opacity = dissolveFloor + (1.0f - dissolveFloor) * (1.0f - ofClamp(stateElapsed / dissolveDuration, 0.0f, 1.0f));
		if (stateElapsed >= dissolveDuration) {
			if (dissolveFloor > 0.005f) {
				state = State::GHOST;
				ghostOpacity = dissolveFloor;
				driftingEnabled = false;
				driftOffset = glm::vec2(0, 0);
				stateElapsed = 0.0f;
			} else {
				state = State::DEAD;
			}
		}
		break;

	case State::GHOST:
		opacity = ghostOpacity;
		break;

	case State::DEAD:
		break;
	}
}

void Fragment::draw() const {
	if (state == State::DEAD) {
		return;
	}

	ofPushStyle();
	if (state == State::ARRIVING) {
		drawArrival(ofClamp(stateElapsed / arrivalDuration, 0.0f, 1.0f));
	} else {
		drawStable();
	}
	ofPopStyle();
}

void Fragment::drawArrival(float t) const {
	ofSetColor(placeholderColor, static_cast<int>(255 * t));
	drawStable();
}

void Fragment::drawMaskedFill(float radius) const {
	glm::vec2 pos = getDrawPosition();

	if (!hasTexture()) {
		ofSetColor(placeholderColor, static_cast<int>(255 * opacity));
		if (circularMask && radius > 0.0f) {
			glm::vec2 center = pos + glm::vec2(bounds.width * 0.5f, bounds.height * 0.5f);
			ofDrawCircle(center.x, center.y, radius);
		} else {
			ofDrawRectangle(pos.x, pos.y, bounds.width, bounds.height);
		}
		return;
	}

	float desaturateAmount = (state == State::DRIFTING)
		? ofClamp(stateElapsed / desaturateRampDuration, 0.0f, 1.0f) * desaturateMax
		: 0.0f;
	desaturateAmount = ofClamp(desaturateAmount + externalDesatNudge, 0.0f, 1.0f);

	// Circle path: use a textured disk mesh instead of the fragment shader's
	// gl_FragCoord circular discard. This is the important fix for the live-video
	// circle: the old mask could discard every pixel when the y-axis/window-space
	// math did not line up, leaving only the outline visible.
	if (circularMask && radius > 0.0f) {
		drawTexturedCircleMesh(radius);
		return;
	}

	drawTexturedRect(desaturateAmount);
}

void Fragment::drawTexturedRect(float desaturateAmount) const {
	glm::vec2 pos = getDrawPosition();
	ofSetColor(255, static_cast<int>(255 * opacity));

	if (fragmentShaderReady) {
		fragmentShader.begin();
		fragmentShader.setUniform1f("desaturateAmount", desaturateAmount);
		fragmentShader.setUniform1f("circleMask", 0.0f);
		fragmentShader.setUniform2f("maskCenterPx", 0.0f, 0.0f);
		fragmentShader.setUniform1f("maskRadiusPx", 0.0f);
		videoTexture->drawSubsection(pos.x, pos.y, bounds.width, bounds.height,
			videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
		fragmentShader.end();
	} else {
		videoTexture->drawSubsection(pos.x, pos.y, bounds.width, bounds.height,
			videoCrop.x, videoCrop.y, videoCrop.width, videoCrop.height);
	}
}

void Fragment::drawTexturedCircleMesh(float radius) const {
	glm::vec2 pos = getDrawPosition();
	glm::vec2 center = pos + glm::vec2(bounds.width * 0.5f, bounds.height * 0.5f);

	const int segments = 96;
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_TRIANGLE_FAN);

	// Center vertex.
	mesh.addVertex(glm::vec3(center.x, center.y, 0.0f));
	mesh.addTexCoord(videoTexture->getCoordFromPoint(
		videoCrop.x + videoCrop.width * 0.5f,
		videoCrop.y + videoCrop.height * 0.5f));

	// Ring vertices. Texture coordinates map the circle onto the fragment's
	// rectangular crop, so the live video fills the circle without stretching more
	// than the original crop already does.
	// getCoordFromPoint() normalises pixel coords to [0,1] for GL_TEXTURE_2D
	// (ofDisableArbTex mode) or leaves them as-is for GL_TEXTURE_RECTANGLE.
	for (int i = 0; i <= segments; i++) {
		float a = static_cast<float>(i) / static_cast<float>(segments) * TWO_PI;
		float x = center.x + std::cos(a) * radius;
		float y = center.y + std::sin(a) * radius;

		float uNorm = (x - pos.x) / bounds.width;
		float vNorm = (y - pos.y) / bounds.height;

		mesh.addVertex(glm::vec3(x, y, 0.0f));
		mesh.addTexCoord(videoTexture->getCoordFromPoint(
			videoCrop.x + videoCrop.width * uNorm,
			videoCrop.y + videoCrop.height * vNorm));
	}

	ofSetColor(255, static_cast<int>(255 * opacity));
	videoTexture->bind();
	mesh.draw();
	videoTexture->unbind();
}

void Fragment::drawStable() const {
	drawMaskedFill(maskRadius);
}
