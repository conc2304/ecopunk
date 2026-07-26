#pragma once

#include <functional>
#include <utility>
#include <vector>
#include "TFPattern.h"
#include "TFPatternType.h"
#include "TFFragmentTransition.h"
#include "ofFbo.h"
#include "ofVec2f.h"

// Owns the top-level cycle: which pattern is currently active, how long it
// runs before switching to the next one, and the per-cycle random seed.
// Does not know anything about any pattern's internals — TFPattern is an
// opaque reset()/update()/draw() surface supplied by the sketch (ofApp).
//
// Originally built for exactly two patterns (BSP/Blob Grid, named pointers
// and a ternary switch). Generalized to a registry — an ordered
// std::vector<{type, TFPattern*}> — once this phase brought the total to
// seven; see docs/temporal-fields-new-patterns-brief.md Section 0. Cycling
// through more than two patterns just walks this list in registration
// order, wrapping back to the front.
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

		// `patterns` is the cycle order — TFComposition starts on
		// patterns.front() and advances through the list in order, wrapping.
		// canvasW/canvasH are needed here (not just by the patterns
		// themselves) because a pattern switch now captures a whole-canvas
		// "before" snapshot and composites a whole-canvas "after" render
		// through TFFragmentTransition — the real scene-level transition
		// Phase 1 deferred to this phase. See switchToPattern().
		void setup(const Timing& timing, std::vector<std::pair<TFPatternType, TFPattern*>> patterns, int canvasW, int canvasH);

		// Propagates a new canvas size to every registered pattern (so
		// whichever one is switched to next already has it), and
		// immediately regenerates the currently active pattern's
		// geometry at the new size with its existing cycle seed -- same
		// effect as a pattern switch's reset(), but without triggering
		// a scene transition or picking a new seed.
		void resizeCanvas(int canvasW, int canvasH);

		// Pulled from the live GUI dials every frame (ofApp), since the
		// weights/duration can change at any time and a switch can happen
		// at any time too.
		void setTransitionParams(float duration, float hardCutWeight, float crossfadeWeight, float erosionWeight);

		// Starts a fresh run from scratch: jumps to the first registered
		// pattern, seeds it, and resets the cycle timer.
		void startCycle();

		void update(float dt);
		void draw();

		CyclePhase getPhase() const { return phase; }
		TFPatternType getActivePatternType() const { return activeType; }
		int getCycleSeed() const { return cycleSeed; }
		float getPhaseElapsed() const { return phaseElapsed; }

		// Manual override for testing — jumps straight to the next pattern
		// in registration order without waiting for the cycle timer. Mirrors
		// blueprint_emergence's debug-key idiom (BEComposition::forceAxisFlip()).
		void forceNextPattern();

		// Jumps straight to a specific pattern — used when a loaded preset
		// (or a waypoint) belongs to a pattern that isn't currently running,
		// so its effect is actually visible immediately. No-op if already
		// active, or if `type` isn't registered.
		void forcePattern(TFPatternType type);

		// Suspends the automatic advance to the next pattern after
		// Timing::cycleDuration elapses -- set by ofApp while a multi-state
		// preset timeline (TFPresetTimeline) is running, since that timeline
		// is authored to hold one single pattern for its own much longer
		// duration (minutes) and would otherwise get interrupted by this
		// unrelated auto-cycle. Defaults to false (unchanged, pre-existing
		// behavior); forcePattern()/forceNextPattern() are unaffected either
		// way -- only the automatic phaseElapsed >= cycleDuration advance in
		// update() is gated by this.
		void setAutoCycleSuspended(bool suspended) { autoCycleSuspended = suspended; }

		void setOnPatternChanged(std::function<void(TFPatternType)> cb) { onPatternChangedCb = std::move(cb); }

		// Forwards to every registered pattern — all fire the same callback,
		// but only the currently-active one ever actually calls update()
		// (and therefore reassigns fragments), so only one fires at a time
		// in practice. Call after setup().
		void setOnFragmentReassigned(std::function<void(float nx, float ny)> cb);

		// Pass-through to whichever pattern is currently active.
		std::vector<ofVec2f> getActiveFragmentCenters() const { return activePattern()->getActiveFragmentCenters(); }

	private:
		TFPattern* activePattern() const;
		TFPattern* findPattern(TFPatternType type) const;
		void switchToPattern(TFPatternType type);
		void beginTransitionToNextPattern();
		void captureOutgoingSnapshot();

		Timing timing{};
		std::vector<std::pair<TFPatternType, TFPattern*>> patterns; // cycle order
		int canvasW = 0;
		int canvasH = 0;

		CyclePhase phase = CyclePhase::RUNNING;
		TFPatternType activeType = TFPatternType::BSP;
		float phaseElapsed = 0.0f;
		int cycleSeed = 0;
		bool autoCycleSuspended = false;

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
