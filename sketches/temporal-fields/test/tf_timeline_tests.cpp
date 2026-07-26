// Standalone, dependency-free tests for TFPresetTimeline. Deliberately does
// NOT link any part of openFrameworks -- TFPresetTimeline only depends on
// plain nlohmann::json and the TFTimelineBinding get/set-closure
// abstraction, so it's exercised here against fake in-memory doubles
// instead of real ofParameters (see FakeParam/makeXBinding below). This is
// the smallest test setup that fits a repo with no existing test framework
// (no CMake/Catch2/doctest/gtest anywhere under apps/myApps) -- hand-rolled
// assertions, one plain main(), build via Makefile.tests (see that file).
//
// Deliberately lives in test/ (a sibling of src/, not a subdirectory of
// it) -- the sketch's own openFrameworks Makefile recursively sweeps every
// .cpp under src/ into the app's build, so a src/test/ location caused a
// duplicate main() linker error against the real app the first time this
// was tried.
//
// All time advancement is via explicit update(dt) calls with fixed dt
// values -- never wall-clock sleeps -- so results are fully deterministic.

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../src/TFPresetTimeline.h"
#include "../src/TFTimelineBinding.h"
#include "../src/TFTimelineEasing.h"

namespace {

	int g_total = 0;
	int g_failures = 0;

	void reportFailure(const std::string& file, int line, const std::string& expr) {
		g_failures++;
		std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
	}

}

#define TF_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

#define TF_CHECK_NEAR(a, b, eps) \
	do { \
		g_total++; \
		double _a = static_cast<double>(a); \
		double _b = static_cast<double>(b); \
		if (std::abs(_a - _b) > (eps)) { \
			std::ostringstream _oss; \
			_oss << #a << " (" << _a << ") != " << #b << " (" << _b << ") within " << (eps); \
			reportFailure(__FILE__, __LINE__, _oss.str()); \
		} \
	} while (0)

namespace {

	// --- Fake binding plumbing (no ofParameter involved anywhere) ---

	struct FakeParam {
		double value = 0.0;
		double minValue = 0.0;
		double maxValue = 1.0;
	};

	TFTimelineBinding makeBinding(FakeParam& p, TFTimelineBinding::Kind kind) {
		TFTimelineBinding b;
		b.kind = kind;
		b.get = [&p]() { return p.value; };
		b.set = [&p](double v) { p.value = v; };
		b.hasRange = true;
		b.minValue = p.minValue;
		b.maxValue = p.maxValue;
		return b;
	}

	struct Warnings {
		std::vector<std::string> messages;
		void attach(TFPresetTimeline& tl) {
			tl.onWarning = [this](const std::string& msg) { messages.push_back(msg); };
		}
		bool anyContains(const std::string& needle) const {
			for (auto& m : messages) {
				if (m.find(needle) != std::string::npos) return true;
			}
			return false;
		}
	};

	nlohmann::json parseJson(const std::string& text) { return nlohmann::json::parse(text); }

	// --- Individual tests ---

	void test_defaultInactive() {
		TFPresetTimeline tl;
		TF_CHECK(!tl.isActive());
		TF_CHECK(tl.getCurrentStateName().empty());
		TF_CHECK(tl.getDebugStatusLine().empty());
	}

	void test_missingStatesArraySafe() {
		TFPresetTimeline tl;
		Warnings w;
		w.attach(tl);
		bool ok = tl.load(parseJson(R"({"Loop":"1"})"), {}, {});
		TF_CHECK(!ok);
		TF_CHECK(!tl.isActive());
		TF_CHECK(w.anyContains("no States"));
	}

	void test_emptyStatesArraySafe() {
		TFPresetTimeline tl;
		bool ok = tl.load(parseJson(R"({"States":[]})"), {}, {});
		TF_CHECK(!ok);
		TF_CHECK(!tl.isActive());
	}

