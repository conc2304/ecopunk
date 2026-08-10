#include "TFActivityStatusSelfTest.h"
#include "TFEffectPicker.h"
#include "DefaultVideoEffectCatalog.h"
#include "VideoEffectRegistry.h"
#include "ShaderLibrary.h"
#include "ofLog.h"
#include <cmath>
#include <string>

namespace videoeffects {

	namespace {

		int g_checks = 0;
		int g_failures = 0;

		void check(bool cond, const std::string & what) {
			g_checks++;
			if (!cond) {
				g_failures++;
				ofLogError("TFActivityStatusSelfTest") << "FAIL: " << what;
			}
		}

		bool approxEqual(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

		// Full structural equality -- deliberately stricter than any
		// production consumer needs, so this proves 13.2's "repeated calls
		// without an update are semantically identical" at every field, not
		// just the ones a particular HUD binding happens to read.
		bool statusesEqual(const EffectActivityStatus & a, const EffectActivityStatus & b) {
			if (a.schemaVersion != b.schemaVersion) return false;
			if (a.health != b.health) return false;
			if (a.messageId != b.messageId) return false;
			if (a.slots.size() != b.slots.size()) return false;
			for (std::size_t i = 0; i < a.slots.size(); ++i) {
				const auto & sa = a.slots[i];
				const auto & sb = b.slots[i];
				if (sa.slotId != sb.slotId || sa.effectId != sb.effectId || sa.displayName != sb.displayName) return false;
				if (sa.phase != sb.phase) return false;
				if (!approxEqual(sa.transitionProgress01, sb.transitionProgress01)) return false;
				if (!approxEqual(sa.prominence, sb.prominence)) return false;
			}
			return true;
		}

	} // namespace

