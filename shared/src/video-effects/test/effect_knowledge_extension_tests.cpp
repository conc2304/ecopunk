// Standalone, dependency-free tests for the Shared Effect Knowledge scoped
// extension's pure-logic pieces: EffectActivityStatus's dominance
// resolution and EffectSceneCompatibility's matrix lookups. Deliberately
// does NOT link any part of openFrameworks — same rationale as
// sketches/experience_runtime/test/lifecycle_state_tests.cpp and siblings:
// confirmed by this pass's own build that these two headers pull in
// nothing beyond glm.
//
// Build/run: make -f Makefile.tests test

#include <cmath>
#include <iostream>
#include <string>

#include "EffectActivityStatus.h"
#include "EffectKnowledgePrecedence.h"
#include "EffectPresetId.h"
#include "EffectSceneCompatibility.h"
#include "VideoEffectLoadReport.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string & file, int line, const std::string & expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

bool approxEqualFloat(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

} // namespace

#define EK_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

using videoeffects::DominanceConfig;
using videoeffects::DominantEffectResult;
using videoeffects::EffectActivitySlot;
using videoeffects::EffectActivityStatus;
using videoeffects::EffectHealth;
using videoeffects::EffectLevelKnowledge;
using videoeffects::EvolutionPhase;
using videoeffects::KnowledgeClassification;
using videoeffects::KnowledgeEntry;
using videoeffects::VideoEffectLoadReport;
using videoeffects::deriveEffectHealth;
using videoeffects::isEligibleForAutomaticProductionSelection;
using videoeffects::isReusableAuthoredPreset;
using videoeffects::isWellFormedEffectPresetId;
using videoeffects::resolveCompatibility;
using videoeffects::resolvePiSafe;
using videoeffects::synthesizeMigrationPresetId;

// --- EffectActivityStatus / dominance resolution -------------------------

static void test_empty_status_yields_no_labels() {
	EffectActivityStatus status;
	auto labels = resolveDominantEffectLabels(status);
	EK_CHECK(labels.empty());
}

static void test_single_slot_yields_one_label() {
	EffectActivityStatus status;
	status.slots.push_back({ "primary", "heatmap_recolor", "Heatmap Recolor", EvolutionPhase::Holding, 1.0f, 1.0f });

	auto labels = resolveDominantEffectLabels(status);
	EK_CHECK(labels.size() == 1);
	EK_CHECK(labels[0] == "Heatmap Recolor");
}

static void test_low_prominence_slot_is_dropped() {
	EffectActivityStatus status;
	status.slots.push_back({ "primary", "heatmap_recolor", "Heatmap Recolor", EvolutionPhase::Holding, 1.0f, 0.9f });
	status.slots.push_back({ "faint", "dither", "Dither", EvolutionPhase::Holding, 1.0f, 0.01f });

	DominanceConfig config;
	config.minProminenceToShow = 0.05f;
	auto labels = resolveDominantEffectLabels(status, config);
	EK_CHECK(labels.size() == 1);
	EK_CHECK(labels[0] == "Heatmap Recolor");
}

static void test_most_prominent_sorted_first_and_capped() {
	EffectActivityStatus status;
	status.slots.push_back({ "q0", "dither", "Dither", EvolutionPhase::Holding, 1.0f, 0.2f });
	status.slots.push_back({ "q1", "recolor", "Recolor", EvolutionPhase::Holding, 1.0f, 0.9f });
	status.slots.push_back({ "q2", "bioluminescence", "Bioluminescence", EvolutionPhase::Holding, 1.0f, 0.5f });

	DominanceConfig config;
	config.maxLabels = 2;
	auto labels = resolveDominantEffectLabels(status, config);
	EK_CHECK(labels.size() == 2);
	EK_CHECK(labels[0] == "Recolor");
	EK_CHECK(labels[1] == "Bioluminescence");
}

static void test_same_effect_multiple_slots_collapses_to_one_label() {
	EffectActivityStatus status;
	status.slots.push_back({ "q0", "recolor", "Recolor", EvolutionPhase::Holding, 1.0f, 0.4f });
	status.slots.push_back({ "q1", "recolor", "Recolor", EvolutionPhase::Holding, 1.0f, 0.6f });

	auto labels = resolveDominantEffectLabels(status);
	EK_CHECK(labels.size() == 1);
	EK_CHECK(labels[0] == "Recolor");
}

