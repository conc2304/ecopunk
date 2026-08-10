#pragma once

#include "SceneSemanticTypes.h"

#include <cstdint>
#include <optional>
#include <string>

// TemporalSemanticMapping.h — Temporal Production Scene #2 Migration.
//
// Pure, openFrameworks-free extraction of Temporal's semantic-value
// derivation, mirroring the established shape of
// sketches/experience_runtime/src/BlobSemanticMapping.h (that file's own
// header comment explains why this pattern exists — testable with a bare
// compiler, no OF/GL context). Scene-local, domain-owned code — NOT a
// shared/frozen contract; does not live in shared/src/scene/, does not
// change SceneSemanticTypes.h, and is referenced only by
// TemporalProductionScene.
//
// Every formula here is honest and narrow, per this session's migration
// prompt §10: only the frozen, already-reserved Temporal semantic IDs are
// populated (see docs/shared-project-docs/blob-temporal-blob-hud-
// acceptance-matrix.md and shared/src/hud-compositor/HudVocabularyResolver.cpp/
// fake/FakeTemporalScenario.cpp for the exact reserved ID list this mirrors),
// and a metric is left absent — never zero-filled — whenever no honest,
// already-computed source exists for it. See compute()'s own comment for
// the field-by-field source/formula/class/missing-behavior table (also
// reproduced in this session's completion report).
namespace temporalsemantics {

// Real, already-computed TemporalSceneCore/TFComposition/
// TimeOffsetVideoBuffer state — every field here has an honest, cheap
// source at TemporalProductionScene::buildSemanticData()'s call site (see
// that method's own comment for the exact accessor each field reads).
struct Inputs {
	// TFComposition::getPhase() == RUNNING vs PATTERN_TRANSITION.
	bool isTransitioning = false;

	// TFComposition::getPhaseElapsed() — real elapsed time within the
	// current RUNNING/PATTERN_TRANSITION phase. There is no exposed,
	// honest denominator for a normalized stateProgress (TFComposition
	// does not expose its own transitionDuration/cycleDuration as a
	// getter), so this maps only to SceneTimingStatus::stateElapsedSeconds
	// — stateProgress is deliberately left absent (see compute()'s own
	// comment).
	float phaseElapsedSeconds = 0.0f;

	// TFComposition::getActivePatternType(), pre-lowered to this mapping's
	// own identifier convention (see compute()'s own comment) — passed as
	// an already-resolved string rather than the TFPatternType enum itself
	// so this OF-free header does not need to include TFPatternType.h.
	std::string activePatternId;

	// TimeOffsetVideoBuffer::getHistoryFrameCount()/getHistoryCapacityFrames().
	int historyFrameCount = 0;
	int historyCapacityFrames = 0;

	// TimeOffsetVideoBuffer::getNumPlayheads() — the real, currently
	// configured/active playhead count, never a hard-coded "6".
	int playheadCount = 0;

	float activeSeconds = 0.0f; // TemporalProductionScene's own real elapsed-active timer
	uint64_t generation = 0;    // TemporalProductionScene's own real activation counter
};

// Only ever called once the caller has already decided a semantic snapshot
// should be reported (scene set up and active) — see
// TemporalProductionScene::hudStatus()'s own absent/present decision, which
// is NOT part of this function.
SceneSemanticData compute(const Inputs& in);

} // namespace temporalsemantics