	bool runActivityStatusSelfTest(ShaderLibrary & sharedShaderLib) {
		g_checks = 0;
		g_failures = 0;

		// Independent canonical-catalog registry, mirroring TFEffectPicker.cpp's
		// own anonymous-namespace tfCatalogRegistry() -- used here ONLY to
		// verify emitted effect IDs are real canonical ids (accessor
		// contract §6/§13.6), never to bypass or duplicate TFEffectPicker's
		// own uniform-binding path.
		VideoEffectRegistry catalogCheck;
		registerSinglePassEffects(catalogCheck);

		// --- Case 1: forced "Raw / No Effect" -> present-empty -----------
		{
			TFEffectPicker picker;
			TFEffectPicker::Weights w;
			w.rawWeight = 1000.0f;
			w.effectWeights.clear(); // only "Raw" is a selectable option
			w.cycleInterval = 999999.0f; // never auto-advance mid-test
			picker.setWeights(w);
			picker.setup(&sharedShaderLib); // setup() calls pickNext() once

			check(picker.getCurrentEffectName().empty(), "forced-Raw case actually selected Raw");

			auto before = picker.getCurrentEffectName();
			auto status1 = picker.activityStatus();
			auto afterFirstRead = picker.getCurrentEffectName();
			auto status2 = picker.activityStatus();
			auto afterSecondRead = picker.getCurrentEffectName();

			check(before == afterFirstRead && afterFirstRead == afterSecondRead,
				"present-empty case: activityStatus() reads did not change getCurrentEffectName() (no selection mutation on read)");
			check(status1.health == EffectHealth::Ready, "present-empty case: health is Ready (empty selection is never a failure)");
			check(!status1.messageId.has_value(), "present-empty case: messageId absent when health == Ready");
			check(status1.slots.empty(), "present-empty case: slots.empty() == true");
			check(statusesEqual(status1, status2), "present-empty case: repeated reads without update() are semantically identical");
		}

		// --- Case 2: forced known-good effect -> one active, healthy slot -
		{
			const std::string knownGoodId = "desaturate";
			check(sharedShaderLib.has(knownGoodId), "test precondition: 'desaturate' is actually loaded in the shared ShaderLibrary");
			check(catalogCheck.has(knownGoodId), "test precondition: 'desaturate' is a real canonical catalog id");

			TFEffectPicker picker;
			TFEffectPicker::Weights w;
			w.rawWeight = 0.0f;
			w.effectWeights = { { knownGoodId, 1000.0f } };
			w.cycleInterval = 999999.0f;
			picker.setWeights(w);
			picker.setup(&sharedShaderLib);

			check(picker.getCurrentEffectName() == knownGoodId, "forced-known-good case actually selected the forced effect");

			auto beforeName = picker.getCurrentEffectName();
			auto status1 = picker.activityStatus();
			auto status2 = picker.activityStatus();
			auto afterName = picker.getCurrentEffectName();

			check(beforeName == afterName, "active-slot case: activityStatus() reads did not change getCurrentEffectName()");
			check(status1.health == EffectHealth::Ready, "active-slot case: health is Ready for a real, loaded effect");
			check(status1.slots.size() == 1, "active-slot case: exactly one slot reported");
			if (status1.slots.size() == 1) {
				const auto & slot = status1.slots.front();
				check(slot.effectId == knownGoodId, "active-slot case: slot.effectId is the exact canonical id selected, not a display label");
				check(catalogCheck.has(slot.effectId), "active-slot case: slot.effectId resolves against the canonical catalog");
				check(!slot.displayName.empty(), "active-slot case: displayName populated (presentation support, not identity)");
				check(slot.phase == EvolutionPhase::Holding,
					"active-slot case: phase reflects this picker's real hard-cut model (no blended transition exists to report)");
				check(approxEqual(slot.transitionProgress01, 1.0f), "active-slot case: transitionProgress01 reflects the real (non-transitioning) state");
				check(approxEqual(slot.prominence, 1.0f), "active-slot case: prominence reflects the real single-content-layer state");
			}
			check(statusesEqual(status1, status2), "active-slot case: repeated reads without update() are semantically identical");
		}

		// --- Case 3: forced effect NOT registered in ShaderLibrary -------
		// Proves health is derived from real owned state (shaderLib->has()),
		// not fabricated -- the exact same condition drawCurrent() itself
		// checks for its own raw-draw fallback.
		{
			const std::string missingId = "totally_unregistered_effect_for_selftest_zzz";
			check(!sharedShaderLib.has(missingId), "test precondition: the forced 'missing' id is genuinely not loaded");

			TFEffectPicker picker;
			TFEffectPicker::Weights w;
			w.rawWeight = 0.0f;
			w.effectWeights = { { missingId, 1000.0f } };
			w.cycleInterval = 999999.0f;
			picker.setWeights(w);
			picker.setup(&sharedShaderLib);

			check(picker.getCurrentEffectName() == missingId, "forced-missing case actually selected the forced (unregistered) name");

			auto status1 = picker.activityStatus();
			auto status2 = picker.activityStatus();

			check(status1.health == EffectHealth::Degraded, "forced-missing case: health is Degraded, derived from real shaderLib->has() state, not fabricated");
			check(status1.messageId.has_value() && *status1.messageId == "effect.shader_unavailable",
				"forced-missing case: messageId is a stable id, not raw text, and only present when health != Ready");
			check(status1.slots.size() == 1, "forced-missing case: the slot is still reported (picker's real selection state), just marked Degraded");
			check(statusesEqual(status1, status2), "forced-missing case: repeated reads without update() are semantically identical");

			// Note: this owner has no VideoEffectLoadReport / hard-failure
			// concept at all (see activityStatus()'s own header comment) --
			// EffectHealth::Failed is a real, defined value in the shared
			// enum but is not reachable through THIS particular owner. Not
			// tested here because there is no honest way to reach it, and
			// fabricating one would violate the "no false telemetry"
			// principle this whole increment is built around.
		}

		bool allPassed = (g_failures == 0);
		ofLogNotice("TFActivityStatusSelfTest") << (allPassed ? "PASS" : "FAIL") << ": " << (g_checks - g_failures) << "/" << g_checks << " checks passed";
		return allPassed;
	}

} // namespace videoeffects
