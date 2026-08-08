#pragma once

// ============================================================================
// HudSourceResolver.h — resolves stable source IDs against one immutable
// FakeHudFrameData snapshot into typed HudResolvedValue results.
//
// Responsibilities (per this task's spec):
//   - receives one immutable fake/adapter FakeHudFrameData value at resolve
//     time (never stored, never mutated);
//   - exposes the ~40 universal source IDs listed in the task;
//   - exposes scene metrics by metric ID (scene.<scene-id>.metric.<name>);
//   - returns typed values and data classifications;
//   - distinguishes missing from numeric zero (HudResolvedValue::present);
//   - distinguishes a missing list from an empty list
//     (std::nullopt from resolve() vs. a present-but-empty IdentifierList);
//   - never returns references into mutable services — every result is
//     returned by value;
//   - never knows widget types — HudWidgetType is not referenced anywhere
//     in this file;
//   - never branches on scene ID — the canonical-ID resolution table below
//     is identical for every scene; only the scene-metric fallback reads
//     `frame.scene.sceneId`, and only to construct the metric's own
//     namespaced ID prefix for a same-named-metric-across-scenes
//     disambiguation, never to change resolution *behavior* per scene.
// ============================================================================

#include "FakeHudSemanticTypes.h"
#include "HudDataTypes.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace hudpresent {

class HudSourceResolver {
public:
	HudSourceResolver();

	// Resolves `sourceId` against `frame`. Returns std::nullopt only when
	// `sourceId` is entirely unrecognized — neither one of the fixed
	// canonical IDs nor a syntactically-scene-metric-shaped ID
	// ("scene.<sceneId>.metric.<name>") that HudSourceResolver at least
	// recognizes the *shape* of. A recognized-but-unpopulated source
	// returns HudResolvedValue{present=false}, never std::nullopt — that
	// distinction is exactly "unknown source" vs. "missing value" (see
	// this task's test-list items of the same names).
	std::optional<HudResolvedValue> resolve(const std::string& sourceId, const FakeHudFrameData& frame) const;

	// True for the fixed set of ~40 universal IDs this resolver ships
	// with — the set HudProfileCompiler can validate *statically*, at
	// profile-compile time, without any concrete FakeHudFrameData in
	// hand. Scene-metric IDs are deliberately excluded: which metric IDs
	// a given scene actually populates is only known once a concrete
	// snapshot exists, so the compiler treats them as dynamically
	// validated instead (see HudProfileCompiler.h).
	static bool isKnownCanonicalSource(const std::string& sourceId);

	// The static HudSourceValueType a canonical source is declared to
	// produce, independent of any concrete frame — used by
	// HudProfileCompiler's source-value-type-compatibility check. Returns
	// std::nullopt for anything isKnownCanonicalSource() would also
	// reject.
	static std::optional<HudSourceValueType> staticValueTypeOf(const std::string& sourceId);

	// Every canonical ID this resolver recognizes, in the fixed order
	// they're declared in the .cpp — useful for the Validation Studio's
	// "inspect resolved source IDs" panel and for exhaustive tests.
	static const std::vector<std::string>& allCanonicalSourceIds();

	// True if `sourceId` has the shape "scene.<id>.metric.<name>" — a
	// purely syntactic check, independent of any frame. Namespacing rule
	// per HUD-Semantic-Slot-Model-v1.md §13.
	static bool looksLikeSceneMetricId(const std::string& sourceId);

private:
	std::optional<HudResolvedValue> resolveCanonical(const std::string& sourceId, const FakeHudFrameData& frame) const;
	std::optional<HudResolvedValue> resolveSceneMetric(const std::string& sourceId, const FakeHudFrameData& frame) const;
};

} // namespace hudpresent