static void test_tie_breaks_by_smallest_slot_id_deterministically() {
	EffectActivityStatus statusA;
	statusA.slots.push_back({ "q1", "dither", "Dither", EvolutionPhase::Holding, 1.0f, 0.5f });
	statusA.slots.push_back({ "q0", "recolor", "Recolor", EvolutionPhase::Holding, 1.0f, 0.5f });

	DominanceConfig config;
	config.maxLabels = 1;
	auto labels = resolveDominantEffectLabels(statusA, config);
	// q0 < q1 lexically, so "recolor" (owned by q0) must win the tie.
	EK_CHECK(labels.size() == 1);
	EK_CHECK(labels[0] == "Recolor");
}

static void test_transitioning_slot_gets_annotated() {
	EffectActivityStatus status;
	status.slots.push_back({ "primary", "recolor", "Recolor", EvolutionPhase::Transitioning, 0.4f, 1.0f });

	auto labels = resolveDominantEffectLabels(status);
	EK_CHECK(labels.size() == 1);
	EK_CHECK(labels[0] == "Recolor (shifting)");
}

static void test_transitioning_annotation_can_be_disabled() {
	EffectActivityStatus status;
	status.slots.push_back({ "primary", "recolor", "Recolor", EvolutionPhase::Transitioning, 0.4f, 1.0f });

	DominanceConfig config;
	config.annotateTransitioning = false;
	auto labels = resolveDominantEffectLabels(status, config);
	EK_CHECK(labels.size() == 1);
	EK_CHECK(labels[0] == "Recolor");
}

static void test_group_transitioning_if_any_member_slot_is() {
	EffectActivityStatus status;
	// Two slots on the same effect; only one is actively transitioning —
	// the merged group must still read as transitioning.
	status.slots.push_back({ "q0", "recolor", "Recolor", EvolutionPhase::Holding, 1.0f, 0.5f });
	status.slots.push_back({ "q1", "recolor", "Recolor", EvolutionPhase::Transitioning, 0.2f, 0.5f });

	auto labels = resolveDominantEffectLabels(status);
	EK_CHECK(labels.size() == 1);
	EK_CHECK(labels[0] == "Recolor (shifting)");
}

static void test_missing_display_name_falls_back_to_effect_id() {
	EffectActivityStatus status;
	status.slots.push_back({ "primary", "some_new_effect", "", EvolutionPhase::Holding, 1.0f, 1.0f });

	auto labels = resolveDominantEffectLabels(status);
	EK_CHECK(labels.size() == 1);
	EK_CHECK(labels[0] == "some_new_effect");
}

// --- resolveDominantEffectIds (semantic-ID form, for effects.* slots) ----

static void test_ids_empty_status_yields_no_results() {
	EffectActivityStatus status;
	auto results = resolveDominantEffectIds(status);
	EK_CHECK(results.empty());
}

static void test_ids_all_slots_below_threshold_yields_no_results() {
	EffectActivityStatus status;
	status.slots.push_back({ "primary", "recolor", "Recolor", EvolutionPhase::Holding, 1.0f, 0.01f });
	status.slots.push_back({ "secondary", "dither", "Dither", EvolutionPhase::Holding, 1.0f, 0.02f });

	DominanceConfig config;
	config.minProminenceToShow = 0.05f;
	auto results = resolveDominantEffectIds(status, config);
	EK_CHECK(results.empty());
}

static void test_ids_never_contain_display_text() {
	EffectActivityStatus status;
	status.slots.push_back({ "primary", "recolor", "Recolor", EvolutionPhase::Transitioning, 0.4f, 1.0f });

	auto results = resolveDominantEffectIds(status);
	EK_CHECK(results.size() == 1);
	// Must be the bare canonical id -- no "(shifting)" suffix, no display
	// name substitution. This is the exact property that makes this
	// function, not resolveDominantEffectLabels(), the correct source for
	// HUD-Semantic-Slot-Model-v1.md's effects.active/effects.dominant.
	EK_CHECK(results[0].effectId == "recolor");
	EK_CHECK(results[0].transitioning == true);
	EK_CHECK(approxEqualFloat(results[0].transitionProgress01, 0.4f));
}

