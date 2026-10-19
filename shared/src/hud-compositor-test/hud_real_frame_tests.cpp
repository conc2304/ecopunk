// Standalone tests for HudRealFrameResolver — the ONE part of this
// domain's logic layer that requires openFrameworks on the include path
// (HudFrameData.h -> SceneContract.h -> ofTexture.h), per
// HudRealFrameResolver.h's own header comment on why it's a separate file
// from the dependency-free HudSourceResolver. Built via `make -f
// Makefile.tests test-real` (separate target from the dependency-free
// `test`) — NOT swept into any sketch's build; this file lives in
// shared/src/hud-compositor-test/, a SIBLING of shared/src/hud-compositor/,
// so no sketch's PROJECT_EXTERNAL_SOURCE_PATHS ever reaches it (see this
// directory's other test file's own header comment for the collision this
// avoids).
//
// Links against openFrameworks headers only (compiled via clang++ with
// the repo's real -I paths, exactly as widget syntax-checks were verified
// in Engineering Session 1) — NOT the full libopenFrameworks.a, so no GL
// context/window is created or required; HudFrameData/SceneFrame's
// `texture` pointer is only ever compared against nullptr here, never
// dereferenced.

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include "../hud-compositor/HudRealFrameResolver.h"
#include "../hud-compositor/HudSourceResolver.h" // for the static canonical-ID table

using namespace hudpresent;

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const char* file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define HUD_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

#define HUD_CHECK_EQ_STR(a, b) \
	do { \
		g_total++; \
		std::string _a = (a); \
		std::string _b = (b); \
		if (_a != _b) { \
			std::ostringstream _oss; \
			_oss << #a << " (\"" << _a << "\") != " << #b << " (\"" << _b << "\")"; \
			reportFailure(__FILE__, __LINE__, _oss.str()); \
		} \
	} while (0)

namespace {

HudFrameData makeMinimalRealFrame() {
	HudFrameData f;
	f.schemaVersion = 1;
	f.scene.sceneId = "blob-region-prototype";
	f.scene.displayName = "Region Field Study"; // real field: already-final TEXT, not an ID
	f.scene.health = SceneHealth::Ready;
	f.sceneManager.activeSceneId = "blob-region-prototype";
	f.sceneManager.transitionPhase = SceneTransitionPhase::Idle;
	f.runtime.fps = 30.0f;
	f.runtime.frameTimeMs = 33.3f;
	return f;
}

void test_real_identity_and_health() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();

	auto id = resolver.resolve("scene.id", frame);
	HUD_CHECK(id && id->present);
	HUD_CHECK_EQ_STR(id->textValue, "blob-region-prototype");

	auto title = resolver.resolve("scene.title", frame);
	HUD_CHECK(title && title->present);
	HUD_CHECK(title->valueType == HudSourceValueType::Text); // NOT Identifier — see resolve()'s own comment
	HUD_CHECK_EQ_STR(title->textValue, "Region Field Study");

	auto health = resolver.resolve("scene.health", frame);
	HUD_CHECK(health && health->present);
	HUD_CHECK_EQ_STR(health->textValue, "ready");
}

void test_real_semantic_present_vs_absent() {
	HudRealFrameResolver resolver;

	HudFrameData noSemantic = makeMinimalRealFrame();
	// scene.semantic left std::nullopt (default) -- e.g. Loading.
	auto missingState = resolver.resolve("scene.state.primary", noSemantic);
	HUD_CHECK(missingState.has_value()); // known source, just unpopulated
	HUD_CHECK(missingState->present == false);

	HudFrameData withSemantic = makeMinimalRealFrame();
	SceneSemanticData semantic;
	semantic.state.primaryStateId = "scene.blob.state.analyzing";
	semantic.activity.overall = 0.75f;
	semantic.timing.activeSeconds = 12.5f;
	semantic.timing.generation = 3;
	withSemantic.scene.semantic = semantic;

	auto presentState = resolver.resolve("scene.state.primary", withSemantic);
	HUD_CHECK(presentState && presentState->present);
	HUD_CHECK_EQ_STR(presentState->textValue, "scene.blob.state.analyzing");

	auto activity = resolver.resolve("scene.activity.overall", withSemantic);
	HUD_CHECK(activity && activity->present);
	HUD_CHECK(std::abs(activity->numberValue - 0.75f) < 1e-6);

	// scene.state.progress has no real backing field — always missing.
	auto stateProgress = resolver.resolve("scene.state.progress", withSemantic);
	HUD_CHECK(stateProgress.has_value());
	HUD_CHECK(stateProgress->present == false);
}

void test_real_metric_lookup() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	SceneSemanticData semantic;
	SceneMetric m;
	m.metricId = "scene.blob.metric.region_count";
	m.valueType = HudMetricValueType::Count;
	m.dataClass = HudDataClass::Literal;
	m.value = 7.0f;
	semantic.metrics.push_back(m);
	frame.scene.semantic = semantic;

	auto found = resolver.resolve("scene.blob.metric.region_count", frame);
	HUD_CHECK(found && found->present);
	HUD_CHECK(std::abs(found->numberValue - 7.0f) < 1e-6);

	auto unknown = resolver.resolve("scene.activity.bogus_signal", frame);
	HUD_CHECK(!unknown.has_value()); // not canonical, not metric-shaped -> genuinely unknown
}

