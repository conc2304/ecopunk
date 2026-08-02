#pragma once

#include "VideoEffectDefinition.h"
#include "VideoEffectInstance.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// The canonical registry: one VideoEffectDefinition + one factory per id.
// docs/shared-video-effect-architecture.md §6: "One canonical ID and
// parameter schema per effect. Duplicate registration fails loudly." — "no
// migrated sketch should maintain a separate hard-coded canonical effect
// registry."
namespace videoeffects {

	using VideoEffectFactory = std::function<std::unique_ptr<VideoEffectInstance>()>;

	class VideoEffectRegistry {
	public:
		// Registers a definition + factory under definition.id. Logs an error
		// via ofLogError and returns false (does not throw) if that id is
		// already registered — matches this project's existing
		// ShaderLibrary::load() error-handling convention rather than
		// introducing exceptions this codebase doesn't otherwise use.
		bool registerEffect(VideoEffectDefinition definition, VideoEffectFactory factory);

		bool has(const std::string & id) const;
		const VideoEffectDefinition * getDefinition(const std::string & id) const;

		// Constructs a fresh instance via the registered factory and wires its
		// getDefinition() up to point at the stored VideoEffectDefinition. Does
		// NOT call setup() — callers (normally VideoEffectService::createInstance())
		// are responsible for that.
		std::unique_ptr<VideoEffectInstance> create(const std::string & id) const;

		std::vector<std::string> allIds() const;

	private:
		struct Entry {
			VideoEffectDefinition definition;
			VideoEffectFactory factory;
		};
		// std::map (not unordered_map/vector) specifically for pointer/reference
		// stability of Entry::definition across further registerEffect() calls —
		// VideoEffectInstance::definition points directly into this map's
		// storage for the instance's whole lifetime.
		std::map<std::string, Entry> entries;
	};

} // namespace videoeffects
