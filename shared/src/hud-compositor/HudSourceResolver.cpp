#include "HudSourceResolver.h"

namespace hudpresent {

namespace {

// Internal dispatch key for the fixed canonical-ID table. Kept private to
// this .cpp — HudProfileCompiler and callers only ever see string IDs and
// HudResolvedValue, never this enum.
enum class CanonicalSource : uint8_t {
	SceneId,
	SceneTitle,
	SceneHealth,
	SceneMessage,
	SceneStatePrimary,
	SceneStateSecondary,
	SceneStateProgress,
	SceneActivityOverall,
	SceneActivityMotion,
	SceneActivityDensity,
	SceneActivityVariation,
	SceneActivityTransition,
	SceneTimeActive,
	SceneTimeStateElapsed,
	SceneTimeStateProgress,
	SceneGeneration,
	ManagerTransitionPhase,
	ManagerTransitionProgress,
	ManagerMessage,
	MediaId,
	MediaTitle,
	MediaPlaybackProgress,
	MediaHoldProgress,
	MediaHoldRemaining,
	MediaSelectionManual,
	MediaHealth,
	EffectsActive,
	EffectsDominant,
	EffectsTransitionProgress,
	EffectsIntensity,
	EffectsHealth,
	RuntimeFps,
	RuntimeFrameTime,
	RuntimeMemory,
	RuntimeTemperature,
	RuntimeThrottled,
	RuntimeQualityProfile,
	RuntimeHealth,
	ControlScenePreviousAvailable,
	ControlSceneNextAvailable,
	ControlMediaPreviousAvailable,
	ControlMediaNextAvailable,
	ControlSceneReseedAvailable
};

struct CanonicalEntry {
	const char* id;
	CanonicalSource kind;
	HudSourceValueType valueType;
};

// Single source of truth for both isKnownCanonicalSource()/
// staticValueTypeOf() (static, frame-independent) and resolveCanonical()
// (frame-dependent).
//
// Engineering Session 2 note: `scene.state.progress`,
// `effects.dominant`, `effects.transition.progress`, `effects.intensity`,
// and `effects.health` are kept REGISTERED (still recognized, still
// statically typed) but now always resolve as missing against BOTH the
// fake and (once HudRealFrameResolver.cpp exists) the real path —
// `scene.state.progress` because the real ::SceneSemanticState this
// session confirmed has no `progress` field at all (only
// SceneTimingStatus::stateProgress does — see `scene.time.state_progress`
// below, which DOES have a real backing field); the four `effects.*`
// entries beyond `effects.active` because no HudFrameData.effects field
// exists in the real contract (confirmed by direct inspection this
// session — see shared/src/hud-runtime/HudFrameData.h). Kept registered
// rather than deleted so a profile written against them fails loudly as
// "recognized but always empty," not as "unknown source" — and so the
// reserved slot namespace HUD-Semantic-Slot-Model-v1.md §12.6 describes
// stays intact for whenever/if a real effects status type is approved.
const std::vector<CanonicalEntry>& canonicalTable() {
	static const std::vector<CanonicalEntry> table = {
		{"scene.id", CanonicalSource::SceneId, HudSourceValueType::Identifier},
		{"scene.title", CanonicalSource::SceneTitle, HudSourceValueType::Identifier},
		{"scene.health", CanonicalSource::SceneHealth, HudSourceValueType::Identifier},
		{"scene.message", CanonicalSource::SceneMessage, HudSourceValueType::Text},
		{"scene.state.primary", CanonicalSource::SceneStatePrimary, HudSourceValueType::Identifier},
		{"scene.state.secondary", CanonicalSource::SceneStateSecondary, HudSourceValueType::Identifier},
		{"scene.state.progress", CanonicalSource::SceneStateProgress, HudSourceValueType::Ratio},
		{"scene.activity.overall", CanonicalSource::SceneActivityOverall, HudSourceValueType::Ratio},
		{"scene.activity.motion", CanonicalSource::SceneActivityMotion, HudSourceValueType::Ratio},
		{"scene.activity.density", CanonicalSource::SceneActivityDensity, HudSourceValueType::Ratio},
		{"scene.activity.variation", CanonicalSource::SceneActivityVariation, HudSourceValueType::Ratio},
		{"scene.activity.transition", CanonicalSource::SceneActivityTransition, HudSourceValueType::Ratio},
		{"scene.time.active", CanonicalSource::SceneTimeActive, HudSourceValueType::DurationSeconds},
		{"scene.time.state_elapsed", CanonicalSource::SceneTimeStateElapsed, HudSourceValueType::DurationSeconds},
		{"scene.time.state_progress", CanonicalSource::SceneTimeStateProgress, HudSourceValueType::Ratio},
		{"scene.generation", CanonicalSource::SceneGeneration, HudSourceValueType::Count},
		{"manager.transition.phase", CanonicalSource::ManagerTransitionPhase, HudSourceValueType::Identifier},
		{"manager.transition.progress", CanonicalSource::ManagerTransitionProgress, HudSourceValueType::Ratio},
		{"manager.message", CanonicalSource::ManagerMessage, HudSourceValueType::Text},
		{"media.id", CanonicalSource::MediaId, HudSourceValueType::Identifier},
		{"media.title", CanonicalSource::MediaTitle, HudSourceValueType::Identifier},
		{"media.playback.progress", CanonicalSource::MediaPlaybackProgress, HudSourceValueType::Ratio},
		{"media.hold.progress", CanonicalSource::MediaHoldProgress, HudSourceValueType::Ratio},
		{"media.hold.remaining", CanonicalSource::MediaHoldRemaining, HudSourceValueType::DurationSeconds},
		{"media.selection.manual", CanonicalSource::MediaSelectionManual, HudSourceValueType::Boolean},
		{"media.health", CanonicalSource::MediaHealth, HudSourceValueType::Identifier},
		{"effects.active", CanonicalSource::EffectsActive, HudSourceValueType::IdentifierList},
		{"effects.dominant", CanonicalSource::EffectsDominant, HudSourceValueType::Identifier},
		{"effects.transition.progress", CanonicalSource::EffectsTransitionProgress, HudSourceValueType::Ratio},
		{"effects.intensity", CanonicalSource::EffectsIntensity, HudSourceValueType::Ratio},
		{"effects.health", CanonicalSource::EffectsHealth, HudSourceValueType::Identifier},
		{"runtime.fps", CanonicalSource::RuntimeFps, HudSourceValueType::Scalar},
		{"runtime.frame_time", CanonicalSource::RuntimeFrameTime, HudSourceValueType::DurationSeconds},
		{"runtime.memory", CanonicalSource::RuntimeMemory, HudSourceValueType::Scalar},
		{"runtime.temperature", CanonicalSource::RuntimeTemperature, HudSourceValueType::Scalar},
		{"runtime.throttled", CanonicalSource::RuntimeThrottled, HudSourceValueType::Boolean},
		{"runtime.quality_profile", CanonicalSource::RuntimeQualityProfile, HudSourceValueType::Identifier},
		{"runtime.health", CanonicalSource::RuntimeHealth, HudSourceValueType::Identifier},
		{"control.scene.previous.available", CanonicalSource::ControlScenePreviousAvailable, HudSourceValueType::Boolean},
		{"control.scene.next.available", CanonicalSource::ControlSceneNextAvailable, HudSourceValueType::Boolean},
		{"control.media.previous.available", CanonicalSource::ControlMediaPreviousAvailable, HudSourceValueType::Boolean},
		{"control.media.next.available", CanonicalSource::ControlMediaNextAvailable, HudSourceValueType::Boolean},
		{"control.scene.reseed.available", CanonicalSource::ControlSceneReseedAvailable, HudSourceValueType::Boolean},
	};
	return table;
}

const std::unordered_map<std::string, const CanonicalEntry*>& canonicalIndex() {
	static const std::unordered_map<std::string, const CanonicalEntry*> index = [] {
		std::unordered_map<std::string, const CanonicalEntry*> m;
		for (const auto& entry : canonicalTable()) {
			m.emplace(entry.id, &entry);
		}
		return m;
	}();
	return index;
}

const char* sceneHealthId(FakeSceneHealth h) {
	switch (h) {
		case FakeSceneHealth::Ready: return "ready";
		case FakeSceneHealth::Loading: return "loading";
		case FakeSceneHealth::Degraded: return "degraded";
		case FakeSceneHealth::Failed: return "failed";
	}
	return "ready";
}

// VideoPlaybackHealth is the REAL type (shared/src/video-playback/
// VideoPlaybackStatus.h) — used directly by the fake path too now, since
// FakeHudFrameData::media IS a real ::VideoPlaybackStatus (see
// FakeHudSemanticTypes.h's reconciliation note). Five values, not four:
// "unavailable" is new relative to Session 1's private FakeMediaHealth.
const char* mediaHealthId(VideoPlaybackHealth h) {
	switch (h) {
		case VideoPlaybackHealth::Unavailable: return "unavailable";
		case VideoPlaybackHealth::Loading: return "loading";
		case VideoPlaybackHealth::Ready: return "ready";
		case VideoPlaybackHealth::Degraded: return "degraded";
		case VideoPlaybackHealth::Failed: return "failed";
	}
	return "unavailable";
}

const char* transitionPhaseId(FakeSceneTransitionPhase p) {
	switch (p) {
		case FakeSceneTransitionPhase::Idle: return "idle";
		case FakeSceneTransitionPhase::FadingOut: return "fading_out";
		case FakeSceneTransitionPhase::Loading: return "loading";
		case FakeSceneTransitionPhase::FadingIn: return "fading_in";
		case FakeSceneTransitionPhase::Failed: return "failed";
	}
	return "idle";
}

HudIdentifierList toIdentifierList(const std::vector<std::string>& ids) {
	HudIdentifierList list;
	for (const auto& id : ids) list.push(id);
	return list;
}

// media.title's fallback chain — Engineering Session 2 §6: "titleId
// through vocabulary -> fallbackDisplayTitle -> mediaId -> generic
// unavailable-media vocabulary." The first three tiers are resolved
// HERE, at the raw-value level (which of the three identity fields is
// available); the fourth tier (generic unavailable-media vocabulary) is
// deliberately NOT this resolver's job — it falls out for free from the
// existing HudMissingPolicy::Placeholder path once this function returns
// present=false, exactly like every other "required identity fallback"
// slot already works. titleId is the only tier treated as an
// Identifier (vocabulary-resolvable, per VideoPlaybackStatus.h's own
// "catalog-authored, never a raw filename" framing of titleId
// specifically); fallbackDisplayTitle and mediaId are both surfaced as
// literal Text — fallbackDisplayTitle because MediaCatalog synthesizes it
// as already-final display text (see shared/src/video-playback/README.md),
// mediaId because showing a raw catalog ID as a last-resort label should
// read as literal text, not be sent through a vocabulary lookup that was
// never meant to have an entry for it.
HudResolvedValue resolveMediaTitle(const VideoPlaybackStatus& media) {
	if (media.titleId && !media.titleId->empty()) {
		return HudResolvedValue::identifier(*media.titleId);
	}
	if (media.fallbackDisplayTitle && !media.fallbackDisplayTitle->empty()) {
		return HudResolvedValue::text(*media.fallbackDisplayTitle);
	}
	if (media.mediaId && !media.mediaId->empty()) {
		return HudResolvedValue::text(*media.mediaId);
	}
	return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
}

} // namespace

HudSourceResolver::HudSourceResolver() = default;

bool HudSourceResolver::isKnownCanonicalSource(const std::string& sourceId) {
	return canonicalIndex().count(sourceId) != 0;
}

std::optional<HudSourceValueType> HudSourceResolver::staticValueTypeOf(const std::string& sourceId) {
	auto it = canonicalIndex().find(sourceId);
	if (it == canonicalIndex().end()) return std::nullopt;
	return it->second->valueType;
}

const std::vector<std::string>& HudSourceResolver::allCanonicalSourceIds() {
	static const std::vector<std::string> ids = [] {
		std::vector<std::string> v;
		v.reserve(canonicalTable().size());
		for (const auto& entry : canonicalTable()) v.emplace_back(entry.id);
		return v;
	}();
	return ids;
}

bool HudSourceResolver::looksLikeSceneMetricId(const std::string& sourceId) {
	// "scene." <non-empty> ".metric." <non-empty> — deliberately loose
	// (does not require the middle segment to equal any particular
	// scene's ID) since this is a syntactic/static check; resolveSceneMetric()
	// does the actual, frame-aware metricId match.
	static const std::string prefix = "scene.";
	static const std::string marker = ".metric.";
	if (sourceId.size() <= prefix.size()) return false;
	if (sourceId.compare(0, prefix.size(), prefix) != 0) return false;
	auto pos = sourceId.find(marker, prefix.size());
	if (pos == std::string::npos) return false;
	if (pos == prefix.size()) return false; // empty scene-id segment
	if (pos + marker.size() >= sourceId.size()) return false; // empty metric-name segment
	return true;
}

std::optional<HudResolvedValue> HudSourceResolver::resolve(const std::string& sourceId, const FakeHudFrameData& frame) const {
	if (auto v = resolveCanonical(sourceId, frame)) return v;
	if (looksLikeSceneMetricId(sourceId)) return resolveSceneMetric(sourceId, frame);
	return std::nullopt;
}

std::optional<HudResolvedValue> HudSourceResolver::resolveCanonical(const std::string& sourceId, const FakeHudFrameData& frame) const {
	auto it = canonicalIndex().find(sourceId);
	if (it == canonicalIndex().end()) return std::nullopt;

	const auto& scene = frame.scene;
	const auto& manager = frame.manager;
	const auto& runtime = frame.runtime;
	const auto& controls = frame.controls;
	const auto* semantic = scene.semantic ? &(*scene.semantic) : nullptr;

	switch (it->second->kind) {
		case CanonicalSource::SceneId:
			return HudResolvedValue::identifier(scene.sceneId);
		case CanonicalSource::SceneTitle:
			return HudResolvedValue::identifier(scene.displayTitleId);
		case CanonicalSource::SceneHealth:
			return HudResolvedValue::identifier(sceneHealthId(scene.health));
		case CanonicalSource::SceneMessage:
			if (!scene.messageId) return HudResolvedValue::missing(HudSourceValueType::Text, HudDataClass::Literal);
			return HudResolvedValue::text(*scene.messageId);

		case CanonicalSource::SceneStatePrimary:
			if (!semantic) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
			return HudResolvedValue::identifier(semantic->state.primaryStateId);
		case CanonicalSource::SceneStateSecondary:
			if (!semantic || !semantic->state.secondaryStateId) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
			return HudResolvedValue::identifier(*semantic->state.secondaryStateId);
		case CanonicalSource::SceneStateProgress:
			// No real backing field — see canonicalTable()'s header comment.
			return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);

		case CanonicalSource::SceneActivityOverall:
			if (!semantic || !semantic->activity.overall) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
			return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.overall, HudDataClass::Normalized);
		case CanonicalSource::SceneActivityMotion:
			if (!semantic || !semantic->activity.motion) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
			return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.motion, HudDataClass::Normalized);
		case CanonicalSource::SceneActivityDensity:
			if (!semantic || !semantic->activity.density) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
			return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.density, HudDataClass::Normalized);
		case CanonicalSource::SceneActivityVariation:
			if (!semantic || !semantic->activity.variation) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
			return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.variation, HudDataClass::Normalized);
		case CanonicalSource::SceneActivityTransition:
			if (!semantic || !semantic->activity.transition) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
			return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.transition, HudDataClass::Normalized);

		case CanonicalSource::SceneTimeActive:
			if (!semantic) return HudResolvedValue::missing(HudSourceValueType::DurationSeconds, HudDataClass::Literal);
			return HudResolvedValue::number(HudSourceValueType::DurationSeconds, semantic->timing.activeSeconds, HudDataClass::Literal);
		case CanonicalSource::SceneTimeStateElapsed:
			if (!semantic || !semantic->timing.stateElapsedSeconds) return HudResolvedValue::missing(HudSourceValueType::DurationSeconds, HudDataClass::Literal);
			return HudResolvedValue::number(HudSourceValueType::DurationSeconds, *semantic->timing.stateElapsedSeconds, HudDataClass::Literal);
		case CanonicalSource::SceneTimeStateProgress:
			if (!semantic || !semantic->timing.stateProgress) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
			return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->timing.stateProgress, HudDataClass::Normalized);
		case CanonicalSource::SceneGeneration:
			if (!semantic) return HudResolvedValue::missing(HudSourceValueType::Count, HudDataClass::Literal);
			return HudResolvedValue::number(HudSourceValueType::Count, static_cast<float>(semantic->timing.generation), HudDataClass::Literal);

		case CanonicalSource::ManagerTransitionPhase:
			return HudResolvedValue::identifier(transitionPhaseId(manager.transitionPhase));
		case CanonicalSource::ManagerTransitionProgress:
			return HudResolvedValue::number(HudSourceValueType::Ratio, manager.transitionProgress, HudDataClass::Normalized);
		case CanonicalSource::ManagerMessage:
			if (!manager.message) return HudResolvedValue::missing(HudSourceValueType::Text, HudDataClass::Literal);
			return HudResolvedValue::text(*manager.message);

		case CanonicalSource::MediaId:
			if (!frame.media || !frame.media->mediaId) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
			return HudResolvedValue::identifier(*frame.media->mediaId);
		case CanonicalSource::MediaTitle:
			if (!frame.media) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
			return resolveMediaTitle(*frame.media);
		case CanonicalSource::MediaPlaybackProgress:
			if (!frame.media || !frame.media->playbackProgress) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
			return HudResolvedValue::number(HudSourceValueType::Ratio, *frame.media->playbackProgress, HudDataClass::Normalized);
		case CanonicalSource::MediaHoldProgress:
			if (!frame.media || !frame.media->holdProgress) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
			return HudResolvedValue::number(HudSourceValueType::Ratio, *frame.media->holdProgress, HudDataClass::Normalized);
		case CanonicalSource::MediaHoldRemaining:
			if (!frame.media || !frame.media->holdRemainingSeconds) return HudResolvedValue::missing(HudSourceValueType::DurationSeconds, HudDataClass::Literal);
			return HudResolvedValue::number(HudSourceValueType::DurationSeconds, *frame.media->holdRemainingSeconds, HudDataClass::Literal);
		case CanonicalSource::MediaSelectionManual:
			// Derived, not a real field — see this file's header note and
			// VideoPlaybackStatus.h's MediaSelectionOrigin. Unambiguous:
			// exactly two of its five values represent a manual pick.
			if (!frame.media) return HudResolvedValue::missing(HudSourceValueType::Boolean, HudDataClass::Literal);
			return HudResolvedValue::boolean(
				frame.media->selectionOrigin == MediaSelectionOrigin::ManualPrevious ||
				frame.media->selectionOrigin == MediaSelectionOrigin::ManualNext);
		case CanonicalSource::MediaHealth:
			if (!frame.media) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
			return HudResolvedValue::identifier(mediaHealthId(frame.media->health));

		case CanonicalSource::EffectsActive:
			// Maps to the real SceneHudStatus::activeEffects (already
			// curated display TEXT, not vocabulary IDs) — see
			// FakeHudSemanticTypes.h's reconciliation note. An empty
			// vector here is a legitimate, present, "nothing active"
			// answer, distinct from scene.semantic being entirely absent
			// (which has no bearing on this field at all — activeEffects
			// lives at the SceneHudStatus level, not inside `semantic`).
			return HudResolvedValue::list(toIdentifierList(scene.activeEffectIds));
		case CanonicalSource::EffectsDominant:
		case CanonicalSource::EffectsTransitionProgress:
		case CanonicalSource::EffectsIntensity:
		case CanonicalSource::EffectsHealth:
			// No real (or fake) backing field — see canonicalTable()'s
			// header comment.
			return HudResolvedValue::missing(it->second->valueType, HudDataClass::Literal);

		case CanonicalSource::RuntimeFps:
			if (!runtime.fps) return HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
			return HudResolvedValue::number(HudSourceValueType::Scalar, *runtime.fps, HudDataClass::Literal);
		case CanonicalSource::RuntimeFrameTime:
			if (!runtime.frameTimeMs) return HudResolvedValue::missing(HudSourceValueType::DurationSeconds, HudDataClass::Literal);
			return HudResolvedValue::number(HudSourceValueType::DurationSeconds, *runtime.frameTimeMs, HudDataClass::Literal);
		case CanonicalSource::RuntimeMemory:
			if (!runtime.memoryBytes) return HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
			return HudResolvedValue::number(HudSourceValueType::Scalar, *runtime.memoryBytes, HudDataClass::Literal);
		case CanonicalSource::RuntimeTemperature:
			if (!runtime.temperatureC) return HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
			return HudResolvedValue::number(HudSourceValueType::Scalar, *runtime.temperatureC, HudDataClass::Literal);
		case CanonicalSource::RuntimeThrottled:
			if (!runtime.throttled) return HudResolvedValue::missing(HudSourceValueType::Boolean, HudDataClass::Literal);
			return HudResolvedValue::boolean(*runtime.throttled);
		case CanonicalSource::RuntimeQualityProfile:
			// No real field — see FakeHudSemanticTypes.h's reconciliation note.
			if (!runtime.qualityProfileId) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
			return HudResolvedValue::identifier(*runtime.qualityProfileId);
		case CanonicalSource::RuntimeHealth:
			// No real field — see FakeHudSemanticTypes.h's reconciliation note.
			if (!runtime.runtimeHealthId) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
			return HudResolvedValue::identifier(*runtime.runtimeHealthId);

		case CanonicalSource::ControlScenePreviousAvailable:
			return HudResolvedValue::boolean(controls.scenePrevious);
		case CanonicalSource::ControlSceneNextAvailable:
			return HudResolvedValue::boolean(controls.sceneNext);
		case CanonicalSource::ControlMediaPreviousAvailable:
			return HudResolvedValue::boolean(controls.mediaPrevious);
		case CanonicalSource::ControlMediaNextAvailable:
			return HudResolvedValue::boolean(controls.mediaNext);
		case CanonicalSource::ControlSceneReseedAvailable:
			return HudResolvedValue::boolean(controls.reseed);
	}
	return std::nullopt;
}

