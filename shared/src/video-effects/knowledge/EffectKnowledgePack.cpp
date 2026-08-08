#include "EffectKnowledgePack.h"
#include "EffectKnowledgeSerialization.h"
#include "EffectPresetId.h"
#include "ofFileUtils.h"
#include "ofJson.h"
#include "ofLog.h"
#include "ofUtils.h"
#include <algorithm>
#include <filesystem>
#include <set>
#include <system_error>

namespace videoeffects {

	bool exportEffectKnowledgePack(
		const EffectKnowledgeBase & sourceKb, const std::vector<std::string> & effectIds,
		const std::string & outputPath, const std::string & sourceTool) {
		EffectKnowledgePack pack;
		pack.sourceTool = sourceTool;
		pack.exportedAtUtc = ofGetTimestampString("%Y-%m-%dT%H:%M:%SZ");

		for (const auto & effectId : effectIds) {
			for (auto & entry : sourceKb.loadWhitelist(effectId)) pack.whitelist.push_back(std::move(entry));
			for (auto & entry : sourceKb.loadBlacklist(effectId)) pack.blacklist.push_back(std::move(entry));
			std::optional<EffectLevelKnowledge> effectDefault = sourceKb.loadEffectLevelKnowledge(effectId);
			if (effectDefault.has_value()) pack.effectDefaults.push_back(std::move(*effectDefault));
		}

		ofJson root;
		root["schemaVersion"] = pack.schemaVersion;
		root["sourceTool"] = pack.sourceTool;
		root["exportedAtUtc"] = pack.exportedAtUtc;
		ofJson whitelistJson = ofJson::array();
		for (const auto & entry : pack.whitelist) whitelistJson.push_back(knowledgeEntryToJson(entry));
		root["whitelist"] = whitelistJson;
		ofJson blacklistJson = ofJson::array();
		for (const auto & entry : pack.blacklist) blacklistJson.push_back(knowledgeEntryToJson(entry));
		root["blacklist"] = blacklistJson;
		ofJson effectDefaultsJson = ofJson::array();
		for (const auto & def : pack.effectDefaults) {
			ofJson defJson;
			defJson["effectId"] = def.effectId;
			defJson["compatibleSceneIds"] = def.compatibleSceneIds;
			if (def.piSafe.has_value()) defJson["piSafe"] = *def.piSafe;
			effectDefaultsJson.push_back(defJson);
		}
		root["effectDefaults"] = effectDefaultsJson;

		std::string resolvedPath = ofToDataPath(outputPath, true);
		ofDirectory::createDirectory(ofFilePath::getEnclosingDirectory(resolvedPath), true, true);

		// Atomic write, matching EffectKnowledgeBase::appendEntry's own
		// temp-file + rename pattern — a pack is meant to be safely
		// re-exportable/re-importable without ever leaving a half-written
		// file behind for a scene's own startup import to trip over.
		std::string tmpPath = resolvedPath + ".tmp";
		if (!ofSavePrettyJson(tmpPath, root)) {
			ofLogError("EffectKnowledgePack") << "failed to write " << tmpPath;
			return false;
		}
		std::error_code ec;
		std::filesystem::rename(tmpPath, resolvedPath, ec);
		if (ec) {
			ofLogError("EffectKnowledgePack") << "failed to finalize write to " << resolvedPath << ": " << ec.message();
			return false;
		}
		return true;
	}

