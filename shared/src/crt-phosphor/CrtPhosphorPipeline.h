#pragma once

// CrtPhosphorPipeline — sketch-agnostic multi-pass green-phosphor CRT
// treatment. Header-only on purpose (no build-config change for consumers).
// Used by sketches/crt-phosphor (standalone) and by the experience_runtime
// experimental CrtPhosphorScene. It owns only its own transient FBOs/shaders;
// it never owns media, the runtime scene FBO, or any selection authority.
//
//   caller draws source into sourceFbo()  (begin/end)
//   process(time, dt, opts)  -> base -> bright -> blur H/V -> composite(+history)
//   output()                 -> ofFbo to present (history FBO, or sourceFbo on failure)
//
// All targets are explicit GL_TEXTURE_2D (independent of ofDisableArbTex).
// Desktop GL 2.1 / GLSL 1.20 tested only; GLES2/Pi is UNTESTED.

#include "ofMain.h"
#include "CrtRegistries.h"
#include "CrtShaders.h"

#include "glm/vec3.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>

namespace crt {

struct Params {
	const char* name = "";
	float black = 0.08f, gain = 1.25f, gamma = 0.9f;
	float grillePitch = 3.0f, grilleDepth = 0.55f, grilleSharp = 1.6f, bleed = 0.5f;
	float scanPitch = 3.0f, scanDepth = 0.22f;
	float drift = 0.0006f, band = 0.15f, tear = 0.0f;
	float bloomThresh = 0.5f, bloomAmount = 0.6f;
	float tau = 0.06f; // persistence time constant, seconds
	float noise = 0.05f, streak = 0.15f, flicker = 0.02f, vignette = 0.35f;
};

inline std::array<Params, 3> makePresets() {
	std::array<Params, 3> p;
	// 1. GREEN_PHOSPHOR_REFERENCE (default)
	p[0].name = "GREEN_PHOSPHOR_REFERENCE";
	p[0].gain = 1.5f; p[0].black = 0.06f;
	p[0].grillePitch = 4.0f; p[0].grilleDepth = 0.7f; p[0].grilleSharp = 2.2f; p[0].bleed = 0.4f;
	p[0].bloomThresh = 0.45f; p[0].bloomAmount = 0.7f;
	// 2. CLEAN_MONITOR
	p[1] = p[0];
	p[1].name = "CLEAN_MONITOR";
	p[1].bloomAmount = 0.30f;
	p[1].drift = 0.f; p[1].band = 0.f; p[1].tear = 0.f;
	p[1].noise = 0.f; p[1].streak = 0.f; p[1].flicker = 0.f;
	p[1].tau = 0.03f; p[1].vignette = 0.2f;
	// 3. DAMAGED_SIGNAL
	p[2] = p[0];
	p[2].name = "DAMAGED_SIGNAL";
	p[2].drift = 0.004f; p[2].band = 1.0f; p[2].tear = 0.55f;
	p[2].noise = 0.14f; p[2].streak = 1.0f; p[2].flicker = 0.10f;
	p[2].tau = 0.16f; p[2].bloomAmount = 0.75f;
	return p;
}


// ---------------------------------------------------------------- palette
// Phosphor palette INPUT (sketch-local; not a shared contract). Colors are
// display-referred sRGB-encoded values in 0..1 and are used directly in the
// emission ramp: the pipeline is 8-bit and display-referred end to end (source
// luma is computed on encoded values, as before), so no gamma conversion is
// applied anywhere and nothing is double-encoded.
struct Palette {
	glm::vec3 primary   = {0.10f, 1.00f, 0.34f}; // dominant emission hue
	glm::vec3 peak      = {0.55f, 1.00f, 0.82f}; // bright-peak tint of the primary
	glm::vec3 secondary = {0.55f, 1.00f, 0.82f}; // restrained accent (highlights / mode accents)
	float secondaryInfluence = 0.30f;            // default low
	const char* source = "fallback:green";       // provenance label for the diagnostic overlay
};

inline glm::vec3 hexToRgb(unsigned hex) {
	return { ((hex >> 16) & 255) / 255.f, ((hex >> 8) & 255) / 255.f, (hex & 255) / 255.f };
}

// Emission normalisation: keep the HUE, force V=1 (so a dark theme colour still
// emits) and S>=minSat (so a desaturated one stays coloured). Peak = primary
// lifted 45% toward white. Documented clamp, never a literal very-dark colour.
inline glm::vec3 normalizeEmission(glm::vec3 c, float minSat) {
	float mx = std::max(c.r, std::max(c.g, c.b)), mn = std::min(c.r, std::min(c.g, c.b));
	if (mx <= 1e-4f) return {0.10f, 1.00f, 0.34f}; // black -> green fallback hue
	float sat = (mx - mn) / mx;
	c /= mx; // V = 1
	if (sat < minSat) { // pull channels away from the max to reach minSat, hue kept
		for (int i = 0; i < 3; i++) if (c[i] < 1.f) c[i] = 1.f - (1.f - c[i]) * (minSat / std::max(sat, 1e-3f));
		for (int i = 0; i < 3; i++) c[i] = std::min(std::max(c[i], 0.f), 1.f);
		if (sat < 1e-3f) c = {1.f - minSat, 1.f, 1.f - minSat}; // pure grey: no hue to keep
	}
	return c;
}

inline Palette makePalette(unsigned primaryHex, unsigned secondaryHex, const char* source, float secInf = 0.30f) {
	Palette p;
	p.primary = normalizeEmission(hexToRgb(primaryHex), 0.45f);
	p.peak = p.primary + (glm::vec3(1.f) - p.primary) * 0.45f;
	p.secondary = normalizeEmission(hexToRgb(secondaryHex), 0.30f);
	p.secondaryInfluence = secInf;
	p.source = source;
	return p;
}

// Developer manual palettes (index 0 is the local green fallback).
inline std::array<Palette, 3> manualPalettes() {
	std::array<Palette, 3> a;
	a[0] = Palette(); // green fallback
	a[1] = makePalette(0xFFB000, 0x19D3C5, "manual:amber/teal");
	a[2] = makePalette(0x9B5CFF, 0x3FE0FF, "manual:violet/cyan");
	return a;
}

// Strongly scoped, per-mode developer settings.
struct ExcavationSettings { float scanSeconds = 6.f; float bandWidth = 0.018f; float tau = 1.6f; bool vertical = true; bool reverse = false; };
struct MemorySettings     { float tau = 0.55f; float motionGain = 5.f; float motionTau = 3.f; };
struct ColumnSettings     { float groupPx = 22.f; float amp = 0.035f; float drift = 0.6f; };
struct ModeSettings { ExcavationSettings excavation; MemorySettings memory; ColumnSettings columns; };


struct StructureTuning { float cell, aspect, gap, radius, norm, scan; };
inline StructureTuning defaultTuning(Structure s, bool dense) {
	// Cell sizes are authored at pitch scale 1.0. The sketch DEFAULT is dense + scale 0.5 (the minimum), so
	// the on-screen default pitch = dense cell x 0.5 (triad 5 px, grille 4.5 px, round 5 px, ASCII 6 px).
	switch (s) {
	case Structure::RgbTriad:       return dense ? StructureTuning{ 10.f, 1.4f, 0.45f, 0.23f, 4.6f, 0.5f } : StructureTuning{ 16.f, 1.4f, 0.35f, 0.21f, 4.0f, 0.5f };
	case Structure::ApertureGrille: return dense ? StructureTuning{ 9.f, 1.f, 0.22f, 0.f, 2.3f, 0.5f }    : StructureTuning{ 12.f, 1.f, 0.18f, 0.f, 2.4f, 0.5f };
	case Structure::RoundMatrix:    return dense ? StructureTuning{ 10.f, 1.f, 0.35f, 0.45f, 1.35f, 0.3f } : StructureTuning{ 20.f, 1.f, 0.25f, 0.42f, 1.5f, 0.3f };
	case Structure::AsciiMatrix:    return dense ? StructureTuning{ 12.f, 1.3f, 0.f, 0.f, 3.4f, 0.3f }     : StructureTuning{ 16.f, 1.3f, 0.f, 0.f, 3.2f, 0.3f };
	case Structure::VectorBeam:     return StructureTuning{ 1.f, 1.f, 0.f, 0.f, 1.7f, 0.f };
	default:                        return StructureTuning{ 4.f, 1.f, 0.f, 0.f, 1.f, 1.f };
	}
}

struct DisplaySettings {
	bool dense = true;                     // default: dense tuning
	float cellScale = 0.5f;                // developer pitch multiplier (0.5 = smallest, the default)
	std::string glyphRamp = " .:-=+*#%@";  // dark -> bright (sparser -> denser)
	float beamW = 2.2f, levels = 7.f, edgeT = 0.05f;
	float floatEnergy = 0.55f;             // calm ~0.3 / energized ~0.9
	float fieldSep = 2.5f, fieldHz = 30.f;
	float mosaicT1 = 0.14f, mosaicT2 = 0.07f;
};

struct Display {
	Structure structure = Structure::Original;
	ColorMap cmap = ColorMap::ClassicMono;
	Behavior behavior = Behavior::None;
	int dither = 0;                        // 0 off, 1 low (2x2), 2 high (4x4)
	uint32_t seed = 1;
	DisplaySettings settings;
};

// Palette data (display-referred sRGB). EGA is the literal 16-entry set.
inline std::array<glm::vec3, 16> egaColors() {
	static const unsigned h[16] = { 0x000000,0x0000AA,0x00AA00,0x00AAAA,0xAA0000,0xAA00AA,0xAA5500,0xAAAAAA,
	                                0x555555,0x5555FF,0x55FF55,0x55FFFF,0xFF5555,0xFF55FF,0xFFFF55,0xFFFFFF };
	std::array<glm::vec3, 16> a;
	for (int i = 0; i < 16; i++) a[i] = hexToRgb(h[i]);
	return a;
}
inline std::array<glm::vec3, 5> dualColors() {
	return { hexToRgb(0x126F82), hexToRgb(0x27DCCB), hexToRgb(0xD96748), hexToRgb(0xFFBE7B), hexToRgb(0x05151B) };
}
// Returns ramp stop count (0 if the map is not a ramp map).
inline int rampFor(ColorMap c, std::array<glm::vec3, 8>& out, bool& stepped) {
	static const unsigned sunset[] = { 0x100820,0x382060,0x7735A8,0xC14B9A,0xFC6D64,0xFFB84B,0xFFE5A3 };
	static const unsigned biolum[] = { 0x051811,0x08644E,0x12A88B,0x45D8BD,0xC8EA65,0xF5FFBC };
	static const unsigned thermal[] = { 0x090C33,0x2632A5,0x753BD2,0xE23D67,0xFF883D,0xFFE869 };
	static const unsigned four[]    = { 0x17220C,0x405C26,0x84AD44,0xC9E98E };
	const unsigned* t = nullptr; int n = 0; stepped = false;
	switch (c) {
	case ColorMap::Sunset:   t = sunset;  n = 7; break;
	case ColorMap::Biolum:   t = biolum;  n = 6; break;
	case ColorMap::Thermal:  t = thermal; n = 6; break;
	case ColorMap::FourTone: t = four;    n = 4; stepped = true; break;
	default: return 0;
	}
	for (int i = 0; i < 8; i++) out[i] = i < n ? hexToRgb(t[i]) : glm::vec3(0.f);
	return n;
}

class Pipeline {
public:
	enum Pass { P_SRC, P_MODE, P_BASE, P_BLOOM, P_COMP, P_COUNT };

