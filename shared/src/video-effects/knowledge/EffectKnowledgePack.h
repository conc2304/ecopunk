#pragma once

#include "EffectKnowledgeBase.h"
#include "EffectLevelKnowledge.h"
#include <string>
#include <vector>

// Cross-scene debugger export/import format — Shared Effect Knowledge,
// scoped extension (see docs/shared-effect-knowledge-scoped-extension.md).
//
// EffectKnowledgeBase (EffectKnowledgeBase.h) is already the persistence
// layer, but each EffectKnowledgeBase instance is deliberately per-sketch
// (its own header comment: "dataDir is resolved... no cross-sketch
// sharing, matching the asset-duplication precedent"). That's correct for
// what it does, but it means shader-effect-debugger's curated whitelist/
// blacklist entries never reach a production scene without a human
// hand-copying JSON files — the exact gap the roadmap's Phase 3 flags
// ("Debugger outputs can be consumed without manual code changes").
//
// This file adds ONE additive capability on top of EffectKnowledgeBase,
// not a second knowledge store: bundle several effects' whitelist/blacklist
// entries into one exported file, and import that file's entries into any
// target EffectKnowledgeBase (idempotently — reuses
// EffectKnowledgeBase::isDuplicate, so importing the same pack twice is a
// no-op the second time). Where the exported/imported file physically lives
// is a repo/build-tooling question (does it become a synced asset like
// shader files are, via scripts/sync-video-effect-assets.py?) that this
// pass does not decide — see the design doc's "proposed contract changes"
// section. Callers pass whatever path they resolve; this file does not
// hardcode one.
//
// NOTE ON PRESET IDs (updated by the Shared Effects Architecture-Closure
// Session, DEC-016): KnowledgeEntry now carries an OPTIONAL stable
// `presetId` (EffectPresetId.h) alongside its (effect, parameter-snapshot)
// content identity. An entry with no presetId is a "legacy anonymous
// preset" — still fully valid, still identified by content equality for
// EffectKnowledgeBase::isDuplicate's purposes, just not eligible to be
// treated as a stable, reusable, production-selectable preset (see
// EffectPresetId::isReusableAuthoredPreset()). Multiple entries sharing the
// same `effect` id remains normal and expected (several presets for one
// effect); what import now rejects deterministically is two entries
// sharing the same `presetId` string, or a `presetId` string whose own
// embedded effect segment does not match the entry's actual `effect`
// field — see importEffectKnowledgePack()'s presetId validation pass and
// EffectKnowledgePackImportReport::skippedDuplicatePresetId/
// skippedInvalidPresetId below.
namespace videoeffects {

	// The schema version this build writes and fully understands.
	//
	// Bumped 1 -> 2 by the Shared Effects Architecture-Closure Session:
	// KnowledgeEntry::compatibleSceneIds changed from a plain vector to
	// std::optional<vector> (distinguishing "no override" from "explicit
	// empty override" -- see EffectKnowledgeBase.h), KnowledgeEntry gained
	// `presetId`, and this pack gained the `effectDefaults` array below.
	// Every schemaVersion-1 pack remains fully readable (see
	// kEffectKnowledgePackMinSupportedSchemaVersion below) -- the version
	// bump exists so a FUTURE reader can tell "this pack was written by a
	// v2-aware tool" apart from "this pack predates preset identity and
	// effect-level metadata entirely," not to reject old data.
	constexpr uint32_t kEffectKnowledgePackCurrentSchemaVersion = 2;

	// The oldest schemaVersion this build will still import. Packs older
	// than this (schemaVersion present and < this value) are rejected the
	// same way unsupported-newer versions are — there is currently only one
	// version, so this constant exists to make the policy explicit and
	// future-proof, not because a lower version is known to exist yet.
	constexpr uint32_t kEffectKnowledgePackMinSupportedSchemaVersion = 1;

	struct EffectKnowledgePack {
		uint32_t schemaVersion = kEffectKnowledgePackCurrentSchemaVersion;
		std::string sourceTool;    // e.g. "shader-effect-debugger"
		std::string exportedAtUtc;
		std::vector<KnowledgeEntry> whitelist;
		std::vector<KnowledgeEntry> blacklist;

		// Effect-level defaults (Shared Effect Knowledge v1 Freeze Policy,
		// DEC-016 / Architecture-Closure Session — see
		// EffectLevelKnowledge.h). New in pack schemaVersion 2. Absent
		// entirely from any schemaVersion-1 pack (that key never existed
		// yet) -- importing an old pack simply yields an empty vector here,
		// same as "field was absent," never a parse failure.
		std::vector<EffectLevelKnowledge> effectDefaults;
	};

	// Reads every whitelist/blacklist entry for each of `effectIds` out of
	// `sourceKb` and writes them as one combined file at `outputPath`
	// (resolved the same way EffectKnowledgeBase resolves its own paths —
	// via ofToDataPath, so both bin/data-relative and absolute paths work).
	// Overwrites any existing pack at that path outright — a pack is a full
	// export snapshot, not an incremental log; re-run against a superset of
	// effectIds to fold in more effects rather than merging by hand.
	bool exportEffectKnowledgePack(
		const EffectKnowledgeBase & sourceKb, const std::vector<std::string> & effectIds,
		const std::string & outputPath, const std::string & sourceTool = "shader-effect-debugger");

