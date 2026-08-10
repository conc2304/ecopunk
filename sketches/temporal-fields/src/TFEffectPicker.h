#pragma once

#include <map>
#include <optional>
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

	// Test-support surface, NOT part of this class's real production
	// role -- lets a self-test seed real (effect, snapshot, presetId,
	// compatibility) knowledge into this instance's own
	// EffectKnowledgeBase without going through file-based pack
	// import, and lets a test force pickNext()'s effect-selection
	// coin flip while still exercising the REAL applyEligibleCanonicalPreset()/
	// isReusableAuthoredPreset()/resolveCompatibility() call chain.
	// See TFEligibilitySelfTest.h.
	videoeffects::EffectKnowledgeBase& knowledgeBaseForTest() { return knowledgeBase; }
	void forceEffectForTest(const std::string& effectName) {
		Weights w;
		w.rawWeight = effectName.empty() ? 1000.0f : 0.0f;
		w.cycleInterval = 999999.0f;
		if (!effectName.empty()) w.effectWeights = { { effectName, 1000.0f } };
		setWeights(w);
		pickNext();
	}
	// Exposes which path the most recent pickNext() actually took --
	// test-only visibility into applyEligibleCanonicalPreset()'s
	// return value, which pickNext() itself only otherwise expresses
	// through which private param-setting function it called.
	bool lastPickUsedCanonicalPreset() const { return lastPickUsedCanonicalPreset_; }
	std::optional<std::string> lastAppliedPresetId() const { return lastAppliedPresetId_; }
	// Exposes the exact same snapshot representation applyEligibleCanonicalPreset()/
	// randomizeEffectParams() left paramX..W in, for the currently-selected
	// effect -- the same private currentParamSnapshot() the real blacklist-
	// avoidance check and (by construction) the real render path's values
	// derive from. Lets a test prove a selected preset's authored values
	// genuinely reached this class's internal state, not just that some
	// function returned true.
	std::map<std::string, float> currentParamSnapshotForTest() const { return currentParamSnapshot(currentEffect); }

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

		// Shared Effect Knowledge v1 Freeze Policy (DEC-016) production-
		// selector adoption -- see EffectKnowledgePrecedence.h. Consults
		// the real canonical eligibility chain (preset override -> effect
		// default -> Unclassified; blocked always excluded; legacy
		// anonymous presets never automatic-selection-eligible) against
		// this scene's id ("temporal-fields") for `effectName`'s
		// WHITELIST entries only -- this governs which PARAMETER VALUES
		// an already-chosen effect uses, not which effect gets chosen
		// (that remains this class's own `weights.effectWeights` local
		// policy, unaffected -- see this method's .cpp comment for why
		// gating effect-selection itself on eligibility was rejected).
		// On success, applies the chosen entry's snapshot into
		// paramX..W (via the same per-effect key mapping
		// currentParamSnapshot() uses in the other direction) and
		// returns true. Returns false (leaving paramX..W untouched) if
		// no eligible preset exists, so the caller can fall back to
		// randomizeEffectParams() exactly as before this increment.
		bool applyEligibleCanonicalPreset(const std::string& effectName);
		void applyParamsFromSnapshot(const std::string& name, const std::map<std::string, float>& snapshot);

		ShaderLibrary* shaderLib = nullptr;
		Weights weights;
		float timer = 0.0f;
		std::string currentEffect; // "" = raw

		// Imported once in setup(); absent/malformed pack leaves this empty and
		// every knowledge-based check below becomes a no-op, so pickNext()'s
		// pre-this-increment behavior is completely unchanged when no pack
		// has ever been exported, or when nothing eligible has been
		// authored for the chosen effect -- see setup()'s own comment.
		videoeffects::EffectKnowledgeBase knowledgeBase;

		// Diagnostic only (not consumed by drawCurrent()/activityStatus()) --
		// updated at the top of pickNext() each time, so it always
		// reflects the most recent pick, never stale.
		bool lastPickUsedCanonicalPreset_ = false;
		std::optional<std::string> lastAppliedPresetId_;

		// Mirrors BEFragment's per-instance randomized params (effectSlot.params),
		// picked once when an effect is selected, not regenerated every frame.
		float paramX = 0.0f;
		float paramY = 0.0f;
		float paramZ = 0.0f;
		float paramW = 0.0f;

		ofFbo sourceFbo; // clean [0,1] crop of the source, re-rendered each draw call
		ofFbo resultFbo; // shader-processed result, composited at destRect
};