void test_real_media_title_fallback_chain() {
	HudRealFrameResolver resolver;

	// Tier 1: titleId present -> Identifier.
	{
		HudFrameData frame = makeMinimalRealFrame();
		VideoPlaybackStatus video;
		video.titleId = "media.sample_01";
		video.fallbackDisplayTitle = "Some Fallback";
		video.mediaId = "raw_id_01";
		frame.video = video;
		auto title = resolver.resolve("media.title", frame);
		HUD_CHECK(title && title->present);
		HUD_CHECK(title->valueType == HudSourceValueType::Identifier);
		HUD_CHECK_EQ_STR(title->textValue, "media.sample_01");
	}
	// Tier 2: titleId absent, fallbackDisplayTitle present -> Text.
	{
		HudFrameData frame = makeMinimalRealFrame();
		VideoPlaybackStatus video;
		video.fallbackDisplayTitle = "Fungal Bloom Study";
		video.mediaId = "raw_id_02";
		frame.video = video;
		auto title = resolver.resolve("media.title", frame);
		HUD_CHECK(title && title->present);
		HUD_CHECK(title->valueType == HudSourceValueType::Text);
		HUD_CHECK_EQ_STR(title->textValue, "Fungal Bloom Study");
	}
	// Tier 3: only mediaId present -> Text (raw ID as literal last resort).
	{
		HudFrameData frame = makeMinimalRealFrame();
		VideoPlaybackStatus video;
		video.mediaId = "raw_id_03";
		frame.video = video;
		auto title = resolver.resolve("media.title", frame);
		HUD_CHECK(title && title->present);
		HUD_CHECK(title->valueType == HudSourceValueType::Text);
		HUD_CHECK_EQ_STR(title->textValue, "raw_id_03");
	}
	// Tier 4: nothing at all -> missing (Placeholder policy + generic
	// vocabulary handles the rest downstream, per this resolver's own
	// header comment).
	{
		HudFrameData frame = makeMinimalRealFrame();
		VideoPlaybackStatus video; // all identity fields nullopt
		frame.video = video;
		auto title = resolver.resolve("media.title", frame);
		HUD_CHECK(title.has_value());
		HUD_CHECK(title->present == false);
	}
	// frame.video entirely absent -> also missing, distinctly from "empty".
	{
		HudFrameData frame = makeMinimalRealFrame();
		auto title = resolver.resolve("media.title", frame);
		HUD_CHECK(title.has_value());
		HUD_CHECK(title->present == false);
	}
}

// With a vocabulary attached (the production renderer's wiring), an
// unresolvable titleId — e.g. MediaCatalog's synthesized
// "media.title.media.auto.<hash>" — falls through to fallbackDisplayTitle
// instead of surfacing the raw key; a resolvable one still wins.
void test_real_media_title_skips_unresolvable_title_id() {
	HudVocabularyResolver vocabulary;
	HudRealFrameResolver resolver;
	resolver.setVocabulary(&vocabulary);
	{
		HudFrameData frame = makeMinimalRealFrame();
		VideoPlaybackStatus video;
		video.titleId = "media.title.media.auto.1f0329b7";
		video.fallbackDisplayTitle = "ElectricLightning Storm";
		video.mediaId = "media.auto.1f0329b7";
		frame.video = video;
		auto title = resolver.resolve("media.title", frame);
		HUD_CHECK(title && title->present);
		HUD_CHECK(title->valueType == HudSourceValueType::Text);
		HUD_CHECK_EQ_STR(title->textValue, "ElectricLightning Storm");
	}
	{
		HudFrameData frame = makeMinimalRealFrame();
		VideoPlaybackStatus video;
		video.titleId = "media.sample_01"; // registered canonical vocabulary key
		video.fallbackDisplayTitle = "Some Fallback";
		frame.video = video;
		auto title = resolver.resolve("media.title", frame);
		HUD_CHECK(title && title->present);
		HUD_CHECK(title->valueType == HudSourceValueType::Identifier);
		HUD_CHECK_EQ_STR(title->textValue, "media.sample_01");
	}
	{
		HudFrameData frame = makeMinimalRealFrame();
		VideoPlaybackStatus video;
		video.titleId = "media.title.media.auto.deadbeef";
		video.mediaId = "media.auto.deadbeef"; // no fallback title -> stable media ID
		frame.video = video;
		auto title = resolver.resolve("media.title", frame);
		HUD_CHECK(title && title->present);
		HUD_CHECK_EQ_STR(title->textValue, "media.auto.deadbeef");
	}
}

void test_real_media_selection_manual_derivation() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	VideoPlaybackStatus video;
	video.selectionOrigin = MediaSelectionOrigin::ManualNext;
	frame.video = video;
	auto manual = resolver.resolve("media.selection.manual", frame);
	HUD_CHECK(manual && manual->present && manual->boolValue == true);

	frame.video->selectionOrigin = MediaSelectionOrigin::Automatic;
	auto automatic = resolver.resolve("media.selection.manual", frame);
	HUD_CHECK(automatic && automatic->present && automatic->boolValue == false);
}

