#pragma once

#include "FakeScene.h"
#include "SceneContract.h"

// SceneManager — the smallest shell needed to own one FakeScene, per this
// increment's scope. Deliberately does NOT implement real multi-scene
// loading, asset discovery, production-scene factories, or completed
// visual transitions (Stage B of Scene-HUD-Contract-v1.md §14 — gated on
// unresolved "collision-resolution prerequisites") is out of scope here.
//
// Development Stream 1's two central disciplines this class exists to
// enforce, both by CONSTRUCTION (not just by convention):
//
//   1. capabilities() is queried exactly once per activation, cached, and
//      never re-queried until the next activation — see activateScene()
//      and activeCapabilities().
//   2. hudStatus() is queried exactly once per completed runtime frame,
//      immediately after update() — see captureSceneStatus() and
//      currentSceneStatus(). Nothing else in this class (or
//      ExperienceRuntime) is allowed to call the scene's hudStatus()
//      again for that frame; drawing must never trigger another pull.
//
// SceneManagerStatus stays valid throughout with the fake scene as the
// (only, always-resident) active scene and transitionPhase permanently
// Idle.
class SceneManager {
public:
	void setup(const SceneServices& services);

	// Activates the resident scene and caches its SceneCapabilities
	// exactly once (see class comment). Re-activation re-caches.
	void activateScene();

	void updateActiveScene(float dt);

	// Captures hudStatus() exactly once and stores it — call this exactly
	// once per runtime frame, immediately after updateActiveScene(), and
	// never again before the next frame's updateActiveScene(). Drawing
	// must happen after this, never trigger a second capture.
	void captureSceneStatus();

	// Architecture-Closure Session: the authoritative Shared Effects
	// snapshot, captured exactly once per runtime frame — a separate pull
	// from captureSceneStatus() (per the task's own instruction: "do not
	// derive canonical effect activity from SceneHudStatus::activeEffects").
	// Call this once per frame, same window as captureSceneStatus().
	void captureEffectActivityStatus();

	void drawActiveScene();
	void deactivateScene();
	void shutdown();

	// Scene-HUD-Contract-v1.md §9: "the scene's own executeCommand()
	// returning false is sufficient; no command queue in v1." Routed here
	// from InputRouter, separately from RuntimeCommand (see
	// ExperienceRuntime::handleRuntimeCommand).
	bool dispatchSceneCommand(SceneCommand command);

	// Scene-switch RuntimeCommands (NextScene/PreviousScene) ARE routed
	// through SceneManager — per the contract's draw-order note (§4:
	// "SceneManager routes any pending SceneCommands to the active
	// scene, handles any pending RuntimeCommands itself"). This
	// increment has exactly one resident scene, so both are no-ops that
	// log the request rather than perform a switch.
	void handleSceneSwitchCommand(RuntimeCommand command);

	// Returns the status captured by the most recent captureSceneStatus()
	// call. Before the first capture (or if no scene has ever been
	// activated), returns an explicit fallback status — never a
	// default-constructed/ambiguous SceneHudStatus.
	const SceneHudStatus& currentSceneStatus() const { return cachedStatus_; }

	// Returns the capabilities cached by the most recent activateScene()
	// call — NOT re-queried here.
	const SceneCapabilities& activeCapabilities() const { return cachedCapabilities_; }

	// Returns the value captured by the most recent
	// captureEffectActivityStatus() call. std::nullopt is a valid,
	// distinct captured value (see FakeScene::EffectActivityTestState::
	// NoSnapshot) — this is not a fallback default, it's whatever the
	// authoritative source last honestly reported.
	const std::optional<videoeffects::EffectActivityStatus>& currentEffectActivityStatus() const {
		return cachedEffectActivityStatus_;
	}

	SceneManagerStatus status() const;
	glm::ivec2 activeSceneNativeRenderSize() const;

	// Development/test-only access to the resident fake scene (health
	// override, semantic-variant selection, GL-contamination toggle,
	// direct reset() vs. SceneCommand::Reset exercising, lifecycle/poll
	// counters). A real SceneManager holding real scenes would not
	// expose this.
	FakeScene& devScene() { return fakeScene_; }
	const FakeScene& devScene() const { return fakeScene_; }

private:
	static SceneHudStatus fallbackStatus();

	FakeScene fakeScene_;
	bool didSetup_ = false;
	bool hasBeenActivated_ = false;

	SceneHudStatus cachedStatus_ = fallbackStatus();
	SceneCapabilities cachedCapabilities_{};
	std::optional<videoeffects::EffectActivityStatus> cachedEffectActivityStatus_;
};
