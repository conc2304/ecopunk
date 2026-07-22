#pragma once

#include "ofFbo.h"
#include "ofShader.h"
#include "ofRectangle.h"
#include "ofTexture.h"

// Animates one fragment's content changing from an old source to a new
// one, in one of three styles. Deliberately NOT built on shared/src's
// ErosionFBO: that class wraps a whole scene's beginCapture()/endCapture()
// and blends against its OWN running history (a self-referential ping-pong
// decay) — there's no way to hand it two arbitrary caller-specified
// textures, and instancing a full ErosionFBO (3 FBOs + shader) per
// fragment would be wasteful. This reuses only the *idea* — a small GLSL
// blend between a captured "before" and a live "after" — built for this
// per-fragment, two-arbitrary-texture, fixed-duration use case. See
// docs/temporal-fields-implementation-brief.md Section 1/7.
//
// The "from" side is captured once, into a small owned FBO, at begin() —
// a stable still, immune to the fact that playheads are a shared, mutable
// resource that could get reassigned to a different offset mid-transition.
// The "to" side is always sampled live each frame, never frozen, so the
// transition catches up to whatever the destination actually looks like
// *now* rather than a stale snapshot of it.
class TFFragmentTransition {
	public:
		enum class Style { HARD_CUT, CROSSFADE, EROSION };

		// `oldTex`/`oldSrcRect` describe the "before" content to snapshot:
		// oldSrcRect is in oldTex's own pixel space (as you'd pass to
		// ofTexture::drawSubsection's source-rect arguments).
		void begin(Style style, float duration, const ofRectangle& destBounds,
			const ofTexture& oldTex, const ofRectangle& oldSrcRect);

		void update(float dt);
		bool isActive() const { return active; }

		// Draws the current blended state at destBounds. Safe to call
		// unconditionally every frame regardless of whether a transition
		// is in progress — draws the live newTex directly once finished.
		// `alpha` (0..1) is an external opacity multiplier applied on top
		// of the transition's own blend — e.g. Blob Grid's per-fragment
		// edge-feather alpha, which must survive independently of whatever
		// this transition is doing internally.
		void draw(const ofRectangle& destBounds, const ofTexture& newTex, const ofRectangle& newSrcRect, float alpha = 1.0f);

	private:
		static ofShader& dissolveShader();

		ofFbo snapshotFbo;
		Style style = Style::HARD_CUT;
		float elapsed = 0.0f;
		float duration = 0.8f;
		bool active = false;
};

// Weighted-random style pick from the three tunable transition weights.
TFFragmentTransition::Style tfPickTransitionStyle(float hardCutWeight, float crossfadeWeight, float erosionWeight);