void test_real_control_availability_derivation() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();

	// No video, no capabilities -> UNSUPPORTED, which resolves as missing
	// (present=false), not present-false — see HudRealFrameResolver.cpp's
	// three-state control comment (unsupported/disabled/enabled).
	auto reseedUnsupported = resolver.resolve("control.scene.reseed.available", frame);
	HUD_CHECK(reseedUnsupported.has_value());
	HUD_CHECK(reseedUnsupported->present == false);
	auto mediaPrevUnsupported = resolver.resolve("control.media.previous.available", frame);
	HUD_CHECK(mediaPrevUnsupported.has_value());
	HUD_CHECK(mediaPrevUnsupported->present == false);

	// Capability advertised -> reseed available.
	SceneCommandDescriptor regen;
	regen.command = SceneCommand::Regenerate;
	regen.label = "Reseed";
	frame.capabilities.commands.push_back(regen);
	auto reseedSupported = resolver.resolve("control.scene.reseed.available", frame);
	HUD_CHECK(reseedSupported && reseedSupported->present && reseedSupported->boolValue == true);

	// Media canSelectNext=true -> media.next ENABLED (present, true).
	// Video now configured but canSelectPrevious=false -> media.previous
	// DISABLED (present, false) — a real, situational signal, distinct
	// from "no video service at all" (unsupported, checked above).
	VideoPlaybackStatus video;
	video.canSelectNext = true;
	video.canSelectPrevious = false;
	frame.video = video;
	auto mediaNext = resolver.resolve("control.media.next.available", frame);
	HUD_CHECK(mediaNext && mediaNext->present && mediaNext->boolValue == true);
	auto mediaPrev = resolver.resolve("control.media.previous.available", frame);
	HUD_CHECK(mediaPrev && mediaPrev->present && mediaPrev->boolValue == false);

	// Scene-switch controls: no real signal exists yet (single resident
	// scene) -> genuinely UNSUPPORTED (missing), not just disabled.
	auto scenePrev = resolver.resolve("control.scene.previous.available", frame);
	HUD_CHECK(scenePrev.has_value());
	HUD_CHECK(scenePrev->present == false);
}

// Architecture-Closure Session (DEC-015/DEC-016) — replaces the old
// Engineering-Session-2-era test_real_effects_from_active_effects(), which
// asserted the PRE-closure behavior (effects.active always backed by
// SceneHudStatus::activeEffects, every other effects.* slot permanently
// missing). That behavior is superseded now that a real
// HudFrameData.effects sibling snapshot exists — see
// HudRealFrameResolver.cpp's resolveEffects() for the implementation this
// suite verifies.
using videoeffects::EffectActivitySlot;
using videoeffects::EffectActivityStatus;
using videoeffects::EffectHealth;
using videoeffects::EvolutionPhase;

void test_real_effects_absent_snapshot() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	frame.effects = std::nullopt; // explicit: "no authoritative snapshot this frame"

	// Every effects.* slot resolves missing — NOT "zero effects" (DEC-015:
	// "Missing effect snapshot is not the same as present-empty").
	// Compatibility fallback defaults to off, so effects.active is no
	// exception here even though scene.activeEffects below is populated.
	frame.scene.activeEffects = {"Edge Glow", "Bioluminescence"};
	for (const char* slot : {"effects.active", "effects.dominant", "effects.transition.progress",
			"effects.intensity", "effects.health"}) {
		auto v = resolver.resolve(slot, frame);
		HUD_CHECK(v.has_value());
		HUD_CHECK(v && v->present == false);
	}
}

void test_real_effects_compatibility_fallback_opt_in() {
	HudRealFrameResolver resolver;
	resolver.setActiveEffectsCompatibilityFallbackEnabled(true);
	HUD_CHECK(resolver.activeEffectsCompatibilityFallbackEnabled());

	HudFrameData frame = makeMinimalRealFrame();
	frame.effects = std::nullopt;
	frame.scene.activeEffects = {"Edge Glow", "Bioluminescence"};

	// Only effects.active honors the opt-in flag — every other slot has
	// no compatibility source and stays missing even with the flag on.
	auto active = resolver.resolve("effects.active", frame);
	HUD_CHECK(active && active->present && active->listValue.count == 2);
	HUD_CHECK_EQ_STR(active->listValue.items[0], "Edge Glow");

	auto dominant = resolver.resolve("effects.dominant", frame);
	HUD_CHECK(dominant.has_value());
	HUD_CHECK(dominant->present == false);
}

void test_real_effects_present_empty() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	EffectActivityStatus status;
	status.health = EffectHealth::Ready;
	status.slots = {}; // present-empty: a fully valid, common state
	frame.effects = status;

	auto active = resolver.resolve("effects.active", frame);
	HUD_CHECK(active && active->present && active->listValue.empty());

	auto dominant = resolver.resolve("effects.dominant", frame);
	HUD_CHECK(dominant && dominant->present == false); // no slots -> no dominant, but NOT a failure

	auto health = resolver.resolve("effects.health", frame);
	HUD_CHECK(health && health->present);
	HUD_CHECK_EQ_STR(health->textValue, "ready");

	auto transition = resolver.resolve("effects.transition.progress", frame);
	HUD_CHECK(transition && transition->present == false);
}

