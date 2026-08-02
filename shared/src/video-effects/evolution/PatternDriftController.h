#pragma once

#include "VideoEffectDefinition.h"
#include "VideoEffectParameters.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Low-amplitude continuous drift, distinct from EffectEvolutionController's
// discrete current->target transitions — per docs/shader-effect-system-probe.md
// §6, modeled on temporal-fields' TFPatternParticleField (the only
// implementation in the repo with genuine continuous drift math, vs. the
// several IDLE/FADE_IN/ACTIVE/FADE_OUT state machines which are *transitions*,
// not drift). Generalizes its noise-driven-selection approach from "which
// texture" to "which parameter value": each allowed parameter gets an
// independent ofNoise() phase so multiple drifting parameters don't move in
// visible lockstep, same reasoning TFPatternParticleField uses per-particle.
namespace videoeffects {

	struct PatternDriftConfig {
		bool enabled = false;
		float strength = 0.1f; // fraction of the parameter's artistic range
		float speed = 0.2f; // noise-time advance per second
		std::vector<std::string> allowedParams; // empty = every safeToAnimate Float param
		std::optional<uint32_t> seed;
		bool pausesDuringTransition = true;
	};

	class PatternDriftController {
	public:
		void setup(const VideoEffectDefinition & definitionRef, const PatternDriftConfig & driftConfig);
		void setConfig(const PatternDriftConfig & c) { config = c; }
		const PatternDriftConfig & getConfig() const { return config; }

		// `base` is the value to drift around — normally
		// EffectEvolutionController::getCurrent() when evolution is active, or
		// the effect's live parameter values otherwise. `transitionActive`
		// lets the caller report EffectEvolutionController's phase without
		// this class depending on that class directly.
		void update(float dt, bool transitionActive);

		VideoEffectParameters apply(const VideoEffectParameters & base) const;

	private:
		const VideoEffectDefinition * definition = nullptr;
		PatternDriftConfig config;
		float noiseTime = 0.0f;

		bool paramAllowed(const VideoEffectParameterSchema & param) const;
	};

} // namespace videoeffects