	// Diagnostic result of an import attempt — richer than a bare bool so a
	// caller (a scene's setup(), or a test) can tell "nothing imported
	// because there was nothing to import" apart from "nothing imported
	// because the pack was rejected," and can log/report exactly how many
	// entries were skipped and why.
	struct EffectKnowledgePackImportReport {
		// false only for a read/parse failure or a rejected schema version —
		// i.e. "the pack as a whole could not be trusted." An absent pack
		// file is NOT a failure (ok stays true, all counts stay 0) — a scene
		// may legitimately run before any pack has ever been exported.
		bool ok = true;

		// false if the pack's schemaVersion is outside
		// [kEffectKnowledgePackMinSupportedSchemaVersion,
		//  kEffectKnowledgePackCurrentSchemaVersion]. A version-rejected pack
		// imports nothing at all (fatal, not degraded) — see
		// importEffectKnowledgePack's own comment for why.
		bool versionSupported = true;

		uint32_t schemaVersion = 0; // the version actually found (0 if absent/unreadable)
		bool schemaVersionWasAbsent = false; // true if the field was missing entirely (treated as version 1)

		int importedWhitelist = 0;
		int importedBlacklist = 0;

		// Entries that failed knowledgeEntryFromJson (missing required
		// fields, wrong JSON types, etc.) — the whole malformed entry is
		// skipped, not just the bad field.
		int skippedMalformed = 0;

		// Entries whose `effect` id was not found in the `knownEffectIds`
		// list passed to importEffectKnowledgePack(). Only counted when a
		// non-empty knownEffectIds list is supplied — an empty list means
		// "don't validate against the catalog," not "reject everything."
		int skippedUnknownEffect = 0;

		// Entries EffectKnowledgeBase::appendWhitelist/appendBlacklist
		// rejected as a content-duplicate of an entry already present in
		// targetKb (see EffectKnowledgeBase::isDuplicate) — expected and
		// harmless on a re-import of the same pack.
		int skippedDuplicate = 0;

		// Entries whose presetId is present but malformed — either not a
		// well-formed "preset.<effectId>.<slug>" string at all, or one
		// whose embedded effect segment doesn't match the entry's own
		// `effect` field (see EffectPresetId::isWellFormedEffectPresetId).
		// The entry itself is still imported as a legacy-anonymous preset
		// (presetId dropped, not the whole entry rejected) — a malformed ID
		// is an authoring mistake in one field, not evidence the rest of
		// the entry is untrustworthy.
		int skippedInvalidPresetId = 0;

		// Entries whose presetId is well-formed but collides with another
		// presetId already seen EARLIER in this same import batch (first
		// occurrence wins the ID; every later duplicate is imported as a
		// legacy-anonymous preset instead, same "drop just the ID, keep
		// the entry" policy as skippedInvalidPresetId above). Duplicate
		// detection is scoped to one import call, not cross-checked
		// against targetKb's pre-existing entries — targetKb has no
		// presetId-indexed lookup of its own (whitelist/blacklist are
		// stored per-effect, not globally), so cross-import duplicate
		// detection is future work, not silently claimed here.
		int skippedDuplicatePresetId = 0;

		// Effect-level default records (EffectLevelKnowledge) imported —
		// always overwrites any existing default for that effect id
		// (there is exactly one per effect, unlike whitelist/blacklist's
		// many entries — see EffectKnowledgeBase::saveEffectLevelKnowledge).
		int importedEffectDefaults = 0;
	};

	// Reads a pack from `packPath` and appends every entry into `targetKb`
	// via its normal appendWhitelist/appendBlacklist (duplicate-checked, so
	// safe to call on every scene startup without accumulating repeats).
	//
	// `knownEffectIds`, if non-empty, is used to reject entries whose
	// `effect` id doesn't match any known canonical effect (e.g. a stale ID
	// from a renamed/removed effect, or a hand-edited typo) — pass
	// `registry.allIds()` from the caller's VideoEffectRegistry to enable
	// this check. An empty list (the default) skips this validation
	// entirely, so a caller without a live registry handy can still import.
	//
	// Version policy: a pack with schemaVersion above
	// kEffectKnowledgePackCurrentSchemaVersion, or (if ever applicable)
	// below kEffectKnowledgePackMinSupportedSchemaVersion, is rejected in
	// full — report.ok is set false, report.versionSupported is set false,
	// and NOTHING is imported. This is deliberately fatal, not degraded:
	// a newer schema version could have silently repurposed an existing
	// field's meaning (e.g. redefining what `compatibleSceneIds` means), and
	// there is no way to know from this side whether importing "the fields
	// we happen to recognize" is actually safe. A missing schemaVersion
	// field is treated as version 1 (the oldest/only version), not rejected
	// — see report.schemaVersionWasAbsent.
	EffectKnowledgePackImportReport importEffectKnowledgePack(
		const std::string & packPath, EffectKnowledgeBase & targetKb,
		const std::vector<std::string> & knownEffectIds = {});

} // namespace videoeffects