void test_real_effects_present_active_one_slot() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	EffectActivityStatus status;
	status.health = EffectHealth::Ready;
	EffectActivitySlot slot;
	slot.slotId = "primary";
	slot.effectId = "heatmap_recolor";
	slot.displayName = "Heatmap Recolor";
	slot.phase = EvolutionPhase::Holding;
	slot.prominence = 1.0f;
	status.slots = {slot};
	frame.effects = status;

	auto active = resolver.resolve("effects.active", frame);
	HUD_CHECK(active && active->present && active->listValue.count == 1);
	HUD_CHECK_EQ_STR(active->listValue.items[0], "heatmap_recolor");

	auto dominant = resolver.resolve("effects.dominant", frame);
	HUD_CHECK(dominant && dominant->present);
	HUD_CHECK_EQ_STR(dominant->textValue, "heatmap_recolor");
}

// Proves dominance uses the canonical resolver's prominence-ranking, never
// vector/insertion order — this test's whole point is that slots[0] is
// NOT the dominant result. Final Narrow Closure Patch: effects.active and
// effects.dominant are now independent computations (Task 1/Task 2) —
// effects.active is the complete active set in first-seen SLOT order (no
// dominance ranking applied to it at all), effects.dominant alone is
// prominence-ranked. This test proves BOTH: effects.dominant is NOT
// slots[0], and effects.active's own order is NOT dominance-derived
// either (it stays in insertion order, unaffected by prominence).
void test_real_effects_dominance_not_first_in_vector() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	EffectActivityStatus status;
	status.health = EffectHealth::Ready;

	EffectActivitySlot low;
	low.slotId = "quadrant_0";
	low.effectId = "desaturate";
	low.prominence = 0.2f;

	EffectActivitySlot high;
	high.slotId = "quadrant_1";
	high.effectId = "bioluminescence";
	high.prominence = 0.9f;

	status.slots = {low, high}; // low-prominence slot inserted FIRST
	frame.effects = status;

	auto dominant = resolver.resolve("effects.dominant", frame);
	HUD_CHECK(dominant && dominant->present);
	HUD_CHECK_EQ_STR(dominant->textValue, "bioluminescence"); // the higher-prominence slot, not slots[0]

	auto active = resolver.resolve("effects.active", frame);
	HUD_CHECK(active && active->present && active->listValue.count == 2);
	// effects.active is NOT dominance-ordered — it reflects the complete
	// active set in the slot order it was given, independent of prominence.
	HUD_CHECK_EQ_STR(active->listValue.items[0], "desaturate");
	HUD_CHECK_EQ_STR(active->listValue.items[1], "bioluminescence");
}

void test_real_effects_transition_progress() {
	HudRealFrameResolver resolver;

	// Steady-state dominant slot -> no meaningful "in progress" value.
	{
		HudFrameData frame = makeMinimalRealFrame();
		EffectActivityStatus status;
		EffectActivitySlot slot;
		slot.effectId = "contour_glow";
		slot.phase = EvolutionPhase::Holding;
		slot.transitionProgress01 = 1.0f;
		status.slots = {slot};
		frame.effects = status;

		auto progress = resolver.resolve("effects.transition.progress", frame);
		HUD_CHECK(progress && progress->present == false);
	}

	// Actively transitioning dominant slot -> real, bounded mid-progress
	// value preserved from the frozen snapshot.
	{
		HudFrameData frame = makeMinimalRealFrame();
		EffectActivityStatus status;
		EffectActivitySlot slot;
		slot.effectId = "contour_glow";
		slot.phase = EvolutionPhase::Transitioning;
		slot.transitionProgress01 = 0.42f;
		status.slots = {slot};
		frame.effects = status;

		auto progress = resolver.resolve("effects.transition.progress", frame);
		HUD_CHECK(progress && progress->present);
		HUD_CHECK(progress->numberValue > 0.41f && progress->numberValue < 0.43f);
	}
}

void test_real_effects_health_degraded_empty() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	EffectActivityStatus status;
	status.health = EffectHealth::Degraded;
	status.slots = {}; // empty AND degraded — the two fields are independent
	frame.effects = status;

	auto health = resolver.resolve("effects.health", frame);
	HUD_CHECK(health && health->present);
	HUD_CHECK_EQ_STR(health->textValue, "degraded");

	auto active = resolver.resolve("effects.active", frame);
	HUD_CHECK(active && active->present && active->listValue.empty());
}

void test_real_effects_health_degraded_active() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	EffectActivityStatus status;
	status.health = EffectHealth::Degraded;
	EffectActivitySlot slot;
	slot.effectId = "channel_shift";
	slot.prominence = 1.0f;
	status.slots = {slot};
	frame.effects = status;

	auto health = resolver.resolve("effects.health", frame);
	HUD_CHECK_EQ_STR(health->textValue, "degraded");
	auto active = resolver.resolve("effects.active", frame);
	HUD_CHECK(active && active->present && active->listValue.count == 1);
}

void test_real_effects_health_failed() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	EffectActivityStatus status;
	status.health = EffectHealth::Failed;
	status.slots = {};
	frame.effects = status;

	auto health = resolver.resolve("effects.health", frame);
	HUD_CHECK(health && health->present);
	HUD_CHECK_EQ_STR(health->textValue, "failed");
}

