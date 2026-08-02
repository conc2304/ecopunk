#pragma once

#include "VideoEffectInstance.h"
#include "ofShader.h"

// Composite effect consuming a motion_extraction instance's output textures
// (context.auxiliaryTextures["motionTex"]/["motionDelayedTex"]) — see
// docs/video-effect-promotion-inventory.md §2's motion_extraction ->
// motion_composite pipeline note. Does not own or auto-create a
// MotionExtractionEffect itself; the caller (a sketch, or the debugger) is
// responsible for running one and supplying its textures each frame — kept
// this way rather than an implicit internal instance so a sketch that
// already runs its own motion_extraction for other purposes doesn't end up
// running the (non-trivial: 3-pass, persistent-state) extraction twice.
namespace videoeffects {

	class MotionCompositeEffect : public VideoEffectInstance {
	public:
		bool setup() override;
		void render(const VideoEffectContext & context, const VideoEffectParameters & params) override;

	private:
		ofShader shader;
	};

} // namespace videoeffects