static void test_ids_dominant_result_is_front() {
	EffectActivityStatus status;
	status.slots.push_back({ "q0", "dither", "Dither", EvolutionPhase::Holding, 1.0f, 0.9f });
	status.slots.push_back({ "q1", "recolor", "Recolor", EvolutionPhase::Holding, 1.0f, 0.3f });

	auto results = resolveDominantEffectIds(status);
	EK_CHECK(!results.empty());
	EK_CHECK(results.front().effectId == "dither");
}

static void test_ids_and_labels_agree_on_ranking() {
	// The two resolvers share one internal grouping/sort implementation --
	// this pins that down so a future edit can't silently make them
	// disagree on order.
	EffectActivityStatus status;
	status.slots.push_back({ "q0", "dither", "Dither", EvolutionPhase::Holding, 1.0f, 0.2f });
	status.slots.push_back({ "q1", "recolor", "Recolor", EvolutionPhase::Holding, 1.0f, 0.9f });
	status.slots.push_back({ "q2", "bioluminescence", "Bioluminescence", EvolutionPhase::Holding, 1.0f, 0.5f });

	DominanceConfig config;
	config.maxLabels = 3;
	auto ids = resolveDominantEffectIds(status, config);
	auto labels = resolveDominantEffectLabels(status, config);
	EK_CHECK(ids.size() == 3 && labels.size() == 3);
	EK_CHECK(ids[0].effectId == "recolor" && labels[0] == "Recolor");
	EK_CHECK(ids[1].effectId == "bioluminescence" && labels[1] == "Bioluminescence");
	EK_CHECK(ids[2].effectId == "dither" && labels[2] == "Dither");
}

// --- EffectSceneCompatibility ---------------------------------------------

static void test_all_six_roadmap_scenes_present() {
	const auto & matrix = videoeffects::defaultSceneCompatibilityMatrix();
	static const std::string kSixScenes[] = {
		"blob-region-prototype", "contour-portrait", "temporal-fields",
		"fragment-trail", "quadrant-crosshair", "blueprint_emergence"
	};
	EK_CHECK(matrix.size() == 6);
	for (const auto & sceneId : kSixScenes) {
		EK_CHECK(videoeffects::findSceneCompatibility(matrix, sceneId) != nullptr);
	}
}

static void test_shared_service_consumer_scenes_report_true() {
	const auto & matrix = videoeffects::defaultSceneCompatibilityMatrix();
	EK_CHECK(videoeffects::sceneConsumesSharedEffects(matrix, "temporal-fields"));
	EK_CHECK(videoeffects::sceneConsumesSharedEffects(matrix, "blueprint_emergence"));
	EK_CHECK(videoeffects::sceneConsumesSharedEffects(matrix, "quadrant-crosshair"));
}

static void test_local_fork_and_unintegrated_scenes_report_false() {
	const auto & matrix = videoeffects::defaultSceneCompatibilityMatrix();
	EK_CHECK(!videoeffects::sceneConsumesSharedEffects(matrix, "fragment-trail"));
	EK_CHECK(!videoeffects::sceneConsumesSharedEffects(matrix, "contour-portrait"));
}

static void test_unknown_scene_id_is_not_a_crash_and_reports_false() {
	const auto & matrix = videoeffects::defaultSceneCompatibilityMatrix();
	EK_CHECK(videoeffects::findSceneCompatibility(matrix, "radar-pulse") == nullptr);
	EK_CHECK(!videoeffects::sceneConsumesSharedEffects(matrix, "radar-pulse"));
}

static void test_no_scene_reports_pi_validated_yet() {
	// Scene-HUD-Contract-v1.md §16: "nothing in this contract has been
	// measured on real Pi 3B hardware." This matrix must not claim
	// otherwise until real Pi validation happens (Phase 9).
	const auto & matrix = videoeffects::defaultSceneCompatibilityMatrix();
	for (const auto & entry : matrix) {
		EK_CHECK(entry.piValidated == false);
	}
}

// --- EffectPresetId (DEC-016 preset identity) -----------------------------

static void test_well_formed_preset_id_accepted() {
	EK_CHECK(isWellFormedEffectPresetId("preset.heatmap_recolor.low_solar", "heatmap_recolor"));
}

static void test_preset_id_wrong_effect_segment_rejected() {
	// A preset ID naming a DIFFERENT effect than the entry's own `effect`
	// field must never validate — the shape-level half of "associated
	// with exactly one canonical effect ID" (EffectPresetId.h's own
	// comment).
	EK_CHECK(!isWellFormedEffectPresetId("preset.heatmap_recolor.low_solar", "dither"));
}

