#pragma once

#include "EffectKnowledgeBase.h"
#include "EffectRandomizer.h"
#include "VideoEffectDefinition.h"
#include "VideoEffectParameters.h"

// Scene evolution scoped to ONE effect's parameter set — per
// docs/shader-effect-system-probe.md §6, modeled on temporal-fields'
// TFPresetTimeline (state+phase+easing, the richest such system found in
// the repo) but deliberately narrowed to just effect-parameter transitions,
// not TFPresetTimeline's full scene/pattern/composition scope (probe open
// question 4). Float-typed parameters interpolate with the configured
// easing curve; Int/Bool-typed parameters switch discretely at the
// transition's midpoint rather than lerping (an enum has no meaningful
// halfway point).
namespace videoeffects {

	enum class EvolutionPhase {
		Transitioning,
		Holding
	};

	enum class EvolutionEasing {
		Linear,
		Smoothstep,
		Smootherstep,
		EaseInOutSine
	};

	float applyEvolutionEasing(EvolutionEasing easing, float t01);

	class EffectEvolutionController {
	public:
		struct Config {
			float transitionDuration = 3.0f;
			float holdDuration = 6.0f;
			EvolutionEasing easing = EvolutionEasing::Smoothstep;
		};

		void setup(const VideoEffectDefinition & definitionRef, const VideoEffectParameters & initial);
		void setConfig(const Config & c) { config = c; }
		const Config & getConfig() const { return config; }

		// randomizer/knowledgeBase may be nullptr — with no randomizer, target
		// is just left equal to current (no-op evolution, phase stays Holding
		// forever) rather than crashing.
		void update(float dt, const EffectRandomizer * randomizer, const EffectKnowledgeBase * knowledgeBase);

		// Re-scopes to a different effect's parameter set — called on effect
		// switch. Resets rather than attempts to migrate state across effects,
		// since even same-named parameters (e.g. "alpha") mean different
		// things per effect (docs/shader-effect-system-probe.md §5).
		void reset(const VideoEffectDefinition & definitionRef, const VideoEffectParameters & initial);

		const VideoEffectParameters & getCurrent() const { return currentParams; }
		EvolutionPhase getPhase() const { return phase; }
		float getProgress01() const;

	private:
		const VideoEffectDefinition * definition = nullptr;
		VideoEffectParameters startParams;
		VideoEffectParameters targetParams;
		VideoEffectParameters currentParams;
		EvolutionPhase phase = EvolutionPhase::Holding;
		float elapsed = 0.0f;
		Config config;

		void beginTransition(const EffectRandomizer * randomizer, const EffectKnowledgeBase * knowledgeBase);
	};

} // namespace videoeffects
