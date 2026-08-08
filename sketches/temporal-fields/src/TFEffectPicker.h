#pragma once

#include <map>
#include <string>
#include "ofFbo.h"
#include "ofRectangle.h"
#include "ofShader.h"
#include "ofTexture.h"
#include "ShaderLibrary.h"
#include "EffectActivityStatus.h"
#include "EffectKnowledgeBase.h"

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

		// Shared Effect Knowledge integration (Engineering Session 2's first
		// real proof target -- see docs/shared-effect-knowledge-scoped-extension.md
		// and docs/temporal-fields-knowledge-pack-integration.md). Empty
		// (zero slots) when currentEffect == "" (Raw / No Effect selected) --
		// there is no active effect to report in that case, not an effect with
		// an empty id. phase is always EvolutionPhase::Holding: this picker
		// hard-cuts between effects on its own timer, it does not blend/
		// transition the way EffectEvolutionController does, so there is no
		// genuine "Transitioning" state to report.
		videoeffects::EffectActivityStatus activityStatus() const;

	private:
		void pickNext();
		void randomizeEffectParams(const std::string& name);
		void applyEffectUniforms(ofShader& sh, const std::string& name, float w, float h) const;

		// Best-effort content-based avoidance of blacklisted parameter
		// combinations for the small set of effects this class randomizes
		// itself (see randomizeEffectParams()). Returns the snapshot map used
		// for that check (empty map for effects this class doesn't randomize,
		// e.g. "invert"/"solarize" -- nothing to compare, so nothing to avoid,
		// matching this class's existing "only 7 effects have real
		// per-instance randomization" design).
		std::map<std::string, float> currentParamSnapshot(const std::string& name) const;

		ShaderLibrary* shaderLib = nullptr;
		Weights weights;
		float timer = 0.0f;
		std::string currentEffect; // "" = raw

		// Imported once in setup(); absent/malformed pack leaves this empty and
		// every knowledge-based check below becomes a no-op, so pickNext()'s
		// existing behavior is completely unchanged when no pack has ever been
		// exported -- see setup()'s own comment.
		videoeffects::EffectKnowledgeBase knowledgeBase;

		// Mirrors BEFragment's per-instance randomized params (effectSlot.params),
		// picked once when an effect is selected, not regenerated every frame.
		float paramX = 0.0f;
		float paramY = 0.0f;
		float paramZ = 0.0f;
		float paramW = 0.0f;

		ofFbo sourceFbo; // clean [0,1] crop of the source, re-rendered each draw call
		ofFbo resultFbo; // shader-processed result, composited at destRect
};
