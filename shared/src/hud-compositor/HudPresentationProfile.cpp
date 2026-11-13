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
		b.addSource({"text", "media.file_label", false});
		b.addSource({"progress", "media.playback.progress", false});
		b.addSource({"suffix", "media.playback.time", false});
		b.addSource({"sample", "media.sample_label", false});
		b.addSource({"hold", "media.hold.progress", false}); // present only in automatic mode
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
		b.variantId = "frameless"; // region 4: no widget-drawn outline (Blueprint v3 reference review)
		b.addSource({"value", "scene.state.primary", true});
		// v0.3: the scene's secondary state (e.g. Quadrant's active
		// strategy/effect) and a phase icon keyed by the primary state ID.
		b.addSource({"detail", "scene.state.secondary", false});
		b.addSource({"icon", "scene.state.primary", false});
		p.bindings.push_back(b);
	}

	// -- Activity (OC-004): the segmented meter shows the runtime-measured visual activity
	// (scene.visual.activity: 64x36 frame difference, fixed-log level, HUD-smoothed), NOT the
	// scene-semantic scene.activity.overall. Placeholder policy: sampling disabled, Unavailable,
	// Stale or Transition render an explicit "--" with an empty bar - never a retained/dimmed old
	// value, never a "0%" that could pass for real zero activity, never a fallback source. --
	{
		auto b = makeBinding("universal.activity.bar", "universal.timing",
			HudWidgetType::ProgressBar, HudMissingPolicy::Placeholder, HudFormatKind::Percentage,
			std::string("slot.scene.activity.label"));
		b.addSource({"value", "scene.visual.activity", true});
		b.variantId = "segmented";
		p.bindings.push_back(b);
	}

	// -- Effect summary + timing (deferred from the first vertical slice
	// per this task's §13, still fully defined/compiled/testable here) --
	{
		// Active effects in universal.effect_summary (Orbit Console v0.1
		// reallocation: shares the region with the scene status), with the frozen
		// EffectActivityStatus::health (DEC-015/DEC-016: never derived from
		// slot count) as a right-aligned trailing "status" on the same row
		// instead of the old separate stacked badge. Hide: no authoritative
		// snapshot -> nothing; the optional status is simply not drawn.
		auto b = makeBinding("universal.effect_summary.chips", "universal.effect_summary",
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
	addVital("universal.vitals.fps", "flexible.primary", "runtime.vitals.fps", "slot.runtime.vitals.fps.label");
	addVital("universal.vitals.frame_time", "flexible.primary", "runtime.vitals.frame_time_max", "slot.runtime.vitals.frame_time.label");
	addVital("universal.vitals.cpu", "flexible.primary", "runtime.vitals.cpu", "slot.runtime.vitals.cpu.label");
	addVital("universal.vitals.gpu_temp", "flexible.primary", "runtime.vitals.gpu_temp", "slot.runtime.vitals.gpu_temp.label");
	addVital("universal.vitals.rss", "flexible.primary", "runtime.vitals.rss", "slot.runtime.vitals.rss.label");

	// -- Orbit Console v0.1: one coordinated instrument over the two trace
	// regions. Both bindings key off runtime.scene_color (the series itself is
	// read by the renderer's orbit cache, like the vitals traces); the widget
	// draws an honest NO DATA / STALE state, never zeros.
	auto addOrbit = [&](const char* bindingId, const char* regionId, const char* variant) {
		auto b = makeBinding(bindingId, regionId, HudWidgetType::Sparkline, HudMissingPolicy::Hide);
		b.variantId = variant;
		b.addSource({"value", "runtime.scene_color", true});
		p.bindings.push_back(b);
	};
	addOrbit("universal.orbit.upper", "universal.activity_trace", "orbit.upper");
	addOrbit("universal.orbit.lower", "universal.secondary_trace", "orbit.lower");

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
	// Orbit Console v0.1 reallocation: scene status lives in universal.effect_summary
	// (compact, one line); flexible.primary now hosts System Performance.
	auto b = makeBinding("blob.status.regions", "universal.effect_summary",
		HudWidgetType::NumericValue, HudMissingPolicy::Hide,
		HudFormatKind::Automatic, std::string("scene.blob.card.label"));
	b.variantId = "inline.col";
	b.addSource({"value", "scene.blob.metric.region_count", true});
	p.bindings.push_back(b);
	// Second line (Option B: two visibility-aware slots in universal.effect_summary).
	auto frag = makeBinding("blob.status.fragments", "universal.effect_summary",
		HudWidgetType::NumericValue, HudMissingPolicy::Hide,
		HudFormatKind::Automatic, std::string("scene.blob.fragments.label"));
	frag.variantId = "inline.col";
	frag.addSource({"value", "scene.blob.metric.fragment_count", true});
	p.bindings.push_back(frag);
	return p;
}

ScenePresentationProfile buildContourProfile() {
	ScenePresentationProfile p;
	p.profileId = "contour-portrait";
	auto b = makeBinding("contour.status.breakup", "universal.effect_summary",
		HudWidgetType::NumericValue, HudMissingPolicy::Hide, HudFormatKind::Percentage,
		std::string("scene.contour.metric.breakup_progress.label"));
	b.variantId = "inline";
	b.addSource({"value", "scene.contour.metric.breakup_progress", true});
	p.bindings.push_back(b);
	return p;
}

ScenePresentationProfile buildTemporalProfile() {
	ScenePresentationProfile p;
	p.profileId = "temporal-fields";
	auto b = makeBinding("temporal.status.field", "universal.effect_summary",
		HudWidgetType::NumericValue, HudMissingPolicy::Hide, HudFormatKind::Automatic,
		std::string("scene.temporal.metric.field_activity.label"));
	b.variantId = "inline";
	b.addSource({"value", "scene.temporal.metric.field_activity", true});
	p.bindings.push_back(b);
	return p;
}

ScenePresentationProfile buildFragmentProfile() {
	ScenePresentationProfile p;
	p.profileId = "fragment-trail";
	auto b = makeBinding("fragment.status.count", "universal.effect_summary",
		HudWidgetType::NumericValue, HudMissingPolicy::Hide,
		HudFormatKind::Automatic, std::string("scene.fragment.card.label"));
	b.variantId = "inline.col";
	b.addSource({"value", "scene.fragment.metric.fragment_count", true});
	p.bindings.push_back(b);
	// Second line: the movement preset (identifier -> vocabulary, e.g. HUNT).
	auto preset = makeBinding("fragment.status.preset", "universal.effect_summary",
		HudWidgetType::NumericValue, HudMissingPolicy::Hide,
		HudFormatKind::Automatic, std::string("scene.fragment.preset.label"));
	preset.variantId = "inline.col";
	preset.addSource({"value", "scene.fragment.metric.movement_preset", true});
	p.bindings.push_back(preset);
	return p;
}

ScenePresentationProfile buildQuadrantProfile() {
	ScenePresentationProfile p;
	p.profileId = "quadrant-crosshair";
	auto b = makeBinding("quadrant.status.channels", "universal.effect_summary",
		HudWidgetType::ChannelStrip, HudMissingPolicy::Hide,
		HudFormatKind::Automatic, std::string("scene.quadrant.channels.label"));
	b.variantId = "grid"; // 2x2 channel states, full #5 height
	b.addSource({"channel0", "scene.quadrant.metric.channel_0_state", true});
	b.addSource({"channel1", "scene.quadrant.metric.channel_1_state", false});
	b.addSource({"channel2", "scene.quadrant.metric.channel_2_state", false});
	b.addSource({"channel3", "scene.quadrant.metric.channel_3_state", false});
	p.bindings.push_back(b);
	return p;
}

ScenePresentationProfile buildCrtProfile() {
	ScenePresentationProfile p;
	p.profileId = "crt-phosphor";
	// One interactive selector group in universal.effect_summary: two compact option rows
	// (PIXELS / PALETTE), 21.6 px each while the effect chips are hidden. The composition MODE is
	// NOT repeated here: universal.media_title already presents it (scene.mode.*) and
	// controls.mode already steps it. Rows read scene metrics; the < > chevrons and the row
	// cursor are drawn from the interaction phase, not from this profile.
	auto row = [&p](const char* bindingId, const char* caption, const char* textSource, const char* positionSource) {
		auto b = makeBinding(bindingId, "universal.effect_summary",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic, std::string(caption));
		b.variantId = "selector";
		b.addSource({"text", textSource, true});
		b.addSource({"suffix", positionSource, false});
		p.bindings.push_back(b);
	};
	row("crt.selector.pixels", "scene.crt.row.pixels.label", "scene.crt.metric.structure", "scene.crt.metric.structure_pos");
	row("crt.selector.palette", "scene.crt.row.palette.label", "scene.crt.metric.palette", "scene.crt.metric.palette_pos");
	p.interactiveRegions.push_back({"universal.effect_summary", HudInteractionKind::SelectorGroup, 2});
	return p;
}

// Shared Effects Gallery: up to TWO curated effect controls, in the same interactive selector group
// (universal.effect_summary) the CRT scene uses. Rows are data-driven: each reads its own caption,
// value and optional position from scene metrics ("caption" role = dynamic label), and hides when
// the active mode has no such control. The interactive declaration is the MAXIMUM row count.
ScenePresentationProfile buildFxGalleryProfile() {
	ScenePresentationProfile p;
	p.profileId = "shared-effects-gallery";
	auto row = [&p](const char* bindingId, const char* label, const char* value, const char* pos) {
		auto b = makeBinding(bindingId, "universal.effect_summary",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic);
		b.variantId = "selector";
		b.addSource({"text", value, true});
		b.addSource({"suffix", pos, false});
		b.addSource({"caption", label, false});
		p.bindings.push_back(b);
	};
	row("fx.control.0", "scene.fx.metric.c0_label", "scene.fx.metric.c0_value", "scene.fx.metric.c0_pos");
	row("fx.control.1", "scene.fx.metric.c1_label", "scene.fx.metric.c1_value", "scene.fx.metric.c1_pos");
	p.interactiveRegions.push_back({"universal.effect_summary", HudInteractionKind::SelectorGroup, 2});
	return p;
}

// Phosphor Contours: two selector rows (DENSITY / DECAY) in the same interactive selector group, data-driven
// exactly like the Shared Effects Gallery (caption, value and position all come from scene metrics).
ScenePresentationProfile buildContoursProfile() {
	ScenePresentationProfile p;
	p.profileId = "phosphor-contours";
	auto row = [&p](const char* bindingId, const char* label, const char* value, const char* pos) {
		auto b = makeBinding(bindingId, "universal.effect_summary",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic);
		b.variantId = "selector";
		b.addSource({"text", value, true});
		b.addSource({"suffix", pos, false});
		b.addSource({"caption", label, false});
		p.bindings.push_back(b);
	};
	row("contours.control.0", "scene.contours.metric.c0_label", "scene.contours.metric.c0_value", "scene.contours.metric.c0_pos");
	row("contours.control.1", "scene.contours.metric.c1_label", "scene.contours.metric.c1_value", "scene.contours.metric.c1_pos");
	p.interactiveRegions.push_back({"universal.effect_summary", HudInteractionKind::SelectorGroup, 2});
	return p;
}

// Raw Video: three selector rows (BRIGHTNESS / CONTRAST / SATURATION) in the same interactive selector group,
// data-driven exactly like Contours (caption, value and position all come from scene metrics).
ScenePresentationProfile buildRawVideoProfile() {
	ScenePresentationProfile p;
	p.profileId = "raw-video";
	auto row = [&p](const char* bindingId, const char* label, const char* value, const char* pos) {
		auto b = makeBinding(bindingId, "universal.effect_summary",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic);
		b.variantId = "selector";
		b.addSource({"text", value, true});
		b.addSource({"suffix", pos, false});
		b.addSource({"caption", label, false});
		p.bindings.push_back(b);
	};
	row("raw.control.0", "scene.raw.metric.c0_label", "scene.raw.metric.c0_value", "scene.raw.metric.c0_pos");
	row("raw.control.1", "scene.raw.metric.c1_label", "scene.raw.metric.c1_value", "scene.raw.metric.c1_pos");
	row("raw.control.2", "scene.raw.metric.c2_label", "scene.raw.metric.c2_value", "scene.raw.metric.c2_pos");
	p.interactiveRegions.push_back({"universal.effect_summary", HudInteractionKind::SelectorGroup, 3});
	return p;
}

// Shred Reconstruction: two selector rows = the ACTIVE mode's two dials, in the same interactive selector group,
// data-driven exactly like Contours / FX Gallery (caption, value and position all come from scene metrics).
ScenePresentationProfile buildShredProfile() {
	ScenePresentationProfile p;
	p.profileId = "shred-reconstruction";
	auto row = [&p](const char* bindingId, const char* label, const char* value, const char* pos) {
		auto b = makeBinding(bindingId, "universal.effect_summary",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic);
		b.variantId = "selector";
		b.addSource({"text", value, true});
		b.addSource({"suffix", pos, false});
		b.addSource({"caption", label, false});
		p.bindings.push_back(b);
	};
	row("shred.control.0", "scene.shred.metric.c0_label", "scene.shred.metric.c0_value", "scene.shred.metric.c0_pos");
	row("shred.control.1", "scene.shred.metric.c1_label", "scene.shred.metric.c1_value", "scene.shred.metric.c1_pos");
	p.interactiveRegions.push_back({"universal.effect_summary", HudInteractionKind::SelectorGroup, 2});
	return p;
}

// Mosaic Assembly: two selector rows = the ACTIVE mode's two dials, same interactive selector group and data-driven
// metric binding as Shred Reconstruction / Contours / FX Gallery. No new geometry or scene-specific shared-HUD branch.
ScenePresentationProfile buildMosaicProfile() {
	ScenePresentationProfile p;
	p.profileId = "mosaic-assembly";
	auto row = [&p](const char* bindingId, const char* label, const char* value, const char* pos) {
		auto b = makeBinding(bindingId, "universal.effect_summary",
			HudWidgetType::Label, HudMissingPolicy::Hide, HudFormatKind::Automatic);
		b.variantId = "selector";
		b.addSource({"text", value, true});
		b.addSource({"suffix", pos, false});
		b.addSource({"caption", label, false});
		p.bindings.push_back(b);
	};
	row("mosaic.control.0", "scene.mosaic.metric.c0_label", "scene.mosaic.metric.c0_value", "scene.mosaic.metric.c0_pos");
	row("mosaic.control.1", "scene.mosaic.metric.c1_label", "scene.mosaic.metric.c1_value", "scene.mosaic.metric.c1_pos");
	p.interactiveRegions.push_back({"universal.effect_summary", HudInteractionKind::SelectorGroup, 2});
	return p;
}

ScenePresentationProfile buildBlueprintProfile() {
	ScenePresentationProfile p;
	p.profileId = "blueprint_emergence";
	// Progress text + thin bar. Placeholder (not Hide): when phase_progress is absent the
	// slot keeps "CYCLE PROGRESS" with an explicit "-" and an empty bar instead of vanishing.
	auto b = makeBinding("blueprint.status.cycle", "universal.effect_summary",
		HudWidgetType::ProgressBar, HudMissingPolicy::Placeholder, HudFormatKind::Percentage,
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
	flexibleProfiles_.push_back(buildCrtProfile());
	flexibleProfiles_.push_back(buildFxGalleryProfile());
	flexibleProfiles_.push_back(buildContoursProfile());
	flexibleProfiles_.push_back(buildRawVideoProfile());
	flexibleProfiles_.push_back(buildShredProfile());
	flexibleProfiles_.push_back(buildMosaicProfile());
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
