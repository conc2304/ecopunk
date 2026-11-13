#pragma once

// PhosphorContoursPipeline — sketch-agnostic multi-pass "phosphor contour" treatment: luminance-derived
// isophote bands rendered as a dotted/dashed CRT raster with analogue slip, dropout, persistence and glow.
// Header-only (no build-config change for consumers), same shape as crt::Pipeline. Used by
// sketches/phosphor-contours (standalone) and by experience_runtime's PhosphorContoursScene.
//
// It owns ONLY its transient FBOs/shaders. It never owns media, the runtime scene FBO, or any selection /
// playlist authority. The caller draws ANY source (a full video frame, or a cropped/fragment texture it has
// already composed) into sourceFbo(); the pipeline only ever samples sourceFbo() in normalised uv, so crop
// semantics stay entirely with the caller.
//
//   caller draws source into sourceFbo()  (begin/end)
//   process(params, time, dt, opts)  -> luma -> contour -> raster -> accumulate -> glow -> final
//   output()                         -> ofFbo to present (finalFbo, or sourceFbo if the shaders failed)
//
// All targets are explicit GL_TEXTURE_2D (independent of ofDisableArbTex), 8-bit RGBA (no HDR requirement).
// Desktop GL 2.1 / GLSL 1.20 only has been run. PiSafe is a reduced profile; Pi 3B+ feasibility is
// UNMEASURED.

#include "ofMain.h"
#include "PhosphorContoursParams.h"
#include "PhosphorContoursShaders.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>

namespace phc {

class Pipeline {
public:
	bool loadShaders() {
		const std::string noise = kNoise;
		okLuma = load(shLuma, kLumaFrag, "luma");
		okContour = load(shContour, noise + kStructureFrag, "structure");
		okRaster = load(shRaster, noise + kRasterFrag, "raster");
		okAccum = load(shAccum, kAccumFrag, "accumulate");
		okBright = load(shBright, kBrightFrag, "bright");
		okBlur = load(shBlur, kBlurFrag, "blur");
		okFinal = load(shFinal, noise + kFinalFrag, "final");
		okMono = load(shMono, kMonoFrag, "mono-view");
		okStats = load(shStats, kStatsFrag, "range-stats");
		status_ = std::string("luma:") + (okLuma ? "ok" : "FAIL") + " contour:" + (okContour ? "ok" : "FAIL")
			+ " raster:" + (okRaster ? "ok" : "FAIL") + " accum:" + (okAccum ? "ok" : "FAIL")
			+ " glow:" + (okBright && okBlur ? "ok" : "FAIL") + " final:" + (okFinal ? "ok" : "FAIL");
		return usable();
	}

	// (Re)allocates every target. Call on setup / resize / quality change only, never per frame.
	// Desktop: structural luma 1/4, contour full res, glow 1/4. PiSafe: structural luma 1/8, contour 1/2, glow 1/8, no history.
	void allocate(int w, int h, Quality q = Quality::Desktop) {
		quality_ = q;
		W = std::max(8, w);
		H = std::max(8, h);
		// The structural luma is deliberately far lower-res than the contour target: it is blurred there (cheap) and
		// the bilinear upsample is a free low-pass, which is what makes lines read as flowing bands, not noise.
		lumaDiv_ = q == Quality::Desktop ? 4 : 8;
		const int lumaDiv = lumaDiv_, contourDiv = q == Quality::Desktop ? 1 : 2;
		bloomDiv = q == Quality::Desktop ? 4 : 8;
		allocOne(srcFbo, W, H);
		allocOne(lumaFbo, W / lumaDiv, H / lumaDiv);
		allocOne(structB, W / lumaDiv, H / lumaDiv);
		allocOne(statsFbo, kStatsW, kStatsH);
		fill(statsMesh, kStatsW, kStatsH, false);
		allocOne(contourFbo, W / contourDiv, H / contourDiv);
		allocOne(rasterFbo, W, H);
		allocOne(hist[0], W, H);
		allocOne(hist[1], W, H);
		allocOne(bloomA, W / bloomDiv, H / bloomDiv);
		allocOne(bloomB, W / bloomDiv, H / bloomDiv);
		allocOne(finalFbo, W, H);
		fill(fullMesh, W, H, false);
		fill(lumaMesh, lumaFbo.getWidth(), lumaFbo.getHeight(), true);
		fill(structMesh, lumaFbo.getWidth(), lumaFbo.getHeight(), false);
		fill(contourMesh, contourFbo.getWidth(), contourFbo.getHeight(), true);
		fill(bloomMesh, bloomA.getWidth(), bloomA.getHeight(), false);
		cur = 0;
		clearHistory();
	}
	void release() {
		for (ofFbo* f : { &srcFbo, &lumaFbo, &structB, &contourFbo, &rasterFbo, &hist[0], &hist[1], &bloomA, &bloomB, &finalFbo, &monoFbo, &statsFbo }) f->clear();
	}

