#pragma once

#include "EffectKnowledgeBase.h"
#include <string>

// Stable canonical preset identity — Shared Effect Knowledge v1 Freeze
// Policy (DEC-016) / Shared Effects Architecture-Closure Session.
//
// Recommended, validated format: "preset.<effect-id>.<slug>", e.g.
//   preset.heatmap_recolor.low_solar
//   preset.chromatic_aberration.soft_split
//
// Rules (DEC-016, transcribed exactly):
//   - stable after publication;
//   - unique within the canonical knowledge pack;
//   - associated with exactly one canonical effect ID;
//   - never generated from vector index;
//   - never generated from file order;
//   - never derived only from current floating-point snapshot serialization
//     at runtime;
//   - may be authored manually or generated once and then persisted;
//   - invalid or duplicate preset IDs must be rejected deterministically.
//
// This header only validates SHAPE (is this string a well-formed preset
// ID, and does it name the effect it claims to). It does NOT check
// uniqueness across a pack — that requires the whole entry list and lives
// in validatePresetIdentity() in EffectKnowledgePack.cpp (the only place
// that already has "every entry in this import" in scope).
namespace videoeffects {

	// True only for a well-formed "preset.<effectId>.<slug>" string where
	// <effectId> matches `effectId` exactly (a preset ID for one effect
	// must never validate as well-formed for a different effect — this is
	// the shape-level half of "associated with exactly one canonical
	// effect ID"; the pack-wide half — "the SAME preset ID string never
	// appears attached to two different effect fields" — is checked in
	// EffectKnowledgePack.cpp, since that needs the whole entry list).
	//
	// Allowed characters in <slug>: lowercase ascii letters, digits, and
	// underscore, non-empty. Deliberately conservative (no dots, no
	// uppercase, no spaces) — a preset ID is meant to be typed into logs,
	// migration tooling, and eventually vocabulary lookups; loosening this
	// later is easy, tightening it after presets are published is not.
	bool isWellFormedEffectPresetId(const std::string& presetId, const std::string& effectId);

	// Deterministic, one-time migration helper — NOT called anywhere during
	// normal load. Synthesizes a candidate preset ID for a legacy anonymous
	// entry (presetId absent) from its effectId plus a caller-supplied slug
	// (e.g. an operator-chosen short name, or an incrementing counter the
	// caller manages explicitly) — deliberately requires the caller to
	// supply `slug` rather than deriving one from the snapshot itself, so
	// this can never be mistaken for (or accidentally invoked as) automatic
	// floating-point-snapshot-derived ID generation, which DEC-016
	// explicitly prohibits.
	//
	// Returns an empty string if `slug` is empty or contains characters
	// isWellFormedEffectPresetId() would reject — callers must check for
	// that and are expected to still validate the result before persisting
	// it (this function does not mutate `entry` itself).
	std::string synthesizeMigrationPresetId(const std::string& effectId, const std::string& slug);

	// True only for an entry that carries a well-formed, effect-matching
	// presetId. A "legacy anonymous preset" (presetId absent, or present
	// but malformed) returns false here — see EffectKnowledgeBase.h's
	// header comment on KnowledgeEntry::presetId for what that means for
	// production eligibility.
	bool isReusableAuthoredPreset(const KnowledgeEntry& entry);

} // namespace videoeffects
