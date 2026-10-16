#include "VideoRegionEffectRenderer.h"
#include "DefaultVideoEffectCatalog.h"
#include "VideoEffectRegistry.h"
#include "VideoEffectTypes.h"
#include "ofGraphics.h"
#include "ofLog.h"
#include "ofMath.h"
#include "ofUtils.h"
#include <algorithm>

void VideoRegionEffectRenderer::ensureScratchFbos(int neededW, int neededH) {
	// Grow-only high-water mark: reallocating on every fragment-size
	// fluctuation (blob boxes resize almost every frame) would thrash the
	// FBOs every draw call. Instead the scratch pair only ever grows to the
	// largest region seen so far and every render() draws into its
	// top-left neededW x neededH sub-rect via drawSubsection, which works
	// correctly even when the FBO itself is bigger than the current region.
	if (neededW <= scratchW && neededH <= scratchH && scratchSourceFbo.isAllocated()) {
		return;
	}

	scratchW = std::max(neededW, scratchW);
	scratchH = std::max(neededH, scratchH);

	ofFbo::Settings s;
	s.width = scratchW;
	s.height = scratchH;
	s.internalformat = GL_RGBA;
	s.useDepth = false;

	scratchSourceFbo.allocate(s);
	scratchResultFbo.allocate(s);

	ofLogNotice("VideoRegionEffectRenderer") << "scratch FBOs (re)allocated to " << scratchW << "x" << scratchH;
}

namespace {
	// Lazily-built, process-lifetime registry of the Contract-A single-pass
	// effect catalog (shared/src/video-effects/catalog/DefaultVideoEffectCatalog.h)
	// — the single source of truth for per-effect uniform names/defaults this
	// method used to hand-duplicate. Function-local static: openFrameworks'
	// draw loop is single-threaded, so no synchronization is needed, and this
	// avoids adding a service-lifecycle dependency to a class that otherwise
	// has none (VideoRegionEffectRenderer still takes a plain ShaderLibrary&
	// per call, unchanged).
	const videoeffects::VideoEffectRegistry & catalogRegistry() {
		static videoeffects::VideoEffectRegistry registry = [] {
			videoeffects::VideoEffectRegistry r;
			videoeffects::registerSinglePassEffects(r);
			return r;
		}();
		return registry;
	}
}

// Per-shader uniform dispatch, now sourced from the canonical catalog
// instead of a hand-written per-name cascade — the catalog's defaults are
// the same fixed, sensible values this function used to hardcode directly
// (region fragments don't have BEFragment's per-instance ofRandom()
// randomization concept in V1, so "defaults" is the whole story here).
// effectAmount is still threaded into whichever uniform actually means "how
// much of the effect" for a given shader — every effect definition in the
// catalog names that parameter "alpha" (matching the shared convention
// documented in docs/shader-effect-system-probe.md §5), so this simply
// overrides the catalog's "alpha" default with the caller-supplied amount;
// every other parameter binds its catalog default unchanged.
void VideoRegionEffectRenderer::setEffectUniforms(ofShader & sh, const std::string & name, float effectAmount) const {
	using namespace videoeffects;

	const VideoEffectDefinition * def = catalogRegistry().getDefinition(name);
	if (def == nullptr) {
		// Unknown name (e.g. not part of the Contract-A single-pass catalog):
		// only tex/tex0/resolution were set by the caller — safe (unused
		// uniform locations are no-ops in GL) but that effect's own
		// parameters will be whatever the driver defaults them to.
		return;
	}

	for (const auto & param : def->params) {
		if (param.id == "alpha") {
			sh.setUniform1f("alpha", effectAmount);
			continue;
		}
		switch (param.type) {
			case VideoEffectParameterType::Float:
				sh.setUniform1f(param.id, asFloat(param.defaultValue));
				break;
			case VideoEffectParameterType::Int:
				sh.setUniform1i(param.id, asInt(param.defaultValue));
				break;
			case VideoEffectParameterType::Bool:
				sh.setUniform1i(param.id, asBool(param.defaultValue) ? 1 : 0);
				break;
			case VideoEffectParameterType::Vec3: {
				glm::vec3 v = asVec3(param.defaultValue);
				sh.setUniform3f(param.id, v.x, v.y, v.z);
				break;
			}
			default:
				break;
		}
	}

	// "time" is context-derived, not part of any effect's parameter schema
	// (see SinglePassShaderEffect.cpp's identical reasoning) — bound
	// unconditionally, harmless no-op for shaders that don't read it.
	sh.setUniform1f("time", ofGetElapsedTimef());
}