	nlohmann::json twoStateJson() {
		return parseJson(R"({
			"Loop": "0",
			"States": [
				{ "Name": "arrival", "Hold_Duration": "5", "Transition_Duration": "10", "Easing": "linear",
				  "Overrides": { "g": { "P": "50" } } },
				{ "Name": "rest", "Hold_Duration": "5", "Transition_Duration": "10", "Easing": "linear",
				  "Overrides": { "g": { "P": "10" } } }
			]
		})");
	}

	void test_parsesAndPreservesOrder() {
		FakeParam p;
		p.minValue = 0;
		p.maxValue = 100;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.P", makeBinding(p, TFTimelineBinding::Kind::Float) } };
		std::map<std::string, std::string> base = { { "g.P", "0" } };

		TFPresetTimeline tl;
		bool ok = tl.load(twoStateJson(), base, bindings);
		TF_CHECK(ok);
		tl.start();
		TF_CHECK(tl.isActive());
		TF_CHECK(tl.getCurrentStateName() == "arrival");

		tl.advanceToNextState();
		TF_CHECK(tl.getCurrentStateName() == "rest");
	}

	void test_duplicateStateNamesRejected() {
		auto json = parseJson(R"({
			"States": [
				{ "Name": "a", "Hold_Duration": "1", "Transition_Duration": "1" },
				{ "Name": "a", "Hold_Duration": "1", "Transition_Duration": "1" },
				{ "Name": "b", "Hold_Duration": "1", "Transition_Duration": "1" }
			]
		})");
		TFPresetTimeline tl;
		Warnings w;
		w.attach(tl);
		bool ok = tl.load(json, {}, {});
		TF_CHECK(ok);
		tl.start();
		std::string status = tl.getDebugStatusLine();
		TF_CHECK(status.find("(1/2)") != std::string::npos); // 3 defined, 1 duplicate dropped -> 2 states
		TF_CHECK(w.anyContains("duplicate"));
	}

	void test_negativeDurationsClamped() {
		auto json = parseJson(R"({
			"States": [
				{ "Name": "a", "Hold_Duration": "-5", "Transition_Duration": "-2" }
			]
		})");
		TFPresetTimeline tl;
		Warnings w;
		w.attach(tl);
		TF_CHECK(tl.load(json, {}, {}));
		TF_CHECK(w.anyContains("Hold_Duration"));
		TF_CHECK(w.anyContains("Transition_Duration"));
	}

	void test_sparseOverridesResolveAgainstBase() {
		FakeParam a, b;
		a.minValue = 0; a.maxValue = 100;
		b.minValue = 0; b.maxValue = 100;
		std::map<std::string, TFTimelineBinding> bindings = {
			{ "g.A", makeBinding(a, TFTimelineBinding::Kind::Float) },
			{ "g.B", makeBinding(b, TFTimelineBinding::Kind::Float) },
		};
		std::map<std::string, std::string> base = { { "g.A", "10" }, { "g.B", "20" } };

		// State only overrides A -- B should resolve to the base value.
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "1",
				  "Overrides": { "g": { "A": "99" } } }
			]
		})");

		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		a.value = 10; // simulate "already deserialized to base" before start()
		b.value = 20;
		tl.start();
		tl.update(1.0f); // full transition duration -> lands exactly on target
		TF_CHECK_NEAR(a.value, 99.0, 0.001);
		TF_CHECK_NEAR(b.value, 20.0, 0.001); // untouched by this state's overrides -> base value
	}

	void test_firstTransitionStartsFromCurrentRuntimeValue() {
		FakeParam p;
		p.minValue = 0;
		p.maxValue = 1000;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.P", makeBinding(p, TFTimelineBinding::Kind::Float) } };
		std::map<std::string, std::string> base = { { "g.P", "10" } };

		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "10",
				  "Overrides": { "g": { "P": "50" } } }
			]
		})");

		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		p.value = 999.0; // deliberately NOT the base value, to distinguish the two
		tl.start();
		tl.update(0.0f); // t=0 -- should read back exactly the "from" snapshot
		TF_CHECK_NEAR(p.value, 999.0, 0.001);
	}

	void test_floatInterpolationAtKeyProgress() {
		FakeParam p;
		p.minValue = -1000;
		p.maxValue = 1000;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.P", makeBinding(p, TFTimelineBinding::Kind::Float) } };
		std::map<std::string, std::string> base = { { "g.P", "0" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "10", "Easing": "linear",
				  "Overrides": { "g": { "P": "10" } } }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		p.value = 0.0;
		tl.start();
		tl.update(0.0f);
		TF_CHECK_NEAR(p.value, 0.0, 0.001);
		tl.update(5.0f); // total 5/10 = 50%
		TF_CHECK_NEAR(p.value, 5.0, 0.001);
		tl.update(5.0f); // total 10/10 = 100%
		TF_CHECK_NEAR(p.value, 10.0, 0.001);
		TF_CHECK(tl.getPhase() == TFPresetTimeline::Phase::Holding);
	}

	void test_intRoundingPolicy() {
		FakeParam p;
		p.minValue = 0;
		p.maxValue = 100;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.P", makeBinding(p, TFTimelineBinding::Kind::Int) } };
		std::map<std::string, std::string> base = { { "g.P", "0" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "10", "Easing": "linear",
				  "Overrides": { "g": { "P": "5" } } }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		p.value = 0.0;
		tl.start();
		tl.update(3.0f); // 30% of 0->5 = 1.5 -> rounds to 2
		TF_CHECK_NEAR(p.value, 2.0, 0.001);
	}

	void test_boolSwitchPolicy() {
		FakeParam p;
		p.minValue = 0;
		p.maxValue = 1;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.P", makeBinding(p, TFTimelineBinding::Kind::Bool) } };
		std::map<std::string, std::string> base = { { "g.P", "0" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "10", "Easing": "linear",
				  "Overrides": { "g": { "P": "1" } } }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		p.value = 0.0;
		tl.start();
		tl.update(4.0f); // 40% < 50% -> still "from" (0)
		TF_CHECK_NEAR(p.value, 0.0, 0.001);
		tl.update(2.0f); // total 60% >= 50% -> "to" (1)
		TF_CHECK_NEAR(p.value, 1.0, 0.001);
	}

	void test_enumIntSwitchPolicy() {
		FakeParam p;
		p.minValue = 0;
		p.maxValue = 3;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.P", makeBinding(p, TFTimelineBinding::Kind::EnumInt) } };
		std::map<std::string, std::string> base = { { "g.P", "0" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "10", "Easing": "linear",
				  "Overrides": { "g": { "P": "3" } } }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		p.value = 0.0;
		tl.start();
		tl.update(4.0f); // 40% -> still from
		TF_CHECK_NEAR(p.value, 0.0, 0.001);
		tl.update(2.0f); // 60% -> to
		TF_CHECK_NEAR(p.value, 3.0, 0.001);
	}

	void test_easingFunctionsAtKeyPoints() {
		using namespace tftimeline;
		TF_CHECK_NEAR(applyEasing(EasingType::Linear, 0.0f), 0.0, 0.0001);
		TF_CHECK_NEAR(applyEasing(EasingType::Linear, 0.5f), 0.5, 0.0001);
		TF_CHECK_NEAR(applyEasing(EasingType::Linear, 1.0f), 1.0, 0.0001);

		TF_CHECK_NEAR(applyEasing(EasingType::Smoothstep, 0.0f), 0.0, 0.0001);
		TF_CHECK_NEAR(applyEasing(EasingType::Smoothstep, 0.5f), 0.5, 0.0001);
		TF_CHECK_NEAR(applyEasing(EasingType::Smoothstep, 1.0f), 1.0, 0.0001);

		TF_CHECK_NEAR(applyEasing(EasingType::Smootherstep, 0.0f), 0.0, 0.0001);
		TF_CHECK_NEAR(applyEasing(EasingType::Smootherstep, 0.5f), 0.5, 0.0001);
		TF_CHECK_NEAR(applyEasing(EasingType::Smootherstep, 1.0f), 1.0, 0.0001);

		TF_CHECK_NEAR(applyEasing(EasingType::EaseInOutSine, 0.0f), 0.0, 0.0001);
		TF_CHECK_NEAR(applyEasing(EasingType::EaseInOutSine, 0.5f), 0.5, 0.0001);
		TF_CHECK_NEAR(applyEasing(EasingType::EaseInOutSine, 1.0f), 1.0, 0.0001);

		bool unknown = false;
		EasingType t = resolveEasingType("madeUpName", unknown);
		TF_CHECK(unknown);
		TF_CHECK(t == EasingType::Linear);

		unknown = true;
		resolveEasingType("smootherstep", unknown);
		TF_CHECK(!unknown);
	}

	void test_holdThenAdvance() {
		FakeParam p;
		p.minValue = 0;
		p.maxValue = 100;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.P", makeBinding(p, TFTimelineBinding::Kind::Float) } };
		std::map<std::string, std::string> base = { { "g.P", "0" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "a", "Hold_Duration": "3", "Transition_Duration": "2",
				  "Overrides": { "g": { "P": "10" } } },
				{ "Name": "b", "Hold_Duration": "3", "Transition_Duration": "2",
				  "Overrides": { "g": { "P": "20" } } }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		tl.start();
		TF_CHECK(tl.getCurrentStateName() == "a");
		tl.update(2.0f); // transition complete -> holding
		TF_CHECK(tl.getPhase() == TFPresetTimeline::Phase::Holding);
		TF_CHECK(tl.getCurrentStateName() == "a");
		tl.update(2.9f); // still within 3s hold
		TF_CHECK(tl.getCurrentStateName() == "a");
		tl.update(0.2f); // hold elapsed -> auto-advances
		TF_CHECK(tl.getCurrentStateName() == "b");
		TF_CHECK(tl.getPhase() == TFPresetTimeline::Phase::Transitioning);
	}

	void test_loopingBackToFirstState() {
		auto json = parseJson(R"({
			"Loop": "1",
			"States": [
				{ "Name": "a", "Hold_Duration": "1", "Transition_Duration": "1" },
				{ "Name": "b", "Hold_Duration": "1", "Transition_Duration": "1" }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, {}, {}));
		tl.start();
		tl.advanceToNextState();
		TF_CHECK(tl.getCurrentStateName() == "b");
		tl.advanceToNextState(); // past the end -> loops
		TF_CHECK(tl.getCurrentStateName() == "a");
		TF_CHECK(tl.isActive());
	}

	void test_nonLoopingStopsAtFinalState() {
		auto json = parseJson(R"({
			"Loop": "0",
			"States": [
				{ "Name": "a", "Hold_Duration": "1", "Transition_Duration": "1" },
				{ "Name": "b", "Hold_Duration": "1", "Transition_Duration": "1" }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, {}, {}));
		tl.start();
		tl.advanceToNextState();
		TF_CHECK(tl.getCurrentStateName() == "b");
		tl.advanceToNextState(); // final state, non-looping -> stays parked
		TF_CHECK(tl.getCurrentStateName() == "b");
		tl.update(100.0f); // further updates never move it again
		TF_CHECK(tl.getCurrentStateName() == "b");
	}

	void test_pauseResumePreservesProgress() {
		FakeParam p;
		p.minValue = 0;
		p.maxValue = 100;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.P", makeBinding(p, TFTimelineBinding::Kind::Float) } };
		std::map<std::string, std::string> base = { { "g.P", "0" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "a", "Hold_Duration": "1", "Transition_Duration": "10",
				  "Overrides": { "g": { "P": "100" } } }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		p.value = 0.0;
		tl.start();
		tl.update(3.0f); // 30%
		TF_CHECK_NEAR(p.value, 30.0, 0.001);
		tl.pause();
		tl.update(50.0f); // ignored entirely while paused
		TF_CHECK_NEAR(p.value, 30.0, 0.001);
		tl.resume();
		tl.update(2.0f); // total progressed time now 5/10 = 50%
		TF_CHECK_NEAR(p.value, 50.0, 0.001);
	}

	void test_jumpToState() {
		auto json = parseJson(R"({
			"States": [
				{ "Name": "a", "Hold_Duration": "100", "Transition_Duration": "1" },
				{ "Name": "b", "Hold_Duration": "100", "Transition_Duration": "1" },
				{ "Name": "c", "Hold_Duration": "100", "Transition_Duration": "1" }
			]
		})");
		TFPresetTimeline tl;
		Warnings w;
		w.attach(tl);
		TF_CHECK(tl.load(json, {}, {}));
		tl.start();
		TF_CHECK(tl.jumpToState("c"));
		TF_CHECK(tl.getCurrentStateName() == "c");
		TF_CHECK(tl.getPhase() == TFPresetTimeline::Phase::Transitioning);
		TF_CHECK(!tl.jumpToState("doesNotExist"));
		TF_CHECK(w.anyContains("unknown state"));
	}

	void test_unknownParameterPathDoesNotCrash() {
		FakeParam known;
		known.minValue = 0;
		known.maxValue = 100;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.Known", makeBinding(known, TFTimelineBinding::Kind::Float) } };
		std::map<std::string, std::string> base = { { "g.Known", "1" }, { "g.Ghost", "2" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "1",
				  "Overrides": { "g": { "AlsoGhost": "9" } } }
			]
		})");
		TFPresetTimeline tl;
		Warnings w;
		w.attach(tl);
		bool ok = tl.load(json, base, bindings);
		TF_CHECK(ok); // still usable -- only the unknown paths are skipped
		tl.start();
		tl.update(1.0f);
		TF_CHECK(w.anyContains("unknown parameter path"));
	}

	void test_outOfRangeValueClamped() {
		FakeParam p;
		p.minValue = 0;
		p.maxValue = 10;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.P", makeBinding(p, TFTimelineBinding::Kind::Float) } };
		std::map<std::string, std::string> base = { { "g.P", "0" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "1",
				  "Overrides": { "g": { "P": "500" } } }
			]
		})");
		TFPresetTimeline tl;
		Warnings w;
		w.attach(tl);
		TF_CHECK(tl.load(json, base, bindings));
		TF_CHECK(w.anyContains("out of range"));
		p.value = 0.0;
		tl.start();
		tl.update(1.0f);
		TF_CHECK_NEAR(p.value, 10.0, 0.001); // clamped to the binding's declared max
	}

	void test_loadingSecondPresetClearsFirst() {
		auto json1 = parseJson(R"({"States":[{"Name":"only-in-first","Hold_Duration":"1","Transition_Duration":"1"}]})");
		auto json2 = parseJson(R"({"States":[{"Name":"only-in-second","Hold_Duration":"1","Transition_Duration":"1"}]})");

		TFPresetTimeline tl;
		TF_CHECK(tl.load(json1, {}, {}));
		tl.start();
		TF_CHECK(tl.getCurrentStateName() == "only-in-first");

		TF_CHECK(tl.load(json2, {}, {}));
		tl.start();
		TF_CHECK(tl.getCurrentStateName() == "only-in-second");
		TF_CHECK(!tl.jumpToState("only-in-first"));
	}

	// Mirrors the real pattern classes' timer accumulator idiom exactly
	// (see TFPatternBands.cpp/TFPatternBSP.cpp/TFPatternBlobGrid.cpp:
	// `regenTimer += dt; if (regenTimer >= params.rate) { regenTimer -=
	// rate; regenerate(); }`) driven by a smoothly timeline-interpolated
	// rate, to demonstrate the timeline doesn't cause runaway/duplicate
	// regen events just because the rate parameter is changing every frame.
	void test_timerRateChangeDoesNotExplodeRegenCount() {
		FakeParam rate;
		rate.minValue = 1;
		rate.maxValue = 100;
		std::map<std::string, TFTimelineBinding> bindings = { { "g.Rate", makeBinding(rate, TFTimelineBinding::Kind::Float) } };
		std::map<std::string, std::string> base = { { "g.Rate", "12" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "20",
				  "Overrides": { "g": { "Rate": "6" } } }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		rate.value = 12.0;
		tl.start();

		float regenTimer = 0.0f;
		int regenCount = 0;
		const float dt = 0.1f;
		const float totalSimulated = 20.0f;
		for (float t = 0.0f; t < totalSimulated; t += dt) {
			tl.update(dt);
			regenTimer += dt;
			if (regenTimer >= rate.value) {
				regenTimer -= rate.value;
				regenCount++;
			}
		}

		// Over 20s with a rate smoothly sweeping 12 -> 6, a reasonable
		// pattern class fires roughly totalSimulated/avgRate times -- not
		// hundreds of times from a single frame's tiny rate change. Loosely
		// bound it well above the expected ~2-3 to catch a genuine runaway
		// (e.g. accidentally resetting regenTimer to 0 every frame) without
		// being a brittle exact-count assertion.
		TF_CHECK(regenCount >= 1);
		TF_CHECK(regenCount <= 6);
	}

	void test_nonAnimatedParametersRetainValue() {
		FakeParam animated, untouched;
		animated.minValue = 0;
		animated.maxValue = 100;
		untouched.minValue = 0;
		untouched.maxValue = 100;
		untouched.value = 42.0; // sentinel -- never referenced by base or overrides

		std::map<std::string, TFTimelineBinding> bindings = {
			{ "g.Animated", makeBinding(animated, TFTimelineBinding::Kind::Float) },
			{ "g.Untouched", makeBinding(untouched, TFTimelineBinding::Kind::Float) },
		};
		std::map<std::string, std::string> base = { { "g.Animated", "0" } };
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "1",
				  "Overrides": { "g": { "Animated": "100" } } }
			]
		})");
		TFPresetTimeline tl;
		TF_CHECK(tl.load(json, base, bindings));
		animated.value = 0.0;
		tl.start();
		tl.update(1.0f);
		TF_CHECK_NEAR(animated.value, 100.0, 0.001);
		TF_CHECK_NEAR(untouched.value, 42.0, 0.001); // never touched
	}

	void test_flattenJsonToPaths() {
		auto node = parseJson(R"({
			"Drift_Speed": "0.18",
			"Effects": { "Bioluminescence": "3.6" }
		})");
		std::map<std::string, std::string> out;
		TFPresetTimeline::flattenJsonToPaths(node, "background", out);
		TF_CHECK(out["background.Drift_Speed"] == "0.18");
		TF_CHECK(out["background.Effects.Bioluminescence"] == "3.6");
	}

	void test_patternOverrideIgnoredWithWarning() {
		auto json = parseJson(R"({
			"States": [
				{ "Name": "s1", "Hold_Duration": "1", "Transition_Duration": "1",
				  "Overrides": { "pattern": "bsp" } }
			]
		})");
		TFPresetTimeline tl;
		Warnings w;
		w.attach(tl);
		TF_CHECK(tl.load(json, {}, {}));
		TF_CHECK(w.anyContains("pattern override is unsupported"));
	}

}

