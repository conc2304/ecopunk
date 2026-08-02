#pragma once

#include "VideoEffectInstance.h"
#include "ofFbo.h"
#include "ofShader.h"

// Genuine extraction, not a wrap — prior to this class, temporal_trails'
// ping-pong FBO orchestration lived entirely inline in
// quadrant-crosshair/src/DebugMode.cpp:314-345 with no dedicated class
// (DebugMode.cpp:332's own comment: "temporal_trails needs its own
// ping-pong draw path"). See docs/video-effect-promotion-inventory.md §2.
// Currently has zero production callers (excluded from QuadrantManager
// per its own comment: "needs per-quadrant FBO") — one instance per
// consumer (one per quadrant, one per fragment, ...) is exactly what
// resolves that, since each VideoEffectInstance owns its own FBO pair.
namespace videoeffects {

	class TemporalTrailsEffect : public VideoEffectInstance {
	public:
		bool setup() override;
		void reset() override;
		void resize(int width, int height) override;
		void render(const VideoEffectContext & context, const VideoEffectParameters & params) override;

	private:
		ofShader shader;
		ofFbo prevFbo, outFbo;
		int width = 0, height = 0;
	};

} // namespace videoeffects
