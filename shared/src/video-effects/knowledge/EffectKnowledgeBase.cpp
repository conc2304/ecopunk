#include "EffectKnowledgeBase.h"
#include "EffectKnowledgeSerialization.h"
#include "ofFileUtils.h"
#include "ofJson.h"
#include "ofLog.h"
#include "ofUtils.h"
#include <cmath>
#include <filesystem>
#include <system_error>
#include <utility>

namespace videoeffects {

	namespace {
		constexpr float kDuplicateEpsilon = 1e-4f;
	} // namespace

	EffectKnowledgeBase::EffectKnowledgeBase(std::string dataDir_)
		: dataDir(std::move(dataDir_)) {}

	std::string EffectKnowledgeBase::pathFor(const std::string & effectId, const std::string & list) const {
		return dataDir + "/" + effectId + "." + list + ".json";
	}

	std::vector<KnowledgeEntry> EffectKnowledgeBase::loadList(const std::string & effectId, const std::string & list) const {
		std::vector<KnowledgeEntry> result;
		std::string resolvedPath = ofToDataPath(pathFor(effectId, list), true);
		ofFile file(resolvedPath);
		if (!file.exists()) {
			return result;
		}

		ofJson root;
		try {
			root = ofLoadJson(resolvedPath);
		} catch (const std::exception & ex) {
			ofLogError("EffectKnowledgeBase") << "failed to parse " << resolvedPath << ": " << ex.what()
											   << " — treating as empty rather than failing startup";
			return result;
		}

		if (!root.is_array()) {
			ofLogWarning("EffectKnowledgeBase") << resolvedPath << " is not a JSON array — treating as empty";
			return result;
		}

		for (const auto & entryJson : root) {
			KnowledgeEntry entry;
			if (knowledgeEntryFromJson(entryJson, entry)) {
				result.push_back(std::move(entry));
			}
		}
		return result;
	}

	std::vector<KnowledgeEntry> EffectKnowledgeBase::loadWhitelist(const std::string & effectId) const {
		return loadList(effectId, "whitelist");
	}

	std::vector<KnowledgeEntry> EffectKnowledgeBase::loadBlacklist(const std::string & effectId) const {
		return loadList(effectId, "blacklist");
	}

	bool EffectKnowledgeBase::isDuplicate(const KnowledgeEntry & entry, const std::vector<KnowledgeEntry> & existing) const {
		for (const auto & other : existing) {
			if (other.snapshot.size() != entry.snapshot.size()) continue;
			bool allMatch = true;
			for (const auto & kv : entry.snapshot) {
				auto it = other.snapshot.find(kv.first);
				if (it == other.snapshot.end() || std::fabs(it->second - kv.second) > kDuplicateEpsilon) {
					allMatch = false;
					break;
				}
			}
			if (allMatch) return true;
		}
		return false;
	}

	bool EffectKnowledgeBase::appendEntry(const KnowledgeEntry & entry, const std::string & list) {
		std::vector<KnowledgeEntry> existing = loadList(entry.effect, list);
		if (isDuplicate(entry, existing)) {
			ofLogNotice("EffectKnowledgeBase") << "skipped duplicate " << list << " entry for " << entry.effect;
			return false;
		}

		std::string resolvedPath = ofToDataPath(pathFor(entry.effect, list), true);
		ofJson root = ofJson::array();
		ofFile file(resolvedPath);
		if (file.exists()) {
			try {
				root = ofLoadJson(resolvedPath);
				if (!root.is_array()) root = ofJson::array();
			} catch (const std::exception &) {
				root = ofJson::array();
			}
		}

		root.push_back(knowledgeEntryToJson(entry));

		ofDirectory::createDirectory(ofFilePath::getEnclosingDirectory(resolvedPath), true, true);

		// Atomic write: temp file + rename, so a crash mid-write never leaves a
		// truncated/corrupt file in place of a previously-good one.
		std::string tmpPath = resolvedPath + ".tmp";
		if (!ofSavePrettyJson(tmpPath, root)) {
			ofLogError("EffectKnowledgeBase") << "failed to write " << tmpPath;
			return false;
		}
		std::error_code ec;
		std::filesystem::rename(tmpPath, resolvedPath, ec);
		if (ec) {
			ofLogError("EffectKnowledgeBase") << "failed to finalize write to " << resolvedPath << ": " << ec.message();
			return false;
		}
		return true;
	}

	bool EffectKnowledgeBase::appendWhitelist(const KnowledgeEntry & entry) {
		return appendEntry(entry, "whitelist");
	}

	bool EffectKnowledgeBase::appendBlacklist(const KnowledgeEntry & entry) {
		return appendEntry(entry, "blacklist");
	}

	std::optional<EffectLevelKnowledge> EffectKnowledgeBase::loadEffectLevelKnowledge(const std::string & effectId) const {
		std::string resolvedPath = ofToDataPath(dataDir + "/" + effectId + ".effect-defaults.json", true);
		ofFile file(resolvedPath);
		if (!file.exists()) {
			return std::nullopt;
		}

		ofJson root;
		try {
			root = ofLoadJson(resolvedPath);
		} catch (const std::exception & ex) {
			ofLogError("EffectKnowledgeBase") << "failed to parse " << resolvedPath << ": " << ex.what()
											   << " — treating as absent rather than failing startup";
			return std::nullopt;
		}
		if (!root.is_object()) {
			ofLogWarning("EffectKnowledgeBase") << resolvedPath << " is not a JSON object — treating as absent";
			return std::nullopt;
		}

		EffectLevelKnowledge result;
		result.effectId = root.value("effectId", effectId);
		if (root.contains("compatibleSceneIds") && root.at("compatibleSceneIds").is_array()) {
			for (const auto & sceneIdJson : root.at("compatibleSceneIds")) {
				if (sceneIdJson.is_string()) result.compatibleSceneIds.push_back(sceneIdJson.get<std::string>());
			}
		}
		result.piSafe.reset();
		if (root.contains("piSafe") && !root.at("piSafe").is_null()) {
			result.piSafe = root.at("piSafe").get<bool>();
		}
		return result;
	}

	bool EffectKnowledgeBase::saveEffectLevelKnowledge(const EffectLevelKnowledge & effectDefault) {
		std::string resolvedPath = ofToDataPath(dataDir + "/" + effectDefault.effectId + ".effect-defaults.json", true);

		ofJson root;
		root["effectId"] = effectDefault.effectId;
		root["compatibleSceneIds"] = effectDefault.compatibleSceneIds;
		if (effectDefault.piSafe.has_value()) root["piSafe"] = *effectDefault.piSafe;

		ofDirectory::createDirectory(ofFilePath::getEnclosingDirectory(resolvedPath), true, true);

		std::string tmpPath = resolvedPath + ".tmp";
		if (!ofSavePrettyJson(tmpPath, root)) {
			ofLogError("EffectKnowledgeBase") << "failed to write " << tmpPath;
			return false;
		}
		std::error_code ec;
		std::filesystem::rename(tmpPath, resolvedPath, ec);
		if (ec) {
			ofLogError("EffectKnowledgeBase") << "failed to finalize write to " << resolvedPath << ": " << ec.message();
			return false;
		}
		return true;
	}

} // namespace videoeffects
