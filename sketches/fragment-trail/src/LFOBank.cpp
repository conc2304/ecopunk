#include "LFOBank.h"

void LFOBank::setup() {
	lanes = { {
		{ 0.031f, 0.00f, 0.f },   // LFO_CROSSHAIR_PULSE  ~32s cycle
	} };
}

void LFOBank::update(float dt) {
	timeAccum += dt;
	for (auto & lane : lanes)
		lane.value = sinf(timeAccum * lane.freq * TWO_PI + lane.phase);
}

float LFOBank::get(int index) const {
	return lanes[index].value;
}