	struct Options {
		bool bloom = true;
		bool persistence = true;
		Mode mode = Mode::PhosphorPortrait;
		Palette palette;       // default = green fallback
		ModeSettings modes;
		Display display;       // structure / colour mapping / behavior axes
	};

	// Passes the shader dir override (may be empty) and loads/links shaders.
	bool loadShaders(const std::string& overrideDir = std::string()) {
		overrideDir_ = overrideDir;
		okBase = loadOne(shBase, (std::string(kCommon) + kBaseFrag).c_str(), "base");
		struct SD { Structure s; const char* body; const char* label; };
		const SD sds[] = { { Structure::RgbTriad, kStructTriadFrag, "struct_rgb_triad" }, { Structure::ApertureGrille, kStructGrilleFrag, "struct_aperture_grille" },
			{ Structure::RoundMatrix, kStructRoundFrag, "struct_round_matrix" }, { Structure::AsciiMatrix, kStructAsciiFrag, "struct_ascii_matrix" },
			{ Structure::VectorBeam, kStructVectorFrag, "struct_vector_beam" } };
		okStruct[0] = true;
		for (auto& d : sds) okStruct[(int)d.s] = loadOne(shStruct[(int)d.s], (std::string(kCommon) + kStructCommon + d.body).c_str(), d.label);
		okBright = loadOne(shBright, kBrightFrag, "bright");
		okBlur = loadOne(shBlur, kBlurFrag, "blur");
		okComp = loadOne(shComp, kCompositeFrag, "composite");
		okExc = loadOne(shExc, kModeExcavationFrag, "mode_excavation");
		okMem = loadOne(shMem, kModeMemoryFrag, "mode_memory");
		okCol = loadOne(shCol, kModeColumnsFrag, "mode_columns");
		status_ = std::string("base:") + (okBase ? "ok" : "FAIL") + " bright:" + (okBright ? "ok" : "FAIL")
			+ " blur:" + (okBlur ? "ok" : "FAIL") + " comp:" + (okComp ? "ok" : "FAIL")
			+ " modes:" + (okExc && okMem && okCol ? "ok" : "FAIL")
			+ " structures:" + (okStruct[1] && okStruct[2] && okStruct[3] && okStruct[4] && okStruct[5] ? "ok" : "FAIL");
		return usable();
	}

