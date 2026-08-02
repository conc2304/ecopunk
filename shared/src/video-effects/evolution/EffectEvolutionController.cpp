#include "EffectEvolutionController.h"
#include <algorithm>
#include <cmath>

namespace videoeffects {

	namespace {
		constexpr float kPi = 3.14159265358979323846f;
	}

	float applyEvolutionEasing(EvolutionEasing easing, float t01) {
		float t = std::clamp(t01, 0.0f, 1.0f);
		switch (easing) {
			case EvolutionEasing::Linear:
				return t;
			case EvolutionEasing::Smoothstep:
				return t * t * (3.0f - 2.0f * t);
			case EvolutionEasing::Smootherstep:
				return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
			case EvolutionEasing::EaseInOutSine:
				return -(std::cos(kPi * t) - 1.0f) / 2.0f;
		}
		return t;
	}

	void EffectEvolutionController::setup(const VideoEffectDefinition & definitionRef, const VideoEffectParameters & initial) {
		reset(definitionRef, initial);
	}

	void EffectEvolutionController::reset(const VideoEffectDefinition & definitionRef, const VideoEffectParameters & initial) {
		definition = &definitionRef;
		currentParams = initial;
		startParams = initial;
		targetParams = initial;
		phase = EvolutionPhase::Holding;
		elapsed = 0.0f;
	}

	void EffectEvolutionController::beginTransition(const EffectRandomizer * randomizer, const EffectKnowledgeBase * knowledgeBase) {
		startParams = currentParams;
		if (randomizer != nullptr && definition != nullptr) {
			RandomizeRequest req;
			req.effectId = definition->id;
			targetParams = randomizer->generate(req, *definition, knowledgeBase);
		} else {
			targetParams = currentParams; // no-op evolution without a randomizer
		}
		phase = EvolutionPhase::Transitioning;
		elapsed = 0.0f;
	}

	void EffectEvolutionController::update(float dt, const EffectRandomizer * randomizer, const EffectKnowledgeBase * knowledgeBase) {
		if (definition == nullptr) return;

		elapsed += dt;

		if (phase == EvolutionPhase::Holding) {
			if (elapsed >= config.holdDuration) {
				beginTransition(randomizer, knowledgeBase);
			}
			return;
		}

		// Transitioning
		float t = config.transitionDuration > 0.0f ? elapsed / config.transitionDuration : 1.0f;
		bool arrived = t >= 1.0f;
		float eased = applyEvolutionEasing(config.easing, t);

		for (const auto & param : definition->params) {
			switch (param.type) {
				case VideoEffectParameterType::Float: {
					float a = startParams.getFloat(param.id, asFloat(param.defaultValue));
					float b = targetParams.getFloat(param.id, asFloat(param.defaultValue));
					float lo = asFloat(param.hardMin, a);
					float hi = asFloat(param.hardMax, b);
					float v = a + (b - a) * eased;
					currentParams.set(param.id, std::clamp(v, std::min(lo, hi), std::max(lo, hi)));
					break;
				}
				case VideoEffectParameterType::Int: {
					int a = startParams.getInt(param.id, asInt(param.defaultValue));
					int b = targetParams.getInt(param.id, asInt(param.defaultValue));
					currentParams.set(param.id, eased >= 0.5f ? b : a); // discrete switch, not lerped
					break;
				}
				case VideoEffectParameterType::Bool: {
					bool a = startParams.getBool(param.id, asBool(param.defaultValue));
					bool b = targetParams.getBool(param.id, asBool(param.defaultValue));
					currentParams.set(param.id, eased >= 0.5f ? b : a);
					break;
				}
				default:
					currentParams.set(param.id, param.defaultValue); // Vec2/3/4 — not interpolated in v1
					break;
			}
		}

		if (arrived) {
			currentParams = targetParams; // snap exactly, avoid float drift at the boundary
			phase = EvolutionPhase::Holding;
			elapsed = 0.0f;
		}
	}

	float EffectEvolutionController::getProgress01() const {
		if (phase == EvolutionPhase::Holding || config.transitionDuration <= 0.0f) return 1.0f;
		return std::clamp(elapsed / config.transitionDuration, 0.0f, 1.0f);
	}

} // namespace videoeffects
