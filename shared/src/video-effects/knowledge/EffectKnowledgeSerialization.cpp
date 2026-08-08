#include "EffectKnowledgeSerialization.h"
#include "ofLog.h"
#include "ofUtils.h"

namespace videoeffects {

	ofJson knowledgeEntryToJson(const KnowledgeEntry & e) {
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
		if (e.piSafe.has_value()) j["piSafe"] = *e.piSafe;
		// has_value() (not "non-empty") is the write-side test -- an authored
		// EMPTY override must still round-trip as the JSON key being PRESENT
		// (as []), never omitted, or it would be indistinguishable from "no
		// override" on the next read. See KnowledgeEntry::compatibleSceneIds's
		// header comment.
		if (e.compatibleSceneIds.has_value()) j["compatibleSceneIds"] = *e.compatibleSceneIds;
		if (e.presetId.has_value()) j["presetId"] = *e.presetId;
		return j;
	}

	bool knowledgeEntryFromJson(const ofJson & j, KnowledgeEntry & out) {
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
					// Invalid range (min > max): drop just this one key rather than
					// the whole entry, and rather than silently swapping/guessing
					// intent — an inverted range is ambiguous input, not obviously
					// "backwards," per §2.5's "malformed knowledge must not
					// silently change effect-selection behavior."
					if (lo > hi) {
						ofLogWarning("EffectKnowledgeSerialization")
							<< "dropping forbiddenRanges['" << it.key() << "'] for effect '" << j.value("effect", "?")
							<< "': min (" << lo << ") > max (" << hi << ")";
						continue;
					}
					out.forbiddenRanges[it.key()] = { lo, hi };
				}
			}
			out.label = j.value("label", "");
			out.notes = j.value("notes", "");
			out.sourceSketch = j.value("sourceSketch", "");
			out.sourceVideo = j.value("sourceVideo", "");
			out.timestampUtc = j.value("timestampUtc", "");
			out.perfObservedFps.reset();
			if (j.contains("perfObservedFps") && !j.at("perfObservedFps").is_null()) {
				out.perfObservedFps = j.at("perfObservedFps").get<float>();
			}
			out.qualityScore.reset();
			if (j.contains("qualityScore") && !j.at("qualityScore").is_null()) {
				out.qualityScore = j.at("qualityScore").get<float>();
			}
			out.sceneContext = j.value("sceneContext", "");
			out.piSafe.reset();
			if (j.contains("piSafe") && !j.at("piSafe").is_null()) {
				out.piSafe = j.at("piSafe").get<bool>();
			}
			// Key ABSENT -> nullopt (no override authored -- fall through to
			// the effect-level default, or Unclassified if none exists). Key
			// PRESENT (even as an empty array) -> a populated, possibly-empty
			// vector, i.e. an explicit override. This distinction is the
			// whole point of the Architecture-Closure Session's retype of
			// this field from a bare vector to std::optional<vector> -- see
			// KnowledgeEntry::compatibleSceneIds's header comment. Every
			// pre-closure entry (pack schemaVersion 1) never had this key at
			// all, so it correctly becomes nullopt here, not an accidental
			// "explicitly compatible with nothing."
			out.compatibleSceneIds.reset();
			if (j.contains("compatibleSceneIds") && j.at("compatibleSceneIds").is_array()) {
				std::vector<std::string> ids;
				for (const auto & sceneIdJson : j.at("compatibleSceneIds")) {
					ids.push_back(sceneIdJson.get<std::string>());
				}
				out.compatibleSceneIds = std::move(ids);
			}
			out.presetId.reset();
			if (j.contains("presetId") && j.at("presetId").is_string()) {
				std::string id = j.at("presetId").get<std::string>();
				if (!id.empty()) out.presetId = std::move(id);
			}
			return true;
		} catch (const std::exception & ex) {
			ofLogWarning("EffectKnowledgeSerialization") << "skipping malformed entry: " << ex.what();
			return false;
		}
	}

} // namespace videoeffects
