#pragma once

#include "VideoEffectInstance.h"
#include "ofFbo.h"
#include "ofShader.h"

// Promotion of quadrant-crosshair/src/ReactionDiffusion.{h,cpp} — confirmed
// during the Phase 0 inventory to have ZERO current callers anywhere in
// quadrant-crosshair (the class exists, rd_step is registered in its
// ShaderLibrary and reachable via DebugMode's manual cycling, but
// ReactionDiffusion is never instantiated by ofApp/Quadrant/QuadrantManager).
// This class becomes the canonical implementation; the QC-local class is
// left in place (still unused) until quadrant-crosshair's own migration
// phase removes it. Does not consume a video source at all — it's a
// self-contained cellular-automaton simulation, unlike every other
// promoted effect.
namespace videoeffects {

	class ReactionDiffusionEffect : public VideoEffectInstance {
	public:
		static constexpr int GRID_W = 160;
		static constexpr int GRID_H = 90;

		bool setup() override;
		void reset() override;
		void render(const VideoEffectContext & context, const VideoEffectParameters & params) override;

	private:
		ofFbo fboA, fboB;
		bool pingPong = false;
		ofShader shader;

		void seedInitialState();
	};

} // namespace videoeffects
