#pragma once

// ============================================================================
// HudVocabularyResolver.h
//
// Resolution order (per this task's §8, itself matching
// HUD-Semantic-Slot-Model-v1.md §14.4's draft proposal):
//
//     scene override → selected test pack → canonical default → stable ID
//
// This is a compiled TEST vocabulary for this task, per the explicit
// instruction "Use a compiled test vocabulary for this task." It is NOT
// the canonical shared vocabulary docs/probes/hud-runtime-validation-
// studio-probe.md §9 found does not exist anywhere in the repo — this is
// that from-scratch infrastructure, scoped to what this increment's
// profiles actually reference, not a claim of completeness.
//
// Caching: resolve() results are cached (keyed by scene+pack+key) and
// invalidated only when the pack selection or a table changes — never
// re-parsed or re-looked-up during draw, per this task's §8/§15
// performance requirements. In this increment resolve() is called only
// from HudProfileCompiler (once per compile, not per frame) and the
// widget layer's value-resolution path when it needs to turn a resolved
// Identifier's raw text into display text.
// ============================================================================

#include <string>
#include <unordered_map>

namespace hudpresent {

class HudVocabularyResolver {
public:
	HudVocabularyResolver();

	void setCanonicalDefaults(std::unordered_map<std::string, std::string> table);
	void setPack(const std::string& packId, std::unordered_map<std::string, std::string> table);
	void selectPack(const std::string& packId); // "" clears selection
	const std::string& selectedPack() const { return selectedPackId_; }

	void setSceneOverrides(const std::string& sceneId, std::unordered_map<std::string, std::string> table);
	void clearSceneOverrides(const std::string& sceneId);

	// Resolves `key` for `sceneId` (empty sceneId = no scene context, e.g.
	// universal/overlay bindings) through the four-tier order above.
	// Always returns a usable string — the stable-ID fallback tier means
	// this never fails to resolve to *something*, even for a completely
	// unregistered key (falls back to `key` itself).
	const std::string& resolve(const std::string& sceneId, const std::string& key) const;

	// True only if `key` resolved through scene override, the selected
	// pack, or the canonical default tier — false if it fell all the way
	// through to the stable-ID fallback. Used by HudProfileCompiler to
	// raise a Warning-severity "vocabulary key unresolved" issue without
	// blocking compilation (falling back to the ID is a legitimate,
	// functional degraded outcome, not a hard failure).
	bool resolvedThroughVocabulary(const std::string& sceneId, const std::string& key) const;

	void clearCache();

private:
	struct Lookup {
		bool found = false;
		std::string value;
	};
	Lookup lookupUncached(const std::string& sceneId, const std::string& key) const;

	std::unordered_map<std::string, std::string> canonical_;
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> packs_;
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sceneOverrides_;
	std::string selectedPackId_;

	mutable std::unordered_map<std::string, Lookup> cache_; // key: sceneId + "\x1f" + selectedPackId_ + "\x1f" + key
};

} // namespace hudpresent
