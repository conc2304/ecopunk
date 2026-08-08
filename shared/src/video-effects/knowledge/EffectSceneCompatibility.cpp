#include "EffectSceneCompatibility.h"

namespace videoeffects {

	const char * toString(EffectSceneSupportLevel level) {
		switch (level) {
			case EffectSceneSupportLevel::SharedServiceConsumer: return "SharedServiceConsumer";
			case EffectSceneSupportLevel::LocalForkOnly: return "LocalForkOnly";
			case EffectSceneSupportLevel::NotIntegrated: return "NotIntegrated";
		}
		return "Unknown";
	}

	const std::vector<EffectSceneCompatibilityEntry> & defaultSceneCompatibilityMatrix() {
		static const std::vector<EffectSceneCompatibilityEntry> matrix = {
			{
				"blob-region-prototype",
				EffectSceneSupportLevel::SharedServiceConsumer,
				false,
				"Documented as migrated by CLAUDE.md's table, but this pass's own "
				"grep for videoeffects::/VideoEffect* under sketches/blob-region-prototype/src "
				"found zero matches — the sketch links shared/src (no PROJECT_EXCLUSIONS "
				"on video-effects) but this pass could not directly confirm a live call "
				"site. Left as SharedServiceConsumer per the documented source of truth "
				"rather than silently overridden; flagged as an open verification item, "
				"not resolved here.",
				"CLAUDE.md 'Which sketches use the shared service' table (unconfirmed by direct grep, this pass)",
			},
			{
				"contour-portrait",
				EffectSceneSupportLevel::NotIntegrated,
				false,
				"No effect registry of any kind; ContourDisplacementEffect feeds one "
				"fixed GPU preprocessing pass into a CPU-side displacement algorithm. "
				"ContourPresets writes directly into ofParameters, no shader-swapping "
				"concept exists to migrate onto the shared registry.",
				"docs/video-effect-second-wave-evaluation.md, 'contour-portrait (CP)'",
			},
			{
				"temporal-fields",
				EffectSceneSupportLevel::SharedServiceConsumer,
				false,
				"Migrated — fixed-default uniforms source from the shared catalog. "
				"Confirmed by this pass: sketches/temporal-fields/src/TFEffectPicker.cpp "
				"references the shared video-effects service directly.",
				"CLAUDE.md 'Which sketches use the shared service' table; confirmed by grep this pass",
			},
			{
				"fragment-trail",
				EffectSceneSupportLevel::LocalForkOnly,
				false,
				"Deliberately not migrated — local ShaderLibrary/LFOBank/etc. forks "
				"collide by name with shared/src's top-level classes. "
				"config.make explicitly excludes ../../shared/src/video-effects (with a trailing wildcard) via "
				"PROJECT_EXCLUSIONS (confirmed this pass). Real migration path is a "
				"local-class-rename/collision-resolution pass, not attempted here.",
				"CLAUDE.md 'Which sketches use the shared service' table; sketches/fragment-trail/config.make (confirmed this pass)",
			},
			{
				"quadrant-crosshair",
				EffectSceneSupportLevel::SharedServiceConsumer,
				false,
				"Production path is migrated (confirmed this pass: Quadrant.cpp and "
				"ShaderLibrary.h reference the shared video-effects service). Its "
				"DebugMode is explicitly NOT migrated and stays out of scope — every "
				"parameter there is live-tunable via GUI-bound state, and its real "
				"migration path is retirement in favor of shader-effect-debugger, not "
				"a mechanical refactor onto this compatibility entry.",
				"CLAUDE.md 'Which sketches use the shared service' table; confirmed by grep this pass",
			},
			{
				"blueprint_emergence",
				EffectSceneSupportLevel::SharedServiceConsumer,
				false,
				"Migrated — fixed-default uniforms source from the shared catalog. "
				"Confirmed by this pass: sketches/blueprint_emergence/src/BEFragment.cpp "
				"references the shared video-effects service directly.",
				"CLAUDE.md 'Which sketches use the shared service' table; confirmed by grep this pass",
			},
		};
		return matrix;
	}

	const EffectSceneCompatibilityEntry * findSceneCompatibility(
		const std::vector<EffectSceneCompatibilityEntry> & matrix, const std::string & sceneId) {
		for (const auto & entry : matrix) {
			if (entry.sceneId == sceneId) return &entry;
		}
		return nullptr;
	}

	bool sceneConsumesSharedEffects(const std::vector<EffectSceneCompatibilityEntry> & matrix, const std::string & sceneId) {
		const EffectSceneCompatibilityEntry * entry = findSceneCompatibility(matrix, sceneId);
		return entry != nullptr && entry->supportLevel == EffectSceneSupportLevel::SharedServiceConsumer;
	}

} // namespace videoeffects