void VideoRegionEffectRenderer::render(const RenderRequest & req, ShaderLibrary & shaderLib) {
	if (req.sourceTexture == nullptr || !req.sourceTexture->isAllocated()) {
		return;
	}
	if (req.destinationBounds.width <= 0.0f || req.destinationBounds.height <= 0.0f) {
		return;
	}
	if (req.sourceCropPixels.width <= 0.0f || req.sourceCropPixels.height <= 0.0f) {
		return;
	}

	// Local FBO resolution matches the fragment's own destination size, not
	// the window — this is the fragment-local-UV-space fix BEFragment
	// already established (BEFragment.cpp:340-347): the shared texture's
	// crop sub-rect does not span 0..1 in texture space, so shaders that
	// assume screen-space/0..1 math (dithering, ASCII cell grids, pixel
	// snapping) need a clean local buffer to work in, not the raw crop.
	int w = std::max(1, static_cast<int>(std::round(req.destinationBounds.width)));
	int h = std::max(1, static_cast<int>(std::round(req.destinationBounds.height)));

	uint64_t tStart = ofGetElapsedTimeMicros();
	bool hasEffect = !req.effectName.empty() && shaderLib.has(req.effectName);

	// 0. Unshaded direct output: with no shader there is nothing that needs
	// clean local UVs, so the crop goes straight from the shared source
	// texture into destinationBounds — no scratch FBO, one resample instead
	// of two. Same single alpha blend as the scratch path below (the scratch
	// copy of an opaque source is itself opaque), so this also holds for
	// fading fragments (alpha < 1).
	if (!hasEffect && directOutputEnabled) {
		ofSetColor(255, 255, 255, static_cast<int>(ofClamp(req.alpha, 0.0f, 1.0f) * 255));
		req.sourceTexture->drawSubsection(
			req.destinationBounds.x, req.destinationBounds.y, req.destinationBounds.width, req.destinationBounds.height,
			req.sourceCropPixels.x, req.sourceCropPixels.y, req.sourceCropPixels.width, req.sourceCropPixels.height);
		ofSetColor(255);
		stageTimings.directDraws++;
		stageTimings.shaderDirectMs += static_cast<double>(ofGetElapsedTimeMicros() - tStart) / 1000.0;
		return;
	}

	ensureScratchFbos(w, h);

	// 1. Capture this region's crop into the scratch source FBO's top-left
	// w x h sub-rect (zero CPU pixel copies: this is a GPU-to-GPU blit of
	// the shared video texture's sub-region).
	scratchSourceFbo.begin();
	ofClear(0, 0, 0, 0);
	ofSetColor(255);
	req.sourceTexture->drawSubsection(0, 0, w, h,
		req.sourceCropPixels.x, req.sourceCropPixels.y, req.sourceCropPixels.width, req.sourceCropPixels.height);
	scratchSourceFbo.end();

	uint64_t tSourceDone = ofGetElapsedTimeMicros();
	stageTimings.sourceMs += static_cast<double>(tSourceDone - tStart) / 1000.0;

	if (!hasEffect) {
		// No effect configured (or shader failed to load), legacy path
		// forced — composite the plain crop from the scratch copy.
		ofSetColor(255, 255, 255, static_cast<int>(ofClamp(req.alpha, 0.0f, 1.0f) * 255));
		scratchSourceFbo.getTexture().drawSubsection(
			req.destinationBounds.x, req.destinationBounds.y, req.destinationBounds.width, req.destinationBounds.height,
			0, 0, w, h);
		ofSetColor(255);
		stageTimings.legacyDraws++;
		stageTimings.compositeMs += static_cast<double>(ofGetElapsedTimeMicros() - tSourceDone) / 1000.0;
		return;
	}

	// 2. Run the effect shader, at the region's own local resolution — NOT
	// ofGetWidth()/ofGetHeight() (the bug this class exists to avoid; see
	// Quadrant.cpp:276 for the sketch-local example that gets it wrong).
	ofShader & sh = shaderLib.get(req.effectName);

	// 2a. Direct output: run the shader straight into the currently bound
	// target at destinationBounds, skipping the scratch result FBO and the
	// separate composite draw. Only taken when it is pixel-equivalent to
	// the 2-3 path below under the standard OF_BLENDMODE_ALPHA blend
	// (SRC_ALPHA, ONE_MINUS_SRC_ALPHA on all four channels): that path
	// blends the shader's output alpha a twice (once into the cleared
	// result FBO, once more in the composite, times req.alpha), giving
	// c*a^3*alpha over dst*(1 - a^2*alpha); a single direct blend gives
	// c*a over dst*(1 - a). Those agree only when req.alpha == 1 and the
	// shader's own alpha is 0 or 1 — which every catalog effect's output
	// is at effectAmount == 1 (the production default). Fading fragments
	// (Fragment DISSOLVING, alpha < 1) or a reduced effectAmount keep the
	// 3-stage path, so no shader or uniform contract changes are needed.
	// Uniforms and texcoords are identical to step 2 (same local
	// resolution, same 0..w/scratchW source sub-rect); only the vertex
	// rect moves, and no catalog shader reads gl_FragCoord.
	bool direct = directOutputEnabled && req.alpha >= 1.0f && req.effectAmount >= 1.0f;
	if (direct) {
		sh.begin();
		sh.setUniformTexture("tex", scratchSourceFbo.getTexture(), 0);
		sh.setUniformTexture("tex0", scratchSourceFbo.getTexture(), 0);
		sh.setUniform2f("resolution", static_cast<float>(w), static_cast<float>(h));
		setEffectUniforms(sh, req.effectName, ofClamp(req.effectAmount, 0.0f, 1.0f));
		ofSetColor(255);
		scratchSourceFbo.getTexture().drawSubsection(
			req.destinationBounds.x, req.destinationBounds.y, req.destinationBounds.width, req.destinationBounds.height,
			0, 0, w, h);
		sh.end();

		stageTimings.directDraws++;
		stageTimings.shaderDirectMs += static_cast<double>(ofGetElapsedTimeMicros() - tSourceDone) / 1000.0;
		return;
	}

	scratchResultFbo.begin();
	ofClear(0, 0, 0, 0);
	sh.begin();
	sh.setUniformTexture("tex", scratchSourceFbo.getTexture(), 0);
	sh.setUniformTexture("tex0", scratchSourceFbo.getTexture(), 0);
	sh.setUniform2f("resolution", static_cast<float>(w), static_cast<float>(h));
	setEffectUniforms(sh, req.effectName, ofClamp(req.effectAmount, 0.0f, 1.0f));
	ofSetColor(255);
	scratchSourceFbo.getTexture().drawSubsection(0, 0, w, h, 0, 0, w, h);
	sh.end();
	scratchResultFbo.end();

	uint64_t tShaderDone = ofGetElapsedTimeMicros();
	stageTimings.shaderResultMs += static_cast<double>(tShaderDone - tSourceDone) / 1000.0;

	// 3. Composite into the destination rect with a plain textured draw (no
	// shader bound), so req.alpha blends correctly even for effects whose
	// own "alpha" uniform is being used for something other than opacity.
	ofSetColor(255, 255, 255, static_cast<int>(ofClamp(req.alpha, 0.0f, 1.0f) * 255));
	scratchResultFbo.getTexture().drawSubsection(
		req.destinationBounds.x, req.destinationBounds.y, req.destinationBounds.width, req.destinationBounds.height,
		0, 0, w, h);
	ofSetColor(255);

	stageTimings.legacyDraws++;
	stageTimings.compositeMs += static_cast<double>(ofGetElapsedTimeMicros() - tShaderDone) / 1000.0;
}
