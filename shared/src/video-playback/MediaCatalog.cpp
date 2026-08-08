#include "MediaCatalog.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace {

// Deterministic, dependency-free "stable identifier from a string" —
// deliberately NOT std::hash (implementation-defined, not guaranteed
// stable across standard library versions/compilers, so it would violate
// "stable ... across a checkout" if we ever changed compilers). FNV-1a is
// a small, well-known, fully-specified 32-bit hash — good enough for a
// human-opaque but stable catalog ID; not a security hash, not required to
// be one.
std::string fnv1a32Hex(const std::string& input) {
	uint32_t hash = 2166136261u;
	for (unsigned char c : input) {
		hash ^= c;
		hash *= 16777619u;
	}
	static const char* hexDigits = "0123456789abcdef";
	std::string out(8, '0');
	for (int i = 7; i >= 0; i--) {
		out[i] = hexDigits[hash & 0xF];
		hash >>= 4;
	}
	return out;
}

std::string stripExtension(const std::string& relativePath) {
	size_t slash = relativePath.find_last_of("/\\");
	std::string base = (slash == std::string::npos) ? relativePath : relativePath.substr(slash + 1);
	size_t dot = base.find_last_of('.');
	return (dot == std::string::npos) ? base : base.substr(0, dot);
}

} // namespace

std::string MediaCatalog::synthesizeMediaId(const std::string& relativePath) {
	// "media." prefix keeps synthesized IDs visually distinct from any
	// future curator-authored mediaId scheme without reserving a specific
	// naming convention for curated IDs.
	return "media.auto." + fnv1a32Hex(relativePath);
}

std::string MediaCatalog::synthesizeTitleId(const std::string& mediaId) {
	// Vocabulary-addressable per VideoPlaybackStatus.h/MediaMetadata.h's
	// header comments. This ID will not resolve in any vocabulary pack
	// until a curator authors one for it — that's expected and is exactly
	// why fallbackDisplayTitle exists (Shared-Video-Playback-HUD-Semantic-
	// Slot-Review.md §5's resolution order: titleId vocabulary -> catalog
	// fallback -> stable media ID -> generic unavailable label).
	return "media.title." + mediaId;
}

std::string MediaCatalog::synthesizeFallbackDisplayTitle(const std::string& relativePath) {
	std::string name = stripExtension(relativePath);
	for (char& c : name) {
		if (c == '_' || c == '-') c = ' ';
	}
	// Collapse runs of spaces produced by adjacent separators (e.g.
	// "foo--bar" or "foo_-bar") without pulling in a regex dependency.
	std::string collapsed;
	collapsed.reserve(name.size());
	bool lastWasSpace = false;
	for (char c : name) {
		bool isSpace = (c == ' ');
		if (isSpace && lastWasSpace) continue;
		collapsed.push_back(c);
		lastWasSpace = isSpace;
	}
	while (!collapsed.empty() && collapsed.front() == ' ') collapsed.erase(collapsed.begin());
	while (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();
	return collapsed.empty() ? relativePath : collapsed;
}

MediaCatalogBuildResult MediaCatalog::build(
	const std::vector<std::string>& discoveredRelativePaths,
	const std::vector<MediaCatalogEntryOverride>& overrides) {

	MediaCatalogBuildResult result;

	// Index overrides by relativePath for O(1) lookup per discovered file.
	// A later override for the same relativePath in the input vector wins
	// over an earlier one (authoring convenience — "last one in the file
	// wins" is the least surprising rule for a hand-edited catalog file);
	// this is independent from, and unrelated to, the mediaId-uniqueness
	// rule enforced below.
	std::unordered_map<std::string, const MediaCatalogEntryOverride*> overrideByPath;
	for (const auto& ov : overrides) {
		if (ov.relativePath.empty()) {
			result.errors.push_back("catalog override with empty relativePath skipped");
			continue;
		}
		overrideByPath[ov.relativePath] = &ov;
	}

	std::unordered_set<std::string> discoveredSet(discoveredRelativePaths.begin(), discoveredRelativePaths.end());
	for (const auto& kv : overrideByPath) {
		if (discoveredSet.find(kv.first) == discoveredSet.end()) {
			result.errors.push_back("catalog override references a file not found beneath the media root: " + kv.first);
		}
	}

	std::unordered_set<std::string> usedMediaIds;

	for (const std::string& relativePath : discoveredRelativePaths) {
		MediaMetadata entry;
		entry.relativePath = relativePath;

		auto it = overrideByPath.find(relativePath);
		const MediaCatalogEntryOverride* ov = (it == overrideByPath.end()) ? nullptr : it->second;

		entry.mediaId = (ov && !ov->mediaId.empty()) ? ov->mediaId : synthesizeMediaId(relativePath);
		entry.titleId = (ov && !ov->titleId.empty()) ? ov->titleId : synthesizeTitleId(entry.mediaId);
		entry.fallbackDisplayTitle = (ov && !ov->fallbackDisplayTitle.empty())
			? ov->fallbackDisplayTitle
			: synthesizeFallbackDisplayTitle(relativePath);
		entry.enabled = ov ? ov->enabled : true;
		entry.piSafe = ov ? ov->piSafe : true;

		if (entry.mediaId.empty()) {
			result.errors.push_back("resolved empty mediaId for: " + relativePath);
			continue;
		}
		if (usedMediaIds.count(entry.mediaId) > 0) {
			result.errors.push_back("duplicate mediaId '" + entry.mediaId + "' for: " + relativePath + " (rejected, first occurrence kept)");
			continue;
		}
		usedMediaIds.insert(entry.mediaId);
		result.entries.push_back(std::move(entry));
	}

	return result;
}
