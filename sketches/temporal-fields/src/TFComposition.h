#pragma once

#include <functional>
#include <vector>
#include "TFPattern.h"
#include "TFPatternType.h"
#include "TFFragmentTransition.h"
#include "ofFbo.h"
#include "ofVec2f.h"

// Owns the top-level cycle: which pattern (BSP vs Blob Grid) is currently
// active, how long it runs before switching, and the per-cycle random seed.
// Does not know anything about either pattern's internals — TFPattern is an
// opaque reset()/update()/draw() surface supplied by the sketch (ofApp).
//
// Deliberately NOT a CompositionBase subclass — CompositionBase's CyclePhase
// (BLANK/PLACEMENT/DENSITY/DISSOLVE/RESET_HOLD) and its phase-transition
// logic are private and fixed to blueprint_emergence's fill/erode narrative
// arc, which doesn't fit this sketch's continuous, cycle-then-switch-pattern
// model. See docs/temporal-fields-implementation-brief.md, Section 1.
class TFComposition {
	public:
		enum class CyclePhase { RUNNING, PATTERN_TRANSITION };

		struct Timing {
			float cycleDuration;
		};

		// canvasW/canvasH are needed here (not just by the patterns
		// themselves) because a pattern switch now captures a whole-canvas
		// "before" snapshot and composites a whole-canvas "after" render
		// through TFFragmentTransition — the real scene-level transition
		// Phase 1 deferred to this phase. See switchToPattern().
		void setup(const Timing& timing, TFPattern* bspPattern, TFPattern* blobGridPattern, int canvasW, int canvasH);

		// Pulled from the live GUI dials every frame (ofApp), since the
		// weights/duration can change at any time and a switch can happen
		// at any time too.
		void setTransitionParams(float duration, float hardCutWeight, float crossfadeWeight, float erosionWeight);

		// Starts a fresh run from scratch: picks a starting pattern, seeds
		// it, and resets the cycle timer.
		void startCycle();

		void update(float dt);
		void draw();

		CyclePhase getPhase() const { return phase; }
		TFPatternType getActivePatternType() const { return activeType; }
		int getCycleSeed() const { return cycleSeed; }
		float getPhaseElapsed() const { return phaseElapsed; }

		// Manual override for testing — jumps straight to the next pattern
		// without waiting for the cycle timer. Mirrors blueprint_emergence's
		// debug-key idiom (BEComposition::forceAxisFlip()).
		void forceNextPattern();

		// Jumps straight to a specific pattern — used when a loaded preset
		// (or, from Phase 7, a waypoint) belongs to the pattern that isn't
		// currently running, so its effect is actually visible immediately.
		// No-op if already active.
		void forcePattern(TFPatternType type);

		void setOnPatternChanged(std::function<void(TFPatternType)> cb) { onPatternChangedCb = std::move(cb); }

		// Forwards to both bspPattern and blobGridPattern — both fire the
		// same callback, but only the currently-active one ever actually
		// calls update() (and therefore reassigns fragments), so only one
		// fires at a time in practice. Call after setup().
		void setOnFragmentReassigned(std::function<void(float nx, float ny)> cb);

		// Pass-through to whichever pattern is currently active.
		std::vector<ofVec2f> getActiveFragmentCenters() const { return activePattern()->getActiveFragmentCenters(); }

	private:
		TFPattern* activePattern() const;
		void switchToPattern(TFPatternType type);
		void beginTransitionToNextPattern();
		void captureOutgoingSnapshot();

		Timing timing{};
		TFPattern* bspPattern = nullptr;
		TFPattern* blobGridPattern = nullptr;
		int canvasW = 0;
		int canvasH = 0;

		CyclePhase phase = CyclePhase::RUNNING;
		TFPatternType activeType = TFPatternType::BSP;
		float phaseElapsed = 0.0f;
		int cycleSeed = 0;

		float transitionDuration = 0.8f;
		float hardCutWeight = 33.0f;
		float crossfadeWeight = 34.0f;
		float erosionWeight = 33.0f;

		// The actual "full scene-level transition" — a whole-canvas
		// snapshot of the outgoing pattern blended against a fresh,
		// live-rendered frame of the incoming one. Reuses Phase 5's
		// per-fragment transition machinery at canvas scale rather than a
		// separate implementation, since the blend styles/technique are
		// identical, just applied to one big rectangle instead of many.
		TFFragmentTransition sceneTransition;
		ofFbo outgoingSnapshotFbo;
		ofFbo incomingRenderFbo;

		std::function<void(TFPatternType)> onPatternChangedCb;
};
