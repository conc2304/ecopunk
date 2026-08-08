#pragma once

#include <optional>
#include <string>
#include <vector>

// Effect-level knowledge defaults — Shared Effect Knowledge v1 Freeze
// Policy (DEC-016) / Shared Effects Architecture-Closure Session.
//
// Session 2's verification found a real gap (docs/shared-effect-knowledge-
// engineering-session-2-verification.md §6): compatibility and Pi-safety
// metadata existed ONLY on individual authored KnowledgeEntry presets, so
// an effect with zero authored presets had no way to carry any
// classification at all. This type is the fix: ONE record per canonical
// effect id, independent of how many (if any) presets exist for it.
//
// This is a DEFAULT, not an override — see KnowledgeEntry::compatibleSceneIds/
// piSafe in EffectKnowledgeBase.h for the preset-level override that takes
// precedence over this when present, and EffectKnowledgePrecedence.h for
// the functions that combine the two.
namespace videoeffects {

	struct EffectLevelKnowledge {
		std::string effectId;

		// Default scene compatibility for every preset of this effect that
		// does not author its own override. Empty = Unclassified (DEC-016:
		// "absent/empty compatibleSceneIds means Unclassified") — this is
		// the SAME rule as the preset-level field, just at this layer there
		// is no separate "absent vs. explicit-empty" distinction to make:
		// an EffectLevelKnowledge record that exists at all but lists no
		// compatible scenes IS the "not yet classified" state, because
		// there is nothing else it could mean at the default layer (there
		// is no layer beneath this one to fall through to except
		// Unclassified itself).
		std::vector<std::string> compatibleSceneIds;

		// Tri-state, same semantics as KnowledgeEntry::piSafe:
		// nullopt = unknown (never coerced to false), true/false = an
		// actual authored judgment. This is metadata about SUITABILITY as
		// judged/recorded, never a claim of measured Pi 3B+ hardware
		// validation — see docs/shared-effect-knowledge-scoped-extension.md's
		// "Pi-safety" section, which this field's semantics extend
		// unchanged to the effect level.
		std::optional<bool> piSafe;
	};

} // namespace videoeffects
