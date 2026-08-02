#include "EffectKnowledgeBase.h"
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

		ofJson entryToJson(const KnowledgeEntry & e) {
			ofJson j;
			j["schemaVersion"] = e.schemaVersion;
			j["effect"] = e.effect;
			j["list"] = e.list;
			j["snapshot"] = e.snapshot;
			if (!e.tolerance.empty()) j["tolerance"] = e.tolerance;
			if (!e.forbiddenRanges.empty()) {
				ofJson ranges;
				for (const auto & kv : e.forbiddenRanges) {
					ranges[kv.first] = { { "min", kv.second.first }, { "max", kv.second.second } };
				}
				j["forbiddenRanges"] = ranges;
			}
			j["label"] = e.label;
			j["notes"] = e.notes;
			j["sourceSketch"] = e.sourceSketch;
			j["sourceVideo"] = e.sourceVideo;
			j["timestampUtc"] = e.timestampUtc.empty() ? ofGetTimestampString("%Y-%m-%dT%H:%M:%SZ") : e.timestampUtc;
			if (e.perfObservedFps.has_value()) j["perfObservedFps"] = *e.perfObservedFps;
			if (e.qualityScore.has_value()) j["qualityScore"] = *e.qualityScore;
			if (!e.sceneContext.empty()) j["sceneContext"] = e.sceneContext;
			return j;
		}

		// Returns false (and logs a warning) if this individual entry is too
		// malformed to use, rather than throwing — one bad entry must not take
		// down the rest of the file per the implementation prompt's malformed-
		// file-recovery requirement.
		bool jsonToEntry(const ofJson & j, KnowledgeEntry & out) {
			if (!j.is_object() || !j.contains("effect") || !j.contains("snapshot")) {
				return false;
			}
			try {
				out.schemaVersion = j.value("schemaVersion", 1);
				out.effect = j.value("effect", "");
				out.list = j.value("list", "");
				out.snapshot.clear();
				for (auto it = j.at("snapshot").begin(); it != j.at("snapshot").end(); ++it) {
					out.snapshot[it.key()] = it.value().get<float>();
				}
				out.tolerance.clear();
				if (j.contains("tolerance") && j.at("tolerance").is_object()) {
					for (auto it = j.at("tolerance").begin(); it != j.at("tolerance").end(); ++it) {
						out.tolerance[it.key()] = it.value().get<float>();
					}
				}
				out.forbiddenRanges.clear();
				if (j.contains("forbiddenRanges") && j.at("forbiddenRanges").is_object()) {
					for (auto it = j.at("forbiddenRanges").begin(); it != j.at("forbiddenRanges").end(); ++it) {
						float lo = it.value().value("min", 0.0f);
						float hi = it.value().value("max", 0.0f);
						out.forbiddenRanges[it.key()] = { lo, hi };
					}
				}
				out.label = j.value("label", "");
				out.notes = j.value("notes", "");
				out.sourceSketch = j.value("sourceSketch", "");
				out.sourceVideo = j.value("sourceVideo", "");
				out.timestampUtc = j.value("timestampUtc", "");
				if (j.contains("perfObservedFps") && !j.at("perfObservedFps").is_null()) {
					out.perfObservedFps = j.at("perfObservedFps").get<float>();
				}
				if (j.contains("qualityScore") && !j.at("qualityScore").is_null()) {
					out.qualityScore = j.at("qualityScore").get<float>();
				}
				out.sceneContext = j.value("sceneContext", "");
				return true;
			} catch (const std::exception & ex) {
				ofLogWarning("EffectKnowledgeBase") << "skipping malformed entry: " << ex.what();
				return false;
			}
		}
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
			if (jsonToEntry(entryJson, entry)) {
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

		root.push_back(entryToJson(entry));

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

} // namespace videoeffects
