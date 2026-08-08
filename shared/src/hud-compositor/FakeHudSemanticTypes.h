#pragma once

// ============================================================================
// FakeHudSemanticTypes.h — renderer-private fake-data aggregate, RECONCILED
// (Engineering Session 2) against the now-real shared types.
//
// Session 1 built this file as a mirror of a still-draft semantic model,
// with the explicit note that it should be "easy to replace with approved
// shared types later." That later point is now: `shared/src/scene/
// SceneSemanticTypes.h` (SceneActivity, SceneSemanticState,
// SceneTimingStatus, SceneMetric, SceneSemanticData, HudDataClass) and
// `shared/src/video-playback/VideoPlaybackStatus.h` (VideoPlaybackStatus,
// VideoPlaybackHealth, MediaSelectionOrigin) are both real, approved, and —
// confirmed by direct inspection — OF-free, exactly like this file's own
// types. Per this session's reconciliation requirement ("no production
// duplicate of SceneSemanticData", "no duplicate VideoPlaybackStatus"),
// every field this file used to mirror privately is now taken directly
// from the real header wherever a real, OF-free counterpart exists:
//
//   REPLACED BY THE REAL TYPE (was a private mirror, now is not):
//     - activity/state/timing/metrics -> ::SceneActivity/::SceneSemanticState/
//       ::SceneTimingStatus/::SceneMetric, nested inside one
//       std::optional<SceneSemanticData>, mirroring SceneHudStatus::semantic's
//       own optionality exactly (Session 1's FakeSceneSemanticSnapshot
//       flattened everything unconditionally; the real contract makes the
//       whole semantic payload one option, and this file now matches that).
//     - media status -> ::VideoPlaybackStatus/::VideoPlaybackHealth/
//       ::MediaSelectionOrigin, used directly, no private mirror at all.
//     - HudDataClass -> see HudDataTypes.h's alias to ::HudDataClass.
//
//   STILL A DELIBERATE, DOCUMENTED PRIVATE MIRROR (real counterpart lives
//   in an OF-DEPENDENT header, so a private copy is the only way to keep
//   this file, and everything the dependency-free test suite builds on
//   top of it, free of the openFrameworks include chain):
//     - FakeSceneHealth  mirrors ::SceneHealth  (SceneContract.h — OF-coupled
//       via SceneFrame's ofTexture member living in the same header)
//     - FakeSceneTransitionPhase mirrors ::SceneTransitionPhase (same file)
//
//   FAKE-TEST-ONLY, NO REAL COUNTERPART EXISTS (kept, but re-scoped and
//   re-documented so nobody mistakes them for a preview of a future real
//   field — seeShared-contract discovery in this session's review doc):
//     - activeEffectIds — maps to the REAL, already-approved
//       SceneHudStatus::activeEffects (vector<string> of ALREADY-CURATED
//       display text, per shared/src/video-effects/knowledge/
//       EffectActivityStatus.h's own comment: "Only curated display labels
//       ever cross that boundary") — not an ID list needing vocabulary
//       resolution. This file's field is a real, direct analog, just
//       without a "SceneHudStatus" wrapper around it in the fake path.
//     - FakeRuntimeTelemetry::qualityProfileId/runtimeHealthId — NO real
//       RuntimeTelemetry field backs these (confirmed: shared/src/scene/
//       SceneContract.h's RuntimeTelemetry has fps/frameTimeMs/
//       cpuTemperatureC/residentMemoryBytes/throttled ONLY). Kept as
//       reserved, always-missing-against-real-data canonical sources — see
//       HudRealFrameResolver.cpp — rather than invented as real fields,
//       per this task's "do not invent public field names" instruction.
//     - FakeControlAvailability — no real "control availability" type
//       exists at all; production derives availability from
//       SceneCapabilities/VideoPlaybackStatus/SceneManagerStatus directly
//       (see HudRealFrameResolver.cpp), never from a status struct shaped
//       like this one. This struct exists purely so the SIX FAKE
//       SCENARIOS have something simple to set.
//     - dominant-effect/transition-progress/intensity/effects-health — NO
//       HudFrameData.effects field exists (confirmed by direct inspection
//       of shared/src/hud-runtime/HudFrameData.h — see this session's
//       review doc, "Shared types consumed"/"Effect bindings"). Removed
//       from this file entirely rather than kept as a speculative
//       placeholder struct nothing production ever populates — see git
//       history / the Session 1 version of this file for what was removed.
//
// Deliberately still ZERO openFrameworks dependency — see HudDataTypes.h's
// header comment for why that matters for testability. This is verified,
// not assumed: SceneSemanticTypes.h and VideoPlaybackStatus.h were each
// read in full this session and confirmed to include only <cstdint>/
// <optional>/<string>/<vector>.
// ============================================================================

