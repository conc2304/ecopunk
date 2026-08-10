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

int main() {
	test_real_identity_and_health();
	test_real_semantic_present_vs_absent();
	test_real_metric_lookup();
	test_real_media_title_fallback_chain();
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
