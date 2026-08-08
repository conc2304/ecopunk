#include "EffectPresetId.h"

namespace {

bool isValidSlugChar(char c) {
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

} // namespace

namespace videoeffects {

bool isWellFormedEffectPresetId(const std::string& presetId, const std::string& effectId) {
	if (effectId.empty()) return false;

	const std::string prefix = "preset." + effectId + ".";
	if (presetId.size() <= prefix.size()) return false;
	if (presetId.compare(0, prefix.size(), prefix) != 0) return false;

	std::string slug = presetId.substr(prefix.size());
	if (slug.empty()) return false;
	for (char c : slug) {
		if (!isValidSlugChar(c)) return false;
	}
	return true;
}

std::string synthesizeMigrationPresetId(const std::string& effectId, const std::string& slug) {
	if (effectId.empty() || slug.empty()) return "";
	for (char c : slug) {
		if (!isValidSlugChar(c)) return "";
	}
	std::string candidate = "preset." + effectId + "." + slug;
	// Self-check against the same rule the candidate will be judged by
	// everywhere else — if this ever fails it means effectId itself
	// contained characters that break the "preset.<effectId>." prefix
	// match (e.g. effectId containing a literal "." itself), which no
	// canonical effect id in this catalog does today, but this function
	// does not assume that silently.
	return isWellFormedEffectPresetId(candidate, effectId) ? candidate : "";
}

bool isReusableAuthoredPreset(const KnowledgeEntry& entry) {
	if (!entry.presetId.has_value()) return false;
	return isWellFormedEffectPresetId(*entry.presetId, entry.effect);
}

} // namespace videoeffects
