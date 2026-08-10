#include "TFEligibilitySelfTest.h"
#include "TFEffectPicker.h"
#include "EffectKnowledgeBase.h"
#include "EffectLevelKnowledge.h"
#include "EffectPresetId.h"
#include "ShaderLibrary.h"
#include "ofLog.h"
#include "ofUtils.h"
#include <cmath>
#include <set>
#include <string>

namespace videoeffects {

	namespace {

		int g_checks = 0;
		int g_failures = 0;

		void check(bool cond, const std::string & what) {
			g_checks++;
			if (!cond) {
				g_failures++;
				ofLogError("TFEligibilitySelfTest") << "FAIL: " << what;
			}
		}

		bool approxEqual(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

		// Fresh, uniquely-named scratch KB per test case (mirrors
		// KnowledgePackSelfTest's own run-id pattern) -- avoids any
		// possibility of one test case's seeded entries leaking into
		// another's via stale on-disk state across repeated launches.
		std::string scratchDir(const std::string & caseName) {
			return "eligibility_selftest/" + ofToString(ofGetSystemTimeMillis()) + "_" + caseName;
		}

		KnowledgeEntry makeEntry(
			const std::string & effect, const std::map<std::string, float> & snapshot, std::optional<std::string> presetId,
			std::optional<std::vector<std::string>> compatibleSceneIds) {
			KnowledgeEntry e;
			e.effect = effect;
			e.list = "whitelist";
			e.snapshot = snapshot;
			e.presetId = std::move(presetId);
			e.compatibleSceneIds = std::move(compatibleSceneIds);
			return e;
		}

		// Content-only -- blacklist exclusion (Production Selector /
		// Eligibility Increment 2) is a pure (effect, snapshot) content
		// match against EffectKnowledgeBase's existing blacklist storage,
		// same as pickNext()'s own pre-existing blacklist-avoidance check;
		// presetId/compatibleSceneIds are irrelevant to a blacklist entry.
		KnowledgeEntry makeBlacklistEntry(const std::string & effect, const std::map<std::string, float> & snapshot) {
			KnowledgeEntry e;
			e.effect = effect;
			e.list = "blacklist";
			e.snapshot = snapshot;
			return e;
		}

	} // namespace

	bool runEligibilitySelfTest(ShaderLibrary & sharedShaderLib) {
		g_checks = 0;
		g_failures = 0;

		// --- 1. Compatible eligible preset is applied ----------------------
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("compatible"));
			kb.appendWhitelist(makeEntry(
				"dither", { { "alpha", 0.5f }, { "maxPixelation", 5.0f } }, "preset.dither.selftest_compatible",
				std::vector<std::string>{ "temporal-fields" }));
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("dither");

			check(picker.lastPickUsedCanonicalPreset(), "case 1: compatible eligible preset was applied");
			check(picker.lastAppliedPresetId().has_value() && *picker.lastAppliedPresetId() == "preset.dither.selftest_compatible",
				"case 1: applied presetId matches the seeded entry, for logging/reference");
			auto snap = picker.currentParamSnapshotForTest();
			check(snap.count("alpha") == 1 && approxEqual(snap.at("alpha"), 0.5f), "case 1: applied alpha reaches the render-path snapshot");
			check(snap.count("maxPixelation") == 1 && approxEqual(snap.at("maxPixelation"), 5.0f),
				"case 1: applied maxPixelation reaches the render-path snapshot");
		}

