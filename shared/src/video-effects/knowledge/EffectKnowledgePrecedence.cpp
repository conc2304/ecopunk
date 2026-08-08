#include "EffectKnowledgePrecedence.h"

#include <algorithm>

namespace {

bool contains(const std::vector<std::string>& ids, const std::string& sceneId) {
	return std::find(ids.begin(), ids.end(), sceneId) != ids.end();
}

} // namespace

namespace videoeffects {

KnowledgeClassification resolveCompatibility(
	const KnowledgeEntry& preset, const EffectLevelKnowledge* effectDefault, const std::string& sceneId) {

	if (preset.compatibleSceneIds.has_value()) {
		return contains(*preset.compatibleSceneIds, sceneId) ? KnowledgeClassification::Allowed
															   : KnowledgeClassification::Disallowed;
	}

	if (effectDefault != nullptr && !effectDefault->compatibleSceneIds.empty()) {
		return contains(effectDefault->compatibleSceneIds, sceneId) ? KnowledgeClassification::Allowed
																	  : KnowledgeClassification::Disallowed;
	}

	return KnowledgeClassification::Unclassified;
}

bool isEligibleForAutomaticProductionSelection(KnowledgeClassification classification) {
	return classification == KnowledgeClassification::Allowed;
}

std::optional<bool> resolvePiSafe(const KnowledgeEntry& preset, const EffectLevelKnowledge* effectDefault) {
	if (preset.piSafe.has_value()) return preset.piSafe;
	if (effectDefault != nullptr && effectDefault->piSafe.has_value()) return effectDefault->piSafe;
	return std::nullopt;
}

} // namespace videoeffects
