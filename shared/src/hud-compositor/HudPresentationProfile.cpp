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
		b.variantId = "title"; // 1.5x text, 10 px left padding
		b.addSource({"text", "scene.title", true});
		// Scene & Pattern Navigation: "  2/5" from SceneManager's registry.
		b.addSource({"suffix", "scene.position", false});
		p.bindings.push_back(b);
	}
	{
		// HUD & Scene Experience v0.3: the media slot shows the media title
		// plus playback progress (bar + "current / total") from canonical
		// Shared Video status. Automatic formatting resolves the title exactly
		// as VocabularyValue did (Identifier -> vocabulary, Text -> as-is).
		auto b = makeBinding("universal.media_title.label", "universal.primary_state",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic);
		b.addSource({"text", "media.title", false});
		b.addSource({"progress", "media.playback.progress", false});
		b.addSource({"suffix", "media.playback.time", false});
		p.bindings.push_back(b);
	}
	{
		// Scene mode on ONE line in universal.media_title: "Mode: <name>   x/y"
		// (informational; NEXT MODE is the controls.mode button).
		{
			auto m = makeBinding("universal.mode.line", "universal.media_title",
				HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic,
				std::string("slot.scene.mode.line.label"));
			m.variantId = "modeline";
			m.addSource({"text", "scene.mode.name", true});
			m.addSource({"suffix", "scene.mode.position", false});
			p.bindings.push_back(m);
		}
		auto b = makeBinding("universal.primary_state.badge", "universal.activity",
			HudWidgetType::StatusBadge, HudMissingPolicy::Placeholder);
		b.addSource({"value", "scene.state.primary", true});
		// v0.3: the scene's secondary state (e.g. Quadrant's active
		// strategy/effect) and a phase icon keyed by the primary state ID.
		b.addSource({"detail", "scene.state.secondary", false});
		b.addSource({"icon", "scene.state.primary", false});
		p.bindings.push_back(b);
	}

	// -- Activity (optional per §12.3; dimmed rather than hidden so the
	// universal region doesn't visibly flicker on a momentary nullopt) --
	{
		auto b = makeBinding("universal.activity.bar", "universal.effect_summary",
			HudWidgetType::ProgressBar, HudMissingPolicy::DimLastValid, HudFormatKind::Percentage,
			std::string("slot.scene.activity.label"));
		b.addSource({"value", "scene.activity.overall", true});
		p.bindings.push_back(b);
	}

	// -- Effect summary + timing (deferred from the first vertical slice
	// per this task's §13, still fully defined/compiled/testable here) --
	{
		// Active effects in universal.timing, with the frozen
		// EffectActivityStatus::health (DEC-015/DEC-016: never derived from
		// slot count) as a right-aligned trailing "status" on the same row
		// instead of the old separate stacked badge. Hide: no authoritative
		// snapshot -> nothing; the optional status is simply not drawn.
		auto b = makeBinding("universal.effect_summary.chips", "universal.timing",
			HudWidgetType::EffectChips, HudMissingPolicy::Hide);
		b.addSource({"value", "effects.active", true});
		b.addSource({"status", "effects.health", false});
		p.bindings.push_back(b);
	}
	{
		auto b = makeBinding("universal.timing.value", "flexible.secondary_a",
			HudWidgetType::NumericValue, HudMissingPolicy::RetainLastValid, HudFormatKind::Duration,
			std::string("slot.scene.timing.label"));
		b.variantId = "inline"; // "ACTIVE TIME    3:42" on one line
		b.addSource({"value", "scene.time.active", true});
		p.bindings.push_back(b);
	}

	// -- HUD Clock v0.1 (Architecture-approved): runtime-owned local wall
	// clock in the frozen flexible.secondary_b region (NumericValue is an
	// accepted widget there). value = local time of day -> "14:32"; the
	// contracted optional label role carries "THU 15 OCT 2026 CEST". Hide:
	// no valid snapshot -> the region stays recessed. Read-only — no
	// "enabled" role, so focus navigation never treats it as a control.
	{
		auto b = makeBinding("universal.clock.value", "flexible.secondary_b",
			HudWidgetType::NumericValue, HudMissingPolicy::Hide, HudFormatKind::ClockTime);
		b.variantId = "clock"; // large time left, two-line date/zone right
		b.addSource({"value", "runtime.clock.local_seconds", true});
		b.addSource({"label", "runtime.clock.local_label", false});
		p.bindings.push_back(b);
	}

	// -- System Telemetry v1 traces: five system-vitals sparklines in the two
	// trace regions (3 + 2), fed by HudFrameData::systemVitals (timestamped,
	// runtime-global 60 s / 5 min histories) — NOT by HudHistoryStore, so no
	// `history` request. These replace the earlier FullProfile-only
	// scene.activity.overall / .motion trace bindings (never drawn in
	// production). variantId "vitals" selects the history-fed layout.
	auto addVital = [&](const char* bindingId, const char* regionId, const char* sourceId, const char* labelId) {
		auto b = makeBinding(bindingId, regionId, HudWidgetType::Sparkline, HudMissingPolicy::Hide,
			HudFormatKind::Automatic, std::string(labelId));
		b.variantId = "vitals";
		b.addSource({"value", sourceId, true});
		p.bindings.push_back(b);
	};
	addVital("universal.vitals.fps", "universal.activity_trace", "runtime.vitals.fps", "slot.runtime.vitals.fps.label");
	addVital("universal.vitals.frame_time", "universal.activity_trace", "runtime.vitals.frame_time_max", "slot.runtime.vitals.frame_time.label");
	addVital("universal.vitals.cpu", "universal.activity_trace", "runtime.vitals.cpu", "slot.runtime.vitals.cpu.label");
	addVital("universal.vitals.gpu_temp", "universal.secondary_trace", "runtime.vitals.gpu_temp", "slot.runtime.vitals.gpu_temp.label");
	addVital("universal.vitals.rss", "universal.secondary_trace", "runtime.vitals.rss", "slot.runtime.vitals.rss.label");

	// -- Controls (nonfunctional visual states only — see this task's
	// frozen constraints; "enabled" governs dim/nonfunctional rendering,
	// never actual command dispatch) --------------------------------
	auto addControl = [&](const char* bindingId, const char* regionId, const char* vocabId, const char* availSource) {
		auto b = makeBinding(bindingId, regionId, HudWidgetType::Label, HudMissingPolicy::Hide,
			HudFormatKind::VocabularyValue, std::string(vocabId));
		b.addSource({"enabled", availSource, false});
		b.variantId = "button"; // label centred in its control
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
	// HUD-006 v2: controls.reseed is retired (IMMERSIVE is keyboard 'i';
	// SceneCommand::Regenerate stays on key '2'). MODE (SceneCommand::NextMode, scene-advertised) and
	// FULLSCREEN, which enters IMMERSIVE: the scene fills the OF window
	// viewport (HUD-006 v2 decision D1 option (b)). Native window fullscreen
	// stays on key 'f'. Static caption — the HUD is hidden while immersive,
	// so there is no "on" state to show.
	addControl("controls.mode.label", "controls.mode",
		"control.scene.next_mode.label", "control.scene.next_mode.available");
	{
		auto b = makeBinding("controls.fullscreen.label", "controls.fullscreen", HudWidgetType::Label, HudMissingPolicy::Hide,
			HudFormatKind::VocabularyValue, std::string("control.runtime.fullscreen.label"));
		b.addSource({"enabled", "control.runtime.immersive.available", false});
		b.variantId = "button";
		p.bindings.push_back(b);
	}
	// Seventh control: MENU opens the Experience Settings page.
	addControl("controls.menu.label", "controls.menu",
		"control.runtime.menu.label", "control.runtime.menu.available");

	return p;
}

