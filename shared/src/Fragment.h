#pragma once

#include "glm/vec2.hpp"
#include "ofColor.h"
#include "ofRectangle.h"
#include "ofShader.h"
#include "ofTexture.h"

// Base state machine for a single placed crop. Owns timing, opacity,
// ambient drift, a shared live video texture pointer, and fragment effects.
//
// Important: videoTexture is NOT owned by Fragment. It points at the active
// ofVideoPlayer/ofTexture owned elsewhere. Because it is shared/live, the image
// updates automatically as the player advances.
class Fragment {
public:
	enum class State { ARRIVING,
		STABLE,
		DRIFTING,
		DISSOLVING,
		DEAD };

	struct Params {
		ofRectangle bounds;
		ofColor placeholderColor;
		float phaseOffset = 0.0f;
		float arrivalDuration = 1.0f;
		glm::vec2 driftAmp { 0, 0 };
		glm::vec2 driftFreq { 0, 0 };
		float desaturateRampDuration = 1.0f;
		float desaturateMax = 0.0f;
		bool circularMask = false;
		float maskRadius = 0.0f;
	};

	virtual ~Fragment() = default;

	void setup(const Params & p);
	void update(float dt);
	void draw() const;

	void enterDrifting();
	void startDissolve(float fadeDuration);

	// Points this fragment at the video player's shared GPU texture and assigns
	// a crop rectangle in video pixel coordinates. No copying. The texture updates
	// automatically as the player advances.
	void setVideoSource(const ofTexture * tex, ofRectangle crop);

	bool isDead() const { return state == State::DEAD; }
	State getState() const { return state; }
	const ofRectangle & getBounds() const { return bounds; }

	static void loadFragmentShader(const std::string & vertPath, const std::string & fragPath);

protected:
	virtual void drawArrival(float t) const;
	virtual void drawStable() const;

	// Draws the current media source at the fragment draw position.
	// For circular fragments this now uses a real textured circle mesh instead
	// of relying on gl_FragCoord discard. That avoids the common "outline only"
	// failure when the shader mask coordinate space does not match OF's window.
	void drawMaskedFill(float radius) const;

	glm::vec2 getDrawPosition() const { return glm::vec2(bounds.getPosition()) + driftOffset; }
	float getStateElapsedSeconds() const { return stateElapsed; }

	bool hasTexture() const { return videoTexture != nullptr && videoTexture->isAllocated(); }
	bool hasMedia() const { return hasTexture(); }

	ofRectangle bounds;
	ofColor placeholderColor;
	const ofTexture * videoTexture = nullptr; // shared; not owned
	ofRectangle videoCrop; // pixel rect within the video frame
	float opacity = 1.0f;
	float arrivalDuration = 0.0f;

private:
	void drawTexturedCircleMesh(float radius) const;
	void drawTexturedRect(float desaturateAmount) const;

	static ofShader fragmentShader;
	static bool fragmentShaderReady;

	State state = State::ARRIVING;
	float stateElapsed = 0.0f;
	float dissolveDuration = 0.0f;
	float phaseOffset = 0.0f;
	bool driftingEnabled = false;
	glm::vec2 driftOffset { 0, 0 };
	glm::vec2 driftAmp { 0, 0 };
	glm::vec2 driftFreq { 0, 0 };
	float desaturateRampDuration = 1.0f;
	float desaturateMax = 0.0f;
	bool circularMask = false;
	float maskRadius = 0.0f;
};
