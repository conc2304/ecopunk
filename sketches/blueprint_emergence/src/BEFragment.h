#pragma once

#include "Fragment.h"
#include "RidgelineRenderer.h"
#include "ShaderLibrary.h"
#include "glm/vec2.hpp"
#include "glm/vec4.hpp"
#include "ofFbo.h"
#include <string>

// If your project already defines GeometryType somewhere else, keep that one
// and remove this enum from this header. In the original Blueprint Emergence
// layout, BEFragment.h is the natural place for it.
enum class GeometryType {
	RECT,
	CIRCLE,
	SLIVER,
	SQUARE
};

class BEFragment : public Fragment {
public:
	void setupBE(Fragment::Params params, GeometryType geometryType_, int canvasW, int canvasH);

	// Non-owning; set once by BEComposition right after construction. When
	// set, the fragment autonomously cycles through the shader pool while
	// DRIFTING/STABLE — see updateEffectCycle().
	void setShaderLibrary(ShaderLibrary * lib) { shaderLib = lib; }

	// Picks one of {0,1} at placement time so BEComposition can drive two
	// independent LFO lanes for per-fragment desaturation nudges (doc's
	// FRAG_DESAT_OFFSET_A/B groups) without every fragment moving in lockstep.
	void setEffectGroup(int group) { effectGroup = group; }
	int getEffectGroup() const { return effectGroup; }

	// LFO-driven nudge applied on top of the normal desaturate ramp; set each
	// frame by BEComposition from LFOBank, read back during drawStable().
	void setDesatNudge(float nudge) { desatNudge = nudge; }

	// Full-frame video pixels used by the ridgeline effect. Non-owning pointer;
	// set each frame by BEComposition from VideoSampler::getPixels().
	void setRidgelinePixels(const ofPixels* px) { ridgelinePixels = px; }

	// Wraps Fragment::draw() with a slow noise-driven scale oscillation
	// around the fragment's center — the "quadrant-style" per-slot scale
	// drift (see Quadrant::update()'s currentScale in quadrant-crosshair).
	void draw() const override;

protected:
	void update(float dt) override;
	void drawArrival(float t) const override;
	void drawStable() const override;
	void drawOverlay() const override;

private:
	void drawScanReveal(float elapsed) const;
	void drawClockwiseBorder(glm::vec2 pos, float w, float h, float t) const;
	void drawSlideIn(float elapsed) const;
	void drawIrisOpen(float elapsed) const;

	void updateEffectCycle(float dt);
	void pickAndStartEffect();
	void setEffectUniforms(ofShader & sh, const std::string & effect) const;
	void updateScaleDrift(float dt);
	void drawRidgelineEffect() const;

	GeometryType geometryType = GeometryType::RECT;
	glm::vec2 slideStartPos { 0, 0 };
	float targetRadius = 0.0f;

	// ── Quadrant-style scale drift ────────────────────────────────────────
	float scaleMin = 1.0f;
	float scaleMax = 1.0f;
	float scaleSeed = 0.0f;
	float currentScale = 1.0f;

	// ── Autonomous shader-effect cycling ─────────────────────────────────
	struct EffectSlot {
		enum class State { IDLE, FADE_IN, ACTIVE, FADE_OUT };
		std::string name;
		float alpha = 0.0f;
		float fadeDur = 1.5f;
		float dwellDur = 10.0f;
		float dwellAcc = 0.0f;
		glm::vec4 params { 0, 0, 0, 0 }; // per-effect random parameters, meaning depends on `name`
		State state = State::IDLE;
		bool isIdle() const { return state == State::IDLE; }
	};

	ShaderLibrary * shaderLib = nullptr;
	EffectSlot effectSlot;
	float effectSilenceAcc = 0.0f;
	float effectSilenceDur = 0.0f;
	int effectGroup = 0;
	float desatNudge = 0.0f;
	mutable ofFbo effectSourceFbo;
	mutable ofFbo effectResultFbo;
	mutable bool effectFbosAllocated = false;

	// ── Ridgeline effect ─────────────────────────────────────────────────────
	const ofPixels* ridgelinePixels = nullptr; // non-owning
	mutable RidgelineRenderer ridgelineRenderer;
	mutable int ridgelineSetupW = -1;
	mutable int ridgelineSetupH = -1;
};