static void test_preset_id_malformed_shapes_rejected() {
	EK_CHECK(!isWellFormedEffectPresetId("heatmap_recolor.low_solar", "heatmap_recolor")); // missing "preset." prefix
	EK_CHECK(!isWellFormedEffectPresetId("preset.heatmap_recolor.", "heatmap_recolor")); // empty slug
	EK_CHECK(!isWellFormedEffectPresetId("preset.heatmap_recolor", "heatmap_recolor")); // no slug segment at all
	EK_CHECK(!isWellFormedEffectPresetId("preset.heatmap_recolor.Low_Solar", "heatmap_recolor")); // uppercase in slug
	EK_CHECK(!isWellFormedEffectPresetId("preset.heatmap_recolor.low solar", "heatmap_recolor")); // space in slug
	EK_CHECK(!isWellFormedEffectPresetId("preset.heatmap_recolor.low.solar", "heatmap_recolor")); // dot in slug
	EK_CHECK(!isWellFormedEffectPresetId("", "heatmap_recolor")); // empty string entirely
}

static void test_synthesize_migration_preset_id_happy_path() {
	std::string id = synthesizeMigrationPresetId("dither", "20260807_1");
	EK_CHECK(id == "preset.dither.20260807_1");
	EK_CHECK(isWellFormedEffectPresetId(id, "dither"));
}

static void test_synthesize_migration_preset_id_rejects_bad_slug() {
	// Never silently sanitizes -- an invalid slug yields empty string, not
	// a best-effort mangled ID a caller might accidentally persist.
	EK_CHECK(synthesizeMigrationPresetId("dither", "Not Valid!").empty());
	EK_CHECK(synthesizeMigrationPresetId("dither", "").empty());
	EK_CHECK(synthesizeMigrationPresetId("", "some_slug").empty());
}

static void test_is_reusable_authored_preset() {
	KnowledgeEntry withGoodId;
	withGoodId.effect = "recolor";
	withGoodId.presetId = "preset.recolor.warm_evening";
	EK_CHECK(isReusableAuthoredPreset(withGoodId));

	KnowledgeEntry anonymous;
	anonymous.effect = "recolor";
	// presetId left nullopt -- a legacy anonymous preset.
	EK_CHECK(!isReusableAuthoredPreset(anonymous));

	KnowledgeEntry mismatchedId;
	mismatchedId.effect = "recolor";
	mismatchedId.presetId = "preset.dither.warm_evening"; // wrong effect segment
	EK_CHECK(!isReusableAuthoredPreset(mismatchedId));
}

// --- EffectLevelKnowledge / DEC-016 precedence chain ----------------------
//
// Precedence under test: preset override -> effect-level default ->
// Unclassified/Unknown. See EffectKnowledgePrecedence.h's own doc comment
// for the exact rule table these pin down.

static void test_precedence_no_override_no_default_is_unclassified() {
	KnowledgeEntry preset;
	preset.effect = "dither";
	// compatibleSceneIds left nullopt -- no preset-level override authored.
	KnowledgeClassification result = resolveCompatibility(preset, nullptr, "temporal-fields");
	EK_CHECK(result == KnowledgeClassification::Unclassified);
	EK_CHECK(!isEligibleForAutomaticProductionSelection(result));
}

static void test_precedence_falls_through_to_effect_default_when_preset_silent() {
	KnowledgeEntry preset;
	preset.effect = "dither";
	// no preset-level override

	EffectLevelKnowledge effectDefault;
	effectDefault.effectId = "dither";
	effectDefault.compatibleSceneIds = { "temporal-fields", "blueprint_emergence" };

	EK_CHECK(resolveCompatibility(preset, &effectDefault, "temporal-fields") == KnowledgeClassification::Allowed);
	EK_CHECK(resolveCompatibility(preset, &effectDefault, "fragment-trail") == KnowledgeClassification::Disallowed);
}

static void test_precedence_preset_override_wins_over_effect_default_even_when_narrower() {
	KnowledgeEntry preset;
	preset.effect = "dither";
	preset.compatibleSceneIds = { "quadrant-crosshair" }; // preset's own, narrower override

	EffectLevelKnowledge effectDefault;
	effectDefault.effectId = "dither";
	effectDefault.compatibleSceneIds = { "temporal-fields", "blueprint_emergence" }; // would allow temporal-fields

	// The preset override wins OUTRIGHT -- the effect default is never
	// consulted once the preset has an opinion, even though it would have
	// said "Allowed" for temporal-fields.
	EK_CHECK(resolveCompatibility(preset, &effectDefault, "temporal-fields") == KnowledgeClassification::Disallowed);
	EK_CHECK(resolveCompatibility(preset, &effectDefault, "quadrant-crosshair") == KnowledgeClassification::Allowed);
}