	// Test seam: simulate a shader compile/link failure (loadShaders() restores it).
	void failShadersForTesting() { okLuma = okContour = okRaster = okAccum = okFinal = false; }
	bool usable() const { return okLuma && okContour && okRaster && okAccum && okFinal; }
	bool allocated() const { return srcFbo.isAllocated(); }
	int width() const { return W; }
	int height() const { return H; }
	Quality quality() const { return quality_; }
	const std::string& status() const { return status_; }
	size_t gpuBytes() const {
		size_t b = 0;
		for (const ofFbo* f : { &srcFbo, &lumaFbo, &structB, &contourFbo, &rasterFbo, &hist[0], &hist[1], &bloomA, &bloomB, &finalFbo })
			if (f->isAllocated()) b += size_t(f->getWidth()) * size_t(f->getHeight()) * 4;
		return b;
	}
	ofFbo& sourceFbo() { return srcFbo; }
	// Frame to present after process(); the untreated source when the shaders are unusable.
	ofFbo& output() { return outOk ? finalFbo : srcFbo; }
	// Debug/inspection views (read-only use). Contour/Luma may be lower-res under PiSafe.
	ofFbo& view(View v) {
		switch (v) {
		case View::Source: return srcFbo;
		case View::Luma: return lumaFbo;
		case View::Contour: return contourFbo;
		case View::Raster: return rasterFbo;
		default: return output();
		}
	}

	// Diagnostic grey views of the structure target (lazily allocated, never touched by process()):
	// chan 0 = the bare line signal (no phosphor mask / dropout / glow), chan 1 = the structural luma the geometry uses
	// (after tonal range). Both are upright. Not counted in gpuBytes(): inspection only.
	ofFbo& monoView(int chan) {
		if (!okMono || !contourFbo.isAllocated()) return contourFbo;
		if (!monoFbo.isAllocated() || monoFbo.getWidth() != W || monoFbo.getHeight() != H) allocOne(monoFbo, W, H);
		monoFbo.begin();
		shMono.begin();
		shMono.setUniformTexture("tex0", contourFbo.getTexture(), 0);
		shMono.setUniform1f("uChan", chan ? 1.f : 0.f);
		fullMesh.draw();
		shMono.end();
		monoFbo.end();
		return monoFbo;
	}

	// Source change, resize, reactivation, reset/reseed, discontinuity. Safe to call any time.
	void clearHistory() {
		for (auto& h : hist) if (h.isAllocated()) { h.begin(); ofClear(0, 0, 0, 255); h.end(); }
		timeAccum_ = 0.f;
		rangeValid_ = false; // a new clip / reset: re-measure and snap, never fade from the previous clip's exposure
		statsCounter_ = 0;
	}

	// Current tonal range used by the structure pass (auto or manual) - diagnostics/tests.
	float rangeLo() const { return rangeLo_; }
	float rangeHi() const { return rangeHi_; }

