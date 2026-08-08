#pragma once

#include "EffectKnowledgeBase.h"
#include "EffectLevelKnowledge.h"
#include <optional>
#include <string>

// Approved precedence rules — Shared Effect Knowledge v1 Freeze Policy
// (DEC-016) / Shared Effects Architecture-Closure Session §3.
//
// This file is the ONLY place these rules are implemented. Nothing else
// in this subsystem re-derives compatibility/piSafe/eligibility by hand —
// see EffectKnowledgePack.cpp and shared/src/video-effects/test/ for the
// only two call sites.
namespace videoeffects {

	enum class KnowledgeClassification : uint8_t {
		Unclassified,
		Allowed,
		Disallowed
	};

	// §3.1 Compatibility:
	//   preset explicit override -> effect-level default -> Unclassified
	//
	//   - preset.compatibleSceneIds has_value(): the preset's own override
	//     wins outright, regardless of what the effect-level default says.
	//     - contains sceneId          -> Allowed
	//     - does not contain sceneId  -> Disallowed (this covers BOTH a
	//       non-empty list that simply omits sceneId, and an explicitly
	//       authored EMPTY override — "compatible with no production
	//       scenes", DEC-016's own wording)
	//   - preset.compatibleSceneIds is nullopt (no override authored):
	//     fall through to effectDefault, if one is supplied.
	//     - effectDefault present AND its compatibleSceneIds is non-empty:
	//       - contains sceneId          -> Allowed
	//       - does not contain sceneId  -> Disallowed
	//     - effectDefault absent, OR present with an empty
	//       compatibleSceneIds -> Unclassified (there is no third layer to
	//       fall through to).
	KnowledgeClassification resolveCompatibility(
		const KnowledgeEntry& preset, const EffectLevelKnowledge* effectDefault, const std::string& sceneId);

	// §3.1 "Automatic production selection: Allowed -> eligible; Disallowed
	// -> ineligible; Unclassified -> ineligible." This is the ONE function
	// a production automatic-selection path is required to consult before
	// treating a preset as usable without a human explicitly invoking it —
	// see docs/shared-effect-knowledge-schema-v1.md's "Selection-policy
	// ownership" section for exactly which current call sites do (and do
	// not yet) call this.
	bool isEligibleForAutomaticProductionSelection(KnowledgeClassification classification);

	// §3.2 Pi metadata: preset piSafe override -> effect-level piSafe ->
	// Unknown (nullopt). Never returns true merely because both inputs are
	// absent — "Unknown must never become true" (DEC-016).
	std::optional<bool> resolvePiSafe(const KnowledgeEntry& preset, const EffectLevelKnowledge* effectDefault);

} // namespace videoeffects
