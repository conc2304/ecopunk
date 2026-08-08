#include "HudRealFrameResolver.h"

#include <algorithm>

namespace hudpresent {

namespace {

const char* sceneHealthId(SceneHealth h) {
	switch (h) {
		case SceneHealth::Ready: return "ready";
		case SceneHealth::Loading: return "loading";
		case SceneHealth::Degraded: return "degraded";
		case SceneHealth::Failed: return "failed";
	}
	return "ready";
}

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

const char* effectHealthId(videoeffects::EffectHealth h) {
	switch (h) {
		case videoeffects::EffectHealth::Ready: return "ready";
		case videoeffects::EffectHealth::Degraded: return "degraded";
		case videoeffects::EffectHealth::Failed: return "failed";
	}
	return "ready";
}

const char* transitionPhaseId(SceneTransitionPhase p) {
	switch (p) {
		case SceneTransitionPhase::Idle: return "idle";
		case SceneTransitionPhase::FadingOut: return "fading_out";
		case SceneTransitionPhase::Loading: return "loading";
		case SceneTransitionPhase::FadingIn: return "fading_in";
		case SceneTransitionPhase::Failed: return "failed";
	}
	return "idle";
}

HudIdentifierList toIdentifierList(const std::vector<std::string>& ids) {
	HudIdentifierList list;
	for (const auto& id : ids) list.push(id);
	return list;
}

// Same three-tier fallback as HudSourceResolver.cpp's resolveMediaTitle()
// — see that function's header comment for the full rationale (titleId is
// vocabulary-resolvable Identifier text; fallbackDisplayTitle/mediaId are
// literal Text). Kept as a near-duplicate rather than shared code because
// the two frame types' `media` fields, while both real ::VideoPlaybackStatus
// now, live at different access paths (frame.media vs frame.media->...)
// and sharing a template/overload here would cost more clarity than the
// ~8 lines it would save.
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

std::optional<HudResolvedValue> HudRealFrameResolver::resolve(const std::string& sourceId, const HudFrameData& frame) const {
	if (auto v = resolveCanonical(sourceId, frame)) return v;
	if (HudSourceResolver::looksLikeSceneMetricId(sourceId)) return resolveSceneMetric(sourceId, frame);
	return std::nullopt;
}

std::optional<HudResolvedValue> HudRealFrameResolver::resolveCanonical(const std::string& sourceId, const HudFrameData& frame) const {
	if (!HudSourceResolver::isKnownCanonicalSource(sourceId)) return std::nullopt;

	const auto& scene = frame.scene;                 // SceneHudStatus
	const auto* semantic = scene.semantic ? &(*scene.semantic) : nullptr; // optional<SceneSemanticData>
	const auto& manager = frame.sceneManager;         // SceneManagerStatus
	const auto& runtime = frame.runtime;              // RuntimeTelemetry

	// -- Identity / health / message (SceneHudStatus — always present) --
	if (sourceId == "scene.id") return HudResolvedValue::identifier(scene.sceneId);
	if (sourceId == "scene.title") {
		// Engineering Session 2 finding: the REAL SceneHudStatus::displayName
		// is already-final TEXT (an operational-compatibility field
		// predating the semantic model), NOT a vocabulary ID like the fake
		// path's displayTitleId — see HudFormattingService.cpp's
		// VocabularyValue fix, which this depends on to render correctly.
		return HudResolvedValue::text(scene.displayName);
	}
	if (sourceId == "scene.health") return HudResolvedValue::identifier(sceneHealthId(scene.health));
	if (sourceId == "scene.message") {
		if (!scene.message) return HudResolvedValue::missing(HudSourceValueType::Text, HudDataClass::Literal);
		return HudResolvedValue::text(*scene.message);
	}

	// -- Semantic state/activity/timing (optional payload) --------------
	if (sourceId == "scene.state.primary") {
		if (!semantic) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
		return HudResolvedValue::identifier(semantic->state.primaryStateId);
	}
	if (sourceId == "scene.state.secondary") {
		if (!semantic || !semantic->state.secondaryStateId) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
		return HudResolvedValue::identifier(*semantic->state.secondaryStateId);
	}
	if (sourceId == "scene.state.progress") {
		// No real backing field (::SceneSemanticState has no `progress` —
		// see HudSourceResolver.cpp's canonicalTable() comment).
		return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
	}
	if (sourceId == "scene.activity.overall") {
		if (!semantic || !semantic->activity.overall) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
		return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.overall, HudDataClass::Normalized);
	}
	if (sourceId == "scene.activity.motion") {
		if (!semantic || !semantic->activity.motion) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
		return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.motion, HudDataClass::Normalized);
	}
	if (sourceId == "scene.activity.density") {
		if (!semantic || !semantic->activity.density) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
		return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.density, HudDataClass::Normalized);
	}
	if (sourceId == "scene.activity.variation") {
		if (!semantic || !semantic->activity.variation) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
		return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.variation, HudDataClass::Normalized);
	}
	if (sourceId == "scene.activity.transition") {
		if (!semantic || !semantic->activity.transition) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
		return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->activity.transition, HudDataClass::Normalized);
	}
	if (sourceId == "scene.time.active") {
		if (!semantic) return HudResolvedValue::missing(HudSourceValueType::DurationSeconds, HudDataClass::Literal);
		return HudResolvedValue::number(HudSourceValueType::DurationSeconds, semantic->timing.activeSeconds, HudDataClass::Literal);
	}
	if (sourceId == "scene.time.state_elapsed") {
		if (!semantic || !semantic->timing.stateElapsedSeconds) return HudResolvedValue::missing(HudSourceValueType::DurationSeconds, HudDataClass::Literal);
		return HudResolvedValue::number(HudSourceValueType::DurationSeconds, *semantic->timing.stateElapsedSeconds, HudDataClass::Literal);
	}
	if (sourceId == "scene.time.state_progress") {
		if (!semantic || !semantic->timing.stateProgress) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
		return HudResolvedValue::number(HudSourceValueType::Ratio, *semantic->timing.stateProgress, HudDataClass::Normalized);
	}
	if (sourceId == "scene.generation") {
		if (!semantic) return HudResolvedValue::missing(HudSourceValueType::Count, HudDataClass::Literal);
		return HudResolvedValue::number(HudSourceValueType::Count, static_cast<float>(semantic->timing.generation), HudDataClass::Literal);
	}

	// -- Manager (SceneManagerStatus — always present) -------------------
	if (sourceId == "manager.transition.phase") return HudResolvedValue::identifier(transitionPhaseId(manager.transitionPhase));
	if (sourceId == "manager.transition.progress") return HudResolvedValue::number(HudSourceValueType::Ratio, manager.transitionProgress, HudDataClass::Normalized);
	if (sourceId == "manager.message") {
		if (!manager.message) return HudResolvedValue::missing(HudSourceValueType::Text, HudDataClass::Literal);
		return HudResolvedValue::text(*manager.message);
	}

	// -- Media (optional VideoPlaybackStatus) ----------------------------
	if (sourceId == "media.id") {
		if (!frame.video || !frame.video->mediaId) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
		return HudResolvedValue::identifier(*frame.video->mediaId);
	}
	if (sourceId == "media.title") {
		if (!frame.video) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
		return resolveMediaTitle(*frame.video);
	}
	if (sourceId == "media.playback.progress") {
		if (!frame.video || !frame.video->playbackProgress) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
		return HudResolvedValue::number(HudSourceValueType::Ratio, *frame.video->playbackProgress, HudDataClass::Normalized);
	}
	if (sourceId == "media.hold.progress") {
		if (!frame.video || !frame.video->holdProgress) return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
		return HudResolvedValue::number(HudSourceValueType::Ratio, *frame.video->holdProgress, HudDataClass::Normalized);
	}
	if (sourceId == "media.hold.remaining") {
		if (!frame.video || !frame.video->holdRemainingSeconds) return HudResolvedValue::missing(HudSourceValueType::DurationSeconds, HudDataClass::Literal);
		return HudResolvedValue::number(HudSourceValueType::DurationSeconds, *frame.video->holdRemainingSeconds, HudDataClass::Literal);
	}
	if (sourceId == "media.selection.manual") {
		if (!frame.video) return HudResolvedValue::missing(HudSourceValueType::Boolean, HudDataClass::Literal);
		return HudResolvedValue::boolean(
			frame.video->selectionOrigin == MediaSelectionOrigin::ManualPrevious ||
			frame.video->selectionOrigin == MediaSelectionOrigin::ManualNext);
	}
	if (sourceId == "media.health") {
		if (!frame.video) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
		return HudResolvedValue::identifier(mediaHealthId(frame.video->health));
	}

	// -- Effects (Architecture-Closure Session, DEC-015/DEC-016): now a
	// real HudFrameData.effects sibling snapshot exists — see
	// resolveEffects() for the full absent/present-empty/present-active
	// handling and why SceneHudStatus::activeEffects is compatibility-
	// only, gated behind an explicit opt-in flag rather than a silent
	// production fallback.
	if (sourceId == "effects.active" || sourceId == "effects.dominant" ||
		sourceId == "effects.transition.progress" || sourceId == "effects.intensity" ||
		sourceId == "effects.health") {
		return resolveEffects(sourceId, frame);
	}

	// -- Runtime telemetry (RuntimeTelemetry — fps/frameTimeMs always
	// present per the real type; the rest optional) ----------------------
	if (sourceId == "runtime.fps") return HudResolvedValue::number(HudSourceValueType::Scalar, runtime.fps, HudDataClass::Literal);
	if (sourceId == "runtime.frame_time") return HudResolvedValue::number(HudSourceValueType::DurationSeconds, runtime.frameTimeMs, HudDataClass::Literal);
	if (sourceId == "runtime.memory") {
		if (!runtime.residentMemoryBytes) return HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
		return HudResolvedValue::number(HudSourceValueType::Scalar, static_cast<float>(*runtime.residentMemoryBytes), HudDataClass::Literal);
	}
	if (sourceId == "runtime.temperature") {
		if (!runtime.cpuTemperatureC) return HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
		return HudResolvedValue::number(HudSourceValueType::Scalar, *runtime.cpuTemperatureC, HudDataClass::Literal);
	}
	if (sourceId == "runtime.throttled") {
		if (!runtime.throttled) return HudResolvedValue::missing(HudSourceValueType::Boolean, HudDataClass::Literal);
		return HudResolvedValue::boolean(*runtime.throttled);
	}
	if (sourceId == "runtime.quality_profile" || sourceId == "runtime.health") {
		// No real field backs either — RuntimeTelemetry has no quality-
		// profile or health concept (see FakeHudSemanticTypes.h).
		auto staticType = HudSourceResolver::staticValueTypeOf(sourceId);
		return HudResolvedValue::missing(staticType ? *staticType : HudSourceValueType::Identifier, HudDataClass::Literal);
	}

	// -- Controls: derived, never a real dedicated field (see this file's
	// header comment / this task's §5). Three-state distinction:
	//   - UNSUPPORTED (this task's "unsupported"): no real capability
	//     backs the control at all -> resolves MISSING (present=false),
	//     which (combined with the Hide missingPolicy every control
	//     binding uses) hides the control's label entirely — see
	//     widgets/LabelWidget.cpp's "enabled" role handling. Never shown,
	//     not even dimmed: showing an unusable button at all would be
	//     misleading.
	//   - DISABLED (this task's "supported but disabled"): a real
	//     capability exists but is not currently actionable -> resolves
	//     PRESENT + false. LabelWidget dims but still shows it.
	//   - ENABLED: PRESENT + true. LabelWidget shows it at full opacity.
	// -----------------------------------------------------------------
	if (sourceId == "control.scene.previous.available" || sourceId == "control.scene.next.available") {
		// No real signal exists for "is more than one scene available to
		// switch to" (SceneManagerStatus has no scene count/list; this
		// increment's SceneManager has exactly one resident scene and
		// handleSceneSwitchCommand() is a documented no-op) — genuinely
		// UNSUPPORTED today, not merely disabled, so this resolves
		// missing rather than present-false. Revisit once SceneManager
		// exposes a real multi-scene signal.
		return HudResolvedValue::missing(HudSourceValueType::Boolean, HudDataClass::Literal);
	}
	if (sourceId == "control.media.previous.available") {
		// No video service configured at all -> UNSUPPORTED (missing).
		// Video configured but at a catalog boundary (canSelectPrevious
		// == false) -> DISABLED (present, false) — a real, situational
		// signal from VideoPlaybackStatus, not a capability gap.
		if (!frame.video) return HudResolvedValue::missing(HudSourceValueType::Boolean, HudDataClass::Literal);
		return HudResolvedValue::boolean(frame.video->canSelectPrevious);
	}
	if (sourceId == "control.media.next.available") {
		if (!frame.video) return HudResolvedValue::missing(HudSourceValueType::Boolean, HudDataClass::Literal);
		return HudResolvedValue::boolean(frame.video->canSelectNext);
	}
	if (sourceId == "control.scene.reseed.available") {
		bool supported = std::any_of(frame.capabilities.commands.begin(), frame.capabilities.commands.end(),
			[](const SceneCommandDescriptor& d) { return d.command == SceneCommand::Regenerate; });
		// Once advertised, Regenerate has no further "temporarily
		// disabled" concept in this contract — capability-advertised is
		// the whole signal, so this is a strict UNSUPPORTED-or-ENABLED
		// binary (never the DISABLED middle state).
		if (!supported) return HudResolvedValue::missing(HudSourceValueType::Boolean, HudDataClass::Literal);
		return HudResolvedValue::boolean(true);
	}

	return std::nullopt;
}

