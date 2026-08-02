#include "PatternDriftController.h"
#include "ofMath.h"
#include <algorithm>
#include <functional>

namespace videoeffects {

	void PatternDriftController::setup(const VideoEffectDefinition & definitionRef, const PatternDriftConfig & driftConfig) {
		definition = &definitionRef;
		config = driftConfig;
		noiseTime = 0.0f;
	}

	void PatternDriftController::update(float dt, bool transitionActive) {
		if (!config.enabled) return;
		if (config.pausesDuringTransition && transitionActive) return;
		noiseTime += dt * config.speed;
	}

	bool PatternDriftController::paramAllowed(const VideoEffectParameterSchema & param) const {
		if (param.type != VideoEffectParameterType::Float) return false;
		if (!param.safeToAnimate) return false;
		if (config.allowedParams.empty()) return true;
		return std::find(config.allowedParams.begin(), config.allowedParams.end(), param.id) != config.allowedParams.end();
	}

	VideoEffectParameters PatternDriftController::apply(const VideoEffectParameters & base) const {
		VideoEffectParameters result = base;
		if (!config.enabled || definition == nullptr) return result;

		for (const auto & param : definition->params) {
			if (!paramAllowed(param)) continue;

			float lo = asFloat(param.hardMin, 0.0f);
			float hi = asFloat(param.hardMax, 1.0f);
			float range = hi - lo;
			if (range <= 0.0f) continue;

			// Per-parameter phase offset (from its id, optionally salted by a
			// configured seed for reproducible drift patterns) so multiple
			// drifting parameters don't move in visible lockstep — same
			// reasoning TFPatternParticleField uses per-particle noise offsets.
			std::size_t h = std::hash<std::string>{}(param.id) ^ static_cast<std::size_t>(config.seed.value_or(0));
			float phase = static_cast<float>(h % 1000) * 0.01f;

			float n = ofNoise(phase, noiseTime); // 0..1
			float offset = (n - 0.5f) * 2.0f * config.strength * range;

			float baseValue = base.getFloat(param.id, asFloat(param.defaultValue));
			result.set(param.id, std::clamp(baseValue + offset, lo, hi));
		}
		return result;
	}

} // namespace videoeffects
