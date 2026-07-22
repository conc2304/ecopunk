#pragma once

#include <map>
#include <string>
#include "ofFbo.h"
#include "ofRectangle.h"
#include "ofShader.h"
#include "ofTexture.h"
#include "ShaderLibrary.h"

// Weighted-random cycling among ShaderLibrary's 16 registered effects plus
// a "Raw / No Effect" option (17 weighted choices total), on its own timer
// — generalizes TFFragmentTransition's tfPickTransitionStyle from 3 fixed
// options to a named list via tfWeightedPick. Also owns the per-effect
// uniform dispatch and randomized per-instance params (dither's pixelation
// amount, recolor's tint, etc.) — copy-adapted from the real, working block
// at sketches/blueprint_emergence/src/BEFragment.cpp:149-263, including its
// two-stage FBO render (crop into a clean same-size FBO first, then run the
// shader against that): several effects assume texcoords spanning [0,1]
// over the destination's own pixel size (Bayer dithering, ASCII's cell
// grid, pixel snapping), which a direct drawSubsection() of an arbitrary
// crop sub-rect doesn't give them — see BEFragment.cpp:330-347's identical
// reasoning.
class TFEffectPicker {
	public:
		struct Weights {
			float cycleInterval = 8.0f;
			float rawWeight = 20.0f;
			std::map<std::string, float> effectWeights; // name -> weight, keyed by ShaderLibrary names
		};

		void setup(ShaderLibrary* shaderLib);
		void setWeights(const Weights& w) { weights = w; }
		void update(float dt);

		// Crops sourceTex to fill destRect (no distortion — see
		// TFTextureCropFill.h) and applies whichever effect is currently
		// selected, or draws it raw if "Raw" is selected.
		void drawCurrent(const ofTexture& sourceTex, const ofRectangle& destRect, float alpha = 1.0f);

		std::string getCurrentEffectName() const { return currentEffect; } // "" = raw

	private:
		void pickNext();
		void randomizeEffectParams(const std::string& name);
		void applyEffectUniforms(ofShader& sh, const std::string& name, float w, float h) const;

		ShaderLibrary* shaderLib = nullptr;
		Weights weights;
		float timer = 0.0f;
		std::string currentEffect; // "" = raw

		// Mirrors BEFragment's per-instance randomized params (effectSlot.params),
		// picked once when an effect is selected, not regenerated every frame.
		float paramX = 0.0f;
		float paramY = 0.0f;
		float paramZ = 0.0f;

		ofFbo sourceFbo; // clean [0,1] crop of the source, re-rendered each draw call
		ofFbo resultFbo; // shader-processed result, composited at destRect
};
