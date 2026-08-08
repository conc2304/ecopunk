#pragma once

#include "EffectLevelKnowledge.h"
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

		// Additive fields (Shared Effect Knowledge — scoped extension, see
		// docs/shared-effect-knowledge-scoped-extension.md). Absent in every
		// entry written before this change; readers must treat a missing
		// piSafe as "unknown," never as false. Nothing above this comment
		// changed shape or meaning.
		std::optional<bool> piSafe;

		// Compatibility OVERRIDE for this specific authored preset — Shared
		// Effect Knowledge v1 Freeze Policy (DEC-016) / Architecture-Closure
		// Session. `std::optional` (not a bare vector) so absent and
		// explicit-empty are distinguishable on read, per DEC-016's
		// precedence rule:
		//
		//   preset override (this field, if present)
		//     -> effect-level default (EffectLevelKnowledge::compatibleSceneIds)
		//     -> Unclassified
		//
		//   - nullopt            : no preset-level override authored; fall
		//                          through to the effect-level default.
		//   - present, non-empty : explicitly compatible with exactly these
		//                          scene ids (a positive list).
		//   - present, EMPTY     : explicitly compatible with NO production
		//                          scenes — a real, intentional override,
		//                          not the same as "not yet classified."
		//
		// See EffectKnowledgePrecedence.h for the functions that actually
		// resolve this against an effect-level default and a target scene
		// id — this field only stores the authored value, it does not
		// interpret it.
		//
		// On-disk note: this was a plain std::vector<std::string> before the
		// Architecture-Closure Session (pack schemaVersion 1); every pre-
		// closure entry had the JSON key entirely absent (the field did not
		// exist yet in Session 1), so it deserializes to nullopt under the
		// new type — i.e. every legacy entry correctly becomes "no override,
		// fall through," never "explicitly compatible with nothing." See
		// EffectKnowledgeSerialization.cpp.
		std::optional<std::vector<std::string>> compatibleSceneIds;

		// Stable canonical preset identity — DEC-016. Recommended format
		// "preset.<effect-id>.<slug>" (EffectPresetId.h validates it).
		// nullopt = a "legacy anonymous preset": an entry written before
		// stable preset identity existed (or authored without one since).
		// Legacy anonymous entries:
		//   - remain fully loadable (this field simply stays nullopt);
		//   - are NOT promoted to a stable, reusable, production-selectable
		//     preset identity merely by loading — see
		//     isReusableAuthoredPreset() in EffectPresetId.h;
		//   - remain available for manual/debug inspection and for the
		//     existing (effect, snapshot)-based blacklist-avoidance content
		//     match (TFEffectPicker's existing use, unaffected by this
		//     field's presence or absence).
		// Never synthesized from vector index, file order, or a floating-
		// point snapshot hash during normal load — see
		// EffectKnowledgeSerialization.cpp's read path, which only ever
		// copies this field verbatim from JSON, and EffectPresetId.h for the
		// one-time, explicit, opt-in migration helper that is the sole
		// sanctioned way to assign an ID to a previously-anonymous entry.
		std::optional<std::string> presetId;
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

		// Effect-level knowledge defaults (Shared Effect Knowledge v1 Freeze
		// Policy, DEC-016 — see EffectLevelKnowledge.h). One record per
		// effect id, stored at <dataDir>/<effectId>.effect-defaults.json —
		// deliberately a SEPARATE file per effect (not one shared file for
		// every effect) so this follows the exact same per-effect storage
		// shape whitelist/blacklist already use, rather than introducing a
		// second storage convention into this class.
		//
		// Returns nullopt if no effect-level record has ever been saved for
		// this effect id — callers (EffectKnowledgePrecedence.h's
		// functions) already treat a null/absent EffectLevelKnowledge* as
		// "no effect-level default," so this is the correct absent
		// representation, not a default-constructed empty record.
		std::optional<EffectLevelKnowledge> loadEffectLevelKnowledge(const std::string & effectId) const;

		// Overwrites (not appends — there is exactly one effect-level
		// record per effect id, unlike whitelist/blacklist's many entries)
		// the stored default for effectDefault.effectId.
		bool saveEffectLevelKnowledge(const EffectLevelKnowledge & effectDefault);

	private:
		std::string dataDir;

		std::string pathFor(const std::string & effectId, const std::string & list) const;
		bool appendEntry(const KnowledgeEntry & entry, const std::string & list);
		std::vector<KnowledgeEntry> loadList(const std::string & effectId, const std::string & list) const;
	};

} // namespace videoeffects
