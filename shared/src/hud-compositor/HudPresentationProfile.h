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

#include <cstdint>
#include <string>
#include <vector>

namespace hudpresent {

// Declarative, read-only opt-in: "this region is an interactive control group". The profile only
// SAYS the region is interactive and how many selectable rows it hosts; the renderer derives the
// look from the active skin and the interaction phase (idle / focused / active / disabled), and
// ExperienceRuntime owns the input + focus state. It carries no colours, no scene pointers and
// no callbacks. Absent (the default) = passive region: never focusable, never highlighted.
enum class HudInteractionKind : uint8_t {
	SelectorGroup, // N stacked option rows; Up/Down picks the row, Left/Right steps its value
};

struct HudInteractiveRegionDecl {
	std::string regionId;
	HudInteractionKind kind = HudInteractionKind::SelectorGroup;
	int rowCount = 0; // selectable rows, in binding order within the region
};

struct ScenePresentationProfile {
	std::string profileId; // a sceneId for flexible profiles; "universal"/"overlay.health"/"overlay.transition" otherwise
	std::vector<HudSlotBinding> bindings;
	std::vector<HudInteractiveRegionDecl> interactiveRegions; // empty for every scene except those that opt in
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
