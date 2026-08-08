#pragma once

#include "EffectEvolutionController.h" // reuses EvolutionPhase as the transition-phase model — lives in
                                        // evolution/, resolved via this monorepo's flat per-subdirectory
                                        // include path (same pattern EffectEvolutionController.h itself
                                        // uses to reach knowledge/'s EffectKnowledgeBase.h/EffectRandomizer.h)
#include "VideoEffectLoadReport.h" // the one real, owned condition deriveEffectHealth() reads from
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// HUD-facing effect activity status — Shared Effect Knowledge, scoped
// extension (see docs/shared-effect-knowledge-scoped-extension.md).
//
// Deliberately NOT a shared-contract type: this does not live in
// shared/src/scene/SceneContract.h and does not change SceneHudStatus's
// shape. Only curated data ever crosses that boundary; raw shader
// parameters never do, per HUD-Initiative-Scope-and-Product-Direction-
// Addendum.md §4/§10 ("must not expose raw shader parameters").
//
// Kept free of any openFrameworks include (only pulls in the OF-free parts
// of evolution/, core/, knowledge/) so it can be exercised by a standalone,
// dependency-free test binary — see shared/src/video-effects/test/.
//
// RELATIONSHIP TO docs/HUD-Semantic-Slot-Model-v1.md §12.6 (`effects.*`
// slots) — read this before adding a new consumer:
//   effects.active              -> list of effect IDs   -> resolveDominantEffectIds(), .effectId per result
//   effects.dominant             -> one effect ID         -> resolveDominantEffectIds().front().effectId (if non-empty)
//   effects.transition.progress -> 0..1                  -> resolveDominantEffectIds().front().transitionProgress01
//   effects.intensity           -> 0..1                  -> resolveDominantEffectIds().front().prominence (see caveat below)
//   effects.health               -> enum                  -> NOT REPRESENTED — no field in this type maps to it; see
//                                                             docs/shared-effect-knowledge-scoped-extension.md's
//                                                             "HUD Semantic Slot Model gap analysis" section.
// `resolveDominantEffectLabels()` (still present, unchanged behavior) is a
// DIFFERENT function that returns pre-formatted, English display strings
// (including a hardcoded "(shifting)" suffix) — that is presentation text,
// not a semantic ID, and must never be fed into `effects.active`/
// `effects.dominant` (the slot model's vocabulary-resolution step happens
// downstream of this type, not inside it). It exists only because
// SceneHudStatus::activeEffects (the frozen, pre-existing field) already
// expects ready-made strings — keep using it for that field specifically,
// and resolveDominantEffectIds() for anything feeding the newer
// effects.*.slot model.
//
// Caveat on `effects.intensity`: this type's `prominence` is a caller-
// supplied dominance-ranking input (how much visual weight a slot has),
// not a measured "current effect intensity" in the sense the slot model
// describes (e.g. how strongly a shader's own alpha/mix parameter is
// currently applied). Reusing it for `effects.intensity` is a reasonable
// approximation, not an exact semantic match — flagged, not silently
// presented as equivalent.
namespace videoeffects {

	// One effect instance's activity, as seen by whatever owns it (a
	// production scene, per-quadrant slot, etc.). The caller populates this
	// from its own VideoEffectInstance/EffectEvolutionController state —
	// this type does not know how to obtain any of these values itself.
	struct EffectActivitySlot {
		std::string slotId;      // caller-defined, e.g. "primary", "quadrant_0"
		std::string effectId;    // canonical id, e.g. "heatmap_recolor"
		std::string displayName; // curated label, e.g. VideoEffectDefinition::displayName

		// Reuses EffectEvolutionController's own phase enum rather than
		// inventing a second one — Holding is "steady state," Transitioning
		// is "actively moving toward a new target." A slot with no evolution
		// controller at all should report Holding, progress 1.0.
		EvolutionPhase phase = EvolutionPhase::Holding;
		float transitionProgress01 = 1.0f;

		// Caller-assigned salience for dominance resolution — e.g. "how much
		// of the frame does this slot occupy," "how recently did it change."
		// Not normalized against other slots by this type; resolution below
		// only compares slots within one EffectActivityStatus.
		float prominence = 1.0f;
	};

	// Shared Effects' own owned failure/degradation modes -- deliberately
	// NOT modeled after SceneHealth's four-value shape (Ready/Loading/
	// Degraded/Failed): this subsystem has no in-process "Loading" phase
	// of its own (VideoEffectService::setup() either finishes or it
	// doesn't, synchronously, before any scene can query activity status)
	// -- see deriveEffectHealth()'s own comment for exactly which
	// VideoEffectLoadReport/import conditions map to which value below.
	enum class EffectHealth : uint8_t {
		Ready,
		Degraded,
		Failed
	};

	struct EffectActivityStatus {
		uint32_t schemaVersion = 1;

		EffectHealth health = EffectHealth::Ready;
		// Stable, vocabulary-resolvable message id (never raw exception/log
		// text -- same discipline as SceneHudStatus::message and
		// VideoPlaybackStatus::messageId) explaining a non-Ready health
		// value. Always nullopt when health == Ready.
		std::optional<std::string> messageId;