// Architecture-Closure Session (DEC-015/DEC-016) — the canonical production
// effect-activity binding. `frame.effects` (std::optional<
// videoeffects::EffectActivityStatus>) is authoritative whenever present:
//   - std::nullopt            -> "no authoritative snapshot available this
//                                 frame" (NOT "zero effects" — every
//                                 effects.* slot resolves MISSING, per
//                                 HudFrameData.h's own header comment).
//   - present, slots.empty()  -> "present-empty": a fully valid, common
//                                 state (EffectActivityStatus.h's own
//                                 comment: "health==Ready with EMPTY slots
//                                 is a fully valid, common state"), never
//                                 downgraded to Failed/Degraded on that
//                                 basis alone.
//   - present, slots non-empty -> "present-active": canonical IDs/display
//                                 data via the shared, deterministic
//                                 resolveDominantEffectIds() — never this
//                                 file's own tie-breaker, never vector
//                                 order (see that function's own
//                                 "Dominance-resolution rules" comment).
std::optional<HudResolvedValue> HudRealFrameResolver::resolveEffects(const std::string& sourceId, const HudFrameData& frame) const {
	using videoeffects::EffectActivityStatus;
	using videoeffects::resolveDominantEffectIds;

	if (frame.effects) {
		const EffectActivityStatus& status = *frame.effects;
		// Default DominanceConfig (maxLabels=2, minProminenceToShow=0.05,
		// annotateTransitioning=true) — the same config
		// EffectActivityStatus.h's own header comment documents as the
		// canonical effects.active/effects.dominant/effects.transition.progress
		// mapping; this file does not override any of its fields.
		auto dominant = resolveDominantEffectIds(status);

		if (sourceId == "effects.active") {
			HudIdentifierList list;
			for (const auto& d : dominant) list.push(d.effectId);
			return HudResolvedValue::list(list); // present, possibly empty — present-empty is not missing
		}
		if (sourceId == "effects.dominant") {
			if (dominant.empty()) return HudResolvedValue::missing(HudSourceValueType::Identifier, HudDataClass::Literal);
			return HudResolvedValue::identifier(dominant.front().effectId);
		}
		if (sourceId == "effects.transition.progress") {
			// Only present when the dominant group is actually
			// transitioning — a steady-state dominant effect (transitioning
			// == false, transitionProgress01 defaulted to 1.0f) has no
			// meaningful "in-progress" value to show, so this resolves
			// missing rather than a misleading fixed 1.0.
			if (dominant.empty() || !dominant.front().transitioning) {
				return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
			}
			return HudResolvedValue::number(HudSourceValueType::Ratio, dominant.front().transitionProgress01, HudDataClass::Normalized);
		}
		if (sourceId == "effects.intensity") {
			// DEC-016, verbatim: "`prominence` is a dominance-ranking
			// value and is not `effects.intensity`; `effects.intensity`
			// may remain absent in v1." This stays missing regardless of
			// EffectActivityStatus.h's own inline comment suggesting
			// `prominence` as an approximation for this slot — that
			// comment predates DEC-016 and is superseded by it; see this
			// session's closure review, "Effect binding deviations," for
			// the discrepancy. prominence is never read here.
			return HudResolvedValue::missing(HudSourceValueType::Ratio, HudDataClass::Normalized);
		}
		if (sourceId == "effects.health") {
			// Direct from the frozen enum — never inferred from slot
			// count (EffectActivityStatus.h's own comment: a caller "must
			// never infer Failed/Degraded merely because slots is empty").
			return HudResolvedValue::identifier(effectHealthId(status.health));
		}
	}

	// No authoritative snapshot this frame. Every effects.* slot resolves
	// missing EXCEPT effects.active, which may optionally fall back to
	// the compatibility field — gated behind an explicit, off-by-default
	// flag (see this method's declaration comment) so production code
	// never silently substitutes compatibility data for an honestly-
	// missing authoritative snapshot; only an explicitly-opted-in
	// tooling/fixture caller (e.g. one deliberately-labeled Validation
	// Studio scenario) sees this path exercised at all.
	if (sourceId == "effects.active" && compatibilityFallbackEnabled_) {
		return HudResolvedValue::list(toIdentifierList(frame.scene.activeEffects));
	}
	auto staticType = HudSourceResolver::staticValueTypeOf(sourceId);
	return HudResolvedValue::missing(staticType ? *staticType : HudSourceValueType::Identifier, HudDataClass::Literal);
}

std::optional<HudResolvedValue> HudRealFrameResolver::resolveSceneMetric(const std::string& sourceId, const HudFrameData& frame) const {
	if (!frame.scene.semantic) {
		return HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
	}
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
	return HudResolvedValue::missing(HudSourceValueType::Scalar, HudDataClass::Literal);
}

} // namespace hudpresent