// DEC-016, verbatim: "prominence is a dominance-ranking value and is not
// effects.intensity; effects.intensity may remain absent in v1." — proves
// this holds even for a maximally-prominent single-slot snapshot, so a
// future reader can't mistake "it happened to stay absent in the empty
// case" for the actual rule.
void test_real_effects_intensity_always_absent() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	EffectActivityStatus status;
	EffectActivitySlot slot;
	slot.effectId = "bioluminescence";
	slot.prominence = 1.0f; // maximal prominence — must NOT leak into effects.intensity
	status.slots = {slot};
	frame.effects = status;

	auto intensity = resolver.resolve("effects.intensity", frame);
	HUD_CHECK(intensity.has_value());
	HUD_CHECK(intensity && intensity->present == false);
}

// Final Narrow Closure Patch, Task 3 — the mandatory >2-effect regression:
// 4 active effects, one below the canonical dominance threshold
// (minProminenceToShow=0.05, DominanceConfig's default), dominant effect
// NOT at vector index 0. Proves effects.active retains ALL FOUR
// (including the below-threshold one) while effects.dominant matches
// ONLY the canonical dominance result — the two are independent
// computations, exactly per Task 1/Task 2.
void test_real_effects_active_full_set_with_below_threshold_and_dominance_not_first() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	EffectActivityStatus status;
	status.health = EffectHealth::Ready;

	EffectActivitySlot a; // index 0 — not dominant
	a.slotId = "quadrant_0";
	a.effectId = "heatmap_recolor";
	a.prominence = 0.3f;

	EffectActivitySlot b; // index 1 — BELOW dominance threshold (0.02 < 0.05)
	b.slotId = "quadrant_1";
	b.effectId = "desaturate";
	b.prominence = 0.02f;

	EffectActivitySlot c; // index 2
	c.slotId = "quadrant_2";
	c.effectId = "bioluminescence";
	c.prominence = 0.5f;

	EffectActivitySlot d; // index 3 — the DOMINANT slot, deliberately last
	d.slotId = "quadrant_3";
	d.effectId = "channel_shift";
	d.prominence = 0.9f;

	status.slots = {a, b, c, d};
	frame.effects = status;

	auto active = resolver.resolve("effects.active", frame);
	HUD_CHECK(active && active->present);
	HUD_CHECK(active->listValue.count == 4); // ALL FOUR survive, including the below-threshold one
	HUD_CHECK_EQ_STR(active->listValue.items[0], "heatmap_recolor");
	HUD_CHECK_EQ_STR(active->listValue.items[1], "desaturate"); // below-threshold, still present
	HUD_CHECK_EQ_STR(active->listValue.items[2], "bioluminescence");
	HUD_CHECK_EQ_STR(active->listValue.items[3], "channel_shift");

	auto dominant = resolver.resolve("effects.dominant", frame);
	HUD_CHECK(dominant && dominant->present);
	HUD_CHECK_EQ_STR(dominant->textValue, "channel_shift"); // canonical dominance result ONLY — not index 0, not the below-threshold slot
}

void test_real_manager_transition_fields() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	frame.sceneManager.transitionPhase = SceneTransitionPhase::FadingOut;
	frame.sceneManager.transitionProgress = 0.42f;
	frame.sceneManager.message = "Switching to temporal-fields";

	auto phase = resolver.resolve("manager.transition.phase", frame);
	HUD_CHECK(phase && phase->present);
	HUD_CHECK_EQ_STR(phase->textValue, "fading_out");

	auto progress = resolver.resolve("manager.transition.progress", frame);
	HUD_CHECK(progress && progress->present);
	HUD_CHECK(std::abs(progress->numberValue - 0.42f) < 1e-6);

	auto message = resolver.resolve("manager.message", frame);
	HUD_CHECK(message && message->present);
	HUD_CHECK_EQ_STR(message->textValue, "Switching to temporal-fields");
}

void test_real_exhaustive_canonical_coverage() {
	HudRealFrameResolver resolver;
	HudFrameData blank;
	for (const auto& id : HudSourceResolver::allCanonicalSourceIds()) {
		auto v = resolver.resolve(id, blank);
		HUD_CHECK(v.has_value());
	}
}

} // namespace

// ---------------------------------------------------------------------
// HUD Clock v0.1 — runtime.clock.* against the real HudFrameData::wallClock.
// ---------------------------------------------------------------------
namespace {

WallClockStatus clockAt(int64_t unixSeconds, int32_t offset, const std::string& abbreviation, std::optional<bool> isDst) {
	WallClockStatus c;
	c.unixSeconds = unixSeconds;
	c.utcOffsetSeconds = offset;
	c.timezoneAbbreviation = abbreviation;
	c.isDst = isDst;
	return c;
}

} // namespace

void test_real_clock_missing_and_invalid_snapshot() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();

	// Absent snapshot: both sources known, typed, and missing.
	auto secs = resolver.resolve("runtime.clock.local_seconds", frame);
	auto label = resolver.resolve("runtime.clock.local_label", frame);
	HUD_CHECK(secs && !secs->present && secs->valueType == HudSourceValueType::DurationSeconds);
	HUD_CHECK(label && !label->present && label->valueType == HudSourceValueType::Text);

	// Snapshots that cannot be a real local time also resolve missing.
	frame.wallClock = clockAt(0, 90000, "BAD", std::nullopt); // offset >= one day
	HUD_CHECK(!resolver.resolve("runtime.clock.local_seconds", frame)->present);
	HUD_CHECK(!resolver.resolve("runtime.clock.local_label", frame)->present);
	frame.wallClock = clockAt(hudclock::kMaxUnixSeconds + 1, 0, "UTC", false); // year > 9999
	HUD_CHECK(!resolver.resolve("runtime.clock.local_seconds", frame)->present);
	HUD_CHECK(!resolver.resolve("runtime.clock.local_label", frame)->present);
	HUD_CHECK(resolver.clockTextCacheForTesting().labelBuilds() == 0); // nothing formatted from bad data
}

