#pragma once

// ============================================================================
// FakeHudScenarioBase.h — shared phase-schedule + frame-assembly logic for
// every Fake*Scenario, so the required-case list (this task's §11) is
// implemented once, not six times.
//
// Each of the 14 required cases —
//   nominal cycle; boundary values; missing optional data;
//   empty flexible data; Ready; Loading; Degraded; Failed;
//   manager transitions; scene epoch change; generation change;
//   longest valid strings; maximum effect list; maximum metric count
// — is one FakeCasePhase, deterministically selected from `elapsedSeconds`
// alone (see phaseForElapsed()/elapsedSecondsForPhase()) so the Validation
// Studio (or a test) can land on any specific case just by stepping to the
// right elapsed-time value — no scenario-specific "which second is
// Failed?" knowledge needed anywhere else in this domain.
//
// Only buildBaseline() is scene-specific; everything else (which fields
// get nulled for MissingOptional, padded for MaxMetricCount, etc.) is
// identical machinery shared by all six concrete scenarios.
// ============================================================================

#include "../FakeHudSemanticTypes.h"
#include "IFakeHudScenario.h"

#include <cstdint>
#include <string>
#include <vector>

namespace hudpresent {

enum class FakeCasePhase : int {
	Nominal = 0,
	Boundary,
	MissingOptional,
	EmptyFlexible,
	Ready,
	Loading,
	Degraded,
	Failed,
	ManagerTransition,
	SceneEpochChange,
	GenerationChange,
	LongestStrings,
	MaxEffectList,
	MaxMetricCount,
	kCount
};

const char* fakeCasePhaseName(FakeCasePhase phase);

class FakeHudScenarioBase : public IFakeHudScenario {
public:
	// Every phase gets an equal, fixed-length slot on the elapsedSeconds
	// timeline — deterministic and independent of frame rate (callers
	// step `elapsedSeconds` directly, in fixed-dt or jump-to-phase mode).
	static constexpr float kPhaseDurationSeconds = 4.0f;
	static constexpr float kTotalCycleSeconds = kPhaseDurationSeconds * static_cast<int>(FakeCasePhase::kCount);

	static FakeCasePhase phaseForElapsed(float elapsedSeconds);
	static float elapsedSecondsForPhase(FakeCasePhase phase, float offsetWithinPhase = 0.0f);

	std::string scenarioId() const override { return sceneId_; }
	void reset(uint64_t seed) override { seed_ = seed; }
	FakeHudFrameData sample(float elapsedSeconds) override;

protected:
	FakeHudScenarioBase(std::string sceneId, std::string displayTitleId);

	struct SceneBaseline {
		std::string primaryStateId;
		std::vector<std::string> alternateStateIds; // for GenerationChange / cycling variety
		// Real ::SceneMetric (shared/src/scene/SceneSemanticTypes.h) —
		// Engineering Session 2 reconciliation: fake scenarios build the
		// same metric type production code consumes, not a private mirror
		// (see FakeHudSemanticTypes.h's header comment).
		std::vector<SceneMetric> metrics;
		// Already-curated display TEXT, mapped directly onto
		// SceneHudStatus::activeEffects — NOT raw effect IDs needing
		// vocabulary resolution (see FakeHudSemanticTypes.h).
		std::vector<std::string> effectIds;
	};

	// Pure function of (t, seed) — no I/O, no wall-clock, no mutable
	// scenario state beyond what reset(seed) already fixed. Represents
	// "what does a normal (Nominal-phase) frame of this scene look like
	// at time t".
	virtual SceneBaseline buildBaseline(float t, uint64_t seed) const = 0;

	uint64_t seed() const { return seed_; }

private:
	std::string sceneId_;
	std::string displayTitleId_;
	uint64_t seed_ = 0;
};

} // namespace hudpresent
