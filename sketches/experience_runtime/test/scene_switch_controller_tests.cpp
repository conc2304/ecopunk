// RT-003 — standalone, dependency-free tests for sceneswitch::Controller
// (../src/SceneSwitchController.h), the exact production state machine
// SceneManager delegates every Blob <-> Temporal switch decision to. Same
// bare-compiler convention as lifecycle_state_tests.cpp: no openFrameworks,
// no GL, no SceneContract.h.
//
// A RecordingTarget stands in for SceneManager's real IEcopunkScene calls,
// recording every setup/activate/deactivate in order and optionally throwing
// to exercise the failure path. Index 0 = Blob, index 1 = Temporal, matching
// ExperienceRuntime's registration order.
//
// Build/run: make -C test -f Makefile.tests test

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/SceneSwitchController.h"

using namespace sceneswitch;

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

struct RecordingTarget : LifecycleTarget {
	std::vector<std::string> events;
	int throwOnSetupIndex = -1;
	int throwOnActivateIndex = -1;
	int setupCount[2] = {0, 0};
	int activateCount[2] = {0, 0};
	int deactivateCount[2] = {0, 0};
	bool active[2] = {false, false};

	void setupEntry(int i) override {
		events.push_back("setup:" + std::to_string(i));
		setupCount[i]++;
		if (i == throwOnSetupIndex) throw std::runtime_error("injected setup failure");
	}
	void activateEntry(int i) override {
		events.push_back("activate:" + std::to_string(i));
		activateCount[i]++;
		if (i == throwOnActivateIndex) throw std::runtime_error("injected activation failure");
		active[i] = true;
	}
	void deactivateEntry(int i) override {
		events.push_back("deactivate:" + std::to_string(i));
		deactivateCount[i]++;
		active[i] = false;
	}
};

// Mirrors SceneManager::setup() + activateScene() for the startup scene.
Controller startedController(RecordingTarget& t, int startup = 0, Config cfg = Config{3, 2}) {
	Controller c;
	c.configure(2, cfg);
	c.setOwner(startup);
	t.setupEntry(startup);
	c.markSetupDone(startup);
	t.activateEntry(startup);
	t.events.clear();
	return c;
}

// Runs frames until Idle (or a frame limit), returning the plans seen.
std::vector<FramePlan> runUntilIdle(Controller& c, RecordingTarget& t, int limit = 100) {
	std::vector<FramePlan> plans;
	for (int i = 0; i < limit; ++i) {
		plans.push_back(c.beginFrame(t));
		if (c.phase() == Phase::Idle) break;
		if (c.phase() == Phase::Failed) break;
	}
	return plans;
}

} // namespace

#define SW_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

static void test_ring_traversal_two_scenes() {
	RecordingTarget t;
	Controller c = startedController(t, 0);
	SW_CHECK(c.targetIndexFor(Direction::Next) == 1);     // Blob NextScene -> Temporal
	SW_CHECK(c.targetIndexFor(Direction::Previous) == 1); // Blob PreviousScene -> Temporal
	c.setOwner(1);
	SW_CHECK(c.targetIndexFor(Direction::Next) == 0);     // Temporal NextScene -> Blob
	SW_CHECK(c.targetIndexFor(Direction::Previous) == 0); // Temporal PreviousScene -> Blob
}

static void test_registry_size_guards() {
	RecordingTarget t;
	Controller one;
	one.configure(1, Config{});
	one.setOwner(0);
	SW_CHECK(one.requestSwitch(Direction::Next, t) == RequestResult::RejectedNoOtherScene);
	SW_CHECK(one.phase() == Phase::Idle);

	Controller none;
	none.configure(0, Config{});
	SW_CHECK(none.requestSwitch(Direction::Next, t) == RequestResult::RejectedNoOtherScene);

	Controller noOwner;
	noOwner.configure(2, Config{});
	SW_CHECK(noOwner.requestSwitch(Direction::Next, t) == RequestResult::RejectedNoOwner);
	SW_CHECK(t.events.empty());
}

