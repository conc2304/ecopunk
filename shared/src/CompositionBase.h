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
		int getCycleSeed() const { return cycleSeed; }
		const std::vector<std::unique_ptr<Fragment>>& getFragments() const { return fragments; }

		void setOnFragmentPlaced(std::function<void(Fragment*, Fragment*)> cb){ onFragmentPlaced = std::move(cb); }

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

		void notifyFragmentPlaced(Fragment* newFrag, Fragment* nearest){
			if(onFragmentPlaced){
				onFragmentPlaced(newFrag, nearest);
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

		CyclePhase phase = CyclePhase::BLANK;
		float phaseElapsed = 0;
		float cycleDuration = 0;
		float placementDuration = 0;
		float placementTimer = 0;
		int cycleSeed = 0;
		std::vector<DissolveEntry> dissolveSchedule;
		std::function<void(Fragment*, Fragment*)> onFragmentPlaced;
};