// HUD content relocation (2026-10-23): overlay.health (profile id kept) now
// hosts the TRANSITION status — "TRANSITION x%" + bar, with the scene health
// ("READY", accent) merged right-aligned on the same caption line — and the
// phase label beneath. The old transition message ("NextScene accepted ...")
// is no longer drawn.
ScenePresentationProfile buildOverlayHealthProfile() {
	ScenePresentationProfile p;
	p.profileId = "overlay.health";
	// Automatic (not Percentage): the format kind applies to every role of a
	// binding, and the trailing "status" (scene health) is an identifier;
	// Automatic still renders the Ratio value as a percentage.
	auto b = makeBinding("overlay.transition.bar", "overlay.health",
		HudWidgetType::ProgressBar, HudMissingPolicy::RetainLastValid, HudFormatKind::Automatic,
		std::string("overlay.transition.label"));
	b.variantId = "centered"; // "TRANSITION x%" centred; the health stays right-aligned
	b.addSource({"value", "manager.transition.progress", true});
	b.addSource({"status", "scene.health", false});
	p.bindings.push_back(b);
	auto phaseLabel = makeBinding("overlay.transition.phase_label", "overlay.health",
		HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::VocabularyValue);
	phaseLabel.variantId = "centered"; // "IDLE" centred
	phaseLabel.addSource({"text", "manager.transition.phase", true});
	p.bindings.push_back(phaseLabel);
	return p;
}

// overlay.transition (profile id kept) now hosts the recent activity log: the
// last few console lines, truncated to the region (2-4 lines by text size).
ScenePresentationProfile buildOverlayTransitionProfile() {
	ScenePresentationProfile p;
	p.profileId = "overlay.transition";
	auto b = makeBinding("overlay.activity_log", "overlay.transition",
		HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic);
	b.variantId = "log";
	b.addSource({"text", "runtime.log.recent", true});
	p.bindings.push_back(b);
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
