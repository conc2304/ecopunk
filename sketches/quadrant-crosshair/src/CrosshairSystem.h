#pragma once
#include "LFOBank.h"
#include "ofMain.h"
#include <array>

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
	static constexpr int GHOST_LAG = 72; // 3 seconds at 24fps
	static constexpr int HISTORY_SIZE = 96; // 4 seconds at 24fps

	void setup();
	void update(float dt, const LFOBank & lfo);
	void draw();
	void setPreset(int index);
	void nextPreset();

	void sampleColor(const ofPixels & px, float cx, float cy);
	void triggerBloom(float targetRadius, float durationSecs);
	void setBloomFill(float target, float rampSecs);
	void setMotionAttractor(float x, float y, float energy);

	CrosshairState getState() const { return state; }
	int getCurrentPreset() const { return presetIndex; }

	bool showGhost = false;

private:
	std::vector<CrosshairPreset> presets;
	int presetIndex = 0;
	float noiseT = 0.0f;
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

	// Line metrics
	float lineWidth = 10.f;

	// Ghost trail
	std::array<glm::vec2, HISTORY_SIZE> posHistory;
	int historyHead = 0;
	glm::vec2 ghostPos = { 640.f, 360.f };

	// Halo
	glm::vec2 haloPos = { 640.f, 360.f };

	// Intersection bloom
	float bloomRadius = 4.f;
	float bloomTarget = 4.f;
	float bloomFill = 0.f;

	void drawGradientArms(float cx, float cy, float opacH, float opacV);
	void drawDashArms(float cx, float cy, float speed);
	void addArmQuad(ofMesh & mesh, glm::vec2 a, glm::vec2 b,
		float T, ofColor ca, ofColor cb);
};