	// (Re)allocates every target. Call on setup / resize / quality change only,
	// never per frame. quality 0 = full, 1 = low (half-res, bloom/8, no persistence).
	void allocate(int w, int h, int quality = 0) {
		quality_ = quality;
		W = std::max(2, w);
		H = std::max(2, h);
		bloomDiv = quality == 0 ? 4 : 8;
		allocOne(srcFbo, W, H);
		allocOne(baseFbo, W, H);
		allocOne(bloomA, W / bloomDiv, H / bloomDiv);
		allocOne(bloomB, W / bloomDiv, H / bloomDiv);
		allocOne(hist[0], W, H);
		allocOne(hist[1], W, H);
		buildQuad(W, H);
		cur = 0;
		clearHistory();
	}

	bool usable() const { return okBase && okComp; }
	bool allocated() const { return srcFbo.isAllocated(); }
	int width() const { return W; }
	int height() const { return H; }
	int bloomWidth() const { return (int)bloomA.getWidth(); }
	int bloomHeight() const { return (int)bloomA.getHeight(); }
	const std::string& status() const { return status_; }
	size_t gpuBytes() const {
		size_t b = 0;
		for (const ofFbo* f : { &srcFbo, &baseFbo, &bloomA, &bloomB, &hist[0], &hist[1], &mh[0], &mh[1], &modeOut, &atlas })
			if (f->isAllocated()) b += size_t(f->getWidth()) * size_t(f->getHeight()) * 4;
		return b;
	}

