#pragma once
#include "ContourDisplacementEffect.h"
#include <string>
#include <vector>

// Five built-in artistic starting points (brief's "Presets" section),
// applied by directly writing ContourDisplacementEffect's ofParameters.
// Distinct from ofApp's save/load-to-XML flow (via ofxPanel::save/
// loadToFile), which lets a user capture their own named presets on top of
// these once they've tuned something worth keeping.
namespace ContourPresets {
	const std::vector<std::string> & names();
	void apply(ContourDisplacementEffect & fx, const std::string & name);
}
