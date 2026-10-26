#pragma once
#include "LFOBank.h"
#include "ofMain.h"
#include <array>

namespace quadrantcrosshair {

struct CrosshairPreset {
	float freqA, freqB;
	float ampA, ampB;
	float margin;
	std::string name;
};

struct CrosshairState {
	float cx, cy;
	float vx, vy;
	float speed;
	float chWidth;
};

class CrosshairSystem {
public:
	static constexpr float HIGH_THRESH = 1.5f;

	void setup();
	void update(float dt, const LFOBank & lfo);
	void draw(float uiFadeAlpha = 1.f);
	void setPreset(int index);
	void nextPreset();

	// Expansion control — called by ofApp each frame during an active sequence
	void setExpansionControl(bool active, glm::vec2 pos);
	// Called by ofApp when ExpansionDirector transitions TRAVEL_BACK → IDLE
	void beginResume();

	void sampleColor(const ofPixels & px, float cx, float cy);
	void triggerBloom(float targetRadius, float durationSecs);
	void setBloomFill(float target, float rampSecs);
	void setMotionAttractor(float x, float y, float energy);

	CrosshairState getState() const { return state; }
	int getCurrentPreset() const { return presetIndex; }
	// Installation ENERGY v0.1: scales how fast the crosshair's noise path
	// advances (0.35x at 0, 1x at the default 0.5, 2.2x at 1). Continuous —
	// the position follows the accumulated noise time, so changes never jump.
	void setEnergy(float energy01);
	float getEnergySpeedScale() const { return energySpeedScale; }
	int getPresetCount() const { return static_cast<int>(presets.size()); }
	const std::string & getPresetName(int index) const { return presets[static_cast<size_t>(index)].name; }

private:
	std::vector<CrosshairPreset> presets;
	int presetIndex = 0;
	float noiseT = 0.0f;
	float energySpeedScale = 1.0f;
	float seedXA = 0.0f, seedXB = 100.0f;
	float seedYA = 200.0f, seedYB = 300.0f;

	CrosshairState state;
	CrosshairState prevState;

	// Video-sampled color
	ofColor crosshairColor = ofColor(255);
	ofColor sampledColor = ofColor(255);

	// Motion attractor (pixel coords)
	float attractX = 640.f;
	float attractY = 360.f;
	float attractForce = 0.f;
	static constexpr float ATTRACT_LERP = 0.02f;
	// v0.3 smoothing (frame-rate independent). Time constants in seconds.
	static constexpr float ATTRACT_SMOOTH_SECONDS = 0.6f;
	static constexpr float SPEED_SMOOTH_SECONDS = 0.15f;
	static constexpr float DASH_HYSTERESIS = 0.8f; // dashed arms drop out below 0.8x HIGH_THRESH
	glm::vec2 smoothedAttract = { 640.f, 360.f };
	float smoothedAttractForce = 0.f;
	bool attractInitialised = false;
	float smoothedSpeed = 0.f;
	bool dashArms = false;
	void smoothAttractor(float dt);
	static float smoothTowards(float current, float target, float seconds, float dt);
	static float perFrameRate(float rateAt60, float dt);

	// Line metrics
	float lineWidth = 7.f;

	// Halo
	glm::vec2 haloPos = { 640.f, 360.f };

	// Intersection bloom
	float bloomRadius = 4.f;
	float bloomTarget = 4.f;
	float bloomFill = 0.f;

	void drawGradientArms(float cx, float cy, float opacH, float opacV);
	void drawDashArms(float cx, float cy, float speed, float uiFadeAlpha = 1.f);

	// Expansion state
	bool      expansionActive = false;
	glm::vec2 expansionPos;
	float     resumeLerp      = 0.f;
	bool      resuming        = false;
	void addArmQuad(ofMesh & mesh, glm::vec2 a, glm::vec2 b,
		float T, ofColor ca, ofColor cb);
};

} // namespace quadrantcrosshair