	ofFbo& sourceFbo() { return srcFbo; }
	// The frame to present after process(); sourceFbo when treatment is unavailable.
	ofFbo& output() { return outIsHist ? hist[1 - cur] : srcFbo; }

	// Explicitly clear persistence history (setup, resize, source change,
	// discontinuity, reactivation, restart). Safe to call any time.
	void clearHistory() {
		for (auto& h : hist) { h.begin(); ofClear(0, 0, 0, 255); h.end(); }
		clearModeHistory();
	}
	// Mode-local history only (mode change / re-entry). Lazy resources that were
	// never allocated are left alone.
	void clearModeHistory() {
		for (auto& h : mh) if (h.isAllocated()) { h.begin(); ofClear(0, 0, 0, 0); h.end(); }
		if (modeOut.isAllocated()) { modeOut.begin(); ofClear(0, 0, 0, 0); modeOut.end(); }
	}

	void setPassTiming(bool on) { timing = on; }
	double passMs(Pass p) const { return passMs_[p]; }

	// Runs base/bloom/composite from sourceFbo into the history ping-pong.
	// Saves/restores blend state; leaves FBO bindings as found.
	void process(const Params& P, float time, float dt, const Options& opt) {
		outIsHist = false;
		if (!usable() || !allocated()) return;
		dt = std::min(std::max(dt, 0.001f), 0.25f);

		const GLboolean blendWas = glIsEnabled(GL_BLEND);
		ofDisableBlendMode();
		ofDisableAlphaBlending();
		ofSetColor(255);

		// ---- mode stage (image/signal construction) -> signal frame
		if (opt.mode != lastMode) { clearModeHistory(); lastMode = opt.mode; } // re-entry never exposes stale history
		ofFbo* signal = &srcFbo;
		float accentMode = 0.f;
		beginPass();
		switch (opt.mode) {
		case Mode::RasterExcavation:  if (okExc) { signal = runExcavation(opt.modes.excavation, time, dt); accentMode = 1.f; } break;
		case Mode::PhosphorMemory:    if (okMem) signal = runMemory(opt.modes.memory, dt); break;
		case Mode::SignalColumns:     if (okCol) signal = runColumns(opt.modes.columns, time); break;
		case Mode::PhosphorPortrait:
		default: break; // planned modes fall through to the faithful portrait path
		}
		endPass(P_MODE);

		if (opt.display.structure != lastStructCfg_ || opt.display.behavior != lastBehCfg_) {
			lastStructCfg_ = opt.display.structure; lastBehCfg_ = opt.display.behavior;
			clearHistory(); // never carry another structure's/behavior's ghosts
		}
		// ---- display stage: structure program (+ colour mapper + behavior) -> baseFbo
		const Display& D = opt.display;
		Structure st = D.structure;
		if (st != Structure::Original && !okStruct[(int)st]) st = Structure::Original; // failed program: baseline
		Behavior bh = D.behavior;
		if (behaviorBlockReason(st, bh) != nullptr) bh = Behavior::None;           // never an alternate look
		if (st == Structure::AsciiMatrix) ensureAtlas(D.settings.glyphRamp);
		ofShader& sh = (st == Structure::Original) ? shBase : shStruct[(int)st];

		beginPass();
		baseFbo.begin();
		sh.begin();
		sh.setUniformTexture("tex0", signal->getTexture(), 0);
		sh.setUniform2f("uRes", W, H);
		sh.setUniform1f("uTime", time);
		sh.setUniform1f("uBlack", P.black); sh.setUniform1f("uGain", P.gain); sh.setUniform1f("uGamma", P.gamma);
		sh.setUniform1f("uScanPitch", P.scanPitch); sh.setUniform1f("uScanDepth", P.scanDepth);
		sh.setUniform1f("uDrift", P.drift); sh.setUniform1f("uBand", P.band); sh.setUniform1f("uTear", P.tear);
		sh.setUniform3f("uPrimary", opt.palette.primary.x, opt.palette.primary.y, opt.palette.primary.z);
		sh.setUniform3f("uPeak", opt.palette.peak.x, opt.palette.peak.y, opt.palette.peak.z);
		sh.setUniform3f("uSecondary", opt.palette.secondary.x, opt.palette.secondary.y, opt.palette.secondary.z);
		sh.setUniform1f("uSecInf", opt.palette.secondaryInfluence);
		sh.setUniform1f("uAccentMode", accentMode);
		// colour mapping axis
		{
			int palMode = 0; int n = 0; bool stepped = false;
			std::array<glm::vec3, 8> ramp{};
			switch (D.cmap) {
			case ColorMap::SourceRgb: palMode = 1; break;
			case ColorMap::Ega16: palMode = 3; break;
			case ColorMap::DualPhosphor: palMode = 4; break;
			case ColorMap::Sunset: case ColorMap::Biolum: case ColorMap::Thermal: case ColorMap::FourTone:
				n = rampFor(D.cmap, ramp, stepped); palMode = 2; break;
			default: palMode = 0; break; // classic mono / theme use the primary/secondary emission path
			}
			sh.setUniform1i("uPalMode", palMode);
			sh.setUniform1i("uDither", D.dither);
			if (palMode == 2) {
				sh.setUniform3fv("uRamp", &ramp[0].x, 8);
				sh.setUniform1f("uRampN", (float)n);
				sh.setUniform1f("uStepped", stepped ? 1.f : 0.f);
			} else if (palMode == 3) {
				auto ega = egaColors();
				sh.setUniform3fv("uEga", &ega[0].x, 16);
			} else if (palMode == 4) {
				auto dual = dualColors();
				sh.setUniform3fv("uDual", &dual[0].x, 5);
			}
			if (palMode != 2) { sh.setUniform1f("uRampN", 2.f); sh.setUniform1f("uStepped", 0.f); }
		}
		// behavior axis
		sh.setUniform1i("uBehavior", (int)bh);
		sh.setUniform1f("uBhA", D.settings.floatEnergy);
		sh.setUniform1f("uBhB", D.settings.fieldSep);
		sh.setUniform1f("uFieldHz", D.settings.fieldHz);
		sh.setUniform1f("uT1", D.settings.mosaicT1); sh.setUniform1f("uT2", D.settings.mosaicT2);
		sh.setUniform1f("uSeed", (float)(D.seed % 1000u));
		if (st == Structure::Original) {
			sh.setUniform1f("uGrillePitch", P.grillePitch); sh.setUniform1f("uGrilleDepth", P.grilleDepth);
			sh.setUniform1f("uGrilleSharp", P.grilleSharp); sh.setUniform1f("uBleed", P.bleed);
		} else {
			StructureTuning t = defaultTuning(st, D.settings.dense);
			sh.setUniform1f("uCell", std::max(2.f, t.cell * D.settings.cellScale));
			sh.setUniform1f("uCellAspect", t.aspect);
			sh.setUniform1f("uGap", t.gap); sh.setUniform1f("uRadius", t.radius);
			sh.setUniform1f("uNorm", t.norm); sh.setUniform1f("uScan", t.scan);
			if (st == Structure::AsciiMatrix) {
				const float cw = std::max(4.f, t.cell * D.settings.cellScale);
				sh.setUniformTexture("uAtlas", atlas.getTexture(), 3);
				sh.setUniform1f("uGlyphN", (float)std::max<size_t>(2, atlasRamp.size()));
				sh.setUniform2f("uCellSize", cw, cw * t.aspect);
			}
			if (st == Structure::VectorBeam) {
				sh.setUniform1f("uBeamW", D.settings.beamW);
				sh.setUniform1f("uLevels", D.settings.levels);
				sh.setUniform1f("uEdgeT", D.settings.edgeT);
			}
		}
		fullMesh.draw();
		sh.end();
		baseFbo.end();
		lastStructure_ = st; lastBehavior_ = bh;
		endPass(P_BASE);

		const bool doBloom = opt.bloom && okBright && okBlur; // graceful fallback if optional pass failed
		if (doBloom) {
			beginPass();
			const float bw = bloomA.getWidth(), bh = bloomA.getHeight();
			bloomA.begin();
			shBright.begin();
			shBright.setUniformTexture("tex0", baseFbo.getTexture(), 0);
			shBright.setUniform2f("uTexel", 1.f / W, 1.f / H);
			shBright.setUniform1f("uThresh", P.bloomThresh);
			bloomMesh.draw();
			shBright.end();
			bloomA.end();
			shBlur.begin();
			bloomB.begin();
			shBlur.setUniformTexture("tex0", bloomA.getTexture(), 0);
			shBlur.setUniform2f("uDir", 1.f / bw, 0.f);
			bloomMesh.draw();
			bloomB.end();
			bloomA.begin();
			shBlur.setUniformTexture("tex0", bloomB.getTexture(), 0);
			shBlur.setUniform2f("uDir", 0.f, 1.f / bh);
			bloomMesh.draw();
			bloomA.end();
			shBlur.end();
			endPass(P_BLOOM);
		}

		beginPass();
		// vector_beam gets a bounded phosphor trail (tau floor) so beams linger briefly.
		const float tauEff = lastStructure_ == Structure::VectorBeam ? std::max(P.tau, 0.10f) : P.tau;
		const bool persist = opt.persistence && quality_ == 0 && tauEff > 0.f;
		const float decay = persist ? std::exp(-dt / tauEff) : 0.f;
		const int latest = 1 - cur; // previous output; a DIFFERENT texture from the target
		hist[cur].begin();
		shComp.begin();
		shComp.setUniformTexture("tex0", baseFbo.getTexture(), 0);
		shComp.setUniformTexture("tex1", bloomA.getTexture(), 1);
		shComp.setUniformTexture("tex2", hist[latest].getTexture(), 2);
		shComp.setUniform2f("uRes", W, H);
		shComp.setUniform1f("uTime", time);
		shComp.setUniform1f("uBloom", doBloom ? P.bloomAmount : 0.f);
		shComp.setUniform1f("uDecay", decay);
		shComp.setUniform1f("uNoise", P.noise); shComp.setUniform1f("uStreak", P.streak);
		shComp.setUniform1f("uFlicker", P.flicker); shComp.setUniform1f("uVignette", P.vignette);
		fullMesh.draw();
		shComp.end();
		hist[cur].end();
		endPass(P_COMP);
		cur = latest; // output() == hist[1 - cur] == the one just written
		outIsHist = true;

		if (blendWas) ofEnableAlphaBlending();
	}

