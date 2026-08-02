#pragma once

#include "VideoEffectParameters.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

// Whitelist/blacklist persistence, per docs/shader-effect-system-probe.md §8
// and docs/shared-video-effect-architecture.md §10. Nothing like this
// existed anywhere in the repo prior to this effort (confirmed by the
// probe) — this is net-new capability, not an extraction.
//
// Storage: one JSON file per effect per list
// (<dataDir>/<effectId>.whitelist.json / .blacklist.json), written
// atomically (temp file + rename) and tolerant of malformed entries (a
// corrupt entry is skipped with a logged warning rather than failing the
// whole file — mirrors temporal-fields' existing tolerant preset loader).
namespace videoeffects {

	struct KnowledgeEntry {
		int schemaVersion = 1;
		std::string effect;
		std::string list; // "whitelist" or "blacklist"

		// Parameter id -> numeric value. Covers Float/Int/Bool uniformly (JSON
		// numbers cover all three) — vec2/3/4 params are not snapshotted by id
		// in v1; see EffectRandomizer's consumer for how this is read back.
		std::map<std::string, float> snapshot;

		// Optional per-parameter jitter radius around `snapshot` used for
		// whitelist-biased sampling. Absent = exact-point reference only.
		std::map<std::string, float> tolerance;

		// Optional per-parameter forbidden range, blacklist entries only.
		// {"gamma": {"min":0.0,"max":0.15}} style — read via forbiddenMin/Max.
		std::map<std::string, std::pair<float, float>> forbiddenRanges;

		std::string label;
		std::string notes;
		std::string sourceSketch;
		std::string sourceVideo;
		std::string timestampUtc;
		std::optional<float> perfObservedFps;
		std::optional<float> qualityScore;
		std::string sceneContext;
	};

	class EffectKnowledgeBase {
	public:
		// dataDir is resolved the same way every other sketch-local data path
		// is in this project (relative to bin/data via ofToDataPath) — no
		// cross-sketch sharing, matching the asset-duplication precedent.
		explicit EffectKnowledgeBase(std::string dataDir = "knowledge");

		bool appendWhitelist(const KnowledgeEntry & entry);
		bool appendBlacklist(const KnowledgeEntry & entry);

		std::vector<KnowledgeEntry> loadWhitelist(const std::string & effectId) const;
		std::vector<KnowledgeEntry> loadBlacklist(const std::string & effectId) const;

		bool isDuplicate(const KnowledgeEntry & entry, const std::vector<KnowledgeEntry> & existing) const;

	private:
		std::string dataDir;

		std::string pathFor(const std::string & effectId, const std::string & list) const;
		bool appendEntry(const KnowledgeEntry & entry, const std::string & list);
		std::vector<KnowledgeEntry> loadList(const std::string & effectId, const std::string & list) const;
	};

} // namespace videoeffects
