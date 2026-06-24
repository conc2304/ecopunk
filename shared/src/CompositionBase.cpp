#include "CompositionBase.h"
#include <cstdlib>
#include <random>
#include "ofLog.h"

float CompositionBase::randRangeF(float lo, float hi){
	return lo + (hi - lo) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
}

int CompositionBase::randRangeI(int lo, int hiInclusive){
	return lo + rand() % (hiInclusive - lo + 1);
}

void CompositionBase::setup(GridSystem* grid_, const Timing& timing_){
	grid = grid_;
	timing = timing_;
}

void CompositionBase::startCycle(){
	cycleSeed = static_cast<int>(std::random_device{}());
	srand(cycleSeed);
	ofLogNotice("CompositionBase") << "starting cycle, seed=" << cycleSeed;

	grid->clear();
	fragments.clear();
	dissolveSchedule.clear();

	cycleDuration = randRangeF(timing.cycleDurationMin, timing.cycleDurationMax);
	placementDuration = cycleDuration - timing.blankDuration - timing.densityDuration - timing.dissolveDuration;

	phase = CyclePhase::BLANK;
	phaseElapsed = 0;

	onCycleStart();

	if(onCycleStartCb){
		onCycleStartCb();
	}
}

bool CompositionBase::allFragmentsDead() const{
	for(const auto& f : fragments){
		if(!f->isDead()){
			return false;
		}
	}
	return true;
}

void CompositionBase::beginDissolve(){
	phase = CyclePhase::DISSOLVE;
	phaseElapsed = 0;
	dissolveSchedule.clear();

	for(auto& f : fragments){
		if(!f->isDead()){
			dissolveSchedule.push_back({
				f.get(),
				randRangeF(0.0f, timing.dissolveDuration * 0.5f),
				randRangeF(timing.dissolveFadeMin, timing.dissolveFadeMax),
				false
			});
		}
	}
}

void CompositionBase::update(float dt){
	for(auto& f : fragments){
		f->update(dt);
	}

	phaseElapsed += dt;

	switch(phase){
		case CyclePhase::BLANK:
			if(phaseElapsed >= timing.blankDuration){
				phase = CyclePhase::PLACEMENT;
				phaseElapsed = 0;
				placementTimer = randRangeF(timing.placementIntervalMin, timing.placementIntervalMax);
			}
			break;

		case CyclePhase::PLACEMENT:
			placementTimer -= dt;
			if(placementTimer <= 0.0f){
				bool atCap = static_cast<int>(fragments.size()) >= timing.maxFragments;
				if(!atCap){
					attemptPlacement(); // return value ignored; canvas-full → just wait for timer
				}
				placementTimer = randRangeF(timing.placementIntervalMin, timing.placementIntervalMax);
			}
			if(phaseElapsed >= placementDuration){
				phase = CyclePhase::DENSITY;
				phaseElapsed = 0;
				placementTimer = timing.placementIntervalDense;
				for(auto& f : fragments){
					f->enterDrifting();
				}
			}
			break;

		case CyclePhase::DENSITY:
			placementTimer -= dt;
			if(placementTimer <= 0.0f){
				bool atCap = static_cast<int>(fragments.size()) >= timing.maxFragments;
				if(!atCap){
					bool placed = attemptPlacement();
					if(placed){
						fragments.back()->enterDrifting();
					}
				}
				placementTimer = timing.placementIntervalDense;
			}
			if(phaseElapsed >= timing.densityDuration){
				beginDissolve();
			}
			break;

		case CyclePhase::DISSOLVE:{
			for(auto& entry : dissolveSchedule){
				if(!entry.started && phaseElapsed >= entry.startOffset){
					entry.fragment->startDissolve(entry.fadeDuration);
					entry.started = true;
				}
			}
			float safetyTimeout = timing.dissolveDuration + timing.dissolveFadeMax + 1.0f;
			if(allFragmentsDead() || phaseElapsed > safetyTimeout){
				phase = CyclePhase::RESET_HOLD;
				phaseElapsed = 0;
			}
			break;
		}

		case CyclePhase::RESET_HOLD:
			if(phaseElapsed >= timing.resetHoldDuration){
				startCycle();
			}
			break;
	}
}