	// ---- mode routines (each separate; resources lazy, single active mode)
	void ensureModeHist() {
		if (!mh[0].isAllocated()) { allocOne(mh[0], W, H); allocOne(mh[1], W, H); mhCur = 0; clearModeHistory(); }
	}
	void ensureModeOut() {
		if (!modeOut.isAllocated()) allocOne(modeOut, W, H);
	}
	ofFbo* runExcavation(const ExcavationSettings& e, float time, float dt) {
		ensureModeHist();
		excPhase += dt / std::max(e.scanSeconds, 0.1f); // dt-driven, never frame-count
		excPhase -= std::floor(excPhase);
		const int prev = 1 - mhCur;
		mh[mhCur].begin();
		shExc.begin();
		shExc.setUniformTexture("tex0", srcFbo.getTexture(), 0);
		shExc.setUniformTexture("tex1", mh[prev].getTexture(), 1);
		shExc.setUniform1f("uPhase", excPhase);
		shExc.setUniform1f("uBandW", e.bandWidth);
		shExc.setUniform1f("uDecay", std::exp(-dt / std::max(e.tau, 0.01f)));
		shExc.setUniform1f("uVertical", e.vertical ? 1.f : 0.f);
		shExc.setUniform1f("uReverse", e.reverse ? 1.f : 0.f);
		preserveMesh.draw();
		shExc.end();
		mh[mhCur].end();
		ofFbo* out = &mh[mhCur];
		mhCur = prev;
		return out;
	}
	ofFbo* runMemory(const MemorySettings& m, float dt) {
		ensureModeHist();
		const int prev = 1 - mhCur;
		mh[mhCur].begin();
		shMem.begin();
		shMem.setUniformTexture("tex0", srcFbo.getTexture(), 0);
		shMem.setUniformTexture("tex1", mh[prev].getTexture(), 1);
		shMem.setUniform1f("uDt", dt);
		shMem.setUniform1f("uTau", m.tau);
		shMem.setUniform1f("uMotionGain", m.motionGain);
		shMem.setUniform1f("uMotionTau", m.motionTau);
		preserveMesh.draw();
		shMem.end();
		mh[mhCur].end();
		ofFbo* out = &mh[mhCur];
		mhCur = prev;
		return out;
	}
	ofFbo* runColumns(const ColumnSettings& c, float time) {
		ensureModeOut();
		modeOut.begin();
		shCol.begin();
		shCol.setUniformTexture("tex0", srcFbo.getTexture(), 0);
		shCol.setUniform1f("uGroupPx", c.groupPx);
		shCol.setUniform1f("uAmp", c.amp);
		shCol.setUniform1f("uDrift", c.drift);
		shCol.setUniform1f("uTime", time);
		preserveMesh.draw();
		shCol.end();
		modeOut.end();
		return &modeOut;
	}

