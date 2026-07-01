#include "CompositionBase.h"
#include "ofLog.h"
#include <cstdlib>
#include <random>

float CompositionBase::randRangeF(float lo, float hi) {
	return lo + (hi - lo) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
}

int CompositionBase::randRangeI(int lo, int hiInclusive) {
	return lo + rand() % (hiInclusive - lo + 1);
}

void CompositionBase::setup(GridSystem * grid_, const Timing & timing_) {
	grid = grid_;
	timing = timing_;
}

void CompositionBase::startCycle() {
	cycleSeed = static_cast<int>(std::random_device {}());
	srand(cycleSeed);
	ofLogNotice("CompositionBase") << "starting cycle, seed=" << cycleSeed;

	grid->clear();
	grid->startNewCycle();
	fragments.clear();
	dissolveSchedule.clear();
	structuralDissolveTriggered = false;

	cycleDuration = randRangeF(timing.cycleDurationMin, timing.cycleDurationMax);
	placementDuration = cycleDuration - timing.blankDuration - timing.densityDuration - timing.dissolveDuration;

	phase = CyclePhase::BLANK;
	phaseElapsed = 0;
	cycleElapsedTotal = 0;

	onCycleStart();

	if (onCycleStartCb) {
		onCycleStartCb();
	}
}

bool CompositionBase::allFragmentsDead() const {
	for (const auto & f : fragments) {
		Fragment::State s = f->getState();
		if (s != Fragment::State::DEAD && s != Fragment::State::GHOST) {
			return false;
		}
	}
	return true;
}

void CompositionBase::jumpToPlacementPhase() {
	setPhase(CyclePhase::PLACEMENT);
	phaseElapsed = 0;
	cycleElapsedTotal = 0;
	placementTimer = pickPlacementInterval(timing.placementIntervalMin, timing.placementIntervalMax);
	dissolveSchedule.clear();
	structuralDissolveTriggered = false;
}

void CompositionBase::setPhase(CyclePhase newPhase) {
	if (newPhase != phase) {
		phase = newPhase;
		if (onPhaseChangedCb) {
			onPhaseChangedCb(newPhase);
		}
	}
}

void CompositionBase::enterDensity() {
	setPhase(CyclePhase::DENSITY);
	phaseElapsed = 0;
	placementTimer = timing.placementIntervalDense;
	for (auto & f : fragments) {
		f->enterDrifting();
	}
}

void CompositionBase::forceEnterDensity() {
	if (phase == CyclePhase::PLACEMENT) {
		enterDensity();
	}
}

void CompositionBase::beginDissolve() {
	setPhase(CyclePhase::DISSOLVE);
	phaseElapsed = 0;
	dissolveSchedule.clear();

	for (auto & f : fragments) {
		// Ghost fragments from prior cycles are already below GHOST_OPACITY_FLOOR
		// and decay independently — don't schedule a second dissolve for them.
		Fragment::State s = f->getState();
		if (s != Fragment::State::DEAD && s != Fragment::State::GHOST) {
			dissolveSchedule.push_back({ f.get(),
				randRangeF(0.0f, timing.dissolveDuration * 0.5f),
				randRangeF(timing.dissolveFadeMin, timing.dissolveFadeMax),
				false });
		}
	}
}

void CompositionBase::update(float dt) {
	onUpdate(dt);

	grid->update(dt);

	for (auto & f : fragments) {
		f->update(dt);
	}

	phaseElapsed += dt;
	cycleElapsedTotal += dt;

	switch (phase) {
	case CyclePhase::BLANK:
		if (phaseElapsed >= timing.blankDuration) {
			setPhase(CyclePhase::PLACEMENT);
			phaseElapsed = 0;
			placementTimer = pickPlacementInterval(timing.placementIntervalMin, timing.placementIntervalMax);
		}
		break;

	case CyclePhase::PLACEMENT:
		if (usesAutomaticPlacementTimer()) {
			placementTimer -= dt;
			if (placementTimer <= 0.0f) {
				bool atCap = static_cast<int>(fragments.size()) >= timing.maxFragments;
				if (!atCap) {
					attemptPlacement(); // return value ignored; canvas-full → just wait for timer
				}
				placementTimer = pickPlacementInterval(timing.placementIntervalMin, timing.placementIntervalMax);
			}
		}
		if (usesAutomaticPhaseTimer() && phaseElapsed >= placementDuration) {
			enterDensity();
		}
		break;

	case CyclePhase::DENSITY:
		if (usesAutomaticPlacementTimer()) {
			placementTimer -= dt;
			if (placementTimer <= 0.0f) {
				bool atCap = static_cast<int>(fragments.size()) >= timing.maxFragments;
				if (!atCap) {
					bool placed = attemptPlacement();
					if (placed) {
						fragments.back()->enterDrifting();
					}
				}
				placementTimer = timing.placementIntervalDense;
			}
		}
		if (usesAutomaticPhaseTimer() && phaseElapsed >= timing.densityDuration) {
			beginDissolve();
		}
		break;

	case CyclePhase::DISSOLVE: {
		float floor = getDissolveFloor();
		for (auto & entry : dissolveSchedule) {
			if (!entry.started && phaseElapsed >= entry.startOffset) {
				entry.fragment->startDissolve(entry.fadeDuration, floor);
				grid->beginLineDissolveForFragment(entry.fragment->getId(), entry.fadeDuration);
				entry.started = true;
			}
		}

		bool fragmentsDone = allFragmentsDead();
		if (fragmentsDone && !structuralDissolveTriggered) {
			grid->beginStructuralDissolve();
			structuralDissolveTriggered = true;
		}

		float safetyTimeout = timing.dissolveDuration + timing.dissolveFadeMax + 1.0f;
		bool gridDone = grid->isDissolveFadeComplete();
		if ((fragmentsDone && gridDone) || phaseElapsed > safetyTimeout) {
			if (onDissolveComplete()) {
				setPhase(CyclePhase::RESET_HOLD);
				phaseElapsed = 0;
			}
		}
		break;
	}

	case CyclePhase::RESET_HOLD:
		if (phaseElapsed >= timing.resetHoldDuration) {
			startCycle();
		}
		break;
	}
}
