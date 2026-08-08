#include "FakeHudScenarioBase.h"

#include "FakeFrameMath.h"

#include <algorithm>
#include <cmath>

namespace hudpresent {

namespace {

using namespace fakemath;

// Channel IDs — arbitrary but stable, so re-running the same (seed, t)
// always perturbs the same "random stream" for the same logical signal.
enum Channel : uint32_t {
	ChActivityOverall = 0,
	ChActivityMotion,
	ChActivityDensity,
	ChActivityVariation,
	ChActivityTransition,
	ChStateProgress,
	ChTimingStateProgress,
	ChMediaPlayback,
	ChMediaHold,
	ChFps,
	ChBoundaryPick,
	ChMetricValue,
	ChManagerPhasePick,
	ChEffectPick,
};

std::string longPlaceholder(const std::string& seed) {
	std::string out;
	out.reserve(96);
	while (out.size() < 90) out += seed + "_";
	out.resize(90);
	return out;
}

} // namespace

const char* fakeCasePhaseName(FakeCasePhase phase) {
	switch (phase) {
		case FakeCasePhase::Nominal: return "Nominal";
		case FakeCasePhase::Boundary: return "Boundary";
		case FakeCasePhase::MissingOptional: return "MissingOptional";
		case FakeCasePhase::EmptyFlexible: return "EmptyFlexible";
		case FakeCasePhase::Ready: return "Ready";
		case FakeCasePhase::Loading: return "Loading";
		case FakeCasePhase::Degraded: return "Degraded";
		case FakeCasePhase::Failed: return "Failed";
		case FakeCasePhase::ManagerTransition: return "ManagerTransition";
		case FakeCasePhase::SceneEpochChange: return "SceneEpochChange";
		case FakeCasePhase::GenerationChange: return "GenerationChange";
		case FakeCasePhase::LongestStrings: return "LongestStrings";
		case FakeCasePhase::MaxEffectList: return "MaxEffectList";
		case FakeCasePhase::MaxMetricCount: return "MaxMetricCount";
		case FakeCasePhase::kCount: break;
	}
	return "Unknown";
}

FakeCasePhase FakeHudScenarioBase::phaseForElapsed(float elapsedSeconds) {
	if (elapsedSeconds < 0.0f) elapsedSeconds = 0.0f;
	int totalPhases = static_cast<int>(FakeCasePhase::kCount);
	int index = static_cast<int>(std::floor(elapsedSeconds / kPhaseDurationSeconds)) % totalPhases;
	if (index < 0) index += totalPhases;
	return static_cast<FakeCasePhase>(index);
}

float FakeHudScenarioBase::elapsedSecondsForPhase(FakeCasePhase phase, float offsetWithinPhase) {
	offsetWithinPhase = std::clamp(offsetWithinPhase, 0.0f, kPhaseDurationSeconds - 0.001f);
	return static_cast<float>(static_cast<int>(phase)) * kPhaseDurationSeconds + offsetWithinPhase;
}

FakeHudScenarioBase::FakeHudScenarioBase(std::string sceneId, std::string displayTitleId)
	: sceneId_(std::move(sceneId)), displayTitleId_(std::move(displayTitleId)) {}

FakeHudFrameData FakeHudScenarioBase::sample(float elapsedSeconds) {
	const FakeCasePhase phase = phaseForElapsed(elapsedSeconds);
	const float phaseLocalT = elapsedSeconds - static_cast<float>(static_cast<int>(phase)) * kPhaseDurationSeconds;

	SceneBaseline baseline = buildBaseline(elapsedSeconds, seed_);

	FakeHudFrameData frame;
	frame.schemaVersion = 1;

	// -- Nominal defaults, overridden per phase below --------------------
	frame.scene.sceneId = sceneId_;
	frame.scene.displayTitleId = displayTitleId_;
	frame.scene.health = FakeSceneHealth::Ready;

	SceneSemanticData semantic;
	semantic.schemaVersion = 1;
	semantic.state.primaryStateId = baseline.primaryStateId;
	// NOTE: real ::SceneSemanticState has no `progress` field (Engineering
	// Session 2 reconciliation — see HudSourceResolver.cpp's
	// canonicalTable() comment) — secondaryStateId is the only other field.
	semantic.activity.overall = clamp01(smoothUnit(seed_, ChActivityOverall, elapsedSeconds));
	semantic.activity.motion = clamp01(smoothUnit(seed_, ChActivityMotion, elapsedSeconds, 0.7f));
	semantic.activity.density = clamp01(smoothUnit(seed_, ChActivityDensity, elapsedSeconds, 0.3f));
	semantic.activity.variation = clamp01(smoothUnit(seed_, ChActivityVariation, elapsedSeconds, 0.4f));
	semantic.activity.transition = clamp01(smoothUnit(seed_, ChActivityTransition, elapsedSeconds, 0.2f));
	semantic.timing.activeSeconds = elapsedSeconds;
	semantic.timing.stateElapsedSeconds = phaseLocalT;
	semantic.timing.stateProgress = clamp01(phaseLocalT / kPhaseDurationSeconds);
	semantic.timing.generation = 1 + (seed_ % 5);
	semantic.metrics = baseline.metrics;

	// Pick 1-3 active effects deterministically from the scene's pool —
	// maps directly onto the real SceneHudStatus::activeEffects (already
	// curated display TEXT, per FakeHudSemanticTypes.h's reconciliation
	// note), so these are nice-looking strings, not raw effect IDs.
	if (!baseline.effectIds.empty()) {
		int wantCount = 1 + static_cast<int>(hashUnit(seed_, ChEffectPick, static_cast<int64_t>(elapsedSeconds)) * 3.0f);
		for (int i = 0; i < wantCount && i < static_cast<int>(baseline.effectIds.size()) && i < static_cast<int>(kMaxActiveEffectIds); ++i) {
			frame.scene.activeEffectIds.push_back(baseline.effectIds[static_cast<size_t>(i)]);
		}
	}

	frame.scene.semantic = semantic;

	frame.manager.activeSceneId = sceneId_;
	frame.manager.transitionPhase = FakeSceneTransitionPhase::Idle;
	frame.manager.transitionProgress = 0.0f;

	VideoPlaybackStatus media;
	media.schemaVersion = 1;
	media.mediaId = std::string("media.sample_0") + std::to_string(1 + (seed_ % 3));
	media.titleId = media.mediaId;
	media.health = VideoPlaybackHealth::Ready;
	media.playbackProgress = clamp01(smoothUnit(seed_, ChMediaPlayback, elapsedSeconds, 0.1f));
	media.durationSeconds = 42.0f;
	media.positionSeconds = *media.playbackProgress * *media.durationSeconds;
	media.holdProgress = clamp01(smoothUnit(seed_, ChMediaHold, elapsedSeconds, 0.05f));
	media.holdDurationSeconds = 45.0f;
	media.holdElapsedSeconds = *media.holdProgress * *media.holdDurationSeconds;
	media.holdRemainingSeconds = (1.0f - *media.holdProgress) * *media.holdDurationSeconds;
	media.selectionOrigin = MediaSelectionOrigin::Automatic;
	media.canSelectPrevious = true;
	media.canSelectNext = true;
	frame.media = media;

	frame.runtime.fps = 28.0f + smoothUnit(seed_, ChFps, elapsedSeconds, 1.0f) * 4.0f;
	frame.runtime.frameTimeMs = 1000.0f / *frame.runtime.fps;
	frame.runtime.memoryBytes = 180.0f * 1024.0f * 1024.0f; // placeholder — no real Pi source, per the probe's own §12/§15 finding
	frame.runtime.temperatureC = std::nullopt; // "no current in-repo source" — Scene-HUD-Contract-v1.md §13
	frame.runtime.throttled = false;
	frame.runtime.qualityProfileId = "desktop"; // no real backing field — see FakeHudSemanticTypes.h
	frame.runtime.runtimeHealthId = "ready";    // no real backing field — see FakeHudSemanticTypes.h

	frame.controls.scenePrevious = true;
	frame.controls.sceneNext = true;
	frame.controls.mediaPrevious = true;
	frame.controls.mediaNext = true;
	frame.controls.reseed = true;

	// -- Phase overrides ----------------------------------------------------
	switch (phase) {
		case FakeCasePhase::Nominal:
			break; // defaults above ARE the nominal cycle

		case FakeCasePhase::Boundary: {
			auto pin = [&](uint32_t channel, int64_t step) {
				return hashUnit(seed_, channel, step) < 0.5f ? 0.0f : 1.0f;
			};
			int64_t step = static_cast<int64_t>(elapsedSeconds * 10.0f);
			frame.scene.semantic->activity.overall = pin(ChBoundaryPick + 0, step);
			frame.scene.semantic->activity.motion = pin(ChBoundaryPick + 1, step);
			frame.scene.semantic->activity.density = pin(ChBoundaryPick + 2, step);
			frame.scene.semantic->activity.variation = pin(ChBoundaryPick + 3, step);
			frame.scene.semantic->activity.transition = pin(ChBoundaryPick + 4, step);
			frame.scene.semantic->timing.stateProgress = pin(ChBoundaryPick + 6, step);
			for (auto& m : frame.scene.semantic->metrics) {
				if (m.value) m.value = pin(ChBoundaryPick + 7, step);
				if (m.normalizedValue) m.normalizedValue = pin(ChBoundaryPick + 8, step);
			}
			break;
		}

		case FakeCasePhase::MissingOptional:
			// Semantic payload PRESENT but individual signals absent —
			// distinct from Loading (semantic absent entirely, see below).
			frame.scene.semantic->activity = SceneActivity{}; // every field nullopt
			frame.scene.semantic->state.secondaryStateId = std::nullopt;
			frame.scene.semantic->timing.stateElapsedSeconds = std::nullopt;
			frame.scene.semantic->timing.stateProgress = std::nullopt;
			frame.scene.messageId = std::nullopt;
			frame.media = std::nullopt;
			for (auto& m : frame.scene.semantic->metrics) {
				m.value = std::nullopt;
				m.normalizedValue = std::nullopt;
				m.valueId = std::nullopt;
			}
			break;

		case FakeCasePhase::EmptyFlexible:
			frame.scene.semantic->metrics.clear();
			frame.scene.activeEffectIds.clear();
			break;

		case FakeCasePhase::Ready:
			frame.scene.health = FakeSceneHealth::Ready;
			break;

		case FakeCasePhase::Loading:
			// Semantic payload ABSENT entirely — matches
			// sketches/experience_runtime/src/FakeScene.h's own
			// SemanticVariant::Absent shape exactly (nothing is known yet).
			frame.scene.health = FakeSceneHealth::Loading;
			frame.scene.semantic = std::nullopt;
			frame.scene.activeEffectIds.clear();
			frame.media = std::nullopt;
			break;

		case FakeCasePhase::Degraded:
			frame.scene.health = FakeSceneHealth::Degraded;
			frame.scene.messageId = "scene.message.degraded_signal";
			if (frame.media) frame.media->health = VideoPlaybackHealth::Degraded;
			break;

		case FakeCasePhase::Failed:
			frame.scene.health = FakeSceneHealth::Failed;
			frame.scene.messageId = "scene.message.failed";
			frame.scene.semantic->activity = SceneActivity{};
			if (frame.media) frame.media->health = VideoPlaybackHealth::Failed;
			break;

		case FakeCasePhase::ManagerTransition: {
			static const FakeSceneTransitionPhase kSweep[] = {
				FakeSceneTransitionPhase::FadingOut, FakeSceneTransitionPhase::Loading,
				FakeSceneTransitionPhase::FadingIn, FakeSceneTransitionPhase::Failed};
			int64_t step = static_cast<int64_t>(phaseLocalT);
			int pick = static_cast<int>(hashUnit(seed_, ChManagerPhasePick, step) * 4.0f);
			pick = std::clamp(pick, 0, 3);
			frame.manager.transitionPhase = kSweep[pick];
			frame.manager.transitionProgress = clamp01(phaseLocalT / kPhaseDurationSeconds);
			frame.manager.pendingSceneId = "temporal-fields";
			if (frame.manager.transitionPhase == FakeSceneTransitionPhase::Failed) {
				frame.manager.message = "Scene transition failed";
			}
			break;
		}

		case FakeCasePhase::SceneEpochChange:
			frame.scene.semantic->timing.generation = 100 + (seed_ % 5);
			frame.scene.semantic->timing.activeSeconds = phaseLocalT; // "just activated" signature
			break;

		case FakeCasePhase::GenerationChange:
			frame.scene.semantic->timing.generation = 200 + (seed_ % 5);
			if (!baseline.alternateStateIds.empty()) {
				size_t idx = static_cast<size_t>(hashUnit(seed_, ChBoundaryPick, static_cast<int64_t>(phaseLocalT)) * baseline.alternateStateIds.size());
				idx = std::min(idx, baseline.alternateStateIds.size() - 1);
				frame.scene.semantic->state.primaryStateId = baseline.alternateStateIds[idx];
			}
			break;

		case FakeCasePhase::LongestStrings: {
			std::string longId = longPlaceholder(sceneId_);
			frame.scene.displayTitleId = longId;
			frame.scene.semantic->state.primaryStateId = longId;
			frame.scene.messageId = longId;
			if (frame.media) frame.media->titleId = longId;
			for (auto& m : frame.scene.semantic->metrics) {
				if (m.valueId) m.valueId = longId;
			}
			break;
		}

		case FakeCasePhase::MaxEffectList: {
			frame.scene.activeEffectIds.clear();
			for (size_t i = 0; i < kMaxActiveEffectIds; ++i) {
				if (!baseline.effectIds.empty()) {
					frame.scene.activeEffectIds.push_back(baseline.effectIds[i % baseline.effectIds.size()]);
				} else {
					frame.scene.activeEffectIds.push_back("Filler Effect " + std::to_string(i));
				}
			}
			break;
		}

		case FakeCasePhase::MaxMetricCount: {
			while (frame.scene.semantic->metrics.size() < kMaxSceneMetrics) {
				SceneMetric filler;
				filler.metricId = "scene." + sceneId_ + ".metric.filler_" + std::to_string(frame.scene.semantic->metrics.size());
				filler.valueType = HudMetricValueType::Scalar;
				filler.dataClass = HudDataClass::Ambient;
				filler.value = smoothUnit(seed_, ChMetricValue, elapsedSeconds + static_cast<float>(frame.scene.semantic->metrics.size()));
				frame.scene.semantic->metrics.push_back(filler);
			}
			break;
		}

		case FakeCasePhase::kCount:
			break;
	}

	return frame;
}

} // namespace hudpresent
