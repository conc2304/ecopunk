#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include <string>
#include <variant>

// Shared vocabulary for the video-effect consolidation described in
// docs/shared-video-effect-architecture.md. See that doc for the full
// rationale — this header only defines the plain-data types every other
// header in shared/src/video-effects/ builds on.
namespace videoeffects {

	// A video effect is not always "one shader" — see architecture doc §1.1/§3.1.
	enum class VideoEffectKind {
		SinglePassShader,
		MultiPassShader,
		TemporalShader,
		Processor,
		Composite,
		CpuRenderer,
		Simulation
	};

	const char * toString(VideoEffectKind kind);

	// Which texcoord/sampler/vertex-shader convention an effect's shader
	// assets expect. Contract A/B per the prior probe (docs/shader-effect-system-probe.md
	// §4) — do not assume these are interchangeable.
	enum class ShaderContract {
		None, // no single-pass shader contract applies (CpuRenderer, custom multi-pass, etc.)
		A,    // tex / vTexCoord, no PLATFORM_PI guard — the effects-pool convention
		B     // videoTex+maskTex / texCoordVarying, PLATFORM_PI-aware — radar-effects-gallery's convention
	};

	const char * toString(ShaderContract contract);

	enum class VideoEffectParameterType {
		Float,
		Int,
		Bool,
		Vec2,
		Vec3,
		Vec4
	};

	// Do not assign universal meaning to parameter names or types across
	// effects — the probe found "alpha" alone means at least three different
	// things depending on which shader is bound. Every VideoEffectParameterSchema
	// entry (VideoEffectParameterSchema.h) is scoped to one effect.
	using VideoEffectParameterValue = std::variant<float, int, bool, glm::vec2, glm::vec3, glm::vec4>;

	VideoEffectParameterType typeOf(const VideoEffectParameterValue & value);

	// Loose numeric coercion across the Float/Int/Bool variant alternatives —
	// used anywhere a schema's defaultValue/hardMin/hardMax/etc. needs to be
	// read as a plain number regardless of which alternative it was
	// constructed with. Returns `fallback` for Vec2/Vec3/Vec4 (unimplemented
	// for randomization/generic-binding purposes, see EffectRandomizer.h).
	float asFloat(const VideoEffectParameterValue & value, float fallback = 0.0f);
	int asInt(const VideoEffectParameterValue & value, int fallback = 0);
	bool asBool(const VideoEffectParameterValue & value, bool fallback = false);
	glm::vec2 asVec2(const VideoEffectParameterValue & value, glm::vec2 fallback = glm::vec2(0.0f));
	glm::vec3 asVec3(const VideoEffectParameterValue & value, glm::vec3 fallback = glm::vec3(0.0f));
	glm::vec4 asVec4(const VideoEffectParameterValue & value, glm::vec4 fallback = glm::vec4(0.0f));

} // namespace videoeffects