#include "HudDataTypes.h"
#include "SceneSemanticTypes.h"       // real: SceneActivity, SceneSemanticState, SceneTimingStatus, SceneMetric, SceneSemanticData, HudMetricValueType
#include "VideoPlaybackStatus.h"      // real: VideoPlaybackStatus, VideoPlaybackHealth, MediaSelectionOrigin

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hudpresent {

// Mirrors shared/src/scene/SceneContract.h's SceneHealth 1:1 (same four
// values, same order) — see this file's header comment for why this one
// stays a private mirror (the real enum lives in an OF-coupled header).
// Any real integration point must convert 1:1, never reinterpret; see
// HudRealFrameResolver.cpp, which does exactly that conversion.
enum class FakeSceneHealth : uint8_t {
	Ready,
	Loading,
	Degraded,
	Failed
};

// Mirrors shared/src/scene/SceneContract.h's SceneTransitionPhase 1:1.
enum class FakeSceneTransitionPhase : uint8_t {
	Idle,
	FadingOut,
	Loading,
	FadingIn,
	Failed
};

// Converts the real (narrower, 5-value) ::HudMetricValueType into this
// domain's (wider, 8-value) HudSourceValueType — a safe, one-directional,
// exhaustive mapping (every real value has an unambiguous target; the
// extra HudSourceValueType values — Boolean/Text/IdentifierList — simply
// have no ::HudMetricValueType producer, which is correct: SceneMetric
// never carries those kinds today).
inline HudSourceValueType fromMetricValueType(HudMetricValueType t) {
	switch (t) {
		case HudMetricValueType::Scalar: return HudSourceValueType::Scalar;
		case HudMetricValueType::Count: return HudSourceValueType::Count;
		case HudMetricValueType::Ratio: return HudSourceValueType::Ratio;
		case HudMetricValueType::DurationSeconds: return HudSourceValueType::DurationSeconds;
		case HudMetricValueType::Identifier: return HudSourceValueType::Identifier;
	}
	return HudSourceValueType::Scalar;
}

// HUD-Semantic-Slot-Model-v1.md §20's recommended v1 caps, reused here as
// the renderer-private producer-side caps for fake data — unchanged from
// Session 1; ::SceneSemanticData/::SceneMetric themselves are uncapped
// std::vectors (the real contract does not enforce a cap in the type
// system), so this remains a producer-side convention, not something the
// real type structurally guarantees.
constexpr size_t kMaxSceneMetrics = 16;
constexpr size_t kMaxActiveEffectIds = 8;

// The fake-scenario aggregate. Deliberately structured to mirror the REAL
// split now visible in SceneHudStatus/HudFrameData: identity/health/
// message/activeEffects live at the "status" level; state/activity/timing/
// metrics live inside one optional `semantic` payload — see this file's
// header comment. This is NOT itself a SceneHudStatus (no schemaVersion,
// no mediaName/modeName/statusLines/progress/motionEnergy/activeItemCount/
// paused compatibility fields — none of Session 1's fake scenarios ever
// used those, and production code never routes through this struct at
// all, only through the real SceneHudStatus inside HudFrameData), just a
// same-shaped-where-it-matters fake-test convenience.
struct FakeSceneSemanticSnapshot {
	std::string sceneId;
	std::string displayTitleId; // vocabulary ID — NOTE: the REAL SceneHudStatus::displayName is already-final TEXT, not an ID; see HudRealFrameResolver.cpp's comment on why the fake and real paths deliberately differ here and how HudFormattingService's VocabularyValue handling was fixed this session to do the right thing for both.

	FakeSceneHealth health = FakeSceneHealth::Ready;
	std::optional<std::string> messageId;

	// Mirrors SceneHudStatus::semantic's own optionality exactly — nullopt
	// here must behave identically to a real scene reporting no semantic
	// payload (Loading/just-activated/not-yet-implemented), not to a
	// present-but-empty one.
	std::optional<SceneSemanticData> semantic;

	// Maps to the real, approved SceneHudStatus::activeEffects — already
	// curated display TEXT (not vocabulary IDs) per
	// shared/src/video-effects/knowledge/EffectActivityStatus.h. Producers
	// should cap at kMaxActiveEffectIds (matching HUD-Semantic-Slot-Model-v1.md
	// §20's recommendation, though the real vector<string> field itself is
	// uncapped).
	std::vector<std::string> activeEffectIds;
};

// Mirrors shared/src/scene/SceneContract.h's SceneManagerStatus, with one
// deliberate, corrected difference from the Session 1 version: `message`
// is now plain text (matching the REAL SceneManagerStatus::message, which
// Session 2's reconciliation confirmed is NOT vocabulary-resolvable ID —
// it is the frozen contract's original operational-compatibility string).
// Session 1's `messageId` naming was a reasonable guess before the real
// type was inspected; corrected here.
struct FakeSceneManagerStatus {
	std::string activeSceneId;
	std::optional<std::string> pendingSceneId;
	FakeSceneTransitionPhase transitionPhase = FakeSceneTransitionPhase::Idle;
	float transitionProgress = 0.0f;
	std::optional<std::string> message;
};

// No draft-document counterpart at all — see this file's header comment.
// Never consulted by the production resolve path.
struct FakeControlAvailability {
	bool scenePrevious = false;
	bool sceneNext = false;
	bool mediaPrevious = false;
	bool mediaNext = false;
	bool reseed = false;
};

// No real RuntimeTelemetry field backs qualityProfileId/runtimeHealthId —
// see this file's header comment. fps/frameTimeMs stay optional here
// (unlike the real, always-populated RuntimeTelemetry) purely so fake
// scenarios can exercise "telemetry not sampled yet" — a state the real
// type cannot represent (its fps/frameTimeMs are bare floats, defaulted to
// 0, never optional).
struct FakeRuntimeTelemetry {
	std::optional<float> fps;
	std::optional<float> frameTimeMs;
	std::optional<float> memoryBytes;
	std::optional<float> temperatureC;
	std::optional<bool> throttled;
	std::optional<std::string> qualityProfileId;
	std::optional<std::string> runtimeHealthId;
};

// The one immutable renderer-private snapshot IFakeHudScenario produces
// and HudSourceResolver (the FAKE-path resolver) resolves against. Notice
// what is now MISSING relative to Session 1: no `effects` field (no real
// HudFrameData.effects exists to mirror — see this file's header
// comment), and `media` is the REAL ::VideoPlaybackStatus, not a private
// mirror.
struct FakeHudFrameData {
	uint32_t schemaVersion = 1;

	FakeSceneSemanticSnapshot scene;
	FakeSceneManagerStatus manager;
	std::optional<VideoPlaybackStatus> media;
	FakeRuntimeTelemetry runtime;
	FakeControlAvailability controls;
};

} // namespace hudpresent
