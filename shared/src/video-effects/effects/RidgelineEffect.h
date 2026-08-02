#pragma once

#include "RidgelineRenderer.h"
#include "VideoEffectInstance.h"

// CpuRenderer wrapper around the existing, already-shared
// shared/src/ridgeline/RidgelineRenderer — used today by both
// blueprint_emergence (BEFragment.cpp) and quadrant-crosshair
// (Quadrant.cpp/DebugMode.cpp), the only promoted effect in this catalog
// with two independent real callers prior to promotion. Does not reimplement
// its CPU polyline construction; owns one instance.
namespace videoeffects {

	class RidgelineEffect : public VideoEffectInstance {
	public:
		bool setup() override;
		void resize(int width, int height) override;
		void render(const VideoEffectContext & context, const VideoEffectParameters & params) override;

	private:
		RidgelineRenderer renderer;
		int setupW = -1;
		int setupH = -1;
	};

} // namespace videoeffects