	// ASCII glyph atlas: built at selection/ramp-change time only (never per frame).
	void ensureAtlas(const std::string& ramp) {
		std::string r = ramp.size() >= 2 ? ramp : std::string(" .:-=+*#%@");
		if (atlas.isAllocated() && r == atlasRamp) return;
		atlasRamp = r;
		const int cw = 16, ch = 26;
		ofFboSettings fs;
		fs.width = cw * (int)r.size(); fs.height = ch;
		fs.textureTarget = GL_TEXTURE_2D; fs.internalformat = GL_RGBA;
		fs.useDepth = false; fs.useStencil = false; fs.numSamples = 0;
		fs.minFilter = GL_LINEAR; fs.maxFilter = GL_LINEAR;
		fs.wrapModeHorizontal = GL_CLAMP_TO_EDGE; fs.wrapModeVertical = GL_CLAMP_TO_EDGE;
		atlas.allocate(fs);
		const GLboolean blendWas = glIsEnabled(GL_BLEND);
		atlas.begin();
		ofClear(0, 0, 0, 255);
		ofEnableAlphaBlending();
		ofSetColor(255);
		for (size_t i = 0; i < r.size(); i++) {
			ofPushMatrix();
			ofTranslate(float(i) * cw, 0.f);
			ofScale(2.f, 2.f);
			ofDrawBitmapString(std::string(1, r[i]), 0.f, 10.f);
			ofPopMatrix();
		}
		atlas.end();
		if (!blendWas) ofDisableAlphaBlending();
	}
	Structure lastStructure() const { return lastStructure_; }
	Behavior lastBehavior() const { return lastBehavior_; }

private:
	bool loadOne(ofShader& s, const char* frag, const char* label) {
		std::string fsrc = frag;
		if (!overrideDir_.empty()) {
			std::string f = ofBufferFromFile(overrideDir_ + "/" + label + ".frag").getText();
			if (!f.empty()) { fsrc = f; ofLogNotice("crt") << "shader override: " << label; }
		}
		s.unload();
		bool ok = s.setupShaderFromSource(GL_VERTEX_SHADER, std::string(kHeader) + kVert)
			&& s.setupShaderFromSource(GL_FRAGMENT_SHADER, std::string(kHeader) + fsrc)
			&& s.linkProgram();
		ofLogNotice("crt") << "shader " << label << ": " << (ok ? "OK" : "FAILED");
		return ok;
	}