static void test_precedence_explicit_empty_preset_override_disallows_everything() {
	// The central distinction this session's compatibleSceneIds retype
	// exists to make representable: present-but-EMPTY is a real override
	// ("compatible with no production scenes"), not the same as absent.
	KnowledgeEntry preset;
	preset.effect = "dither";
	preset.compatibleSceneIds = std::vector<std::string>{}; // present, explicitly empty

	EffectLevelKnowledge effectDefault;
	effectDefault.effectId = "dither";
	effectDefault.compatibleSceneIds = { "temporal-fields" }; // would otherwise allow it

	KnowledgeClassification result = resolveCompatibility(preset, &effectDefault, "temporal-fields");
	EK_CHECK(result == KnowledgeClassification::Disallowed);
	EK_CHECK(!isEligibleForAutomaticProductionSelection(result));
}

static void test_precedence_effect_default_with_empty_scenes_is_unclassified_not_disallowed() {
	// An EffectLevelKnowledge record can exist (e.g. to carry piSafe) while
	// its own compatibleSceneIds stays empty -- that must fall through to
	// Unclassified, not be treated as an effect-level "compatible with
	// nothing" override (only a PRESET's compatibleSceneIds carries
	// override semantics; EffectLevelKnowledge's empty vector has no
	// separate "absent" representation to distinguish from, so it is
	// defined as "no effect-level classification supplied").
	KnowledgeEntry preset;
	preset.effect = "dither";

	EffectLevelKnowledge effectDefault;
	effectDefault.effectId = "dither";
	// compatibleSceneIds left default-constructed (empty vector).

	KnowledgeClassification result = resolveCompatibility(preset, &effectDefault, "temporal-fields");
	EK_CHECK(result == KnowledgeClassification::Unclassified);
	EK_CHECK(!isEligibleForAutomaticProductionSelection(result));
}

static void test_eligibility_only_allowed_is_eligible() {
	EK_CHECK(isEligibleForAutomaticProductionSelection(KnowledgeClassification::Allowed));
	EK_CHECK(!isEligibleForAutomaticProductionSelection(KnowledgeClassification::Disallowed));
	EK_CHECK(!isEligibleForAutomaticProductionSelection(KnowledgeClassification::Unclassified));
}

// --- Pi tri-state precedence (DEC-016 §3.2) --------------------------------

static void test_pi_safe_precedence_preset_override_wins() {
	KnowledgeEntry preset;
	preset.effect = "bioluminescence";
	preset.piSafe = false;

	EffectLevelKnowledge effectDefault;
	effectDefault.effectId = "bioluminescence";
	effectDefault.piSafe = true; // preset's override must still win

	auto result = resolvePiSafe(preset, &effectDefault);
	EK_CHECK(result.has_value() && result.value() == false);
}

static void test_pi_safe_precedence_falls_through_to_effect_default() {
	KnowledgeEntry preset;
	preset.effect = "bioluminescence";
	// preset.piSafe left nullopt

	EffectLevelKnowledge effectDefault;
	effectDefault.effectId = "bioluminescence";
	effectDefault.piSafe = true;

	auto result = resolvePiSafe(preset, &effectDefault);
	EK_CHECK(result.has_value() && result.value() == true);
}

static void test_pi_safe_precedence_both_absent_is_unknown_never_true() {
	KnowledgeEntry preset;
	preset.effect = "bioluminescence";

	EK_CHECK(!resolvePiSafe(preset, nullptr).has_value());

	EffectLevelKnowledge effectDefault;
	effectDefault.effectId = "bioluminescence";
	// effectDefault.piSafe left nullopt too
	EK_CHECK(!resolvePiSafe(preset, &effectDefault).has_value());
}

// --- effects.health (deriveEffectHealth) -----------------------------------

static void test_health_ready_when_load_report_clean() {
	VideoEffectLoadReport report;
	report.requested = { "recolor", "dither" };
	report.registered = { "recolor", "dither" };
	EK_CHECK(deriveEffectHealth(report) == EffectHealth::Ready);
}

