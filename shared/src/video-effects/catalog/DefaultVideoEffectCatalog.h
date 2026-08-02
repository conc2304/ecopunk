#pragma once

#include "VideoEffectRegistry.h"

// Single source of truth for every canonical video-effect id, per
// docs/shared-video-effect-architecture.md §6. Populates `registry` with
// every promoted effect's VideoEffectDefinition + factory. No migrated
// sketch should maintain a separate hard-coded canonical effect registry —
// this function is the only place new effects get added.
namespace videoeffects {

	void registerDefaultVideoEffects(VideoEffectRegistry & registry);

	// Split out per-family for readability; called by registerDefaultVideoEffects().
	// Exposed individually so Phase 2 (single-pass only) and later phases can be
	// validated independently if needed.
	void registerSinglePassEffects(VideoEffectRegistry & registry);
	void registerMotionExtraction(VideoEffectRegistry & registry);
	void registerMotionComposite(VideoEffectRegistry & registry);
	void registerErosionEffects(VideoEffectRegistry & registry);
	void registerRidgeline(VideoEffectRegistry & registry);
	void registerTemporalTrails(VideoEffectRegistry & registry);
	void registerReactionDiffusion(VideoEffectRegistry & registry);

} // namespace videoeffects
