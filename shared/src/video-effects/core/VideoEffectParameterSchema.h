#pragma once

#include "VideoEffectTypes.h"
#include <string>

// One parameter's shape/bounds/metadata, scoped to a single effect — see
// docs/shared-video-effect-architecture.md §3.3. Deliberately does not
// reuse a parameter's name/type across effects; every VideoEffectDefinition
// (VideoEffectDefinition.h) owns its own list of these.
namespace videoeffects {

	struct VideoEffectParameterSchema {
		std::string id; // uniform name this binds to
		std::string label; // human-readable, for GUI
		VideoEffectParameterType type = VideoEffectParameterType::Float;

		VideoEffectParameterValue defaultValue = 0.0f;
		VideoEffectParameterValue hardMin = 0.0f;
		VideoEffectParameterValue hardMax = 1.0f;
		VideoEffectParameterValue artisticMin = 0.0f;
		VideoEffectParameterValue artisticMax = 1.0f;

		float step = 0.01f;

		bool visibleInDebugger = true;
		bool safeToAnimate = true;
		bool randomizable = true;
		bool performanceSensitive = false;

		// Empty = no dependency. Non-empty = this parameter is only meaningful
		// when `dependsOnParam` is set to something other than its own default —
		// e.g. dither.glsl's maxPixelation only matters once alpha (the
		// clean/dither/pixelate arc position) is away from its clean-image ends.
		std::string dependsOnParam;
	};

} // namespace videoeffects
