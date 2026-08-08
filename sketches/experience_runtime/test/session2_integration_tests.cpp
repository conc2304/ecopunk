// Standalone, dependency-free tests for Engineering Session 2's real
// shared-type integration surface: shared/src/video-playback/
// VideoPlaybackStatus.h and shared/src/video-effects/knowledge/
// EffectActivityStatus.h. Both headers are documented OF-free by their own
// domains (see each header's comment) — this file proves that directly by
// compiling with nothing but a bare compiler, same convention as
// lifecycle_state_tests.cpp in this directory.
//
// Scope: this tests THIS increment's integration decisions (how FakeScene/
// ExperienceRuntime use these real types), not the video-playback or
// video-effects domains' own internal logic — their own test suites
// (shared/src/video-playback/test/, shared/src/video-effects/test/) own
// that; duplicating it here would be exactly the kind of second source of
// truth this whole engagement has been avoiding.
//
// Build/run: make -C test -f Makefile.tests session2

#include <iostream>
#include <string>

#include "../../../shared/src/video-playback/VideoPlaybackStatus.h"
#include "../../../shared/src/video-effects/knowledge/EffectActivityStatus.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define S2_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

// -- VideoPlaybackStatus: "missing video snapshot is valid" is tested at
//    the RuntimeServices level (OF-dependent, see GlRestorationHarness);
//    what's bare-compiler-testable here is the REAL type's own default
//    state and the playback-vs-hold-progress distinction this increment's
//    report relies on. -------------------------------------------------
static void test_video_playback_status_defaults() {
	VideoPlaybackStatus status;
	S2_CHECK(status.health == VideoPlaybackHealth::Unavailable);
	S2_CHECK(!status.mediaId.has_value());
	// Playback progress and hold progress are separate optionals, never
	// coerced to a synthetic 0.0f for unavailable/unknown media — both
	// absent by default, independently.
	S2_CHECK(!status.playbackProgress.has_value());
	S2_CHECK(!status.holdProgress.has_value());
	S2_CHECK(!status.canSelectPrevious);
	S2_CHECK(!status.canSelectNext);
}

static void test_video_playback_status_ready_implies_media_id() {
	// Documented rule (VideoPlaybackStatus.h): "health == Ready must imply
	// mediaId names the successfully active item." Not enforceable by the
	// struct itself (a plain struct can't enforce cross-field invariants —
	// same header's own comment) — this test only proves a caller CAN
	// construct a Ready status that upholds the rule, as a usage example,
	// not a runtime-enforced guarantee.
	VideoPlaybackStatus status;
	status.health = VideoPlaybackHealth::Ready;
	status.mediaId = "clip_01";
	status.playbackProgress = 0.42f;
	S2_CHECK(status.health == VideoPlaybackHealth::Ready);
	S2_CHECK(status.mediaId.has_value());
	S2_CHECK(*status.mediaId == "clip_01");
}

// -- EffectActivityStatus / resolveDominantEffectLabels(): the exact logic
//    FakeScene::buildActiveEffectsLabels() exercises, tested directly
//    against the real function. -----------------------------------------
static void test_effect_activity_status_empty_is_valid() {
	videoeffects::EffectActivityStatus status; // zero slots — a real, valid state
	auto labels = videoeffects::resolveDominantEffectLabels(status);
	S2_CHECK(labels.empty());
}

static void test_effect_activity_status_dominance_matches_fakescene_usage() {
	// Mirrors FakeScene::buildActiveEffectsLabels()'s exact two-slot
	// construction (see FakeScene.cpp) — proves the dominance-resolution
	// rule (higher prominence first) this increment's harness observed
	// live ("Heatmap Recolor", "Channel Shift") is not a fluke of that one
	// run.
	videoeffects::EffectActivityStatus status;

	videoeffects::EffectActivitySlot primary;
	primary.slotId = "primary";
	primary.effectId = "heatmap_recolor";
	primary.displayName = "Heatmap Recolor";
	primary.phase = videoeffects::EvolutionPhase::Holding;
	primary.transitionProgress01 = 1.0f;
	primary.prominence = 0.8f;
	status.slots.push_back(primary);

	videoeffects::EffectActivitySlot secondary;
	secondary.slotId = "secondary";
	secondary.effectId = "channelshift";
	secondary.displayName = "Channel Shift";
	secondary.phase = videoeffects::EvolutionPhase::Holding;
	secondary.transitionProgress01 = 1.0f;
	secondary.prominence = 0.3f;
	status.slots.push_back(secondary);

	auto labels = videoeffects::resolveDominantEffectLabels(status);
	S2_CHECK(labels.size() == 2);
	if (labels.size() == 2) {
		S2_CHECK(labels[0] == "Heatmap Recolor"); // higher prominence first
		S2_CHECK(labels[1] == "Channel Shift");
	}
}

