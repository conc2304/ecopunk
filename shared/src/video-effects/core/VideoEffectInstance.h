#pragma once

#include "VideoEffectContext.h"
#include "VideoEffectParameters.h"

namespace videoeffects {

	struct VideoEffectDefinition;
	class VideoEffectRegistry;

	// Base class every promoted effect implementation derives from
	// (SinglePassShaderEffect, MotionExtractionEffect, ErosionEffect, ...).
	// Owns its own GPU/CPU state (shaders, FBOs, history) and is created
	// fresh per logical use via VideoEffectService::createInstance() —
	// sketches keep one instance per "slot" that needs this effect (one per
	// quadrant, one per fragment, one for a fullscreen overlay, ...),
	// matching how ShaderLibrary/MotionExtraction/etc. are owned today
	// (plain members, not singletons) — see architecture doc §13's ownership
	// boundary.
	class VideoEffectInstance {
	public:
		virtual ~VideoEffectInstance() = default;

		// Loads shaders / allocates FBOs / seeds initial state. Called once by
		// VideoEffectService::createInstance() right after construction, by
		// which point getDefinition() is already valid.
		virtual bool setup() = 0;

		// Clears persistent state (history buffers, ping-pong FBOs) back to a
		// fresh-start condition without reallocating. Effects with
		// capabilities.requiresPersistentState should implement this
		// meaningfully; the base no-op is correct for stateless effects.
		virtual void reset() {}

		// Reallocates any resolution-dependent resources. No-op for effects
		// whose FBOs are sized independently of the canvas (e.g. ReactionDiffusion's
		// fixed low-res simulation grid).
		virtual void resize(int width, int height) {
			(void)width;
			(void)height;
		}

		// Advances internal state that isn't tied to a specific render() call
		// (e.g. MotionExtractionEffect's per-frame accumulation pass, which
		// other effects' render() calls may read from as an auxiliary
		// texture). Most single-pass effects leave this as a no-op.
		virtual void update(const VideoEffectContext & context, const VideoEffectParameters & params) {
			(void)context;
			(void)params;
		}

		virtual void render(const VideoEffectContext & context, const VideoEffectParameters & params) = 0;

		virtual bool isReady() const { return ready; }
		const VideoEffectDefinition * getDefinition() const { return definition; }

	protected:
		friend class VideoEffectRegistry;
		const VideoEffectDefinition * definition = nullptr;
		bool ready = false;
	};

} // namespace videoeffects