std::optional<HudResolvedValue> HudSourceResolver::resolveSceneMetric(const std::string& sourceId, const FakeHudFrameData& frame) const {
	if (!frame.scene.semantic) {
		// No semantic payload at all this frame -> every scene-metric ID
		// is "missing," not "unknown" (the shape is still recognized).
		return HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
	}
	// Linear scan is deliberate, not an oversight: kMaxSceneMetrics (16,
	// FakeHudSemanticTypes.h) bounds this to a small, fixed-cost search —
	// cheaper than building/maintaining a per-frame hash map for a vector
	// this short, and avoids allocating one every resolve() call.
	for (const auto& metric : frame.scene.semantic->metrics) {
		if (metric.metricId != sourceId) continue;
		HudSourceValueType valueType = fromMetricValueType(metric.valueType);
		if (valueType == HudSourceValueType::Identifier) {
			if (!metric.valueId) return HudResolvedValue::missing(HudSourceValueType::Identifier, metric.dataClass);
			return HudResolvedValue::identifier(*metric.valueId, metric.dataClass);
		}
		if (!metric.value) return HudResolvedValue::missing(valueType, metric.dataClass);
		return HudResolvedValue::number(valueType, *metric.value, metric.dataClass);
	}
	// Syntactically metric-shaped but not present in this particular
	// snapshot: distinct from "unknown source" (which returns
	// std::nullopt from resolve()) — this scenario just doesn't populate
	// this metric right now. Reported as missing Scalar/Literal since the
	// resolver has no static type to fall back on for an unrecognized
	// metric ID.
	return HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
}

} // namespace hudpresent