		// --- 2. Incompatible preset excluded --------------------------------
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("incompatible"));
			kb.appendWhitelist(makeEntry(
				"threshold", { { "threshold", 0.5f } }, "preset.threshold.selftest_incompatible",
				std::vector<std::string>{ "blueprint_emergence" })); // does NOT list temporal-fields
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("threshold");

			check(!picker.lastPickUsedCanonicalPreset(), "case 2: incompatible preset excluded -- fell back to existing randomization");
		}

		// --- 3. Unclassified (no override, no effect default) excluded -----
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("unclassified"));
			kb.appendWhitelist(makeEntry("channelshift", { { "shift", 0.005f } }, "preset.channelshift.selftest_unclassified", std::nullopt));
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("channelshift");

			check(!picker.lastPickUsedCanonicalPreset(), "case 3: Unclassified preset (no override, no effect default) excluded from automatic selection");
		}

		// --- 4. Preset override wins over a WORSE effect default -----------
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("override_beats_bad_default"));
			kb.appendWhitelist(makeEntry(
				"pixel_sorting", { { "threshold", 0.5f }, { "direction", 1.0f } }, "preset.pixel_sorting.selftest_override_good",
				std::vector<std::string>{ "temporal-fields" })); // positive override
			EffectLevelKnowledge badDefault;
			badDefault.effectId = "pixel_sorting";
			badDefault.compatibleSceneIds = {}; // empty = Unclassified at the default layer
			kb.saveEffectLevelKnowledge(badDefault);
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("pixel_sorting");

			check(picker.lastPickUsedCanonicalPreset(),
				"case 4: preset override (compatible) wins outright over a worse/Unclassified effect-level default");
		}

		// --- 5. Preset override (incompatible) loses despite a BETTER default
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("override_beats_good_default"));
			kb.appendWhitelist(makeEntry(
				"hue_rotate", { { "hueOffset", 10.0f }, { "hueSpeed", 5.0f }, { "saturationMult", 1.0f }, { "valueMult", 1.0f } },
				"preset.hue_rotate.selftest_override_bad", std::vector<std::string>{ "blueprint_emergence" })); // negative override
			EffectLevelKnowledge goodDefault;
			goodDefault.effectId = "hue_rotate";
			goodDefault.compatibleSceneIds = { "temporal-fields" }; // would be Allowed if consulted
			kb.saveEffectLevelKnowledge(goodDefault);
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("hue_rotate");

			check(!picker.lastPickUsedCanonicalPreset(),
				"case 5: preset override (incompatible) wins exclusion outright, even though the effect-level default alone would have allowed it");
		}

		// --- 6. Explicit EMPTY override excludes (not "unclassified") ------
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("empty_override"));
			kb.appendWhitelist(makeEntry(
				"heatmap_recolor", { { "gamma", 1.0f }, { "minLuminance", 0.0f }, { "maxLuminance", 1.0f } },
				"preset.heatmap_recolor.selftest_empty_override",
				std::vector<std::string>{})); // present, explicitly empty
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("heatmap_recolor");

			check(!picker.lastPickUsedCanonicalPreset(), "case 6: explicit empty compatibleSceneIds override excludes (Disallowed, not Unclassified)");
		}

		// --- 7. Effect-level default applies when no preset override exists
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("effect_default_applies"));
			kb.appendWhitelist(
				makeEntry("dither", { { "alpha", 0.7f }, { "maxPixelation", 8.0f } }, "preset.dither.selftest_effect_default", std::nullopt));
			EffectLevelKnowledge def;
			def.effectId = "dither";
			def.compatibleSceneIds = { "temporal-fields" };
			kb.saveEffectLevelKnowledge(def);
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("dither");

			check(picker.lastPickUsedCanonicalPreset(), "case 7: effect-level compatible default applies when the preset authors no override of its own");
		}

		// --- 8. Legacy anonymous preset (no presetId) never auto-eligible --
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("legacy_anonymous"));
			kb.appendWhitelist(makeEntry(
				"threshold", { { "threshold", 0.42f } }, std::nullopt, // no presetId -- legacy anonymous
				std::vector<std::string>{ "temporal-fields" })); // even though positively compatible
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("threshold");

			check(!picker.lastPickUsedCanonicalPreset(),
				"case 8: a legacy anonymous preset (no stable presetId) is never automatic-selection-eligible, even if positively compatible");
		}

		// --- 9. No eligible candidates -> existing safe fallback preserved -
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			// knowledgeBaseForTest() left as whatever setup() imported (real
			// canonical pack, if any) -- pointed at an effect this test
			// never seeds anything for, to prove the untouched fallback path.
			picker.forceEffectForTest("pixel_sorting");

			// Not asserting false here unconditionally -- if the real
			// canonical pack on disk happens to contain an eligible
			// pixel_sorting preset, that's legitimate production behavior,
			// not a test failure. What's actually being proven: the call
			// completes, returns a boolean either way, and getCurrentEffectName()
			// reflects the forced choice regardless.
			check(picker.getCurrentEffectName() == "pixel_sorting", "case 9: forced effect selection is honored regardless of eligibility outcome");
		}

		// --- 10. Multiple eligible presets: uniform pick among them (the
		// "favored = existing whitelist-membership behavior" step 3 policy),
		// never the ineligible one, over many forced re-picks. -------------
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("multi_eligible"));
			kb.appendWhitelist(makeEntry(
				"channelshift", { { "shift", 0.003f } }, "preset.channelshift.selftest_multi_a", std::vector<std::string>{ "temporal-fields" }));
			kb.appendWhitelist(makeEntry(
				"channelshift", { { "shift", 0.008f } }, "preset.channelshift.selftest_multi_b", std::vector<std::string>{ "temporal-fields" }));
			kb.appendWhitelist(makeEntry(
				"channelshift", { { "shift", 0.009f } }, "preset.channelshift.selftest_multi_ineligible",
				std::vector<std::string>{ "blueprint_emergence" })); // incompatible -- must never be picked
			picker.knowledgeBaseForTest() = kb;

			std::set<std::string> seenPresetIds;
			bool sawIneligible = false;
			for (int i = 0; i < 40; ++i) {
				picker.forceEffectForTest("channelshift");
				if (!picker.lastPickUsedCanonicalPreset()) continue;
				const auto & id = picker.lastAppliedPresetId();
				if (id.has_value()) {
					seenPresetIds.insert(*id);
					if (*id == "preset.channelshift.selftest_multi_ineligible") sawIneligible = true;
				}
			}
			check(!sawIneligible, "case 10: the incompatible third preset was never selected across 40 forced re-picks");
			check(seenPresetIds.size() >= 1 && seenPresetIds.count("preset.channelshift.selftest_multi_ineligible") == 0,
				"case 10: only eligible preset ids were ever applied");
			// Not asserting both eligible ids necessarily appear (uniform
			// random over 2 choices in 40 tries could in rare cases miss
			// one) -- what matters and IS asserted is that the ineligible
			// one never does.
		}

		// --- 11. Compatible + reusable + NOT blocked: selectable (control
		// case for 12-14 below -- proves the blocked-filtering code doesn't
		// accidentally exclude a genuinely clean candidate). ----------------
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("not_blocked_control"));
			kb.appendWhitelist(makeEntry(
				"dither", { { "alpha", 0.6f }, { "maxPixelation", 4.0f } }, "preset.dither.selftest_not_blocked",
				std::vector<std::string>{ "temporal-fields" }));
			// A blacklist entry with DIFFERENT content -- must not affect
			// the unrelated eligible candidate.
			kb.appendBlacklist(makeBlacklistEntry("dither", { { "alpha", 0.99f }, { "maxPixelation", 30.0f } }));
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("dither");

			check(picker.lastPickUsedCanonicalPreset(), "case 11: a compatible, reusable, non-blocked candidate remains selectable alongside an unrelated blacklist entry");
		}

		// --- 12. Compatible + reusable + BLOCKED (sole candidate): never
		// selected -- the canonical-preset path must report no selectable
		// preset, not silently apply the blocked one after exhausting the
		// pre-existing retry budget. -----------------------------------------
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("sole_blocked"));
			std::map<std::string, float> blockedSnapshot = { { "shift", 0.005f } };
			kb.appendWhitelist(makeEntry("channelshift", blockedSnapshot, "preset.channelshift.selftest_sole_blocked", std::vector<std::string>{ "temporal-fields" }));
			kb.appendBlacklist(makeBlacklistEntry("channelshift", blockedSnapshot)); // identical content -- blocks the only candidate
			picker.knowledgeBaseForTest() = kb;

			picker.forceEffectForTest("channelshift");

			check(!picker.lastPickUsedCanonicalPreset(),
				"case 12: the sole compatible+reusable candidate, being blocked, yields no canonical preset selection at all (structural exclusion, not a lucky retry)");
		}

		// --- 13. Blocked preset remains excluded across repeated picks -----
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("blocked_repeated"));
			std::map<std::string, float> blockedSnapshot = { { "threshold", 0.5f } };
			kb.appendWhitelist(makeEntry("threshold", blockedSnapshot, "preset.threshold.selftest_blocked_repeated", std::vector<std::string>{ "temporal-fields" }));
			kb.appendBlacklist(makeBlacklistEntry("threshold", blockedSnapshot));
			picker.knowledgeBaseForTest() = kb;

			bool everSelected = false;
			for (int i = 0; i < 20; ++i) {
				picker.forceEffectForTest("threshold");
				if (picker.lastPickUsedCanonicalPreset()) everSelected = true;
			}
			check(!everSelected, "case 13: the blocked sole candidate is excluded consistently across 20 repeated forced picks, not just once");
		}

		// --- 14. Blocked + favored: among multiple eligible-by-compatibility
		// candidates, the blocked one never wins the uniform favored pick,
		// across many iterations. --------------------------------------------
		{
			TFEffectPicker picker;
			picker.setup(&sharedShaderLib);
			EffectKnowledgeBase kb(scratchDir("blocked_plus_favored"));
			kb.appendWhitelist(makeEntry(
				"hue_rotate", { { "hueOffset", 10.0f }, { "hueSpeed", 5.0f }, { "saturationMult", 1.0f }, { "valueMult", 1.0f } },
				"preset.hue_rotate.selftest_clean_a", std::vector<std::string>{ "temporal-fields" }));
			std::map<std::string, float> blockedSnapshot = { { "hueOffset", 200.0f }, { "hueSpeed", -10.0f }, { "saturationMult", 1.2f }, { "valueMult", 0.9f } };
			kb.appendWhitelist(makeEntry("hue_rotate", blockedSnapshot, "preset.hue_rotate.selftest_blocked_favored", std::vector<std::string>{ "temporal-fields" }));
			kb.appendBlacklist(makeBlacklistEntry("hue_rotate", blockedSnapshot)); // same content as the second whitelist entry above
			picker.knowledgeBaseForTest() = kb;

			bool sawBlocked = false;
			int sawClean = 0;
			for (int i = 0; i < 30; ++i) {
				picker.forceEffectForTest("hue_rotate");
				if (!picker.lastPickUsedCanonicalPreset()) continue;
				const auto & id = picker.lastAppliedPresetId();
				if (id.has_value() && *id == "preset.hue_rotate.selftest_blocked_favored") sawBlocked = true;
				if (id.has_value() && *id == "preset.hue_rotate.selftest_clean_a") sawClean++;
			}
			check(!sawBlocked, "case 14: blocked+favored never wins -- the blocked candidate was never selected across 30 iterations despite equal whitelist-membership standing");
			check(sawClean > 0, "case 14: the remaining clean eligible candidate is still selected once the blocked one is filtered out");

			// No persisted numeric weight/preference field exists anywhere in
			// the frozen schema (KnowledgeEntry/EffectLevelKnowledge, both
			// inspected this increment) -- "blocked + higher weight remains
			// excluded if weights exist" is documented here as N/A, not
			// silently skipped: there is no weight to test.
		}

		bool allPassed = (g_failures == 0);
		ofLogNotice("TFEligibilitySelfTest") << (allPassed ? "PASS" : "FAIL") << ": " << (g_checks - g_failures) << "/" << g_checks << " checks passed";
		return allPassed;
	}

} // namespace videoeffects
