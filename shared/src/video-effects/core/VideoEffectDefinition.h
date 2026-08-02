#pragma once

#include "VideoEffectCapabilities.h"
#include "VideoEffectParameterSchema.h"
#include "VideoEffectTypes.h"
#include <string>
#include <vector>

// Static description of one canonical effect — id, kind, contract, asset
// paths, uniforms, parameter schemas, capabilities. This is the "single
// source of truth" object docs/shared-video-effect-architecture.md §6
// describes; DefaultVideoEffectCatalog.cpp is where every known effect's
// VideoEffectDefinition is actually populated.
namespace videoeffects {

	struct VideoEffectDefinition {
		std::string id; // canonical ID, e.g. "heatmap_recolor"
		std::string displayName;
		VideoEffectKind kind = VideoEffectKind::SinglePassShader;
		ShaderContract contract = ShaderContract::A;

		// Canonical-relative asset paths, relative to shared/assets/video-effects/
		// (e.g. {"common/vert.glsl", "single-pass/heatmap_recolor.glsl"}).
		// Empty for effects with no shader asset (e.g. ridgeline). Order matters
		// for shader-pair effects: vertex path first, fragment path second,
		// matching ofShader::load(vertPath, fragPath)'s argument order.
		std::vector<std::string> assetPaths;

		std::vector<std::string> requiredUniforms;
		std::vector<VideoEffectParameterSchema> params;
		VideoEffectCapabilities capabilities;

		bool debuggerAvailable = true;

		const VideoEffectParameterSchema * findParam(const std::string & paramId) const {
			for (const auto & p : params) {
				if (p.id == paramId) return &p;
			}
			return nullptr;
		}
	};

} // namespace videoeffects
