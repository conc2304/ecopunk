#pragma once

// Common interface for the self-contained Blueprint visual grammars that sit
// beside the classic composition and the editorial collage (STRATA, CONTACT
// SHEET, EXPLODED VIEW, SECTION CUT). Each is seeded, dt-driven, draws with
// the shared HUD graphics primitives and samples the one canonical video
// texture. BlueprintSceneCore owns them and pins the composition to
// GHOST_LAYERS (spawning suspended) while one is active.
//
// Shared shader effects: like the classic Blueprint fragments, every video
// element in these grammars can carry a cycling effect (silence -> fade in ->
// dwell -> fade out). Effects run through the canonical shared path —
// shared/src/VideoRegionEffectRenderer (ShaderLibrary + the video-effects
// catalog's fixed defaults) — so no per-effect uniform code lives here.

#include "ShaderLibrary.h"
#include "VideoRegionEffectRenderer.h"
#include "ofRectangle.h"
#include "ofTexture.h"

#include <array>
#include <cstdint>
#include <random>
#include <string>

struct GrammarStats {
	int total = 0;
	int visible = 0;
	int left = 0;
	int right = 0;
};

// One cycling effect on one video element.
struct BEFxSlot {
	enum class Stage { Silence, FadeIn, Dwell, FadeOut };
	Stage stage = Stage::Silence;
	std::string name;
	float alpha = 0.0f; // current fade, 0..1
	float acc = 0.0f;
	float silence = 2.0f; // seconds before the next effect starts
	float fade = 1.0f;
	float dwell = 4.0f;

	bool active() const { return !name.empty() && alpha > 0.01f; }

	// Curated, tasteful, fixed-default catalog effects (no per-instance state).
	static const char * pick(std::mt19937 & rng, const ShaderLibrary * lib) {
		static const char * const kPool[] = { "scanlines", "edge_glow", "ink_outlines", "chromatic_aberration", "solarize",
			"bioluminescence", "heatmap_recolor", "pixel_drift", "water_refraction", "desaturate" };
		constexpr int N = sizeof(kPool) / sizeof(kPool[0]);
		for (int attempt = 0; attempt < 6; attempt++) {
			const char * n = kPool[std::uniform_int_distribution<int>(0, N - 1)(rng)];
			if (lib != nullptr && lib->has(n)) return n;
		}
		return nullptr;
	}

	void update(float dt, std::mt19937 & rng, const ShaderLibrary * lib, bool mayStart, float silLo, float silHi) {
		auto rnd = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
		acc += dt;
		switch (stage) {
		case Stage::Silence:
			if (acc >= silence && mayStart) {
				const char * n = pick(rng, lib);
				if (n == nullptr) {
					acc = 0.0f;
					silence = rnd(silLo, silHi);
					break;
				}
				name = n;
				fade = rnd(0.8f, 1.6f);
				dwell = rnd(3.0f, 7.0f);
				stage = Stage::FadeIn;
				acc = 0.0f;
			}
			break;
		case Stage::FadeIn:
			alpha = std::min(1.0f, acc / fade);
			if (acc >= fade) {
				stage = Stage::Dwell;
				acc = 0.0f;
			}
			break;
		case Stage::Dwell:
			alpha = 1.0f;
			if (acc >= dwell) {
				stage = Stage::FadeOut;
				acc = 0.0f;
			}
			break;
		case Stage::FadeOut:
			alpha = std::max(0.0f, 1.0f - acc / fade);
			if (acc >= fade) {
				stage = Stage::Silence;
				name.clear();
				alpha = 0.0f;
				acc = 0.0f;
				silence = rnd(silLo, silHi);
			}
			break;
		}
	}
};

// Steps a set of slots, never letting more than `maxActive` run at once.
template <size_t N>
void stepFxSlots(std::array<BEFxSlot, N> & slots, int used, float dt, std::mt19937 & rng, const ShaderLibrary * lib, int maxActive,
	float silLo = 3.0f, float silHi = 9.0f) {
	int active = 0;
	for (int i = 0; i < used && i < static_cast<int>(N); i++)
		if (slots[i].stage != BEFxSlot::Stage::Silence) active++;
	for (int i = 0; i < used && i < static_cast<int>(N); i++) {
		const bool wasSilent = slots[i].stage == BEFxSlot::Stage::Silence;
		slots[i].update(dt, rng, lib, active < maxActive, silLo, silHi);
		if (wasSilent && slots[i].stage != BEFxSlot::Stage::Silence) active++;
	}
}

class BEGrammar {
public:
	virtual ~BEGrammar() = default;
	// First call builds; later calls recompose (palette cross-fade, new layout).
	virtual void beginLayout(uint32_t seed) = 0;
	virtual void reset() = 0;
	virtual void update(float dt) = 0;
	virtual void draw(const ofTexture * video) const = 0;
	virtual const char * displayName() const = 0;
	virtual GrammarStats stats() const = 0;

	// The canonical ShaderLibrary (owned by BlueprintSceneCore). Null = no effects.
	void setShaderLibrary(ShaderLibrary * lib) { shaderLib_ = lib; }

protected:
	// Draws `slot`'s effect over the source crop `srcPx` (pixel space of `video`)
	// into `dest`, via the shared region-effect renderer. No-op when idle.
	void drawEffectOverlay(const ofTexture * video, const ofRectangle & srcPx, const ofRectangle & dest, const BEFxSlot & slot, float alpha) const {
		if (!slot.active() || shaderLib_ == nullptr || video == nullptr || !video->isAllocated()) return;
		if (dest.width < 2.0f || dest.height < 2.0f || srcPx.width < 1.0f || srcPx.height < 1.0f) return;
		VideoRegionEffectRenderer::RenderRequest req;
		req.sourceTexture = video;
		req.sourceCropPixels = srcPx;
		req.destinationBounds = dest;
		req.alpha = alpha * slot.alpha;
		req.effectName = slot.name;
		req.effectAmount = 1.0f;
		fxRenderer_.render(req, *shaderLib_);
	}

	ShaderLibrary * shaderLib_ = nullptr;
	mutable VideoRegionEffectRenderer fxRenderer_;
};
