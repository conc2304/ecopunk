#pragma once

#include "VideoEffectTypes.h"
#include <map>
#include <string>

// Name-keyed runtime parameter *values* for one effect instance — distinct
// from VideoEffectParameterSchema (VideoEffectParameterSchema.h), which
// describes a parameter's shape/bounds, not its current value. A missing key
// means "use the schema default," mirroring how GLSL itself zero-inits an
// unbound uniform (see heatmap_recolor.glsl's own safe-fallback comments) —
// callers should still prefer supplying every schema-declared param
// explicitly rather than relying on that fallback.
namespace videoeffects {

	class VideoEffectParameters {
	public:
		void set(const std::string & id, VideoEffectParameterValue value) { values[id] = value; }
		bool has(const std::string & id) const { return values.count(id) > 0; }

		float getFloat(const std::string & id, float fallback = 0.0f) const;
		int getInt(const std::string & id, int fallback = 0) const;
		bool getBool(const std::string & id, bool fallback = false) const;
		glm::vec2 getVec2(const std::string & id, glm::vec2 fallback = glm::vec2(0.0f)) const;
		glm::vec3 getVec3(const std::string & id, glm::vec3 fallback = glm::vec3(0.0f)) const;
		glm::vec4 getVec4(const std::string & id, glm::vec4 fallback = glm::vec4(0.0f)) const;

		const std::map<std::string, VideoEffectParameterValue> & all() const { return values; }

	private:
		std::map<std::string, VideoEffectParameterValue> values;
	};

} // namespace videoeffects
