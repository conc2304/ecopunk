#pragma once

#include "VideoEffectInstance.h"
#include "MotionExtraction.h"

// Processor wrapper around the existing, already-shared shared/src/MotionExtraction
// (probe: "reusable-after-decoupling, fully self-contained") — see
// docs/video-effect-promotion-inventory.md §2. Does not reimplement its
// accumulation/extraction passes; owns one instance and exposes its outputs.
//
// KNOWN WRINKLE: shared/src/MotionExtraction::setup() hardcodes its own
// shader load paths — "shaders/effects/vert.glsl" (note the effects/
// prefix, matching BRP/BE/TF's convention) paired with flat
// "shaders/motion_accum.glsl" / "shaders/motion_extract.glsl" (no prefix) —
// rather than going through VideoEffectAssetRegistry. This predates this
// consolidation and existing direct consumers of shared/src/MotionExtraction
// (blueprint_emergence, temporal-fields, via shared/src) depend on that
// exact path contract, so it is intentionally left unmodified here rather
// than risk changing behavior for sketches not yet migrated. NOTE this is
// shared/src/MotionExtraction's OWN convention, not quadrant-crosshair's
// local fork (sketches/quadrant-crosshair/src/MotionExtraction.cpp), which
// uses a different, flat "shaders/vert.glsl" — the two forks have already
// drifted on this exact point (see docs/video-effect-promotion-inventory.md §2).
// This definition's assetPaths is therefore empty (see registerMotionExtraction())
// — any consuming sketch must additionally carry plain copies of those three
// files at the literal paths shared/src/MotionExtraction.cpp expects (see
// sketches/shader-effect-debugger/bin/data/shaders/ for the reference
// example). Fixing MotionExtraction.cpp to accept an injected path resolver
// is flagged as follow-up work, not done as a side effect of this promotion.
namespace videoeffects {

	class MotionExtractionEffect : public VideoEffectInstance {
	public:
		bool setup() override;
		void resize(int width, int height) override;
		void update(const VideoEffectContext & context, const VideoEffectParameters & params) override;
		void render(const VideoEffectContext & context, const VideoEffectParameters & params) override;

		ofTexture & getMotionTexture() { return motionExtraction.getMotionTexture(); }
		ofTexture & getDelayedMotionTexture() { return motionExtraction.getDelayedMotionTexture(); }
		ofTexture & getAccumTexture() { return motionExtraction.getAccumTexture(); }
		float getMotionEnergy() const { return motionExtraction.getMotionEnergy(); }

	private:
		MotionExtraction motionExtraction;
	};

} // namespace videoeffects