	static void allocOne(ofFbo& f, int w, int h) {
		ofFboSettings s;
		s.width = std::max(w, 1); s.height = std::max(h, 1);
		s.textureTarget = GL_TEXTURE_2D; // never a rectangle texture: normalized UVs throughout
		s.internalformat = GL_RGBA;      // 8-bit: Pi/GLES2-safe
		s.useDepth = false; s.useStencil = false; s.numSamples = 0;
		s.minFilter = GL_LINEAR; s.maxFilter = GL_LINEAR;
		s.wrapModeHorizontal = GL_CLAMP_TO_EDGE; s.wrapModeVertical = GL_CLAMP_TO_EDGE;
		f.allocate(s);
		f.begin(); ofClear(0, 0, 0, 255); f.end();
	}

	static void fill(ofMesh& m, float w, float h) {
		m.clear();
		m.setMode(OF_PRIMITIVE_TRIANGLES);
		// texcoord = (x/w, 1 - y/h): see CrtShaders.h
		m.addVertex({0, 0, 0}); m.addTexCoord({0, 1});
		m.addVertex({w, 0, 0}); m.addTexCoord({1, 1});
		m.addVertex({w, h, 0}); m.addTexCoord({1, 0});
		m.addVertex({0, h, 0}); m.addTexCoord({0, 0});
		m.addIndex(0); m.addIndex(1); m.addIndex(2);
		m.addIndex(0); m.addIndex(2); m.addIndex(3);
	}
	void buildQuad(float w, float h) {
		// orientation-PRESERVING quad for mode passes (flip-mesh flips once per pass)
		preserveMesh.clear();
		preserveMesh.setMode(OF_PRIMITIVE_TRIANGLES);
		preserveMesh.addVertex({0, 0, 0}); preserveMesh.addTexCoord({0, 0});
		preserveMesh.addVertex({w, 0, 0}); preserveMesh.addTexCoord({1, 0});
		preserveMesh.addVertex({w, h, 0}); preserveMesh.addTexCoord({1, 1});
		preserveMesh.addVertex({0, h, 0}); preserveMesh.addTexCoord({0, 1});
		preserveMesh.addIndex(0); preserveMesh.addIndex(1); preserveMesh.addIndex(2);
		preserveMesh.addIndex(0); preserveMesh.addIndex(2); preserveMesh.addIndex(3);
		fill(fullMesh, w, h);
		fill(bloomMesh, std::max(1, int(w) / bloomDiv), std::max(1, int(h) / bloomDiv));
	}