		std::vector<EffectActivitySlot> slots;
	};

	// health == Ready with EMPTY slots is a fully valid, common state ("the
	// effect system is fine, nothing happens to be active right now") -- the
	// two fields are independent. A caller must never infer Failed/Degraded
	// merely because slots is empty, and nothing in this header does.

	struct DominanceConfig {
		// Caps how many labels/ids resolveDominantEffectLabels()/
		// resolveDominantEffectIds() return — the HUD's effects.active/
		// activeEffects line is meant to read as a short curated summary
		// (HUD-Initiative-Scope-and-Product-Direction-Addendum.md §10:
		// "exposing only a small number of direct controls"/curated
		// density), not an enumeration of every internal effect slot
		// (quadrant-crosshair alone can have four).
		std::size_t maxLabels = 2;

		// Slots below this prominence are dropped before dominance
		// resolution runs — keeps near-zero-salience slots (e.g. a
		// barely-visible quadrant) from crowding out genuinely dominant ones.
		float minProminenceToShow = 0.05f;

		// Appends a short suffix to a label when its dominant slot is
		// actively transitioning, so the HUD can show "shifting" without the
		// scene needing its own transition-detection logic. Only affects
		// resolveDominantEffectLabels() — resolveDominantEffectIds() never
		// appends display text of any kind to an ID.
		bool annotateTransitioning = true;
		std::string transitioningSuffix = " (shifting)";
	};

	// A merged, ranked effect group — the semantic-ID counterpart to the
	// label strings resolveDominantEffectLabels() returns. Ordered
	// (front() == most dominant) per the same dominance-resolution rules;
	// see resolveDominantEffectIds()'s own comment.
	struct DominantEffectResult {
		std::string effectId;
		bool transitioning = false;
		float transitionProgress01 = 1.0f;
		float prominence = 0.0f; // merged (max of member slots'), see resolveDominantEffectIds()
	};

	// Dominance-resolution rules (shared by both resolveDominantEffectIds()
	// and resolveDominantEffectLabels()):
	//   1. Drop slots with prominence < minProminenceToShow.
	//   2. Group remaining slots by effectId — multiple slots running the
	//      same effect (e.g. two quadrants both on "recolor") collapse to
	//      one result rather than repeating it. A group's prominence is the
	//      MAX of its member slots' prominence (not the sum) — running the
	//      same effect in more places should not out-rank a single
	//      dramatically prominent different effect just by slot count.
	//   3. Sort groups by prominence descending; ties break by the
	//      lexicographically smallest slotId in the group, for a
	//      deterministic, reproducible order across calls with identical
	//      input (no reliance on map/set iteration order).
	//   4. Take the top maxLabels groups.
	//   5. A group is "transitioning" if ANY member slot is
	//      Transitioning (a scene mid-shift should read as active, not
	//      idle, even if only one of its slots is moving); its
	//      transitionProgress01 is that (first-found) transitioning
	//      member's progress.
	//
	// Semantic-ID form — feeds docs/HUD-Semantic-Slot-Model-v1.md §12.6's
	// effects.active/effects.dominant/effects.transition.progress. Returns
	// canonical effect IDs only, never a display name or annotated string —
	// vocabulary/label resolution belongs downstream of this call, not here.
	std::vector<DominantEffectResult> resolveDominantEffectIds(
		const EffectActivityStatus & status, const DominanceConfig & config = DominanceConfig{});

	// Display-string form — pre-formatted, English, ready to assign directly
	// to the EXISTING, frozen SceneHudStatus::activeEffects field. Do not
	// feed this into a new effects.* slot; see this file's header comment.
	std::vector<std::string> resolveDominantEffectLabels(
		const EffectActivityStatus & status, const DominanceConfig & config = DominanceConfig{});

// Derives EffectHealth from the ONE real, Shared-Effects-owned condition
	// this subsystem can honestly report on without depending on scene,
	// HUD, or RuntimeServices state (per the Architecture-Closure Session's
	// own instruction: "Do not make health depend on unrelated scene/
	// runtime/HUD state"):
	//
	//   Failed   : loadReport has any `missing` or `failed` effect --
	//              something the manifest asked for could not be made
	//              usable at all (VideoEffectLoadReport.h's own doc:
	//              "Failed effects must never remain marked usable").
	//   Degraded : loadReport is otherwise ok but used a `fallback`
	//              substitution for at least one effect, OR the caller
	//              reports its own most recent knowledge-pack import was
	//              rejected (schemaVersion unsupported / malformed) --
	//              the effect system still runs, just not with the
	//              curated knowledge it expected.
	//   Ready    : everything else -- includes the common case of a
	//              missing (never-yet-exported) knowledge pack, which is
	//              an explicitly NOT-a-failure state throughout this
	//              subsystem (see EffectKnowledgePackImportReport::ok's own
	//              comment) and must not downgrade health.
	//
	// knowledgePackRejected defaults to false so a caller with no pack
	// concept at all (or one that hasn't attempted an import yet) gets the
	// correct Ready/Failed answer from loadReport alone.
	EffectHealth deriveEffectHealth(const VideoEffectLoadReport & loadReport, bool knowledgePackRejected = false);

} // namespace videoeffects
