#pragma once

#include "ofxGui.h"

// Runtime-tunable dials for Fragment Trail's own new parameters (Section 1/3
// of the brief), following Temporal Fields' TFParameterPanel convention —
// the only sibling sketch that uses ofxGui at all (blueprint_emergence and
// quadrant-crosshair use compile-time constants instead; see the
// implementation plan's "Settings pattern" decision). Deliberately much
// smaller than TFParameterPanel: no preset save/load or evolution system —
// nothing in this brief asked for either, and CrosshairSystem/TriggerBus
// (copied as-is from quadrant-crosshair) keep their own constexpr thresholds
// rather than being re-exposed here.
class FTParameterPanel {
public:
	void setup();
	void draw();

	void toggleVisible() { visible = !visible; }
	bool isVisible() const { return visible; }

	float getSpawnMinDistance() const { return spawnMinDistance; }
	float getSpawnMinInterval() const { return spawnMinInterval; }
	bool getIdlePulseEnabled() const { return idlePulseEnabled; }
	float getIdlePulseInterval() const { return idlePulseInterval; }

	// Total fragment lifetime is sustain + decay — see FTFragment::getAlpha().
	float getSustainSeconds() const { return sustainSeconds; }
	float getDecaySeconds() const { return decaySeconds; }
	int getMaxFragments() const { return maxFragments; }
	float getMinSize() const { return minSize; }
	float getMaxSize() const { return maxSize; }

	// 0 = crosshair speed has no effect on spawn cadence, 1 = full effect
	// per Section 3's mapping.
	float getSpeedResponse() const { return speedResponse; }

	float getModeDwellDuration() const { return modeDwellDuration; }

	// Crosshair movement — forwarded into CrosshairSystem::applyLiveSettings()
	// each frame (Section: crosshair motion dials).
	float getCrosshairSpeed() const { return crosshairSpeed; }
	float getCrosshairSmoothness() const { return crosshairSmoothness; }
	float getCrosshairSpeedVariance() const { return crosshairSpeedVariance; }
	float getCrosshairVarianceRate() const { return crosshairVarianceRate; }
	int getCrosshairPattern() const { return crosshairPattern; }
	bool getCrosshairAutoCycle() const { return crosshairAutoCycle; }
	float getCrosshairAutoCycleInterval() const { return crosshairAutoCycleInterval; }

private:
	ofParameterGroup spawnGroup;
	ofParameter<float> spawnMinDistance;
	ofParameter<float> spawnMinInterval;
	ofParameter<bool> idlePulseEnabled;
	ofParameter<float> idlePulseInterval;

	ofParameterGroup fragmentGroup;
	ofParameter<float> sustainSeconds;
	ofParameter<float> decaySeconds;
	ofParameter<int> maxFragments;
	ofParameter<float> minSize;
	ofParameter<float> maxSize;

	ofParameterGroup responseGroup;
	ofParameter<float> speedResponse;
	ofParameter<float> modeDwellDuration;

	ofParameterGroup crosshairGroup;
	ofParameter<float> crosshairSpeed;
	ofParameter<float> crosshairSmoothness;
	ofParameter<float> crosshairSpeedVariance;
	ofParameter<float> crosshairVarianceRate;
	ofParameter<int> crosshairPattern;
	ofParameter<bool> crosshairAutoCycle;
	ofParameter<float> crosshairAutoCycleInterval;

	ofParameterGroup rootGroup;
	ofxPanel panel;
	bool visible = true;
};
