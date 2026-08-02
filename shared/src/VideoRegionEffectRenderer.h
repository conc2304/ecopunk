#pragma once

#include "ShaderLibrary.h"
#include "ofFbo.h"
#include "ofRectangle.h"
#include "ofTexture.h"
#include <string>

// Generalized extraction of BEFragment::drawOverlay()'s fragment-local-FBO
// effect pattern (sketches/blueprint_emergence/src/BEFragment.cpp:303-387),
// promoted to shared/src so blob-driven fragments (and, later, any other
// region-driven fragment) reuse it instead of a 4th sketch-local copy.
//
// Ownership: owns exactly one scratch source/result FBO pair, reused
// sequentially for every region drawn in a frame (never one FBO per
// fragment). Does not own a video source, a shader library, or a Fragment —
// callers pass the shared source texture and the canonical ShaderLibrary in
// on every render() call.
//
// Uniform contract: every ShaderLibrary effect gets uniform sampler2D tex
// (+ tex0, for the "nature pack" shaders that use that name instead) and
// uniform vec2 resolution (the *local* FBO size, never the window size —
// see the correctness bug this deliberately avoids at
// sketches/quadrant-crosshair/src/Quadrant.cpp:276). Beyond that, per-shader
// uniforms are NOT uniform across the library (uniform float alpha means
// three different things across desaturate/dither/hue_rotate; several
// nature-pack shaders have no alpha uniform at all) — setEffectUniforms()
// below is the per-name dispatch table for that, deliberately mirroring
// sketches/blueprint_emergence/src/BEFragment.cpp's
// setEffectUniforms()/pickAndStartEffect() but with fixed defaults instead
// of BEFragment's per-instance randomization (V1 region fragments don't
// have that concept). See docs/blob-region-architecture.md for the bug this
// fixed: calling only tex/tex0/resolution/alpha on e.g. "threshold" (whose
// uniform float threshold defaults to 0 when unset) or "hue_rotate" (whose
// uniform float valueMult defaults to 0 when unset) rendered solid
// white/black respectively, regardless of source content.
class VideoRegionEffectRenderer {
public:
	struct RenderRequest {
		const ofTexture * sourceTexture = nullptr; // shared; not owned
		ofRectangle sourceCropPixels; // pixel-space crop within sourceTexture
		ofRectangle destinationBounds; // screen-space destination rect
		float alpha = 1.0f; // overall composite alpha (fragment fade * region confidence, etc.)
		std::string effectName; // ShaderLibrary key; empty/unknown => draw the crop unshaded
		float effectAmount = 1.0f; // forwarded as the shader's "alpha" (intensity) uniform
	};

	// Renders one region's crop, optionally through one ShaderLibrary
	// effect, composited into destinationBounds. Safe to call once per
	// active region per frame — the scratch FBOs are reused across calls.
	void render(const RenderRequest & req, ShaderLibrary & shaderLib);

	// High-water-mark scratch FBO size — exposed for debug HUD / perf
	// reporting, not required for normal use.
	int getScratchWidth() const { return scratchW; }
	int getScratchHeight() const { return scratchH; }

private:
	void ensureScratchFbos(int neededW, int neededH);
	// Looks up shared/src/video-effects' canonical catalog (DefaultVideoEffectCatalog.h)
	// instead of a hand-written per-name cascade — see VideoRegionEffectRenderer.cpp
	// for why this eliminates duplication with BEFragment.cpp/TFEffectPicker.cpp/
	// Quadrant.cpp's near-identical literal-constant blocks without changing
	// this class's public API or any caller.
	void setEffectUniforms(ofShader & sh, const std::string & name, float effectAmount) const;

	ofFbo scratchSourceFbo;
	ofFbo scratchResultFbo;
	int scratchW = 0;
	int scratchH = 0;
};
