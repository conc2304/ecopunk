#pragma once

#include <string>
#include <vector>

// Startup load reporting per docs/shared-video-effect-architecture.md §7:
// "must include requested, registered, skipped, missing, failed,
// unsupported, and fallback effects. Failed effects must never remain
// marked usable." VideoEffectService populates one of these during setup()
// from a sketch's effect-manifest.json against the canonical registry.
namespace videoeffects {

	struct VideoEffectLoadReport {
		std::vector<std::string> requested; // every effect id the manifest names
		std::vector<std::string> registered; // requested, enabled, found in the registry, assets present
		std::vector<std::string> skipped; // present in manifest, enabled=false
		std::vector<std::string> missing; // requested but no such id in the canonical registry
		std::vector<std::string> failed; // registered but a required asset file is absent
		std::vector<std::string> unsupported; // registered, assets present, but capability mismatch for this platform
		std::vector<std::string> fallback; // effect substituted with a safe fallback

		bool allOk() const { return missing.empty() && failed.empty(); }
		std::string summary() const;
	};

} // namespace videoeffects
