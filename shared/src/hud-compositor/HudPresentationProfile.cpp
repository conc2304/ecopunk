#include "HudPresentationProfile.h"

#include <algorithm>

// ============================================================================
// Concrete compiled-C++ profiles.
//
// State/metric source IDs referenced below match
// docs/probes/hud-runtime-validation-studio-probe.md §11's six evidence
// maps and HUD-Semantic-Slot-Model-v1.md §18's six scene mappings (both
// already converge on the same IDs independently) — reused here as
// provisional IDs, per this task's own instruction not to freeze real
// semantic-slot IDs. The matching fake data lives in
// shared/src/hud-compositor/fake/ — see each Fake*Scenario.cpp.
// ============================================================================

namespace hudpresent {

namespace {

HudSlotBinding makeBinding(
	std::string id,
	std::string region,
	HudWidgetType type,
	HudMissingPolicy missingPolicy,
	HudFormatKind format = HudFormatKind::Automatic,
	std::optional<std::string> labelVocabularyId = std::nullopt) {
	HudSlotBinding b;
	b.bindingId = std::move(id);
	b.regionId = std::move(region);
	b.widgetType = type;
	b.missingPolicy = missingPolicy;
	b.format = format;
	b.labelVocabularyId = std::move(labelVocabularyId);
	return b;
}

ScenePresentationProfile buildUniversalProfile() {
	ScenePresentationProfile p;
	p.profileId = "universal";

	// -- Identity (required, per HUD-Semantic-Slot-Model-v1.md §12.1) ---
	{
		auto b = makeBinding("universal.scene_title.label", "universal.scene_title",
			HudWidgetType::Label, HudMissingPolicy::Placeholder, HudFormatKind::VocabularyValue);
		b.addSource({"text", "scene.title", true});
		p.bindings.push_back(b);
	}
	{
		auto b = makeBinding("universal.media_title.label", "universal.media_title",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::VocabularyValue);
		b.addSource({"text", "media.title", false});
		p.bindings.push_back(b);
	}
	{
		auto b = makeBinding("universal.primary_state.badge", "universal.primary_state",
			HudWidgetType::StatusBadge, HudMissingPolicy::Placeholder);
		b.addSource({"value", "scene.state.primary", true});
		p.bindings.push_back(b);
	}

	// -- Activity (optional per §12.3; dimmed rather than hidden so the
	// universal region doesn't visibly flicker on a momentary nullopt) --
	{
		auto b = makeBinding("universal.activity.bar", "universal.activity",
			HudWidgetType::ProgressBar, HudMissingPolicy::DimLastValid, HudFormatKind::Percentage,
			std::string("slot.scene.activity.label"));
		b.addSource({"value", "scene.activity.overall", true});
		p.bindings.push_back(b);
	}

	// -- Effect summary + timing (deferred from the first vertical slice
	// per this task's §13, still fully defined/compiled/testable here) --
	{
		auto b = makeBinding("universal.effect_summary.chips", "universal.effect_summary",
			HudWidgetType::EffectChips, HudMissingPolicy::Hide);
		b.addSource({"value", "effects.active", true});
		p.bindings.push_back(b);
	}
	{
		// Architecture-Closure Session (DEC-015/DEC-016): the frozen
		// EffectActivityStatus::health (Ready/Degraded/Failed), owned by
		// Shared Effects — never derived from slot count (an empty list
		// with health==Ready is a fully valid state, not a failure; see
		// HudRealFrameResolver.cpp's effects.health handling). Hidden on
		// the fake-scenario path (no real EffectActivityStatus exists
		// there, so this always resolves missing) — visible only via the
		// real HudFrameData path, same as every other effects.* slot.
		auto b = makeBinding("universal.effect_summary.health", "universal.effect_summary",
			HudWidgetType::StatusBadge, HudMissingPolicy::Hide);
		b.addSource({"value", "effects.health", true});
		p.bindings.push_back(b);
	}
	{
		auto b = makeBinding("universal.timing.value", "universal.timing",
			HudWidgetType::NumericValue, HudMissingPolicy::RetainLastValid, HudFormatKind::Duration,
			std::string("slot.scene.timing.label"));
		b.addSource({"value", "scene.time.active", true});
		p.bindings.push_back(b);
	}

	// -- Traces (deferred/secondary per §13; still defined here) --------
	{
		auto b = makeBinding("universal.activity_trace.spark", "universal.activity_trace",
			HudWidgetType::Sparkline, HudMissingPolicy::Hide);
		b.addSource({"value", "scene.activity.overall", true});
		b.history = HudHistoryRequest{32, 8.0f};
		p.bindings.push_back(b);
	}
	{
		auto b = makeBinding("universal.secondary_trace.spark", "universal.secondary_trace",
			HudWidgetType::Sparkline, HudMissingPolicy::Hide);
		b.addSource({"value", "scene.activity.motion", true});
		b.history = HudHistoryRequest{32, 8.0f};
		p.bindings.push_back(b);
	}

	// -- Controls (nonfunctional visual states only — see this task's
	// frozen constraints; "enabled" governs dim/nonfunctional rendering,
	// never actual command dispatch) --------------------------------
	auto addControl = [&](const char* bindingId, const char* regionId, const char* vocabId, const char* availSource) {
		auto b = makeBinding(bindingId, regionId, HudWidgetType::Label, HudMissingPolicy::Hide,
			HudFormatKind::VocabularyValue, std::string(vocabId));
		b.addSource({"enabled", availSource, false});
		p.bindings.push_back(b);
	};
	addControl("controls.scene_previous.label", "controls.scene_previous",
		"control.scene.previous.label", "control.scene.previous.available");
	addControl("controls.scene_next.label", "controls.scene_next",
		"control.scene.next.label", "control.scene.next.available");
	addControl("controls.media_previous.label", "controls.media_previous",
		"control.media.previous.label", "control.media.previous.available");
	addControl("controls.media_next.label", "controls.media_next",
		"control.media.next.label", "control.media.next.available");
	addControl("controls.reseed.label", "controls.reseed",
		"control.scene.reseed.label", "control.scene.reseed.available");

	return p;
}

ScenePresentationProfile buildOverlayHealthProfile() {
	ScenePresentationProfile p;
	p.profileId = "overlay.health";
	auto b = makeBinding("overlay.health.badge", "overlay.health",
		HudWidgetType::StatusBadge, HudMissingPolicy::RetainLastValid);
	b.addSource({"value", "scene.health", true});
	p.bindings.push_back(b);
	return p;
}

ScenePresentationProfile buildOverlayTransitionProfile() {
	ScenePresentationProfile p;
	p.profileId = "overlay.transition";
	auto b = makeBinding("overlay.transition.bar", "overlay.transition",
		HudWidgetType::ProgressBar, HudMissingPolicy::RetainLastValid, HudFormatKind::Percentage,
		std::string("overlay.transition.label"));
	b.addSource({"value", "manager.transition.progress", true});
	p.bindings.push_back(b);

	// Phase + message — this task's §4 "Bind SceneManager-owned:
	// transition phase; transition progress; transition message." Second
	// binding into the same region (now maxBindings=2, see
	// HudRegionCatalog.cpp) rather than teaching ProgressBar a second
	// role — "use an overlay/profile layer rather than teaching every
	// widget manager transition semantics" (this task's own instruction).
	{
		auto phaseLabel = makeBinding("overlay.transition.phase_label", "overlay.transition",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::VocabularyValue);
		phaseLabel.addSource({"text", "manager.transition.phase", true});
		p.bindings.push_back(phaseLabel);
	}
	{
		// Optional — most frames have no manager message at all (only
		// populated on a failed transition, per SceneManagerStatus's own
		// contract). Hide policy: no message this frame -> nothing drawn,
		// never a placeholder ("—") cluttering the common case.
		auto messageLabel = makeBinding("overlay.transition.message_label", "overlay.transition",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic);
		messageLabel.addSource({"text", "manager.message", false});
		p.bindings.push_back(messageLabel);
	}
	return p;
}

ScenePresentationProfile buildBlobProfile() {
	ScenePresentationProfile p;
	p.profileId = "blob-region-prototype";
	auto b = makeBinding("blob.flexible_primary.card", "flexible.primary",
		HudWidgetType::MetadataCard, HudMissingPolicy::UseAmbientFallback,
		HudFormatKind::Automatic, std::string("scene.blob.card.label"));
	b.addSource({"title", "scene.state.primary", true});
	b.addSource({"value", "scene.blob.metric.region_count", false});
	p.bindings.push_back(b);
	return p;
}

ScenePresentationProfile buildContourProfile() {
	ScenePresentationProfile p;
	p.profileId = "contour-portrait";
	auto b = makeBinding("contour.flexible_primary.ring", "flexible.primary",
		HudWidgetType::ProgressRing, HudMissingPolicy::UseAmbientFallback, HudFormatKind::Percentage,
		std::string("scene.contour.metric.breakup_progress.label"));
	b.addSource({"value", "scene.contour.metric.breakup_progress", true});
	p.bindings.push_back(b);
	return p;
}

ScenePresentationProfile buildTemporalProfile() {
	ScenePresentationProfile p;
	p.profileId = "temporal-fields";
	auto b = makeBinding("temporal.flexible_primary.spark", "flexible.primary",
		HudWidgetType::Sparkline, HudMissingPolicy::UseAmbientFallback);
	b.addSource({"value", "scene.temporal.metric.field_activity", true});
	b.history = HudHistoryRequest{32, 8.0f};
	p.bindings.push_back(b);
	return p;
}

ScenePresentationProfile buildFragmentProfile() {
	ScenePresentationProfile p;
	p.profileId = "fragment-trail";
	auto b = makeBinding("fragment.flexible_primary.card", "flexible.primary",
		HudWidgetType::MetadataCard, HudMissingPolicy::UseAmbientFallback,
		HudFormatKind::Automatic, std::string("scene.fragment.card.label"));
	b.addSource({"title", "scene.state.primary", true});
	b.addSource({"value", "scene.fragment.metric.fragment_count", false});
	b.addSource({"meta", "scene.fragment.metric.movement_preset", false});
	p.bindings.push_back(b);
	return p;
}

ScenePresentationProfile buildQuadrantProfile() {
	ScenePresentationProfile p;
	p.profileId = "quadrant-crosshair";
	auto b = makeBinding("quadrant.flexible_primary.channels", "flexible.primary",
		HudWidgetType::ChannelStrip, HudMissingPolicy::UseAmbientFallback,
		HudFormatKind::Automatic, std::string("scene.quadrant.channels.label"));
	b.addSource({"channel0", "scene.quadrant.metric.channel_0_state", true});
	b.addSource({"channel1", "scene.quadrant.metric.channel_1_state", false});
	b.addSource({"channel2", "scene.quadrant.metric.channel_2_state", false});
	b.addSource({"channel3", "scene.quadrant.metric.channel_3_state", false});
	p.bindings.push_back(b);
	return p;
}

ScenePresentationProfile buildBlueprintProfile() {
	ScenePresentationProfile p;
	p.profileId = "blueprint_emergence";
	auto b = makeBinding("blueprint.flexible_primary.ring", "flexible.primary",
		HudWidgetType::ProgressRing, HudMissingPolicy::UseAmbientFallback, HudFormatKind::Percentage,
		std::string("scene.blueprint.metric.phase_progress.label"));
	b.addSource({"value", "scene.blueprint.metric.phase_progress", true});
	p.bindings.push_back(b);
	return p;
}

} // namespace

HudPresentationProfileRegistry::HudPresentationProfileRegistry() {
	universal_ = buildUniversalProfile();
	overlayHealth_ = buildOverlayHealthProfile();
	overlayTransition_ = buildOverlayTransitionProfile();

	flexibleProfiles_.push_back(buildBlobProfile());
	flexibleProfiles_.push_back(buildContourProfile());
	flexibleProfiles_.push_back(buildTemporalProfile());
	flexibleProfiles_.push_back(buildFragmentProfile());
	flexibleProfiles_.push_back(buildQuadrantProfile());
	flexibleProfiles_.push_back(buildBlueprintProfile());
}

const ScenePresentationProfile* HudPresentationProfileRegistry::flexibleProfileForScene(const std::string& sceneId) const {
	auto it = std::find_if(flexibleProfiles_.begin(), flexibleProfiles_.end(),
		[&](const ScenePresentationProfile& p) { return p.profileId == sceneId; });
	return it == flexibleProfiles_.end() ? nullptr : &(*it);
}

std::vector<std::string> HudPresentationProfileRegistry::knownSceneIds() const {
	std::vector<std::string> ids;
	ids.reserve(flexibleProfiles_.size());
	for (const auto& p : flexibleProfiles_) ids.push_back(p.profileId);
	return ids;
}

} // namespace hudpresent
