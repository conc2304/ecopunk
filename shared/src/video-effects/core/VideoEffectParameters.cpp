#include "VideoEffectParameters.h"

namespace videoeffects {

	namespace {
		template <typename T>
		T getOr(const std::map<std::string, VideoEffectParameterValue> & values, const std::string & id, T fallback) {
			auto it = values.find(id);
			if (it == values.end()) return fallback;
			if (const T * v = std::get_if<T>(&it->second)) return *v;
			return fallback;
		}
	}

	float VideoEffectParameters::getFloat(const std::string & id, float fallback) const {
		return getOr<float>(values, id, fallback);
	}

	int VideoEffectParameters::getInt(const std::string & id, int fallback) const {
		return getOr<int>(values, id, fallback);
	}

	bool VideoEffectParameters::getBool(const std::string & id, bool fallback) const {
		return getOr<bool>(values, id, fallback);
	}

	glm::vec2 VideoEffectParameters::getVec2(const std::string & id, glm::vec2 fallback) const {
		return getOr<glm::vec2>(values, id, fallback);
	}

	glm::vec3 VideoEffectParameters::getVec3(const std::string & id, glm::vec3 fallback) const {
		return getOr<glm::vec3>(values, id, fallback);
	}

	glm::vec4 VideoEffectParameters::getVec4(const std::string & id, glm::vec4 fallback) const {
		return getOr<glm::vec4>(values, id, fallback);
	}

} // namespace videoeffects
