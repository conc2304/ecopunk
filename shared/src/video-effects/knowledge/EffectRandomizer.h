#pragma once

#include "EffectKnowledgeBase.h"
#include "VideoEffectDefinition.h"
#include "VideoEffectParameters.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>

// Shared random-state generation pipeline, per
// docs/shader-effect-system-probe.md §9's recommended stages (validated
// against QuadrantManager::chooseDitherParams — the one piece of real
// reject/retry prior art in the repo) and
// docs/shared-video-effect-architecture.md §10's evolution/drift
// requirements ("only parameters marked safeToAnimate may evolve" applies
// to callers of the *evolution* controller, not this class — this class
// randomizes any `randomizable` parameter regardless of animate-safety).
//
// Scope: Float and Int-typed parameters are fully randomized. Bool-typed
// parameters get a weighted coin flip. Vec2/Vec3/Vec4-typed parameters are
// left at their default — no current effect definition declares a
// vector-typed randomizable parameter (colors are expressed as separate
// float uniforms throughout this codebase's existing dispatch code, e.g.
// recolor's tint via three float params), so vector randomization is
// intentionally unimplemented rather than speculatively built.
namespace videoeffects {

	struct RandomizeRequest {
		std::string effectId;
		std::optional<uint32_t> seed; // set = deterministic for this call only; never reseeds the global ofRandom() stream
		float whitelistBiasProbability = 0.3f;
		int maxRetries = 5; // mirrors QuadrantManager::chooseDitherParams's bounded-retry cap

		// Sketch-specific narrowing of a parameter's artistic range, e.g. from
		// effect-manifest.json's parameterOverrides. Values outside the
		// schema's hard limits are clamped back into them, never expanded
		// beyond hard limits.
		std::map<std::string, std::pair<float, float>> overrideRanges;
	};

	class EffectRandomizer {
	public:
		// knowledgeBase may be nullptr — falls back to pure artistic-range
		// sampling with no whitelist bias / blacklist avoidance.
		VideoEffectParameters generate(
			const RandomizeRequest & request, const VideoEffectDefinition & definition, const EffectKnowledgeBase * knowledgeBase) const;
	};

} // namespace videoeffects