	void setPassTiming(bool on) { timing = on; }
	double totalMs() const { return totalMs_; }

	// seed: deterministic reproduction of dropout/jitter/warp phase (same seed + same time + same source
	// => same frame, modulo GPU rounding).
	// Each finishing stage can be switched off independently (diagnostics: bare structure -> add stages back).
	struct Options { uint32_t seed = 1; bool persistence = true; bool glow = true; bool dropout = true; bool slip = true; bool grain = true; };

	void process(const Params& P, float time, float dt, const Options& opt) {
		outOk = false;
		if (!usable() || !allocated()) return;
		dt = std::min(std::max(dt, 0.001f), 0.25f);
		const GLboolean blendWas = glIsEnabled(GL_BLEND);
		ofDisableBlendMode();
		ofDisableAlphaBlending();
		ofSetColor(255);
		if (timing) { glFinish(); t0 = std::chrono::steady_clock::now(); }

		// native-resolution scale: art-directed px values were authored at 720p
		const float k = std::max(0.5f, float(H) / 720.f);
		const float seed = float(opt.seed % 1000u) * 0.137f;
		const bool persist = opt.persistence && quality_ == Quality::Desktop && P.persistenceTau > 0.f;

		// ---- pass 1: anti-aliased structural-luma downsample (flips once; see fill())
		lumaFbo.begin();
		shLuma.begin();
		shLuma.setUniformTexture("tex0", srcFbo.getTexture(), 0);
		shLuma.setUniform2f("uTexel", 1.f / W, 1.f / H);
		shLuma.setUniform1f("uOff", float(lumaDiv_) * 0.25f);
		lumaMesh.draw();
		shLuma.end();
		lumaFbo.end();

		// ---- pass 2: structural blur = two separable gaussian pairs (orientation preserving). Total sigma is
		// P.structBlur px at the 720p reference; each pair contributes sigma/sqrt(2). The 5-tap kernel has
		// sigma ~= 1.64 * step texels.
		if (okBlur && P.structBlur > 0.01f) {
			const float sigmaTex = P.structBlur * k / float(lumaDiv_);
			const float step = std::max(0.25f, sigmaTex * 0.431f);
			const float sw = lumaFbo.getWidth(), sh = lumaFbo.getHeight();
			shBlur.begin();
			for (int pair = 0; pair < 2; pair++) {
				structB.begin();
				shBlur.setUniformTexture("tex0", lumaFbo.getTexture(), 0);
				shBlur.setUniform2f("uDir", step / sw, 0.f);
				structMesh.draw();
				structB.end();
				lumaFbo.begin();
				shBlur.setUniformTexture("tex0", structB.getTexture(), 0);
				shBlur.setUniform2f("uDir", 0.f, step / sh);
				structMesh.draw();
				lumaFbo.end();
			}
			shBlur.end();
		}

		// ---- tonal range: manual, or a slow clip-adaptive range (percentiles of a 16x9 mean-pool, read back every
		// kStatsEvery frames - one tiny readback, never per frame; unmeasured on Pi)
		float lo = P.tonalLo, hi = P.tonalHi;
		if (P.autoRange && okStats) { updateRange(dt); lo = rangeLo_; hi = rangeHi_; }
		else { rangeLo_ = lo; rangeHi_ = hi; rangeValid_ = false; }

		// ---- pass 3: structure (Isophote / RasterRelief / Hybrid) into the contour target
		float wIso = 1.f, wRel = 0.f;
		if (P.algorithm == Algorithm::RasterRelief) { wIso = 0.f; wRel = 1.f; }
		else if (P.algorithm == Algorithm::Hybrid) { wIso = P.hybridIso; wRel = 1.f - P.hybridIso; }
		const float cscale = float(contourFbo.getWidth()) / float(W); // contour-target px per output px
		contourFbo.begin();
		shContour.begin();
		shContour.setUniformTexture("tex0", lumaFbo.getTexture(), 0);
		shContour.setUniformTexture("tex1", srcFbo.getTexture(), 1);
		shContour.setUniform2f("uRes", contourFbo.getWidth(), contourFbo.getHeight());
		shContour.setUniform2f("uTexel", 1.f / lumaFbo.getWidth(), 1.f / lumaFbo.getHeight());
		shContour.setUniform1f("uTime", time); shContour.setUniform1f("uSeed", seed);
		shContour.setUniform1f("uWIso", wIso); shContour.setUniform1f("uWRel", wRel);
		shContour.setUniform1f("uLo", lo); shContour.setUniform1f("uHi", hi);
		shContour.setUniform1f("uContrast", P.contrast); shContour.setUniform1f("uGamma", P.gamma);
		shContour.setUniform1f("uDetail", P.detail);
		shContour.setUniform1f("uLevels", P.levels);
		shContour.setUniform1f("uIsoHalf", 0.5f * P.lineWidth * k * cscale);
		shContour.setUniform1f("uAA", P.softness * k * cscale);
		shContour.setUniform1f("uGradEmph", P.gradientEmphasis); shContour.setUniform1f("uLumaFloor", P.lumaFloor);
		shContour.setUniform1f("uWarpX", P.warpX * k * cscale); shContour.setUniform1f("uWarpY", P.warpY * k * cscale);
		shContour.setUniform1f("uWarpRate", P.warpRate); shContour.setUniform1f("uWarpScale", P.warpScale);
		shContour.setUniform1f("uSpacing", reliefSpacingPx(P.reliefLines, 720.f) * k * cscale);
		shContour.setUniform1f("uAmp", P.reliefAmp * k * cscale);
		shContour.setUniform1f("uPivot", P.reliefPivot);
		shContour.setUniform1f("uRelHalf", 0.5f * P.reliefWidth * k * cscale);
		shContour.setUniform1f("uDrift", P.reliefDrift * k * cscale);
		shContour.setUniform1f("uVert", P.reliefVertical ? 1.f : 0.f);
		contourMesh.draw();
		shContour.end();
		contourFbo.end();

		// ---- pass 3: dot/dash raster (output pixel space)
		rasterFbo.begin();
		shRaster.begin();
		shRaster.setUniformTexture("tex0", contourFbo.getTexture(), 0);
		shRaster.setUniform2f("uRes", W, H);
		shRaster.setUniform1f("uTime", time); shRaster.setUniform1f("uSeed", seed);
		shRaster.setUniform2f("uCell", std::max(1.5f, P.cellW * k), std::max(1.5f, P.cellH * k));
		shRaster.setUniform1f("uDotRadius", P.dotRadius);
		shRaster.setUniform1f("uDropout", opt.dropout ? P.dropout : 0.f); shRaster.setUniform1f("uFlickHz", P.flickerHz);
		shRaster.setUniform1f("uJitter", opt.dropout ? P.jitter : 0.f); shRaster.setUniform1f("uStagger", P.stagger);
		shRaster.setUniform1f("uRowAmp", opt.slip ? P.rowAmp * k : 0.f); shRaster.setUniform1f("uRowRate", P.rowRate);
		shRaster.setUniform1f("uSlip", opt.slip ? P.slip * k : 0.f); shRaster.setUniform1f("uSnap", P.cellSnap); shRaster.setUniform1f("uFill", P.fill); shRaster.setUniform1f("uTone", P.toneFill);
		fullMesh.draw();
		shRaster.end();
		rasterFbo.end();

		// ---- pass 4a: phosphor persistence (time-correct decay)
		const int prev = 1 - cur;
		hist[cur].begin();
		shAccum.begin();
		shAccum.setUniformTexture("tex0", rasterFbo.getTexture(), 0);
		shAccum.setUniformTexture("tex1", hist[prev].getTexture(), 1);
		shAccum.setUniform3f("uTint", P.tint.x, P.tint.y, P.tint.z);
		shAccum.setUniform1f("uDecay", persist ? std::exp(-dt / P.persistenceTau) : 0.f);
		fullMesh.draw();
		shAccum.end();
		hist[cur].end();
		ofFbo& H0 = hist[cur];
		cur = prev;

		// ---- pass 4b: reduced-resolution glow (2 H/V pairs on desktop, 1 on PiSafe)
		const bool doGlow = opt.glow && okBright && okBlur && P.bloomAmount > 0.f;
		if (doGlow) {
			const float bw = bloomA.getWidth(), bh = bloomA.getHeight();
			bloomA.begin();
			shBright.begin();
			shBright.setUniformTexture("tex0", H0.getTexture(), 0);
			shBright.setUniform1f("uThresh", P.bloomThresh);
			bloomMesh.draw();
			shBright.end();
			bloomA.end();
			const int pairs = quality_ == Quality::Desktop ? 2 : 1;
			shBlur.begin();
			for (int i = 0; i < pairs; i++) {
				const float r = P.bloomRadius * (1.f + float(i));
				bloomB.begin();
				shBlur.setUniformTexture("tex0", bloomA.getTexture(), 0);
				shBlur.setUniform2f("uDir", r / bw, 0.f);
				bloomMesh.draw();
				bloomB.end();
				bloomA.begin();
				shBlur.setUniformTexture("tex0", bloomB.getTexture(), 0);
				shBlur.setUniform2f("uDir", 0.f, r / bh);
				bloomMesh.draw();
				bloomA.end();
			}
			shBlur.end();
		}

		// ---- pass 5: final composite
		finalFbo.begin();
		shFinal.begin();
		shFinal.setUniformTexture("tex0", H0.getTexture(), 0);
		shFinal.setUniformTexture("tex1", bloomA.getTexture(), 1);
		shFinal.setUniform2f("uRes", W, H);
		shFinal.setUniform1f("uTime", time);
		shFinal.setUniform3f("uTint", P.tint.x, P.tint.y, P.tint.z);
		shFinal.setUniform3f("uBg", P.background.x, P.background.y, P.background.z);
		shFinal.setUniform1f("uGain", P.gain); shFinal.setUniform1f("uBloom", doGlow ? P.bloomAmount : 0.f);
		shFinal.setUniform1f("uGrain", opt.grain ? P.grain : 0.f); shFinal.setUniform1f("uVignette", P.vignette);
		fullMesh.draw();
		shFinal.end();
		finalFbo.end();
		outOk = true;

		if (timing) {
			glFinish();
			const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			totalMs_ = totalMs_ * 0.9 + ms * 0.1;
		}
		if (blendWas) ofEnableAlphaBlending();
	}

private:
	void updateRange(float dt) {
		sinceRead_ += dt;
		if (rangeValid_ && ++statsCounter_ < kStatsEvery) return;
		statsCounter_ = 0;
		statsFbo.begin();
		shStats.begin();
		shStats.setUniformTexture("tex0", lumaFbo.getTexture(), 0);
		shStats.setUniform2f("uCellUV", 1.f / kStatsW, 1.f / kStatsH);
		statsMesh.draw();
		shStats.end();
		statsFbo.end();
		statsFbo.readToPixels(statsPx);
		std::vector<float> v;
		v.reserve(size_t(kStatsW) * kStatsH);
		const int ch = int(statsPx.getNumChannels());
		for (size_t i = 0; i + size_t(ch) <= statsPx.size(); i += size_t(ch)) v.push_back(float(statsPx[i]) / 255.f);
		if (v.empty()) return;
		std::sort(v.begin(), v.end());
		float lo, hi;
		autoRangeFromPercentiles(percentileSorted(v, 0.03f), percentileSorted(v, 0.97f), lo, hi);
		if (!rangeValid_) { rangeLo_ = lo; rangeHi_ = hi; rangeValid_ = true; }
		else {
			const float a = 1.f - std::exp(-sinceRead_ / 1.0f); // ~1 s time constant, independent of frame rate
			rangeLo_ += (lo - rangeLo_) * a;
			rangeHi_ += (hi - rangeHi_) * a;
		}
		sinceRead_ = 0.f;
	}
	bool load(ofShader& s, const std::string& frag, const char* label) {
		s.unload();
		const bool ok = s.setupShaderFromSource(GL_VERTEX_SHADER, std::string(kHeader) + kVert)
			&& s.setupShaderFromSource(GL_FRAGMENT_SHADER, std::string(kHeader) + frag)
			&& s.linkProgram();
		ofLogNotice("phosphor-contours") << "shader " << label << ": " << (ok ? "OK" : "FAILED");
		return ok;
	}
	static void allocOne(ofFbo& f, int w, int h) {
		ofFboSettings s;
		s.width = std::max(w, 1); s.height = std::max(h, 1);
		s.textureTarget = GL_TEXTURE_2D; // never a rectangle texture: normalized UVs throughout
		s.internalformat = GL_RGBA;      // 8-bit, GLES2-safe
		s.useDepth = false; s.useStencil = false; s.numSamples = 0;
		s.minFilter = GL_LINEAR; s.maxFilter = GL_LINEAR;
		s.wrapModeHorizontal = GL_CLAMP_TO_EDGE; s.wrapModeVertical = GL_CLAMP_TO_EDGE;
		f.allocate(s);
		f.begin(); ofClear(0, 0, 0, 255); f.end();
	}
	// flip=true : texcoord = (x/w, 1 - y/h). Every pass that uses it flips the image vertically once.
	// flip=false: texcoord = (x/w, y/h), orientation-preserving.
	// The OF-drawn source needs EXACTLY two flips on its way to the final target to stay upright, so only the
	// two signal passes (luma, contour) use the flipping mesh; raster, history, glow and final preserve
	// (history in particular must never flip frame to frame).
	static void fill(ofMesh& m, float w, float h, bool flip) {
		m.clear();
		m.setMode(OF_PRIMITIVE_TRIANGLES);
		const float t0 = flip ? 1.f : 0.f, t1 = flip ? 0.f : 1.f;
		m.addVertex({0, 0, 0}); m.addTexCoord({0, t0});
		m.addVertex({w, 0, 0}); m.addTexCoord({1, t0});
		m.addVertex({w, h, 0}); m.addTexCoord({1, t1});
		m.addVertex({0, h, 0}); m.addTexCoord({0, t1});
		m.addIndex(0); m.addIndex(1); m.addIndex(2);
		m.addIndex(0); m.addIndex(2); m.addIndex(3);
	}

	ofShader shLuma, shContour, shRaster, shAccum, shBright, shBlur, shFinal, shMono, shStats;
	bool okLuma = false, okContour = false, okRaster = false, okAccum = false, okBright = false, okBlur = false, okFinal = false, okMono = false, okStats = false;
	ofFbo srcFbo, lumaFbo, structB, contourFbo, rasterFbo, bloomA, bloomB, finalFbo, monoFbo, statsFbo;
	std::array<ofFbo, 2> hist;
	ofMesh statsMesh, fullMesh, lumaMesh, structMesh, contourMesh, bloomMesh;
	int cur = 0, W = 0, H = 0, bloomDiv = 4, lumaDiv_ = 4;
	Quality quality_ = Quality::Desktop;
	bool outOk = false, timing = false;
	static constexpr int kStatsW = 16, kStatsH = 9, kStatsEvery = 8;
	ofPixels statsPx;
	float rangeLo_ = 0.f, rangeHi_ = 1.f, sinceRead_ = 0.f;
	bool rangeValid_ = false;
	int statsCounter_ = 0;
	float timeAccum_ = 0.f;
	double totalMs_ = 0.0;
	std::string status_;
	std::chrono::steady_clock::time_point t0;
};

} // namespace phc
