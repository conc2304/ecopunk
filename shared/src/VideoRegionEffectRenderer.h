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

	// Per-stage CPU-side timing, accumulated across render() calls until
	// resetStageTimings() — for Pi profiling of the direct-output vs
	// 3-stage paths (see render()). CPU wall time around GL submission,
	// not GPU time: driver stalls land in whichever stage triggers them.
	struct StageTimings {
		int directDraws = 0; // shader (or unshaded crop) straight into the bound target
		int legacyDraws = 0; // shader -> scratchResultFbo -> composite (or unshaded scratch copy -> composite)
		double sourceMs = 0.0; // crop into scratchSourceFbo (every draw except unshaded direct)
		double shaderDirectMs = 0.0;
		double shaderResultMs = 0.0;
		double compositeMs = 0.0;
		int unshadedDraws = 0; // subset of direct+legacy draws that ran no shader
		int shaderBinds = 0; // shaded draws (one program bind each)
		int shaderSwitches = 0; // shaded draws whose program differs from the previous shaded draw
		int distinctPrograms = 0; // distinct programs bound since the last reset
	};
	const StageTimings & getStageTimings() const { return stageTimings; }
	void resetStageTimings() {
		stageTimings = StageTimings {};
		lastProgram = 0;
		seenProgramCount = 0;
	}

	// false forces every effect draw through the 3-stage path — exists so
	// the same binary can A/B both paths on the Pi.
	void setDirectOutputEnabled(bool enabled) { directOutputEnabled = enabled; }
	bool isDirectOutputEnabled() const { return directOutputEnabled; }

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
	bool directOutputEnabled = true;
	StageTimings stageTimings;

	// Bind/switch bookkeeping for StageTimings (reset with it).
	void countShaderBind(unsigned int program);
	unsigned int lastProgram = 0;
	static constexpr int kMaxSeenPrograms = 32;
	unsigned int seenPrograms[kMaxSeenPrograms] = {};
	int seenProgramCount = 0;
};