static void test_effect_activity_status_transitioning_annotation() {
	videoeffects::EffectActivityStatus status;
	videoeffects::EffectActivitySlot slot;
	slot.slotId = "primary";
	slot.effectId = "heatmap_recolor";
	slot.displayName = "Heatmap Recolor";
	slot.phase = videoeffects::EvolutionPhase::Transitioning;
	slot.transitionProgress01 = 0.5f;
	slot.prominence = 0.8f;
	status.slots.push_back(slot);

	auto labels = videoeffects::resolveDominantEffectLabels(status);
	S2_CHECK(labels.size() == 1);
	if (!labels.empty()) {
		// DominanceConfig::annotateTransitioning defaults to true with
		// suffix " (shifting)" — matches FakeScene's own odd-resetEpoch
		// transitioning case.
		S2_CHECK(labels[0] == "Heatmap Recolor (shifting)");
	}
}

// -- Architecture-Closure Session: EffectHealth / deriveEffectHealth() /
//    resolveDominantEffectIds() — the exact functions FakeScene::
//    currentEffectActivityStatus() (canonical HudFrameData.effects path)
//    exercises, tested directly against the real header. ----------------

static void test_effect_health_ready_with_empty_load_report() {
	videoeffects::VideoEffectLoadReport report; // honestly empty — 0 requested
	S2_CHECK(report.allOk());
	videoeffects::EffectHealth health = videoeffects::deriveEffectHealth(report);
	S2_CHECK(health == videoeffects::EffectHealth::Ready);
}

static void test_effect_health_failed_on_missing_or_failed_effects() {
	videoeffects::VideoEffectLoadReport report;
	report.requested.push_back("nonexistent_effect");
	report.missing.push_back("nonexistent_effect");
	S2_CHECK(!report.allOk());
	S2_CHECK(videoeffects::deriveEffectHealth(report) == videoeffects::EffectHealth::Failed);
}

static void test_effect_health_degraded_on_fallback_or_rejected_pack() {
	videoeffects::VideoEffectLoadReport fallbackReport;
	fallbackReport.requested.push_back("some_effect");
	fallbackReport.fallback.push_back("some_effect");
	S2_CHECK(fallbackReport.allOk()); // fallback doesn't fail allOk() — only missing/failed do
	S2_CHECK(videoeffects::deriveEffectHealth(fallbackReport) == videoeffects::EffectHealth::Degraded);

	videoeffects::VideoEffectLoadReport emptyReport;
	S2_CHECK(videoeffects::deriveEffectHealth(emptyReport, /*knowledgePackRejected=*/true)
		== videoeffects::EffectHealth::Degraded);
}

static void test_effect_activity_status_health_and_empty_slots_are_independent() {
	// "health == Ready with EMPTY slots is a fully valid, common state" —
	// EffectActivityStatus.h's own header comment. Must never be inferred
	// as Failed/Degraded merely because slots is empty.
	videoeffects::EffectActivityStatus status;
	status.health = videoeffects::EffectHealth::Ready;
	S2_CHECK(status.slots.empty());
	S2_CHECK(status.health == videoeffects::EffectHealth::Ready);
	S2_CHECK(!status.messageId.has_value()); // nullopt when health == Ready, per contract
}