	EffectKnowledgePackImportReport importEffectKnowledgePack(
		const std::string & packPath, EffectKnowledgeBase & targetKb, const std::vector<std::string> & knownEffectIds) {
		EffectKnowledgePackImportReport report;

		std::string resolvedPath = ofToDataPath(packPath, true);
		ofFile file(resolvedPath);
		if (!file.exists()) {
			ofLogNotice("EffectKnowledgePack") << "no pack at " << resolvedPath << " — nothing to import";
			return report; // absent pack is not a failure; report stays all-default/ok
		}

		// Deliberately NOT ofLoadJson() here: it catches nlohmann's own
		// parse_error internally, logs it itself, and returns whatever
		// partially-built value the SAX parser had constructed at the point
		// of failure (observed in practice: a `{` followed by invalid
		// content can leave it as a non-null, is_object()==true empty
		// object) rather than a value this function can reliably detect as
		// "parsing failed." Parsing the raw bytes directly via ofJson::parse()
		// keeps failure a real thrown exception this function's own
		// try/catch can act on, so a malformed pack is rejected
		// deterministically instead of silently degrading to "imported
		// nothing" — found by KnowledgePackSelfTest's malformed-pack case
		// (sketches/shader-effect-debugger/src/KnowledgePackSelfTest.cpp).
		ofJson root;
		try {
			ofBuffer buffer = ofBufferFromFile(resolvedPath);
			root = ofJson::parse(buffer.getText());
		} catch (const std::exception & ex) {
			ofLogError("EffectKnowledgePack") << "failed to parse " << resolvedPath << ": " << ex.what();
			report.ok = false;
			return report;
		}

		if (!root.is_object()) {
			ofLogError("EffectKnowledgePack") << resolvedPath << " is not a JSON object — cannot import";
			report.ok = false;
			return report;
		}

		// --- schema version policy -------------------------------------
		if (root.contains("schemaVersion") && !root.at("schemaVersion").is_null()) {
			report.schemaVersionWasAbsent = false;
			try {
				report.schemaVersion = root.at("schemaVersion").get<uint32_t>();
			} catch (const std::exception &) {
				ofLogError("EffectKnowledgePack") << resolvedPath << ": schemaVersion present but not a number — treating as unsupported";
				report.ok = false;
				report.versionSupported = false;
				return report;
			}
		} else {
			// Absent version = oldest/only known version, not a failure.
			report.schemaVersionWasAbsent = true;
			report.schemaVersion = 1;
		}

		if (report.schemaVersion < kEffectKnowledgePackMinSupportedSchemaVersion
			|| report.schemaVersion > kEffectKnowledgePackCurrentSchemaVersion) {
			ofLogError("EffectKnowledgePack") << resolvedPath << ": schemaVersion " << report.schemaVersion
											   << " is outside the supported range [" << kEffectKnowledgePackMinSupportedSchemaVersion << ", "
											   << kEffectKnowledgePackCurrentSchemaVersion << "] — rejecting the whole pack, importing nothing";
			report.ok = false;
			report.versionSupported = false;
			return report;
		}

		// --- entries -----------------------------------------------------
		// Preset-ID uniqueness (DEC-016: "unique within the canonical
		// knowledge pack") is tracked across BOTH lists in this one import
		// call — a presetId is a pack-wide identity, not scoped to
		// whitelist vs. blacklist.
		std::set<std::string> seenPresetIds;

		auto importList = [&](const char * key, bool isWhitelist) {
			if (!root.contains(key) || !root.at(key).is_array()) return;
			for (const auto & entryJson : root.at(key)) {
				KnowledgeEntry entry;
				if (!knowledgeEntryFromJson(entryJson, entry)) {
					report.skippedMalformed++;
					continue;
				}

				// The `list` field inside the JSON is informational only —
				// which array (whitelist/blacklist) an entry lives in is
				// determined by which key it was read from, never by this
				// string. Force it consistent so a hand-edited/malformed
				// `list` value can't leave stored data self-contradictory.
				entry.list = isWhitelist ? "whitelist" : "blacklist";

				if (!knownEffectIds.empty()
					&& std::find(knownEffectIds.begin(), knownEffectIds.end(), entry.effect) == knownEffectIds.end()) {
					ofLogWarning("EffectKnowledgePack") << "skipping entry for unknown effect id '" << entry.effect << "' in " << resolvedPath;
					report.skippedUnknownEffect++;
					continue;
				}

				// Preset-ID validation (DEC-016) — a malformed or duplicate
				// ID drops just the ID (the entry still imports as a
				// legacy-anonymous preset), never the whole entry. See
				// EffectKnowledgePackImportReport::skippedInvalidPresetId/
				// skippedDuplicatePresetId's own comments for why.
				if (entry.presetId.has_value()) {
					if (!isWellFormedEffectPresetId(*entry.presetId, entry.effect)) {
						ofLogWarning("EffectKnowledgePack") << "presetId '" << *entry.presetId
							<< "' is not well-formed for effect '" << entry.effect
							<< "' — importing as a legacy-anonymous preset instead";
						report.skippedInvalidPresetId++;
						entry.presetId.reset();
					} else if (seenPresetIds.count(*entry.presetId) > 0) {
						ofLogWarning("EffectKnowledgePack") << "duplicate presetId '" << *entry.presetId
							<< "' in " << resolvedPath << " — first occurrence kept, importing this one as a legacy-anonymous preset";
						report.skippedDuplicatePresetId++;
						entry.presetId.reset();
					} else {
						seenPresetIds.insert(*entry.presetId);
					}
				}

				bool appended = isWhitelist ? targetKb.appendWhitelist(entry) : targetKb.appendBlacklist(entry);
				if (appended) {
					if (isWhitelist) report.importedWhitelist++; else report.importedBlacklist++;
				} else {
					// appendWhitelist/appendBlacklist only return false for a
					// content-duplicate (already logged by EffectKnowledgeBase
					// itself) or a write error; either way this entry did not
					// end up as new state in targetKb.
					report.skippedDuplicate++;
				}
			}
		};
		importList("whitelist", true);
		importList("blacklist", false);

		// --- effect-level defaults ----------------------------------------
		if (root.contains("effectDefaults") && root.at("effectDefaults").is_array()) {
			for (const auto & defJson : root.at("effectDefaults")) {
				if (!defJson.is_object() || !defJson.contains("effectId") || !defJson.at("effectId").is_string()) {
					report.skippedMalformed++;
					continue;
				}
				EffectLevelKnowledge def;
				def.effectId = defJson.at("effectId").get<std::string>();

				if (!knownEffectIds.empty()
					&& std::find(knownEffectIds.begin(), knownEffectIds.end(), def.effectId) == knownEffectIds.end()) {
					ofLogWarning("EffectKnowledgePack") << "skipping effect-level default for unknown effect id '" << def.effectId << "' in " << resolvedPath;
					report.skippedUnknownEffect++;
					continue;
				}

				if (defJson.contains("compatibleSceneIds") && defJson.at("compatibleSceneIds").is_array()) {
					for (const auto & sceneIdJson : defJson.at("compatibleSceneIds")) {
						if (sceneIdJson.is_string()) def.compatibleSceneIds.push_back(sceneIdJson.get<std::string>());
					}
				}
				if (defJson.contains("piSafe") && !defJson.at("piSafe").is_null()) {
					def.piSafe = defJson.at("piSafe").get<bool>();
				}

				if (targetKb.saveEffectLevelKnowledge(def)) {
					report.importedEffectDefaults++;
				}
			}
		}

		return report;
	}

} // namespace videoeffects