void test_real_clock_values_and_dst() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();

	// 2026-10-15T12:34:56Z in CEST.
	frame.wallClock = clockAt(1792067696, 7200, "CEST", true);
	auto secs = resolver.resolve("runtime.clock.local_seconds", frame);
	auto label = resolver.resolve("runtime.clock.local_label", frame);
	HUD_CHECK(secs && secs->present && secs->numberValue == 14.0f * 3600 + 34 * 60 + 56); // time of day, not epoch
	HUD_CHECK(label && label->present);
	HUD_CHECK_EQ_STR(label->textValue, "THU 15 OCT 2026 CEST");

	// New York fall back, exactly as the OS reports it on each side.
	frame.wallClock = clockAt(1793512799, -14400, "EDT", true); // 01:59:59 EDT
	HUD_CHECK(resolver.resolve("runtime.clock.local_seconds", frame)->numberValue == 1 * 3600 + 59 * 60 + 59);
	HUD_CHECK_EQ_STR(resolver.resolve("runtime.clock.local_label", frame)->textValue, "SUN 01 NOV 2026 EDT");
	frame.wallClock = clockAt(1793512800, -18000, "EST", false); // 01:00:00 EST
	HUD_CHECK(resolver.resolve("runtime.clock.local_seconds", frame)->numberValue == 1 * 3600);
	HUD_CHECK_EQ_STR(resolver.resolve("runtime.clock.local_label", frame)->textValue, "SUN 01 NOV 2026 EST");

	// Spring forward: 01:59:59 EST -> 03:00:00 EDT.
	frame.wallClock = clockAt(1772953200, -14400, "EDT", true);
	HUD_CHECK(resolver.resolve("runtime.clock.local_seconds", frame)->numberValue == 3 * 3600);
	HUD_CHECK_EQ_STR(resolver.resolve("runtime.clock.local_label", frame)->textValue, "SUN 08 MAR 2026 EDT");

	// Zone without a usable abbreviation: numeric offset, never empty.
	frame.wallClock = clockAt(1791547200, 20700, "", std::nullopt);
	HUD_CHECK_EQ_STR(resolver.resolve("runtime.clock.local_label", frame)->textValue, "FRI 09 OCT 2026 +0545");
}

// Same snapshot -> identical clock values regardless of which scene owns the
// frame, its health, or whether it publishes semantic data at all.
void test_real_clock_scene_independent() {
	const WallClockStatus clock = clockAt(1792067696, 19800, "IST", false);
	HudRealFrameResolver resolver;

	struct SceneCase { const char* id; SceneHealth health; };
	const SceneCase cases[] = {
		{"blob-region-prototype", SceneHealth::Ready},
		{"temporal-fields", SceneHealth::Degraded},
		{"blueprint_emergence", SceneHealth::Loading},
		{"", SceneHealth::Failed},
	};
	for (const auto& sc : cases) {
		HudFrameData frame = makeMinimalRealFrame();
		frame.scene.sceneId = sc.id;
		frame.sceneManager.activeSceneId = sc.id;
		frame.scene.health = sc.health;
		frame.scene.semantic.reset();
		frame.wallClock = clock;
		auto secs = resolver.resolve("runtime.clock.local_seconds", frame);
		auto label = resolver.resolve("runtime.clock.local_label", frame);
		HUD_CHECK(secs && secs->present && secs->numberValue == 18.0f * 3600 + 4 * 60 + 56);
		HUD_CHECK(label && label->present);
		HUD_CHECK_EQ_STR(label->textValue, "THU 15 OCT 2026 IST");
	}
	HUD_CHECK(resolver.clockTextCacheForTesting().labelBuilds() == 1); // one build served all four scenes
}

void test_real_clock_label_cache_through_resolver() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();
	const int64_t t = 1792065600; // 2026-10-15T12:00:00Z

	// 120 frames at 60 fps inside one second, then later seconds of the same day.
	frame.wallClock = clockAt(t, 7200, "CEST", true);
	for (int i = 0; i < 120; ++i) resolver.resolve("runtime.clock.local_label", frame);
	frame.wallClock = clockAt(t + 3599, 7200, "CEST", true);
	resolver.resolve("runtime.clock.local_label", frame);
	HUD_CHECK(resolver.clockTextCacheForTesting().labelBuilds() == 1);

	// OS timezone reconfigured (Berlin -> Kolkata) at the same instant.
	frame.wallClock = clockAt(t, 19800, "IST", false);
	HUD_CHECK_EQ_STR(resolver.resolve("runtime.clock.local_label", frame)->textValue, "THU 15 OCT 2026 IST");
	HUD_CHECK(resolver.clockTextCacheForTesting().labelBuilds() == 2);

	// Wall clock corrected back a day.
	frame.wallClock = clockAt(t - 86400, 19800, "IST", false);
	HUD_CHECK_EQ_STR(resolver.resolve("runtime.clock.local_label", frame)->textValue, "WED 14 OCT 2026 IST");
	HUD_CHECK(resolver.clockTextCacheForTesting().labelBuilds() == 3);

	// Snapshot lost, then restored: missing in between, correct afterwards.
	frame.wallClock.reset();
	HUD_CHECK(!resolver.resolve("runtime.clock.local_label", frame)->present);
	frame.wallClock = clockAt(t - 86400, 19800, "IST", false);
	HUD_CHECK_EQ_STR(resolver.resolve("runtime.clock.local_label", frame)->textValue, "WED 14 OCT 2026 IST");
}