static void test_full_switch_phase_sequence_and_ordering() {
	RecordingTarget t;
	Controller c = startedController(t, 0, Config{3, 2});

	SW_CHECK(c.requestSwitch(Direction::Next, t) == RequestResult::Accepted);
	// Outgoing deactivated immediately at acceptance; owner of record still outgoing.
	SW_CHECK(t.events.size() == 1 && t.events[0] == "deactivate:0");
	SW_CHECK(c.phase() == Phase::FadingOut);
	SW_CHECK(c.ownerIndex() == 0);
	SW_CHECK(c.pendingIndex() == 1);
	SW_CHECK(c.pendingIndex() != c.ownerIndex());

	// 3 FadingOut frames: static, frozen outgoing, no lifecycle calls.
	for (int i = 1; i <= 3; ++i) {
		FramePlan p = c.beginFrame(t);
		SW_CHECK(c.phase() == Phase::FadingOut);
		SW_CHECK(!p.runActiveScene);
		SW_CHECK(p.publication == Publication::FrozenOutgoing);
		SW_CHECK(c.progress() > 0.0f && c.progress() <= 1.0f);
	}
	SW_CHECK(t.events.size() == 1);

	// Loading frame: ownership released, incoming set up (first time).
	FramePlan loading = c.beginFrame(t);
	SW_CHECK(c.phase() == Phase::Loading);
	SW_CHECK(loading.ownershipReleasedThisFrame);
	SW_CHECK(loading.publication == Publication::Neutral);
	SW_CHECK(!loading.runActiveScene);
	SW_CHECK(c.ownerIndex() == -1);
	SW_CHECK(c.pendingIndex() == 1);
	SW_CHECK(t.events.size() == 2 && t.events[1] == "setup:1");

	// Activation frame: incoming becomes owner, FadingIn, live, caps now.
	FramePlan act = c.beginFrame(t);
	SW_CHECK(c.phase() == Phase::FadingIn);
	SW_CHECK(act.activationSucceededThisFrame);
	SW_CHECK(act.runActiveScene);
	SW_CHECK(act.publication == Publication::Live);
	SW_CHECK(c.ownerIndex() == 1);
	SW_CHECK(c.pendingIndex() == -1);
	SW_CHECK(t.events.size() == 3 && t.events[2] == "activate:1");

	// Ordering proof: deactivate(outgoing) < setup(incoming) < activate(incoming).
	SW_CHECK(t.events[0] == "deactivate:0" && t.events[1] == "setup:1" && t.events[2] == "activate:1");

	// FadingIn continues live, then Idle; activation flagged only once.
	int activationFlags = 0;
	while (c.phase() != Phase::Idle) {
		FramePlan p = c.beginFrame(t);
		SW_CHECK(p.runActiveScene);
		SW_CHECK(p.publication == Publication::Live);
		if (p.activationSucceededThisFrame) activationFlags++;
	}
	SW_CHECK(activationFlags == 0);
	SW_CHECK(c.ownerIndex() == 1);
	SW_CHECK(c.progress() == 0.0f);
	SW_CHECK(t.events.size() == 3); // no stray lifecycle calls
}

static void test_switch_suppressed_in_every_non_idle_phase() {
	RecordingTarget t;
	Controller c = startedController(t, 0, Config{2, 2});
	SW_CHECK(c.requestSwitch(Direction::Next, t) == RequestResult::Accepted);
	size_t eventsAfterAccept = t.events.size();

	// FadingOut
	SW_CHECK(c.requestSwitch(Direction::Next, t) == RequestResult::RejectedTransitionActive);
	SW_CHECK(c.requestSwitch(Direction::Previous, t) == RequestResult::RejectedTransitionActive);
	SW_CHECK(c.pendingIndex() == 1);
	c.beginFrame(t);
	c.beginFrame(t);
	c.beginFrame(t); // -> Loading
	SW_CHECK(c.phase() == Phase::Loading);
	SW_CHECK(c.requestSwitch(Direction::Next, t) == RequestResult::RejectedTransitionActive);
	c.beginFrame(t); // -> FadingIn
	SW_CHECK(c.phase() == Phase::FadingIn);
	SW_CHECK(c.requestSwitch(Direction::Previous, t) == RequestResult::RejectedTransitionActive);
	runUntilIdle(c, t);
	SW_CHECK(c.phase() == Phase::Idle);
	SW_CHECK(c.ownerIndex() == 1); // exactly one switch happened, nothing queued
	// Only the one switch's lifecycle calls: deactivate:0, setup:1, activate:1.
	SW_CHECK(t.events.size() == eventsAfterAccept + 2);
	// And no second transition starts on its own.
	for (int i = 0; i < 10; ++i) c.beginFrame(t);
	SW_CHECK(c.phase() == Phase::Idle);
	SW_CHECK(c.ownerIndex() == 1);
}

