#pragma once

#include "VideoEffectInstance.h"
#include "ofShader.h"

// Generic VideoEffectInstance for every VideoEffectKind::SinglePassShader
// definition — one ofShader, uniforms bound generically from the
// definition's VideoEffectParameterSchema list plus the VideoEffectContext's
// source texture/alpha. This is the extraction target
// docs/shader-effect-system-probe.md §10 identifies as already
// independently hand-copied four times (VideoRegionEffectRenderer,
// BEFragment, TFEffectPicker, Quadrant/DebugMode) — a single generic binder
// replaces all four's per-effect `if (name == "...")` cascades for the
// ShaderContract::A effects pool.
//
// Only handles ShaderContract::A (tex/vTexCoord, no PLATFORM_PI guard) —
// see docs/shader-effect-system-probe.md §4. Contract B (radar-effects-gallery)
// is explicitly out of scope for this consolidation per
// docs/shared-video-effect-architecture.md's own Contract-A-only boundary.
namespace videoeffects {

	class SinglePassShaderEffect : public VideoEffectInstance {
	public:
		bool setup() override;
		void render(const VideoEffectContext & context, const VideoEffectParameters & params) override;

	private:
		ofShader shader;
	};

} // namespace videoeffects
