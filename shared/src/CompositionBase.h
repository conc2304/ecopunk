#pragma once

#include <vector>
#include <memory>
#include <functional>
#include "Fragment.h"
#include "GridSystem.h"

// Owns the four-stage composition cycle arc (BLANK -> PLACEMENT -> DENSITY ->
// DISSOLVE -> reset, looping) and the fragment list. Knows nothing about how
// placement actually works — that's attemptPlacement(), implemented by a
// sketch-level subclass (e.g. BEComposition).
//
// All numeric tuning lives in the timing struct passed to setup(), not in
// this file, so the class stays reusable by sketches with different
// BESettings.h-style constants.
class CompositionBase {
	public:
		enum class CyclePhase { BLANK, PLACEMENT, DENSITY, DISSOLVE, RESET_HOLD };

		struct Timing {
			float cycleDurationMin;
			float cycleDurationMax;
			float blankDuration;
			float densityDuration;
			float dissolveDuration;
			float resetHoldDuration;
			float placementIntervalMin;
			float placementIntervalMax;
			float placementIntervalDense;
			float dissolveFadeMin;
			float dissolveFadeMax;
			int   maxFragments;
		};

		virtual ~CompositionBase() = default;

		void setup(GridSystem* grid, const Timing& timing);
		void startCycle();
		void update(float dt);

		CyclePhase getPhase() const { return phase; }
		float getPhaseElapsed() const { return phaseElapsed; }
		float getCycleElapsedSeconds() const { return cycleElapsedTotal; }
		int getCycleSeed() const { return cycleSeed; }
		const std::vector<std::unique_ptr<Fragment>>& getFragments() const { return fragments; }

		void setOnFragmentPlaced(std::function<void(Fragment*, Fragment*)> cb){ onFragmentPlaced = std::move(cb); }
		void setOnFragmentRemoved(std::function<void(Fragment*)> cb){ onFragmentRemoved = std::move(cb); }
		void setOnCycleStart(std::function<void()> cb){ onCycleStartCb = std::move(cb); }
		void setOnPhaseChanged(std::function<void(CyclePhase)> cb){ onPhaseChangedCb = std::move(cb); }

	protected:
		// Implemented by the sketch subclass: try to place one new fragment
		// (geometry/size/position selection, scoring, GridSystem::reserve(),
		// push_back into `fragments`). Returns false if no valid placement
		// was found after the sketch's own candidate-search budget.
		virtual bool attemptPlacement() = 0;

		// Called once per cycle, right after the seed is set (so any
		// subclass-level seeded randomness — e.g. a zone color roll — is
		// reproducible from the logged seed too).
		virtual void onCycleStart() {}

		// Called once per frame, before any phase logic runs. Lets a subclass
		// do its own per-frame bookkeeping (silence timers, trigger condition
		// checks, LFO-driven state) without overriding update() itself.
		virtual void onUpdate(float /*dt*/) {}

		// Picks the next PLACEMENT-phase placement interval. Default is a
		// uniform random draw; a subclass can override to bias the range
		// (e.g. with an LFO) while still respecting the same call sites.
		virtual float pickPlacementInterval(float lo, float hi) const { return randRangeF(lo, hi); }

		// When true (default), CompositionBase calls attemptPlacement() on its
		// own placementTimer during PLACEMENT/DENSITY — fine for organic
		// single-fragment-per-interval growth. A subclass managing several
		// independently-paced slots itself (each with its own hold/silence
		// timers, driven from onUpdate()) should override this to false so
		// the base class's single global timer doesn't also fire.
		virtual bool usesAutomaticPlacementTimer() const { return true; }

		// When false, CompositionBase won't advance PLACEMENT→DENSITY or
		// DENSITY→DISSOLVE on its internal phase timer. Override to false in
		// continuous modes (e.g. PERPETUAL) that manage their own phase arc.
		virtual bool usesAutomaticPhaseTimer() const { return true; }

		// Called when DISSOLVE phase finishes. Return false to suppress the
		// default RESET_HOLD transition and take over phase management.
		virtual bool onDissolveComplete() { return true; }

		// Opacity floor passed to startDissolve() during the DISSOLVE phase.
		// Default 0 = fade to DEAD. Override to GHOST_OPACITY_FLOOR in ghost modes.
		virtual float getDissolveFloor() const { return 0.0f; }

		// Jump to PLACEMENT immediately, skipping BLANK. For use by subclasses
		// implementing continuous cycle modes.
		void jumpToPlacementPhase();

		// Invoke the onCycleStart callback registered via setOnCycleStart().
		void fireCycleStartCallback() { if (onCycleStartCb) onCycleStartCb(); }

		// If currently in PLACEMENT, immediately ends it and enters DENSITY
		// without waiting for the phase timer. No-op in any other phase.
		void forceEnterDensity();

		void notifyFragmentPlaced(Fragment* newFrag, Fragment* nearest){
			if(onFragmentPlaced){
				onFragmentPlaced(newFrag, nearest);
			}
		}

		// Must be called before a subclass overwrites/destroys a fragment
		// still tracked elsewhere by raw pointer (e.g. AnnotationRenderer's
		// hub/last-placed tracking) — `fragments[i] = std::move(newFrag)`
		// frees the old Fragment immediately, so any external raw pointer to
		// it left un-invalidated becomes dangling as soon as this returns.
		void notifyFragmentRemoved(Fragment* frag){
			if(onFragmentRemoved){
				onFragmentRemoved(frag);
			}
		}

		static float randRangeF(float lo, float hi);
		static int randRangeI(int lo, int hiInclusive);

		GridSystem* grid = nullptr;
		Timing timing{};
		std::vector<std::unique_ptr<Fragment>> fragments;

	private:
		struct DissolveEntry {
			Fragment* fragment;
			float startOffset;
			float fadeDuration;
			bool started = false;
		};

		void beginDissolve();
		bool allFragmentsDead() const;
		void setPhase(CyclePhase newPhase);
		void enterDensity();

		CyclePhase phase = CyclePhase::BLANK;
		float phaseElapsed = 0;
		float cycleDuration = 0;
		float cycleElapsedTotal = 0;
		float placementDuration = 0;
		float placementTimer = 0;
		int cycleSeed = 0;
		bool structuralDissolveTriggered = false;
		std::vector<DissolveEntry> dissolveSchedule;
		std::function<void(Fragment*, Fragment*)> onFragmentPlaced;
		std::function<void(Fragment*)> onFragmentRemoved;
		std::function<void()> onCycleStartCb;
		std::function<void(CyclePhase)> onPhaseChangedCb;
};
