#pragma once

#include "ofRectangle.h"
#include "ofColor.h"
#include "ofTexture.h"
#include "ofShader.h"
#include "glm/vec2.hpp"

// Base state machine for a single placed crop. Owns timing, opacity,
// ambient drift, the real video texture once captured, and the effects
// (desaturation ramp, optional circular mask) applied via fragmentEffects shader.
class Fragment {
	public:
		enum class State { ARRIVING, STABLE, DRIFTING, DISSOLVING, DEAD };

		// All per-fragment tuning values collected in one place so the call
		// sites don't need to count 10 positional args.
		struct Params {
			ofRectangle bounds;
			ofColor     placeholderColor;
			float       phaseOffset          = 0.0f;
			float       arrivalDuration      = 1.0f;
			glm::vec2   driftAmp             {0, 0};
			glm::vec2   driftFreq            {0, 0};
			float       desaturateRampDuration = 1.0f;
			float       desaturateMax        = 0.0f;
			bool        circularMask         = false; // if true, shader discards pixels outside maskRadius
			float       maskRadius           = 0.0f;  // radius in pixels for the circular discard
		};

		virtual ~Fragment() = default;

		void setup(const Params& p);
		void update(float dt);
		void draw() const;

		void enterDrifting();
		void startDissolve(float fadeDuration);

		// Points this fragment at the video player's shared GPU texture and assigns
		// a crop rectangle (in video pixel coordinates). No copying — the texture
		// updates automatically as the player advances.
		void setVideoSource(const ofTexture* tex, ofRectangle crop);

		bool isDead() const { return state == State::DEAD; }
		State getState() const { return state; }
		const ofRectangle& getBounds() const { return bounds; }

		static void loadFragmentShader(const std::string& vertPath, const std::string& fragPath);

	protected:
		virtual void drawArrival(float t) const;
		virtual void drawStable() const;

		// Binds the fragmentEffects shader, sets circle-mask + desaturation uniforms,
		// draws the texture at the fragment's current draw position. Falls back to
		// a placeholder rect when no texture is loaded. Use this instead of raw
		// texture.draw() in any subclass drawing code that needs the mask.
		// radius: the circle radius to clip to; ignored when circularMask is false.
		void drawMaskedFill(float radius) const;

		glm::vec2 getDrawPosition() const { return glm::vec2(bounds.getPosition()) + driftOffset; }
		float getStateElapsedSeconds() const { return stateElapsed; }
		bool hasTexture() const { return videoTexture != nullptr && videoTexture->isAllocated(); }

		ofRectangle bounds;
		ofColor     placeholderColor;
		const ofTexture* videoTexture = nullptr; // shared; not owned
		ofRectangle      videoCrop;              // pixel rect within the video frame
		float       opacity        = 1.0f;
		float       arrivalDuration = 0;

	private:
		static ofShader fragmentShader;
		static bool     fragmentShaderReady;

		State     state           = State::ARRIVING;
		float     stateElapsed    = 0;
		float     dissolveDuration = 0;
		float     phaseOffset     = 0;
		bool      driftingEnabled = false;
		glm::vec2 driftOffset     {0, 0};
		glm::vec2 driftAmp        {0, 0};
		glm::vec2 driftFreq       {0, 0};
		float     desaturateRampDuration = 1.0f;
		float     desaturateMax   = 0.0f;
		bool      circularMask    = false;
		float     maskRadius      = 0.0f;
};