	void beginPass() { if (timing) { glFinish(); t0 = std::chrono::steady_clock::now(); } }
	void endPass(Pass p) {
		if (!timing) return;
		glFinish();
		double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		passMs_[p] = passMs_[p] * 0.9 + ms * 0.1;
	}

	ofShader shBase, shBright, shBlur, shComp, shExc, shMem, shCol;
	std::array<ofShader, 6> shStruct; // index = Structure (Original unused: shBase)
	std::array<bool, 6> okStruct{};
	ofFbo atlas; std::string atlasRamp; // ASCII glyph atlas (built on ramp change only)
	bool okBase = false, okBright = false, okBlur = false, okComp = false, okExc = false, okMem = false, okCol = false;
	ofFbo mh[2], modeOut; // lazily allocated mode-local resources
	int mhCur = 0;
	Structure lastStructCfg_ = Structure::Original;
	Behavior lastBehCfg_ = Behavior::None;
	Structure lastStructure_ = Structure::Original;
	Behavior lastBehavior_ = Behavior::None;
	Mode lastMode = Mode::PhosphorPortrait;
	float excPhase = 0.f;
	ofMesh preserveMesh;
	ofFbo srcFbo, baseFbo, bloomA, bloomB;
	std::array<ofFbo, 2> hist;
	ofMesh fullMesh, bloomMesh;
	int cur = 0, W = 0, H = 0, bloomDiv = 4, quality_ = 0;
	bool outIsHist = false, timing = false;
	std::string status_, overrideDir_;
	std::array<double, P_COUNT> passMs_{};
	std::chrono::steady_clock::time_point t0;
};

} // namespace crt
