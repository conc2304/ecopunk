#pragma once

// ============================================================================
// HudMissingDataController.h — centralized missing-data handling.
//
// One controller, one decision method, called once per compiled binding
// per frame by the orchestrator (HudWireframeRenderer) — never inside a
// widget (per this task's §7: "Widgets must not… choose source
// fallbacks"). Encodes every rule from this task's §9:
//
//   - required identity fallback         -> HudMissingPolicy::Placeholder
//   - optional hide                      -> HudMissingPolicy::Hide
//   - placeholder                        -> HudMissingPolicy::Placeholder
//   - retain last valid                  -> HudMissingPolicy::RetainLastValid
//   - dim last valid                     -> HudMissingPolicy::DimLastValid
//   - fallback source                    -> HudMissingPolicy::UseFallbackSource
//   - ambient fallback                   -> HudMissingPolicy::UseAmbientFallback
//   - never insert zero into a history for a missing source
//       -> enforced by HudHistoryStore itself (pushSample takes
//          std::optional<float>; nullopt always writes a gap marker,
//          never 0.0f) — this controller's job is only to decide what to
//          *show*, not to touch history directly.
//   - loading pauses histories and may dim last-valid values
//       -> decide() returns dimmed=true whenever health==Loading and a
//          policy would otherwise show a live value; the orchestrator is
//          responsible for calling HudHistoryStore::pause(sourceId, true)
//          while health==Loading (see HudWireframeRenderer).
//   - failure suppresses live traces
//       -> decide() returns shouldRender=false for Sparkline-typed
//          bindings whenever health==Failed, regardless of missingPolicy.
//   - missing and empty lists remain distinct
//       -> HudResolvedValue::present==false (missing) is never conflated
//          with present==true && listValue.empty()==true (empty); see
//          regionNeedsAmbientFallback()'s own distinction between the two.
//   - empty flexible region may instantiate one ambient fallback / one
//     missing optional binding must not automatically trigger it
//       -> regionNeedsAmbientFallback(), a separate, region-level (not
//          per-binding) decision — see its own comment.
//   - invalid flexible bindings must not remove universal HUD content
//       -> structural, enforced by HudProfileCompiler always including
//          the universal profile's bindings regardless of the flexible
//          profile's validity (see HudProfileCompiler.cpp); nothing in
//          this controller needs to re-enforce it.
// ============================================================================

#include "FakeHudSemanticTypes.h"
#include "HudCompiledProfile.h"
#include "HudDataTypes.h"

#include <optional>
#include <vector>

namespace hudpresent {

class HudMissingDataController {
public:
	struct Outcome {
		bool shouldRender = false;
		bool useAmbientFallback = false;
		bool dimmed = false;
		HudResolvedValue valueToShow; // meaningful only when shouldRender && !useAmbientFallback
	};

	// `resolvedNow` is std::nullopt only for a genuinely unknown source
	// (HudSourceResolver::resolve() returned nullopt) — a recognized-but-
	// currently-unpopulated source arrives as
	// HudResolvedValue{present=false}, which this method still treats as
	// "missing" for policy purposes, distinctly from an unknown source
	// (which always hides, regardless of policy — there is nothing valid
	// to retain/placeholder/fallback to).
	//
	// `lastValid` is provided/updated by the caller across frames — this
	// controller is stateless and does not itself remember history.
	//
	// `resolveFallback` lets RetainLastValid/DimLastValid consult a
	// caller-supplied resolver for `binding.fallbackSourceId` without this
	// controller needing to hold a HudSourceResolver + FakeHudFrameData
	// itself; pass nullptr if the binding has no fallback source (the
	// UseFallbackSource policy degrades to Hide in that case).
	Outcome decide(
		const HudCompiledBinding& compiled,
		const std::optional<HudResolvedValue>& resolvedNow,
		const std::optional<HudResolvedValue>& lastValid,
		FakeSceneHealth sceneHealth,
		const std::optional<HudResolvedValue>& fallbackValue) const;

	// Region-level (not per-binding) decision: true only when EVERY
	// binding assigned to `regionId` in `outcomesInRegion` failed to
	// render this frame (all shouldRender==false) AND the region actually
	// has at least one binding assigned (an intentionally-unbound region
	// never gets an ambient fallback — that would be scope creep into
	// regions no profile ever claimed). A single missing-and-hidden
	// optional binding alongside at least one other binding that DID
	// render must not trigger this.
	static bool regionNeedsAmbientFallback(const std::vector<Outcome>& outcomesInRegion);
};

} // namespace hudpresent