int main() {
	test_defaultInactive();
	test_missingStatesArraySafe();
	test_emptyStatesArraySafe();
	test_parsesAndPreservesOrder();
	test_duplicateStateNamesRejected();
	test_negativeDurationsClamped();
	test_sparseOverridesResolveAgainstBase();
	test_firstTransitionStartsFromCurrentRuntimeValue();
	test_floatInterpolationAtKeyProgress();
	test_intRoundingPolicy();
	test_boolSwitchPolicy();
	test_enumIntSwitchPolicy();
	test_easingFunctionsAtKeyPoints();
	test_holdThenAdvance();
	test_loopingBackToFirstState();
	test_nonLoopingStopsAtFinalState();
	test_pauseResumePreservesProgress();
	test_jumpToState();
	test_unknownParameterPathDoesNotCrash();
	test_outOfRangeValueClamped();
	test_loadingSecondPresetClearsFirst();
	test_timerRateChangeDoesNotExplodeRegenCount();
	test_nonAnimatedParametersRetainValue();
	test_flattenJsonToPaths();
	test_patternOverrideIgnoredWithWarning();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	if (g_failures > 0) {
		std::cout << g_failures << " FAILURE(S)\n";
		return 1;
	}
	std::cout << "ALL PASSED\n";
	return 0;
}
