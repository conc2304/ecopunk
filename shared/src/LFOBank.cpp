#include "LFOBank.h"
#include "ofMathConstants.h"
#include <cmath>

void LFOBank::setup(int numLanes) {
	lanes.assign(numLanes, Lane{});
	elapsed = 0.0f;
}

void LFOBank::update(float deltaTime) {
	elapsed += deltaTime;
	for (auto & lane : lanes) {
		lane.phase = elapsed * lane.frequency * TWO_PI + lane.phaseOffset;
		lane.value = std::sin(lane.phase);
	}
}

float LFOBank::get(int lane) const {
	return lanes[lane].value;
}

float LFOBank::getUnipolar(int lane) const {
	return (lanes[lane].value + 1.0f) * 0.5f;
}

void LFOBank::setFrequency(int lane, float hz) {
	lanes[lane].frequency = hz;
}

void LFOBank::setPhaseOffset(int lane, float radians) {
	lanes[lane].phaseOffset = radians;
}
