#pragma once
#include <functional>
#include <vector>

namespace hudoverlay {

// Base for the four composite organisms (design doc Section 04). Each
// organism, once triggered, plays out its own internal sequence of atoms
// with fixed stagger timing — armSequence()/tickSequence() implement that
// shared "timed step list" shape once instead of every organism
// hand-rolling its own timer bookkeeping.
class HudOverlayOrganism {
public:
	virtual ~HudOverlayOrganism() = default;
	virtual void update(float dt) = 0;
	virtual void draw() = 0;
	bool isActive() const { return activeFlag; }

protected:
	struct Step {
		float atSeconds;
		std::function<void()> action;
		bool fired = false;
	};

	// Replaces any in-flight sequence — re-triggering an organism before its
	// previous sequence finished restarts it rather than layering two.
	void armSequence(std::vector<Step> newSteps, float lifespanSeconds) {
		steps = std::move(newSteps);
		for (auto& s : steps) s.fired = false;
		stateT = 0.0f;
		lifespan = lifespanSeconds;
		activeFlag = true;
	}

	void tickSequence(float dt) {
		if (!activeFlag) return;
		stateT += dt;
		for (auto& s : steps) {
			if (!s.fired && stateT >= s.atSeconds) {
				s.fired = true;
				s.action();
			}
		}
		if (stateT >= lifespan) activeFlag = false;
	}

private:
	std::vector<Step> steps;
	float stateT = 0.0f;
	float lifespan = 0.0f;
	bool activeFlag = false;
};

} // namespace hudoverlay
