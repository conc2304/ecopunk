#include "VideoEffectRegistry.h"
#include "ofLog.h"

namespace videoeffects {

	bool VideoEffectRegistry::registerEffect(VideoEffectDefinition definition, VideoEffectFactory factory) {
		if (entries.count(definition.id) > 0) {
			ofLogError("VideoEffectRegistry") << "duplicate registration ignored: " << definition.id;
			return false;
		}
		std::string id = definition.id;
		Entry entry;
		entry.definition = std::move(definition);
		entry.factory = std::move(factory);
		entries.emplace(std::move(id), std::move(entry));
		return true;
	}

	bool VideoEffectRegistry::has(const std::string & id) const {
		return entries.count(id) > 0;
	}

	const VideoEffectDefinition * VideoEffectRegistry::getDefinition(const std::string & id) const {
		auto it = entries.find(id);
		return it == entries.end() ? nullptr : &it->second.definition;
	}

	std::unique_ptr<VideoEffectInstance> VideoEffectRegistry::create(const std::string & id) const {
		auto it = entries.find(id);
		if (it == entries.end()) {
			ofLogError("VideoEffectRegistry") << "create() requested unknown effect id: " << id;
			return nullptr;
		}
		auto instance = it->second.factory();
		if (instance) {
			instance->definition = &it->second.definition;
		}
		return instance;
	}

	std::vector<std::string> VideoEffectRegistry::allIds() const {
		std::vector<std::string> ids;
		ids.reserve(entries.size());
		for (const auto & kv : entries) ids.push_back(kv.first);
		return ids;
	}

} // namespace videoeffects