static void test_three_round_trips_setup_once_and_counts() {
	RecordingTarget t;
	Controller c = startedController(t, 0, Config{2, 2});
	// startup already did setup:0 + activate:0 (counted in t before clear)
	for (int cycle = 0; cycle < 3; ++cycle) {
		SW_CHECK(c.requestSwitch(Direction::Next, t) == RequestResult::Accepted); // Blob -> Temporal
		runUntilIdle(c, t);
		SW_CHECK(c.ownerIndex() == 1);
		SW_CHECK(t.active[1] && !t.active[0]); // exactly one scene lifecycle-active
		SW_CHECK(c.requestSwitch(Direction::Previous, t) == RequestResult::Accepted); // Temporal -> Blob
		runUntilIdle(c, t);
		SW_CHECK(c.ownerIndex() == 0);
		SW_CHECK(t.active[0] && !t.active[1]);
	}
	SW_CHECK(t.setupCount[0] == 1); // startup only — never re-set-up
	SW_CHECK(t.setupCount[1] == 1); // lazily, once, on first switch
	SW_CHECK(t.activateCount[0] == 1 + 3);
	SW_CHECK(t.activateCount[1] == 3);
	SW_CHECK(t.deactivateCount[0] == 3);
	SW_CHECK(t.deactivateCount[1] == 3);
}

static void test_activation_failure() {
	RecordingTarget t;
	t.throwOnActivateIndex = 1;
	Controller c = startedController(t, 0, Config{1, 1});
	SW_CHECK(c.requestSwitch(Direction::Next, t) == RequestResult::Accepted);
	c.beginFrame(t); // FadingOut 1
	c.beginFrame(t); // Loading (setup ok)
	FramePlan p = c.beginFrame(t); // activation throws
	SW_CHECK(p.failedThisFrame);
	SW_CHECK(c.phase() == Phase::Failed);
	SW_CHECK(c.ownerIndex() == -1);   // partially activated incoming NOT published as owner
	SW_CHECK(c.pendingIndex() == 1);  // records which scene failed
	SW_CHECK(!p.runActiveScene);
	SW_CHECK(p.publication == Publication::Neutral);
	SW_CHECK(!p.activationSucceededThisFrame);
	SW_CHECK(t.events.back() == "deactivate:1"); // best-effort cleanup of the partial activation
	SW_CHECK(!c.failureMessage().empty());
	// Suppression persists while failure is unresolved.
	SW_CHECK(c.requestSwitch(Direction::Next, t) == RequestResult::RejectedTransitionActive);
	SW_CHECK(c.requestSwitch(Direction::Previous, t) == RequestResult::RejectedTransitionActive);
	for (int i = 0; i < 5; ++i) {
		FramePlan q = c.beginFrame(t);
		SW_CHECK(q.publication == Publication::Neutral && !q.runActiveScene);
	}
	SW_CHECK(c.phase() == Phase::Failed);
}

static void test_setup_failure() {
	RecordingTarget t;
	t.throwOnSetupIndex = 1;
	Controller c = startedController(t, 0, Config{1, 1});
	SW_CHECK(c.requestSwitch(Direction::Next, t) == RequestResult::Accepted);
	c.beginFrame(t); // FadingOut
	FramePlan p = c.beginFrame(t); // Loading: setup throws
	SW_CHECK(p.failedThisFrame);
	SW_CHECK(c.phase() == Phase::Failed);
	SW_CHECK(!c.didSetup(1));
	SW_CHECK(c.ownerIndex() == -1);
	SW_CHECK(t.activateCount[1] == 0); // never activated after a failed setup
}

static void test_idle_plan_is_live_and_startup_deterministic() {
	RecordingTarget t;
	Controller c = startedController(t, 0);
	for (int i = 0; i < 5; ++i) {
		FramePlan p = c.beginFrame(t);
		SW_CHECK(p.runActiveScene && p.publication == Publication::Live);
		SW_CHECK(!p.activationSucceededThisFrame && !p.ownershipReleasedThisFrame);
	}
	SW_CHECK(c.ownerIndex() == 0);
	SW_CHECK(c.didSetup(0) && !c.didSetup(1));
	SW_CHECK(t.events.empty());

	RecordingTarget t2;
	Controller c2 = startedController(t2, 1); // startup in Temporal
	SW_CHECK(c2.ownerIndex() == 1);
	SW_CHECK(c2.targetIndexFor(Direction::Next) == 0);
}

int main() {
	test_ring_traversal_two_scenes();
	test_registry_size_guards();
	test_full_switch_phase_sequence_and_ordering();
	test_switch_suppressed_in_every_non_idle_phase();
	test_three_round_trips_setup_once_and_counts();
	test_activation_failure();
	test_setup_failure();
	test_idle_plan_is_live_and_startup_deterministic();

	if (g_failures == 0) {
		std::cout << "PASS — " << g_total << " checks, 0 failures\n";
		return 0;
	}
	std::cout << "FAIL — " << g_failures << " of " << g_total << " checks failed\n";
	return 1;
}
