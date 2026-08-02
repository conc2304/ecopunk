#pragma once

#include "VideoEffectInstance.h"
#include "ofFbo.h"
#include "ofShader.h"

// Two unrelated effects that happen to share a family name — see
// docs/video-effect-promotion-inventory.md §3. Registered as distinct
// canonical ids (erosion_accumulation, erosion_history_blend), not merged.
namespace videoeffects {

	// quadrant-crosshair's variant (data/shaders/erosion.glsl): accumulated/videoFrame/decayRate/videoAlpha,
	// no desaturation stage, no first-frame priming.
	class ErosionAccumulationEffect : public VideoEffectInstance {
	public:
		bool setup() override;
		void reset() override;
		void resize(int width, int height) override;
		void render(const VideoEffectContext & context, const VideoEffectParameters & params) override;

	private:
		ofShader shader;
		ofFbo fboA, fboB;
		bool pingPong = false;
		int width = 0, height = 0;

		ofFbo & readFbo() { return pingPong ? fboB : fboA; }
		ofFbo & writeFbo() { return pingPong ? fboA : fboB; }
	};

	// blob-region-prototype/blueprint_emergence's variant
	// (shared/src/shaders/erosion.frag, previously driven by shared/src/ErosionFBO):
	// history/current/decayRate/currentAlpha/desatAmount, with first-frame priming.
	// This implementation FIXES the currentAlpha-never-bound bug identified in
	// docs/shader-effect-system-probe.md §7 and confirmed again in
	// docs/video-effect-promotion-inventory.md §2 — shared/src/ErosionFBO.cpp
	// itself is left untouched (its own consumers are migrated separately, not
	// silently changed by this promotion).
	class ErosionHistoryBlendEffect : public VideoEffectInstance {
	public:
		bool setup() override;
		void reset() override;
		void resize(int width, int height) override;
		void render(const VideoEffectContext & context, const VideoEffectParameters & params) override;

	private:
		ofShader shader;
		ofFbo fboA, fboB;
		bool pingPong = false;
		bool primed = false;
		int width = 0, height = 0;

		ofFbo & readFbo() { return pingPong ? fboB : fboA; }
		ofFbo & writeFbo() { return pingPong ? fboA : fboB; }
	};

} // namespace videoeffects
