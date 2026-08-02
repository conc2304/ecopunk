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

	void setup();
	void update(float dt, const LFOBank & lfo);
	void draw(float uiFadeAlpha = 1.f);
	void setPreset(int index);
	void nextPreset();

	// Re-applies the ofxGui panel's live crosshair-motion dials — called
	// every frame, same convention as FTFragmentPool::applyLiveSettings().
	// speedMult scales how fast the underlying noise field is traversed;
	// smoothing low-pass-filters the resulting position (0 = raw/instant,
	// matching pre-dial behavior); speedVariance/varianceRate drive a slow
	// secondary noise lane that continuously scales speedMult between
	// (1-speedVariance) and (1+speedVariance) so pace ebbs and flows instead
	// of holding one constant rate; patternIndex only takes effect (via
	// setPreset()) when autoCycleEnabled is false and the dial's value has
	// actually changed, so it doesn't fight the 'p' key or an active cycle.
	void applyLiveSettings(float speedMult_, float smoothing_, float speedVariance_,
		float varianceRate_, int patternIndex_, bool autoCycleEnabled_, float autoCycleInterval_);

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

	// Crosshair-motion dials (live-tunable via FTParameterPanel)
	float speedMult = 1.f;
	float smoothing = 0.f;
	float speedVariance = 0.4f;
	float varianceRate = 0.03f;
	float varyPhase = 0.f;
	float seedVary = 0.f;
	bool autoCycleEnabled = false;
	float autoCycleInterval = 15.f;
	float autoCycleTimer = 0.f;
	int lastAppliedPattern = -1;

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