static void test_effect_activity_status_slot_fields_survive_construction() {
	// "Present-active snapshot... verify slot IDs; effect IDs; display
	// names; phase; transition progress; health; all survive transport" —
	// this is the pure-logic half (construction + field echo); the
	// through-HudFrameData half is OF-dependent, see GlRestorationHarness.
	videoeffects::EffectActivityStatus status;
	status.health = videoeffects::EffectHealth::Degraded;
	status.messageId = "effects.message.degraded_fallback";

	videoeffects::EffectActivitySlot slot;
	slot.slotId = "quadrant_2";
	slot.effectId = "bioluminescence";
	slot.displayName = "Bioluminescence";
	slot.phase = videoeffects::EvolutionPhase::Transitioning;
	slot.transitionProgress01 = 0.75f;
	slot.prominence = 0.6f;
	status.slots.push_back(slot);

	S2_CHECK(status.health == videoeffects::EffectHealth::Degraded);
	S2_CHECK(status.messageId.has_value() && *status.messageId == "effects.message.degraded_fallback");
	S2_CHECK(status.slots.size() == 1);
	S2_CHECK(status.slots[0].slotId == "quadrant_2");
	S2_CHECK(status.slots[0].effectId == "bioluminescence");
	S2_CHECK(status.slots[0].displayName == "Bioluminescence");
	S2_CHECK(status.slots[0].phase == videoeffects::EvolutionPhase::Transitioning);
	S2_CHECK(status.slots[0].transitionProgress01 == 0.75f);
	S2_CHECK(status.slots[0].prominence == 0.6f);
}

static void test_resolve_dominant_effect_ids_canonical_semantic_form() {
	// The semantic-ID counterpart to resolveDominantEffectLabels() —
	// "canonical deterministic dominance... do not infer dominance from
	// vector order or names": constructs slots in effectId-alphabetical
	// order but with prominence in the OPPOSITE order, and asserts the
	// result follows prominence, not insertion/alphabetical order.
	videoeffects::EffectActivityStatus status;

	videoeffects::EffectActivitySlot a; // alphabetically first, prominence LOWEST
	a.slotId = "a"; a.effectId = "aaa_effect"; a.displayName = "AAA";
	a.phase = videoeffects::EvolutionPhase::Holding; a.prominence = 0.2f;
	status.slots.push_back(a);

	videoeffects::EffectActivitySlot z; // alphabetically last, prominence HIGHEST
	z.slotId = "z"; z.effectId = "zzz_effect"; z.displayName = "ZZZ";
	z.phase = videoeffects::EvolutionPhase::Holding; z.prominence = 0.9f;
	status.slots.push_back(z);

	auto results = videoeffects::resolveDominantEffectIds(status);
	S2_CHECK(results.size() == 2);
	if (results.size() == 2) {
		S2_CHECK(results[0].effectId == "zzz_effect"); // prominence wins, not insertion/alpha order
		S2_CHECK(results[1].effectId == "aaa_effect");
	}

	// Deterministic and repeatable across calls with identical input.
	auto resultsAgain = videoeffects::resolveDominantEffectIds(status);
	S2_CHECK(results.size() == resultsAgain.size());
	if (!results.empty() && !resultsAgain.empty()) {
		S2_CHECK(results.front().effectId == resultsAgain.front().effectId);
	}
}

static void test_resolve_dominant_effect_ids_never_returns_a_display_name() {
	// Guards against accidentally feeding a label into the semantic-ID
	// path — DominantEffectResult::effectId must be the canonical id, not
	// displayName, regardless of how displayName is spelled.
	videoeffects::EffectActivityStatus status;
	videoeffects::EffectActivitySlot slot;
	slot.slotId = "primary";
	slot.effectId = "heatmap_recolor";
	slot.displayName = "This Is Not The Effect Id";
	slot.prominence = 1.0f;
	status.slots.push_back(slot);

	auto results = videoeffects::resolveDominantEffectIds(status);
	S2_CHECK(results.size() == 1);
	if (!results.empty()) {
		S2_CHECK(results[0].effectId == "heatmap_recolor");
		S2_CHECK(results[0].effectId != slot.displayName);
	}
}

int main() {
	test_video_playback_status_defaults();
	test_video_playback_status_ready_implies_media_id();
	test_effect_activity_status_empty_is_valid();
	test_effect_activity_status_dominance_matches_fakescene_usage();
	test_effect_activity_status_transitioning_annotation();
	test_effect_health_ready_with_empty_load_report();
	test_effect_health_failed_on_missing_or_failed_effects();
	test_effect_health_degraded_on_fallback_or_rejected_pack();
	test_effect_activity_status_health_and_empty_slots_are_independent();
	test_effect_activity_status_slot_fields_survive_construction();
	test_resolve_dominant_effect_ids_canonical_semantic_form();
	test_resolve_dominant_effect_ids_never_returns_a_display_name();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
