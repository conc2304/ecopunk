#pragma once

class ShaderLibrary;

// Proves TFEffectPicker's new production-selector adoption (Shared Effects
// "Production Selector / Eligibility Adoption" increment): that
// applyEligibleCanonicalPreset() genuinely routes through the real,
// unmodified EffectKnowledgePrecedence.h API (resolveCompatibility(),
// isEligibleForAutomaticProductionSelection(), EffectPresetId.h's
// isReusableAuthoredPreset()) rather than a local reimplementation of
// those rules, and that the existing blacklist/fallback behavior is
// preserved when it applies.
//
// Same rationale as TFActivityStatusSelfTest.h: run automatically at
// ofApp::setup(), against dedicated scratch TFEffectPicker instances whose
// EffectKnowledgeBase is seeded directly (via knowledgeBaseForTest()) with
// hand-constructed KnowledgeEntry/EffectLevelKnowledge values -- this
// proves the real compiled selection code path deterministically, without
// depending on whatever happens to be in the real canonical pack on disk
// at test time.
namespace videoeffects {
	bool runEligibilitySelfTest(ShaderLibrary & sharedShaderLib);
}
