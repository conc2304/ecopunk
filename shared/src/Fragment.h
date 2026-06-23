#pragma once

#include "ofRectangle.h"
#include "ofColor.h"
#include "glm/vec2.hpp"

// Base state machine for a single placed crop. Owns timing, opacity, and
// the ambient-drift offset. Geometry-specific arrival animations and the
// eventual real video texture (Phase 3) belong in sketch-level subclasses.
class Fragment {
	public:
		enum class State { ARRIVING, STABLE, DRIFTING, DISSOLVING, DEAD };

		virtual ~Fragment() = default;

		// driftAmp/driftFreq are sketch-tunable (e.g. BESettings.h's DRIFT_AMP_X/Y,
		// DRIFT_FREQ_X/Y) — Fragment owns the drift *mechanism*, not the tuning values.
		void setup(const ofRectangle& bounds, const ofColor& placeholderColor, float phaseOffset, float arrivalDuration,
			glm::vec2 driftAmp, glm::vec2 driftFreq);
		void update(float dt);
		void draw() const;

		void enterDrifting(); // called once when the composition reaches the DENSITY stage
		void startDissolve(float fadeDuration);

		bool isDead() const { return state == State::DEAD; }
		State getState() const { return state; }
		const ofRectangle& getBounds() const { return bounds; }

	protected:
		virtual void drawArrival(float t) const; // t = 0..1 progress through the arrival animation
		virtual void drawStable() const;         // default: filled placeholder rect

		glm::vec2 getDrawPosition() const { return glm::vec2(bounds.getPosition()) + driftOffset; }
		float getStateElapsedSeconds() const { return stateElapsed; }

		ofRectangle bounds;
		ofColor placeholderColor;
		float opacity = 1.0f;
		float arrivalDuration = 0;

	private:
		State state = State::ARRIVING;
		float stateElapsed = 0;
		float dissolveDuration = 0;
		float phaseOffset = 0;
		bool driftingEnabled = false;
		glm::vec2 driftOffset{0, 0};
		glm::vec2 driftAmp{0, 0};
		glm::vec2 driftFreq{0, 0};
};
