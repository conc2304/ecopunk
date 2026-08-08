#pragma once

#include <cstdint>

// FakeSceneLifecycleState — the pure, OpenFrameworks/GL-independent
// lifecycle and counter state machine extracted out of FakeScene, so its
// rules can be bare-compiler tested the same way
// sketches/blob-region-prototype/test/videoregion_math_tests.cpp and
// sketches/temporal-fields/test/tf_timeline_tests.cpp test their own
// OF-independent logic — per those files' own header comments, even
// ofRectangle.h pulls in ofConstants.h -> GL/glew.h, so anything meant to
// be bare-compiler-testable has to avoid the OF include chain entirely,
// including shared/src/scene/SceneContract.h (its SceneFrame member drags
// in ofTexture.h).
//
// FakeScene.cpp owns exactly one instance of this and defers every
// lifecycle/counter decision to it — this header is the single source of
// truth for that logic, not a parallel/duplicated reimplementation.
//
// Deliberately takes plain bool/int parameters rather than the real
// SceneCommand/SceneHealth enums, specifically to stay free of any
// SceneContract.h dependency — see above.
struct FakeSceneLifecycleState {
	struct Counters {
		int setupCalls = 0;
		int activateCalls = 0;
		int deactivateCalls = 0;
		int shutdownCalls = 0;

		int directResetCalls = 0;
		int sceneCommandResetCalls = 0;

		int acceptedCommandCalls = 0;
		int rejectedCommandCalls = 0;

		int allocCount = 0;
		int releaseCount = 0;

		uint64_t updateCalls = 0;
		uint64_t drawCalls = 0;

		int callsAfterShutdown = 0;

		// Development Stream 1: raw ground-truth counts of how many times
		// THIS scene's own hudStatus()/capabilities() were invoked —
		// independent of SceneManager's own caching discipline, so a test
		// can verify SceneManager's "status once per frame" / "capabilities
		// once per activation" claims against the scene's own count, not
		// just against SceneManager's internal bookkeeping. Deliberately
		// NOT gated by shutDown — these are read-only queries, not
		// lifecycle transitions, and a manager may reasonably poll final
		// status after teardown for logging.
		int statusPollCount = 0;
		int capabilityPollCount = 0;

		// Architecture-Closure Session: ground truth for "authoritative
		// effect snapshot captured exactly once per runtime frame" — same
		// reasoning as statusPollCount, a separate pull from hudStatus().
		int effectActivityPollCount = 0;
	};

	void onHudStatusPoll() { counters.statusPollCount++; }
	void onCapabilitiesPoll() { counters.capabilityPollCount++; }
	void onEffectActivityPoll() { counters.effectActivityPollCount++; }

	Counters counters;
	bool didSetup = false;
	bool active = false;
	bool shutDown = false;

	// resetEpoch mirrors FakeScene's own field of the same name — bumped
	// by both onDirectReset() and an accepted onExecuteCommand() reset,
	// and used to prove "hudStatus() is stable across two calls with no
	// intervening command" without needing the real SceneHudStatus type.
	int resetEpoch = 0;

	bool onSetup() {
		counters.setupCalls++;
		if (shutDown) {
			counters.callsAfterShutdown++;
			return false;
		}
		didSetup = true;
		counters.allocCount++;
		return true;
	}

	bool onActivate() {
		if (shutDown) {
			counters.callsAfterShutdown++;
			return false;
		}
		counters.activateCalls++;
		active = true;
		return true;
	}

	bool onDeactivate() {
		if (shutDown) {
			counters.callsAfterShutdown++;
			return false;
		}
		counters.deactivateCalls++;
		active = false;
		// Deliberately does not touch allocCount/releaseCount, didSetup,
		// or resetEpoch — per Scene-HUD-Contract-v1.md §6, deactivate()
		// must not imply resource destruction and must leave the scene
		// valid for a later activate().
		return true;
	}

	bool onUpdate() {
		if (shutDown) {
			counters.callsAfterShutdown++;
			return false;
		}
		counters.updateCalls++;
		return true;
	}

	bool onDraw() {
		if (shutDown) {
			counters.callsAfterShutdown++;
			return false;
		}
		counters.drawCalls++;
		return true;
	}

	// isSupportedCommand: FakeScene decides which SceneCommand values it
	// accepts (only SceneCommand::Reset in this increment) and passes
	// that decision in — this class only owns the counting/rejection
	// rule, not command identity.
	bool onExecuteCommand(bool isSupportedCommand, bool isSceneCommandReset) {
		if (shutDown) {
			counters.callsAfterShutdown++;
			return false;
		}
		if (!isSupportedCommand) {
			counters.rejectedCommandCalls++;
			return false;
		}
		counters.acceptedCommandCalls++;
		if (isSceneCommandReset) {
			counters.sceneCommandResetCalls++;
			resetEpoch++;
		}
		return true;
	}

	bool onDirectReset() {
		if (shutDown) {
			counters.callsAfterShutdown++;
			return false;
		}
		counters.directResetCalls++;
		resetEpoch++;
		return true;
	}

	bool onShutdown() {
		if (shutDown) {
			counters.callsAfterShutdown++;
			return false;
		}
		counters.shutdownCalls++;
		counters.releaseCount++;
		shutDown = true;
		active = false;
		return true;
	}
};
