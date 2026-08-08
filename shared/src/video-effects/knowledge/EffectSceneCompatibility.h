#pragma once

#include <string>
#include <vector>

// Scene compatibility for the shared video-effects catalog — Shared Effect
// Knowledge, scoped extension (see
// docs/shared-effect-knowledge-scoped-extension.md). Deliberately scene-level,
// not per-effect-per-scene: nothing in this repo's own investigation docs
// (CLAUDE.md's migration table, docs/video-effect-second-wave-evaluation.md)
// describes an effect that's incompatible with a *specific* migrated scene —
// the documented boundary is always "does this scene consume the shared
// VideoEffectService at all," which is coarser than per-effect. Per-effect
// eligibility within a consuming scene is already covered by
// VideoEffectCapabilities::safeForAutomaticSelection
// (core/VideoEffectCapabilities.h) — this file does not duplicate that.
//
// Every fact below is transcribed from an existing source document, not
// invented. See each entry's `evidence` string for its citation. Where the
// docs and a direct grep of this pass disagreed (blob-region-prototype, see
// EffectSceneCompatibility.cpp), the discrepancy is recorded rather than
// silently resolved — flag it in a report, don't guess.
namespace videoeffects {

	// How a scene relates to the shared VideoEffectService, per
	// CLAUDE.md's "Which sketches use the shared service" table.
	enum class EffectSceneSupportLevel {
		SharedServiceConsumer, // fixed-default uniforms source from the shared catalog
		LocalForkOnly,         // scene-local effect system, deliberately not migrated
		NotIntegrated          // no shared-service or local-fork effect system at all
	};

	const char * toString(EffectSceneSupportLevel level);

	struct EffectSceneCompatibilityEntry {
		std::string sceneId; // matches IEcopunkScene::sceneId() / the roadmap's six scene ids
		EffectSceneSupportLevel supportLevel = EffectSceneSupportLevel::NotIntegrated;

		// Every field below is explicitly "not yet measured" rather than a
		// guessed value — Scene-HUD-Contract-v1.md §16 is direct about this:
		// "nothing in this contract has been measured on real Pi 3B hardware."
		// Absence of Pi validation is the honest default, not a gap to paper
		// over with an assumed true/false.
		bool piValidated = false;

		std::string notes;
		std::string evidence; // doc + section this entry's facts are drawn from
	};

	// The six roadmap scenes only (per Ecopunk-HUD-System-Master-Roadmap.md §1) —
	// radar-effects-gallery/radar-pulse/shader-effect-debugger/hud_validation_harness/
	// hud_elements/FireplaceWaterfall are explicitly out of the runtime scene
	// cycle and intentionally absent here.
	const std::vector<EffectSceneCompatibilityEntry> & defaultSceneCompatibilityMatrix();

	const EffectSceneCompatibilityEntry * findSceneCompatibility(
		const std::vector<EffectSceneCompatibilityEntry> & matrix, const std::string & sceneId);

	// Convenience predicate used by EffectActivityStatus/production-scene
	// integration: true only for scenes confirmed to consume the shared
	// catalog. LocalForkOnly and NotIntegrated both return false — a scene
	// with its own local fork should not be offered shared-catalog effect
	// selection just because a fork of similar effects exists locally.
	bool sceneConsumesSharedEffects(const std::vector<EffectSceneCompatibilityEntry> & matrix, const std::string & sceneId);

} // namespace videoeffects
