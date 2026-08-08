#pragma once

// ============================================================================
// HudProfileCompiler.h — validates + compiles ScenePresentationProfiles
// into one HudCompiledProfile per scene.
//
// Runs only at setup or scene-profile change, per this task's §15
// performance requirement ("Compile profiles only at setup or
// scene-profile change") — never per frame, never per draw call.
// ============================================================================

#include "HudCompiledProfile.h"
#include "HudPresentationProfile.h"
#include "HudRegionCatalog.h"
#include "HudVocabularyResolver.h"

#include <string>
#include <vector>

namespace hudpresent {

class HudProfileCompiler {
public:
	HudProfileCompiler(const HudRegionCatalog& regions, HudVocabularyResolver& vocabulary);

	// Merges universalProfile() + overlayHealthProfile() +
	// overlayTransitionProfile() + flexibleProfileForScene(sceneId) (if
	// any) from `registry`, validating every binding against `regions_`
	// and this domain's per-widget-type role contract
	// (HudWidgetContract.h). `sceneId` selects which flexible profile to
	// include and is also passed to the vocabulary resolver as scene
	// context for the resolution order's "scene override" tier — this is
	// the ONLY place in this compilation path that reads `sceneId`;
	// individual bindings/widgets never see it.
	HudCompiledProfile compile(const std::string& sceneId, const HudPresentationProfileRegistry& registry) const;

	// Lower-level entry point compile() is built on top of — validates
	// and compiles an arbitrary, caller-supplied binding list directly,
	// without going through HudPresentationProfileRegistry. Exists so
	// tests can exercise compiler rules (in particular, mutually
	// exclusive flexible-region occupancy, which needs two bindings
	// deliberately targeting the same region — not something the fixed,
	// non-conflicting production profiles ever do) without needing to
	// fabricate a whole fake registry.
	HudCompiledProfile compileBindings(const std::string& sceneId, const std::vector<HudSlotBinding>& bindings) const;

private:
	void validateAndAppend(const std::string& sceneId, const HudSlotBinding& binding, HudCompiledProfile& out) const;
	void checkFlexibleOccupancy(HudCompiledProfile& out) const;

	const HudRegionCatalog& regions_;
	HudVocabularyResolver& vocabulary_;
};

} // namespace hudpresent
