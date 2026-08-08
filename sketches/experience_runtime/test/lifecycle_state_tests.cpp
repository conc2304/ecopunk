// Standalone, dependency-free tests for FakeSceneLifecycleState — the
// pure lifecycle/counter state machine FakeScene.cpp delegates every
// setup/activate/deactivate/shutdown/command decision to (see
// ../src/FakeSceneLifecycleState.h's header comment). Deliberately does
// NOT link any part of openFrameworks or shared/src/scene/SceneContract.h
// — same rationale as
// sketches/blob-region-prototype/test/videoregion_math_tests.cpp and
// sketches/temporal-fields/test/tf_timeline_tests.cpp: even ofRectangle.h
// pulls in ofConstants.h -> GL/glew.h, and SceneContract.h's SceneFrame
// member pulls in ofTexture.h the same way.
//
// Covers the pure-logic portion of the implementation prompt's test list:
// setup->activate, deactivate->reactivate, deactivate->shutdown, shutdown
// without activation, unsupported-command rejection, accepted-command
// behavior, no command side effects after shutdown, resource
// allocation/release counts. (SceneFrame::frameNumber monotonicity,
// SceneManagerStatus in the idle case, and stable consecutive hudStatus()
// snapshots are OF-dependent — the real ExperienceRuntime/SceneManager/
// FakeScene types are exercised for those in the GL-restoration harness,
// see ../src/GlRestorationHarness.cpp and this increment's implementation
// report.)
//
// Build/run: make -C test -f Makefile.tests test

#include <iostream>
#include <sstream>
#include <string>

#include "../src/FakeSceneLifecycleState.h"

namespace {

int g_total = 0;
int g_failures = 0;

void reportFailure(const std::string& file, int line, const std::string& expr) {
	g_failures++;
	std::cerr << "FAIL " << file << ":" << line << ": " << expr << "\n";
}

} // namespace