// ---------------------------------------------------------------------
// Scene & Pattern Navigation — scene position, mode position, NEXT MODE
// availability, all derived from SceneManagerStatus / SceneModeStatus.
// ---------------------------------------------------------------------
namespace {

SceneModeStatus modeOf(const std::string& id, const std::string& name, int index, int count) {
	SceneModeStatus m;
	m.modeId = id;
	m.displayName = name;
	m.index = index;
	m.count = count;
	return m;
}

void advertise(HudFrameData& f, SceneCommand c) {
	SceneCommandDescriptor d;
	d.command = c;
	d.label = "x";
	f.capabilities.commands.push_back(d);
}

} // namespace

void test_real_scene_position() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();

	// No registry information (FakeScene path / before activation): missing.
	HUD_CHECK(!resolver.resolve("scene.index", frame)->present);
	HUD_CHECK(!resolver.resolve("scene.count", frame)->present);
	HUD_CHECK(!resolver.resolve("scene.position", frame)->present);

	frame.sceneManager.activeSceneIndex = 1;
	frame.sceneManager.availableSceneCount = 5;
	HUD_CHECK(resolver.resolve("scene.index", frame)->numberValue == 2.0f); // one-based
	HUD_CHECK(resolver.resolve("scene.count", frame)->numberValue == 5.0f);
	HUD_CHECK_EQ_STR(resolver.resolve("scene.position", frame)->textValue, "[Scene 2/5]");

	// Reduced harness rosters report their own real size.
	frame.sceneManager.activeSceneIndex = 2;
	frame.sceneManager.availableSceneCount = 3;
	HUD_CHECK_EQ_STR(resolver.resolve("scene.position", frame)->textValue, "[Scene 3/3]");

	// An index outside the registry is never displayed.
	frame.sceneManager.activeSceneIndex = 5;
	frame.sceneManager.availableSceneCount = 5;
	HUD_CHECK(!resolver.resolve("scene.position", frame)->present);
	frame.sceneManager.activeSceneIndex = -1;
	HUD_CHECK(!resolver.resolve("scene.index", frame)->present);
}

void test_real_mode_sources() {
	HudRealFrameResolver resolver;
	HudVocabularyResolver vocabulary;
	resolver.setVocabulary(&vocabulary);
	HudFrameData frame = makeMinimalRealFrame();
	frame.scene.sceneId = "temporal-fields";

	// No mode model: every mode source missing, NEXT MODE unsupported (hidden).
	HUD_CHECK(!resolver.resolve("scene.mode.name", frame)->present);
	HUD_CHECK(!resolver.resolve("scene.mode.position", frame)->present);
	HUD_CHECK(!resolver.resolve("control.scene.next_mode.available", frame)->present);

	// Curated vocabulary text wins for a known stable ID.
	frame.scene.mode = modeOf("particle_field", "PARTICLE_FIELD", 5, 9);
	HUD_CHECK_EQ_STR(resolver.resolve("scene.mode.id", frame)->textValue, "particle_field");
	HUD_CHECK_EQ_STR(resolver.resolve("scene.mode.name", frame)->textValue, "PARTICLE FIELD");
	HUD_CHECK(resolver.resolve("scene.mode.index", frame)->numberValue == 6.0f);
	HUD_CHECK(resolver.resolve("scene.mode.count", frame)->numberValue == 9.0f);
	HUD_CHECK_EQ_STR(resolver.resolve("scene.mode.position", frame)->textValue, "6/9");

	// Unknown to the vocabulary: the scene's own display name is used.
	frame.scene.sceneId = "quadrant-crosshair";
	frame.scene.mode = modeOf("motion_nervous", "NERVOUS", 3, 5);
	HUD_CHECK_EQ_STR(resolver.resolve("scene.mode.name", frame)->textValue, "NERVOUS");
	HUD_CHECK_EQ_STR(resolver.resolve("scene.mode.position", frame)->textValue, "4/5");

	// Invalid mode status is never displayed.
	frame.scene.mode = modeOf("bad", "BAD", 3, 3);   // index == count
	HUD_CHECK(!resolver.resolve("scene.mode.name", frame)->present);
	frame.scene.mode = modeOf("", "NO ID", 0, 2);    // empty ID
	HUD_CHECK(!resolver.resolve("scene.mode.position", frame)->present);
	frame.scene.mode = modeOf("zero", "ZERO", 0, 0); // empty mode list
	HUD_CHECK(!resolver.resolve("control.scene.next_mode.available", frame)->present);
}

