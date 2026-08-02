#include "VideoEffectTypes.h"

namespace videoeffects {

	const char * toString(VideoEffectKind kind) {
		switch (kind) {
			case VideoEffectKind::SinglePassShader: return "SinglePassShader";
			case VideoEffectKind::MultiPassShader: return "MultiPassShader";
			case VideoEffectKind::TemporalShader: return "TemporalShader";
			case VideoEffectKind::Processor: return "Processor";
			case VideoEffectKind::Composite: return "Composite";
			case VideoEffectKind::CpuRenderer: return "CpuRenderer";
			case VideoEffectKind::Simulation: return "Simulation";
		}
		return "Unknown";
	}

	const char * toString(ShaderContract contract) {
		switch (contract) {
			case ShaderContract::None: return "None";
			case ShaderContract::A: return "A";
			case ShaderContract::B: return "B";
		}
		return "Unknown";
	}

	VideoEffectParameterType typeOf(const VideoEffectParameterValue & value) {
		switch (value.index()) {
			case 0: return VideoEffectParameterType::Float;
			case 1: return VideoEffectParameterType::Int;
			case 2: return VideoEffectParameterType::Bool;
			case 3: return VideoEffectParameterType::Vec2;
			case 4: return VideoEffectParameterType::Vec3;
			default: return VideoEffectParameterType::Vec4;
		}
	}

	float asFloat(const VideoEffectParameterValue & value, float fallback) {
		if (const float * f = std::get_if<float>(&value)) return *f;
		if (const int * i = std::get_if<int>(&value)) return static_cast<float>(*i);
		if (const bool * b = std::get_if<bool>(&value)) return *b ? 1.0f : 0.0f;
		return fallback;
	}

	int asInt(const VideoEffectParameterValue & value, int fallback) {
		if (const int * i = std::get_if<int>(&value)) return *i;
		if (const float * f = std::get_if<float>(&value)) return static_cast<int>(*f);
		if (const bool * b = std::get_if<bool>(&value)) return *b ? 1 : 0;
		return fallback;
	}

	bool asBool(const VideoEffectParameterValue & value, bool fallback) {
		if (const bool * b = std::get_if<bool>(&value)) return *b;
		if (const float * f = std::get_if<float>(&value)) return *f != 0.0f;
		if (const int * i = std::get_if<int>(&value)) return *i != 0;
		return fallback;
	}

	glm::vec2 asVec2(const VideoEffectParameterValue & value, glm::vec2 fallback) {
		if (const glm::vec2 * v = std::get_if<glm::vec2>(&value)) return *v;
		return fallback;
	}

	glm::vec3 asVec3(const VideoEffectParameterValue & value, glm::vec3 fallback) {
		if (const glm::vec3 * v = std::get_if<glm::vec3>(&value)) return *v;
		return fallback;
	}

	glm::vec4 asVec4(const VideoEffectParameterValue & value, glm::vec4 fallback) {
		if (const glm::vec4 * v = std::get_if<glm::vec4>(&value)) return *v;
		return fallback;
	}

} // namespace videoeffects
