#pragma once

// Declares what an effect needs and provides, per docs/shared-video-effect-architecture.md §3.2.
// Drives debugger UI, FBO allocation, effect eligibility, automatic
// selection, and Raspberry Pi filtering — kept as plain data so all of those
// consumers can read it without depending on any specific effect implementation.
namespace videoeffects {

	struct VideoEffectCapabilities {
		bool requiresSourceTexture = true;
		bool requiresPreviousFrame = false;
		bool requiresHistoryBuffer = false;
		bool requiresMotionTexture = false;
		bool requiresDelayedMotionTexture = false;
		bool requiresMaskTexture = false;
		bool requiresCpuPixels = false;
		bool producesAuxiliaryTexture = false;
		bool requiresPersistentState = false;
		bool supportsAlphaMix = true;
		bool safeForAutomaticSelection = true;
		int passCount = 1;
	};

} // namespace videoeffects
