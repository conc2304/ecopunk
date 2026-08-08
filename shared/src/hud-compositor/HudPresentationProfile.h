#pragma once

// ============================================================================
// HudPresentationProfile.h — compiled-C++ profile organization.
//
// Per this task's §5: one universal wireframe profile, six scene-specific
// flexible profiles, one health overlay profile, one transition overlay
// profile — no universal binding duplicated inside any scene profile
// (universal bindings live ONLY in universalProfile(); the six scene
// profiles contain ONLY their flexible-region bindings). The compiler
// (HudProfileCompiler) is what merges universal + the scene-selected
// flexible profile + both overlays into one HudCompiledProfile per scene.
//
// Profiles are compiled C++ configuration in this increment — no JSON
// profile parsing, per this task's explicit instruction.
//
// Widgets never receive the scene ID: HudPresentationProfileRegistry is
// the ONLY place scene ID selects anything. Once
// flexibleProfileForScene(sceneId) has returned a ScenePresentationProfile,
// every downstream consumer (HudProfileCompiler, the widget layer) works
// from bindings/regions/sources alone.
// ============================================================================

#include "HudPresentationTypes.h"

#include <string>
#include <vector>

namespace hudpresent {

struct ScenePresentationProfile {
	std::string profileId; // a sceneId for flexible profiles; "universal"/"overlay.health"/"overlay.transition" otherwise
	std::vector<HudSlotBinding> bindings;
};

class HudPresentationProfileRegistry {
public:
	HudPresentationProfileRegistry();

	const ScenePresentationProfile& universalProfile() const { return universal_; }
	const ScenePresentationProfile& overlayHealthProfile() const { return overlayHealth_; }
	const ScenePresentationProfile& overlayTransitionProfile() const { return overlayTransition_; }

	// nullptr if sceneId names a scene this registry has no flexible
	// profile for (e.g. a typo, or a scene outside the six-scene runtime
	// scope) — HudProfileCompiler turns that into a compile-time error
	// rather than silently falling back to an empty profile.
	const ScenePresentationProfile* flexibleProfileForScene(const std::string& sceneId) const;

	std::vector<std::string> knownSceneIds() const;

private:
	ScenePresentationProfile universal_;
	ScenePresentationProfile overlayHealth_;
	ScenePresentationProfile overlayTransition_;
	std::vector<ScenePresentationProfile> flexibleProfiles_; // one per in-scope scene
};

} // namespace hudpresent