static void test_health_ready_with_empty_active_effects_is_not_a_failure() {
	// The prompt's own required regression: empty active effects must
	// never be conflated with health failure. An untouched, default-
	// constructed load report (nothing requested, nothing missing/failed)
	// must still read Ready.
	VideoEffectLoadReport report;
	EK_CHECK(deriveEffectHealth(report) == EffectHealth::Ready);
}

static void test_health_failed_on_missing() {
	VideoEffectLoadReport report;
	report.requested = { "recolor" };
	report.missing = { "recolor" };
	EK_CHECK(deriveEffectHealth(report) == EffectHealth::Failed);
}

static void test_health_failed_on_failed_asset() {
	VideoEffectLoadReport report;
	report.requested = { "recolor" };
	report.registered = { "recolor" };
	report.failed = { "recolor" };
	EK_CHECK(deriveEffectHealth(report) == EffectHealth::Failed);
}

static void test_health_degraded_on_fallback() {
	VideoEffectLoadReport report;
	report.requested = { "caustics" };
	report.registered = { "caustics" };
	report.fallback = { "caustics" };
	EK_CHECK(deriveEffectHealth(report) == EffectHealth::Degraded);
}

static void test_health_degraded_on_rejected_knowledge_pack() {
	VideoEffectLoadReport report; // otherwise perfectly clean
	EK_CHECK(deriveEffectHealth(report, /*knowledgePackRejected=*/true) == EffectHealth::Degraded);
}

static void test_health_failed_takes_priority_over_degraded() {
	// A report with BOTH a missing effect and a fallback substitution must
	// read Failed, not Degraded -- missing/failed is the more severe
	// condition and must not be masked by an unrelated fallback elsewhere
	// in the same report.
	VideoEffectLoadReport report;
	report.requested = { "recolor", "caustics" };
	report.missing = { "recolor" };
	report.fallback = { "caustics" };
	EK_CHECK(deriveEffectHealth(report) == EffectHealth::Failed);
}

int main() {
	test_empty_status_yields_no_labels();
	test_single_slot_yields_one_label();
	test_low_prominence_slot_is_dropped();
	test_most_prominent_sorted_first_and_capped();
	test_same_effect_multiple_slots_collapses_to_one_label();
	test_tie_breaks_by_smallest_slot_id_deterministically();
	test_transitioning_slot_gets_annotated();
	test_transitioning_annotation_can_be_disabled();
	test_group_transitioning_if_any_member_slot_is();
	test_missing_display_name_falls_back_to_effect_id();

	test_ids_empty_status_yields_no_results();
	test_ids_all_slots_below_threshold_yields_no_results();
	test_ids_never_contain_display_text();
	test_ids_dominant_result_is_front();
	test_ids_and_labels_agree_on_ranking();

	test_all_six_roadmap_scenes_present();
	test_shared_service_consumer_scenes_report_true();
	test_local_fork_and_unintegrated_scenes_report_false();
	test_unknown_scene_id_is_not_a_crash_and_reports_false();
	test_no_scene_reports_pi_validated_yet();

	test_well_formed_preset_id_accepted();
	test_preset_id_wrong_effect_segment_rejected();
	test_preset_id_malformed_shapes_rejected();
	test_synthesize_migration_preset_id_happy_path();
	test_synthesize_migration_preset_id_rejects_bad_slug();
	test_is_reusable_authored_preset();

	test_precedence_no_override_no_default_is_unclassified();
	test_precedence_falls_through_to_effect_default_when_preset_silent();
	test_precedence_preset_override_wins_over_effect_default_even_when_narrower();
	test_precedence_explicit_empty_preset_override_disallows_everything();
	test_precedence_effect_default_with_empty_scenes_is_unclassified_not_disallowed();
	test_eligibility_only_allowed_is_eligible();

	test_pi_safe_precedence_preset_override_wins();
	test_pi_safe_precedence_falls_through_to_effect_default();
	test_pi_safe_precedence_both_absent_is_unknown_never_true();

	test_health_ready_when_load_report_clean();
	test_health_ready_with_empty_active_effects_is_not_a_failure();
	test_health_failed_on_missing();
	test_health_failed_on_failed_asset();
	test_health_degraded_on_fallback();
	test_health_degraded_on_rejected_knowledge_pack();
	test_health_failed_takes_priority_over_degraded();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
