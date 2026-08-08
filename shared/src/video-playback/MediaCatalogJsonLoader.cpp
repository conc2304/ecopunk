// Implements loadCatalogOverridesFromJsonFile(), declared in
// MediaCatalog.h. Kept in its own translation unit, separate from
// MediaCatalog.cpp, specifically so MediaCatalog.cpp (which
// test/media_catalog_tests.cpp compiles directly, per the dependency-free
// test convention) never needs nlohmann/json.hpp on its include path.
// This file is only ever built as part of a real sketch/app build, where
// every openFrameworks project already has libs/json/include available
// (see this subsystem's README.md).
//
// Expected catalog file shape (all fields optional except relativePath):
//
//   {
//     "entries": [
//       {
//         "relativePath": "clip.mp4",
//         "mediaId": "media.clip.hero_shot",
//         "titleId": "media.title.clip.hero_shot",
//         "fallbackDisplayTitle": "Hero Shot",
//         "enabled": true,
//         "piSafe": true
//       }
//     ]
//   }
#include "MediaCatalog.h"

#include "nlohmann/json.hpp"

#include <fstream>

std::vector<MediaCatalogEntryOverride> loadCatalogOverridesFromJsonFile(
	const std::string& path, std::vector<std::string>& outErrors) {

	std::vector<MediaCatalogEntryOverride> result;
	if (path.empty()) {
		return result;  // no catalog file configured — not an error, see MediaCatalog.h
	}

	std::ifstream file(path);
	if (!file.is_open()) {
		// A missing catalog file is expected/normal (no catalog authored
		// yet for this media root) — "catalog loading must not crash the
		// runtime" per the implementation prompt §5.1/§12. Reported for
		// visibility, not treated as fatal.
		outErrors.push_back("catalog file not found (using auto-synthesized entries only): " + path);
		return result;
	}

	nlohmann::json parsed;
	try {
		file >> parsed;
	} catch (const nlohmann::json::parse_error& e) {
		outErrors.push_back("catalog file failed to parse, ignoring entirely: " + path + " (" + e.what() + ")");
		return result;
	}

	if (!parsed.is_object() || !parsed.contains("entries") || !parsed["entries"].is_array()) {
		outErrors.push_back("catalog file missing a top-level \"entries\" array, ignoring entirely: " + path);
		return result;
	}

	for (const auto& item : parsed["entries"]) {
		if (!item.is_object() || !item.contains("relativePath") || !item["relativePath"].is_string()) {
			outErrors.push_back("catalog entry missing required \"relativePath\" string, skipped");
			continue;
		}

		MediaCatalogEntryOverride ov;
		ov.relativePath = item["relativePath"].get<std::string>();
		if (item.contains("mediaId") && item["mediaId"].is_string()) ov.mediaId = item["mediaId"].get<std::string>();
		if (item.contains("titleId") && item["titleId"].is_string()) ov.titleId = item["titleId"].get<std::string>();
		if (item.contains("fallbackDisplayTitle") && item["fallbackDisplayTitle"].is_string())
			ov.fallbackDisplayTitle = item["fallbackDisplayTitle"].get<std::string>();
		if (item.contains("enabled") && item["enabled"].is_boolean()) ov.enabled = item["enabled"].get<bool>();
		if (item.contains("piSafe") && item["piSafe"].is_boolean()) ov.piSafe = item["piSafe"].get<bool>();

		result.push_back(std::move(ov));
	}

	return result;
}