#define LC_CHECK(cond) \
	do { \
		g_total++; \
		if (!(cond)) reportFailure(__FILE__, __LINE__, #cond); \
	} while (0)

static void test_setup_then_activate() {
	FakeSceneLifecycleState s;
	LC_CHECK(s.onSetup());
	LC_CHECK(s.didSetup);
	LC_CHECK(s.counters.setupCalls == 1);
	LC_CHECK(s.counters.allocCount == 1);
	LC_CHECK(!s.active);

	LC_CHECK(s.onActivate());
	LC_CHECK(s.active);
	LC_CHECK(s.counters.activateCalls == 1);
}

static void test_deactivate_then_reactivate() {
	FakeSceneLifecycleState s;
	s.onSetup();
	s.onActivate();

	LC_CHECK(s.onDeactivate());
	LC_CHECK(!s.active);
	LC_CHECK(s.counters.deactivateCalls == 1);
	// Contract §6: deactivate() must not imply resource destruction —
	// allocCount/releaseCount/didSetup must be untouched by it.
	LC_CHECK(s.counters.allocCount == 1);
	LC_CHECK(s.counters.releaseCount == 0);
	LC_CHECK(s.didSetup);
	LC_CHECK(!s.shutDown);

	LC_CHECK(s.onActivate());
	LC_CHECK(s.active);
	LC_CHECK(s.counters.activateCalls == 2);
}

static void test_deactivate_then_shutdown() {
	FakeSceneLifecycleState s;
	s.onSetup();
	s.onActivate();
	s.onDeactivate();

	LC_CHECK(s.onShutdown());
	LC_CHECK(s.shutDown);
	LC_CHECK(!s.active);
	LC_CHECK(s.counters.shutdownCalls == 1);
	LC_CHECK(s.counters.releaseCount == 1);
}

static void test_shutdown_without_activation() {
	FakeSceneLifecycleState s;
	s.onSetup();
	// Never activated.
	LC_CHECK(s.onShutdown());
	LC_CHECK(s.shutDown);
	LC_CHECK(s.counters.activateCalls == 0);
	LC_CHECK(s.counters.shutdownCalls == 1);
	LC_CHECK(s.counters.releaseCount == 1);
}

static void test_unsupported_command_rejection() {
	FakeSceneLifecycleState s;
	s.onSetup();
	s.onActivate();

	bool accepted = s.onExecuteCommand(/*isSupportedCommand=*/false, /*isSceneCommandReset=*/false);
	LC_CHECK(!accepted);
	LC_CHECK(s.counters.rejectedCommandCalls == 1);
	LC_CHECK(s.counters.acceptedCommandCalls == 0);
	// No side effect: resetEpoch must not move on a rejected command.
	LC_CHECK(s.resetEpoch == 0);
}

static void test_accepted_command_behavior() {
	FakeSceneLifecycleState s;
	s.onSetup();
	s.onActivate();

	bool accepted = s.onExecuteCommand(/*isSupportedCommand=*/true, /*isSceneCommandReset=*/true);
	LC_CHECK(accepted);
	LC_CHECK(s.counters.acceptedCommandCalls == 1);
	LC_CHECK(s.counters.rejectedCommandCalls == 0);
	LC_CHECK(s.counters.sceneCommandResetCalls == 1);
	LC_CHECK(s.resetEpoch == 1);

	// directResetCalls stays separate from sceneCommandResetCalls — the
	// deliberately-unresolved reset() vs. SceneCommand::Reset relationship
	// (see FakeScene.h/the implementation report).
	LC_CHECK(s.counters.directResetCalls == 0);
	LC_CHECK(s.onDirectReset());
	LC_CHECK(s.counters.directResetCalls == 1);
	LC_CHECK(s.counters.sceneCommandResetCalls == 1); // unchanged
	LC_CHECK(s.resetEpoch == 2); // both paths bump the same visible epoch
}

static void test_no_command_side_effects_after_shutdown() {
	FakeSceneLifecycleState s;
	s.onSetup();
	s.onActivate();
	s.onShutdown();

	int resetEpochBefore = s.resetEpoch;
	int acceptedBefore = s.counters.acceptedCommandCalls;
	int rejectedBefore = s.counters.rejectedCommandCalls;

	bool acceptedAfterShutdown = s.onExecuteCommand(/*isSupportedCommand=*/true, /*isSceneCommandReset=*/true);
	LC_CHECK(!acceptedAfterShutdown);
	LC_CHECK(s.counters.acceptedCommandCalls == acceptedBefore);
	LC_CHECK(s.counters.rejectedCommandCalls == rejectedBefore); // not even counted as rejected — counted separately
	LC_CHECK(s.resetEpoch == resetEpochBefore);
	LC_CHECK(s.counters.callsAfterShutdown >= 1);

	bool directResetAfterShutdown = s.onDirectReset();
	LC_CHECK(!directResetAfterShutdown);
	LC_CHECK(s.resetEpoch == resetEpochBefore);

	// Every other lifecycle call after shutdown must also be rejected and
	// counted, never silently no-op without a trace.
	int callsAfterShutdownBefore = s.counters.callsAfterShutdown;
	LC_CHECK(!s.onSetup());
	LC_CHECK(!s.onActivate());
	LC_CHECK(!s.onDeactivate());
	LC_CHECK(!s.onUpdate());
	LC_CHECK(!s.onDraw());
	LC_CHECK(!s.onShutdown());
	LC_CHECK(s.counters.callsAfterShutdown == callsAfterShutdownBefore + 6);
}

static void test_resource_allocation_release_counts() {
	FakeSceneLifecycleState s;
	LC_CHECK(s.counters.allocCount == 0);
	LC_CHECK(s.counters.releaseCount == 0);

	s.onSetup();
	LC_CHECK(s.counters.allocCount == 1);
	LC_CHECK(s.counters.releaseCount == 0);

	// Repeated activate/deactivate cycles must NOT change alloc/release
	// counts — only setup()/shutdown() own real resource lifetime.
	for (int i = 0; i < 5; ++i) {
		s.onActivate();
		s.onDeactivate();
	}
	LC_CHECK(s.counters.allocCount == 1);
	LC_CHECK(s.counters.releaseCount == 0);
	LC_CHECK(s.counters.activateCalls == 5);
	LC_CHECK(s.counters.deactivateCalls == 5);

	s.onShutdown();
	LC_CHECK(s.counters.allocCount == 1);
	LC_CHECK(s.counters.releaseCount == 1);

	// A second shutdown() must not double-release.
	s.onShutdown();
	LC_CHECK(s.counters.releaseCount == 1);
}

// Development Stream 1: the raw counter mechanics behind FakeScene's
// status-poll-count/capability-poll-count instrumentation (see
// FakeSceneLifecycleState.h). The actual POLICY ("hudStatus() queried
// exactly once per runtime frame", "capabilities() queried exactly once
// per activation") is enforced by SceneManager (OF-dependent — see
// GlRestorationHarness.cpp's corresponding checks, which pass). This test
// only proves the counters themselves count correctly and are not gated
// by shutDown (read-only queries, not lifecycle transitions — see
// FakeSceneLifecycleState.h's own comment on that choice).
static void test_status_and_capability_poll_counters() {
	FakeSceneLifecycleState s;
	LC_CHECK(s.counters.statusPollCount == 0);
	LC_CHECK(s.counters.capabilityPollCount == 0);

	s.onHudStatusPoll();
	s.onHudStatusPoll();
	s.onHudStatusPoll();
	LC_CHECK(s.counters.statusPollCount == 3);

	s.onCapabilitiesPoll();
	LC_CHECK(s.counters.capabilityPollCount == 1);

	s.onSetup();
	s.onActivate();
	s.onShutdown();

	// Deliberately NOT rejected/counted as callsAfterShutdown — these are
	// read-only queries, unlike the lifecycle-transition methods tested
	// in test_no_command_side_effects_after_shutdown() above.
	int callsAfterShutdownBefore = s.counters.callsAfterShutdown;
	s.onHudStatusPoll();
	s.onCapabilitiesPoll();
	LC_CHECK(s.counters.statusPollCount == 4);
	LC_CHECK(s.counters.capabilityPollCount == 2);
	LC_CHECK(s.counters.callsAfterShutdown == callsAfterShutdownBefore);
}

// Architecture-Closure Session: same raw-counter-mechanics test, for
// effectActivityPollCount — the ground truth behind "authoritative effect
// snapshot captured exactly once per runtime frame."
static void test_effect_activity_poll_counter() {
	FakeSceneLifecycleState s;
	LC_CHECK(s.counters.effectActivityPollCount == 0);
	s.onEffectActivityPoll();
	s.onEffectActivityPoll();
	LC_CHECK(s.counters.effectActivityPollCount == 2);

	s.onSetup();
	s.onShutdown();
	int callsAfterShutdownBefore = s.counters.callsAfterShutdown;
	s.onEffectActivityPoll(); // still not shutdown-gated — read-only query
	LC_CHECK(s.counters.effectActivityPollCount == 3);
	LC_CHECK(s.counters.callsAfterShutdown == callsAfterShutdownBefore);
}

int main() {
	test_setup_then_activate();
	test_deactivate_then_reactivate();
	test_deactivate_then_shutdown();
	test_shutdown_without_activation();
	test_unsupported_command_rejection();
	test_accepted_command_behavior();
	test_no_command_side_effects_after_shutdown();
	test_resource_allocation_release_counts();
	test_status_and_capability_poll_counters();
	test_effect_activity_poll_counter();

	std::cout << (g_total - g_failures) << "/" << g_total << " checks passed\n";
	return g_failures == 0 ? 0 : 1;
}