void test_real_next_mode_availability() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();

	// Modes, NextMode advertised, somewhere to go -> ENABLED.
	frame.scene.mode = modeOf("bsp", "BSP", 0, 9);
	advertise(frame, SceneCommand::NextMode);
	auto enabled = resolver.resolve("control.scene.next_mode.available", frame);
	HUD_CHECK(enabled && enabled->present && enabled->boolValue);

	// Modes shown but NextMode not advertised (Blueprint) -> DISABLED, visible.
	frame.capabilities.commands.clear();
	advertise(frame, SceneCommand::Regenerate); // Regenerate is never NEXT MODE
	frame.scene.mode = modeOf("ghost_layers", "ghost_layers", 0, 2);
	auto disabled = resolver.resolve("control.scene.next_mode.available", frame);
	HUD_CHECK(disabled && disabled->present && !disabled->boolValue);

	// Advertised but a single mode -> DISABLED (nowhere to step to).
	advertise(frame, SceneCommand::NextMode);
	frame.scene.mode = modeOf("only", "ONLY", 0, 1);
	auto single = resolver.resolve("control.scene.next_mode.available", frame);
	HUD_CHECK(single && single->present && !single->boolValue);
}

// HUD & Scene Experience v0.3 — scene controls, IMMERSIVE, playback time.
void test_real_v03_controls_and_playback_time() {
	HudRealFrameResolver resolver;
	HudFrameData frame = makeMinimalRealFrame();

	// One scene (or FakeScene path): scene controls unsupported -> hidden.
	frame.sceneManager.availableSceneCount = 1;
	HUD_CHECK(!resolver.resolve("control.scene.next.available", frame)->present);
	// Five scenes, Idle: enabled; mid-transition: disabled (still visible).
	frame.sceneManager.availableSceneCount = 5;
	auto next = resolver.resolve("control.scene.next.available", frame);
	auto prev = resolver.resolve("control.scene.previous.available", frame);
	HUD_CHECK(next->present && next->boolValue && prev->present && prev->boolValue);
	frame.sceneManager.transitionPhase = SceneTransitionPhase::Loading;
	auto loading = resolver.resolve("control.scene.next.available", frame);
	HUD_CHECK(loading->present && !loading->boolValue);
	HUD_CHECK(!resolver.resolve("control.runtime.immersive.available", frame)->boolValue); // not during transitions
	frame.sceneManager.transitionPhase = SceneTransitionPhase::Idle;
	HUD_CHECK(resolver.resolve("control.runtime.immersive.available", frame)->boolValue);
	frame.sceneManager.activeSceneId.clear();
	HUD_CHECK(!resolver.resolve("control.runtime.immersive.available", frame)->boolValue);

	// Playback time from canonical Shared Video position/duration.
	HUD_CHECK(!resolver.resolve("media.playback.time", frame)->present); // no video status
	VideoPlaybackStatus v;
	frame.video = v;
	HUD_CHECK(!resolver.resolve("media.playback.time", frame)->present); // nothing loaded
	frame.video->positionSeconds = 42.4f;
	HUD_CHECK_EQ_STR(resolver.resolve("media.playback.time", frame)->textValue, "0:42 / --:--"); // unknown duration
	frame.video->durationSeconds = 90.0f;
	HUD_CHECK_EQ_STR(resolver.resolve("media.playback.time", frame)->textValue, "0:42 / 1:30");
	frame.video->positionSeconds = 3725.0f;
	frame.video->durationSeconds = 7200.0f;
	HUD_CHECK_EQ_STR(resolver.resolve("media.playback.time", frame)->textValue, "1:02:05 / 2:00:00");
	frame.video->durationSeconds = 0.0f;
	HUD_CHECK_EQ_STR(resolver.resolve("media.playback.time", frame)->textValue, "1:02:05 / --:--"); // zero duration is unknown
}

int main() {
	test_real_v03_controls_and_playback_time();
	test_real_scene_position();
	test_real_mode_sources();
	test_real_next_mode_availability();
	test_real_clock_missing_and_invalid_snapshot();
	test_real_clock_values_and_dst();
	test_real_clock_scene_independent();
	test_real_clock_label_cache_through_resolver();
	test_real_identity_and_health();
	test_real_semantic_present_vs_absent();
	test_real_metric_lookup();
	test_real_media_title_fallback_chain();
	test_real_media_title_skips_unresolvable_title_id();
	test_real_media_selection_manual_derivation();
	test_real_control_availability_derivation();
	test_real_effects_absent_snapshot();
	test_real_effects_compatibility_fallback_opt_in();
	test_real_effects_present_empty();
	test_real_effects_present_active_one_slot();
	test_real_effects_dominance_not_first_in_vector();
	test_real_effects_active_full_set_with_below_threshold_and_dominance_not_first();
	test_real_effects_transition_progress();
	test_real_effects_health_degraded_empty();
	test_real_effects_health_degraded_active();
	test_real_effects_health_failed();
	test_real_effects_intensity_always_absent();
	test_real_manager_transition_fields();
	test_real_exhaustive_canonical_coverage();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed.\n";
	if (g_failures > 0) {
		std::cerr << g_failures << " FAILURE(S)\n";
		return 1;
	}
	return 0;
}
