#pragma once

#include "FakeScene.h"
#include "SceneContract.h"

#include <functional>

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
	//
	// Priority (highest first):
	//   1. effectActivitySourceOverride_ — test-only (see
	//      setEffectActivitySourceOverrideForTesting() below).
	//   2. productionEffectActivitySource_ — the GENERIC, real, non-test
	//      seam installed alongside whichever production scene is
	//      currently installed (see installProductionScene() above and
	//      Temporal Production Scene #2 Migration's own notes there).
	//   3. fakeScene_.currentEffectActivityStatus() — the pre-existing
	//      fallback, unchanged, for any scene (including Blob, and
	//      fakeScene_ itself) with no real effect-activity owner installed.
	//
	// Final Shared Effects Source-of-Truth Seam Proof session's original
	// note still applies to priority 1: it proves the exact same
	// forwarding code path (this method, the cached member, and
	// ExperienceRuntime::draw()'s `currentHudFrameData_.effects =
	// sceneManager_.currentEffectActivityStatus();` assignment) carries a
	// genuinely different, real production owner's authored value
	// unchanged. Priority 2 is the real production answer this migration
	// adds on top of that proof — see installProductionScene()'s comment
	// for why it is a separate mechanism from the test override, not a
	// promotion of it.
	void captureEffectActivityStatus();

	using EffectActivitySource = std::function<std::optional<videoeffects::EffectActivityStatus>()>;

	// Test-only. Installs (or, with an empty std::function, removes) an
	// override source for captureEffectActivityStatus() above. Never used
	// by production code — see ExperienceRuntime, which never calls this.
	void setEffectActivitySourceOverrideForTesting(EffectActivitySource source) {
		effectActivitySourceOverride_ = std::move(source);
	}

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

	// Blob First Complete Production Migration: installs a real,
	// non-owning production scene as the scene every method below
	// resolves to (see activeScene()), instead of fakeScene_. Must be
	// called before setup()/activateScene() so the installed scene (not
	// fakeScene_) receives setup()/activate() and has its capabilities
	// cached. Passing nullptr (the default — see productionScene_'s own
	// comment) restores exactly today's fakeScene_-only behavior; every
	// existing test/harness path that never calls this is therefore
	// byte-for-byte unaffected. This is additive scaffolding for hosting
	// a second concrete IEcopunkScene, not a general multi-scene registry
	// — see Scene-HUD-Contract-v1.md §14's Stage B note, still out of
	// scope here.
	//
	// Temporal Production Scene #2 Migration adds effectSource (optional,
	// defaults to empty): the GENERIC, non-test, non-scene-ID-branching way
	// for whichever real production scene is installed to supply its own
	// canonical EffectActivityStatus (see captureEffectActivityStatus()
	// below) — installed alongside the scene itself by whichever caller
	// installs it (see ExperienceRuntime::installTemporalProductionScene()),
	// not hardcoded to any one scene type here. Passing an empty
	// std::function (the default — matches BlobProductionScene's own call
	// site, which passes none) leaves captureEffectActivityStatus()'s
	// existing fakeScene_ fallback completely unchanged for any scene with
	// no real effect-activity owner of its own (Blob's own documented gap
	// — see BlobProductionScene.h). This is deliberately NOT the same
	// mechanism as setEffectActivitySourceOverrideForTesting() below (that
	// one remains test-only and, per its own comment, still takes priority
	// over this one if a test has additionally installed it) — this is the
	// real, generic, always-available production seam this session's own
	// instruction required in place of promoting that test hook.
	void installProductionScene(IEcopunkScene* scene, EffectActivitySource effectSource = EffectActivitySource()) {
		productionScene_ = scene;
		productionEffectActivitySource_ = std::move(effectSource);
	}

private:
	static SceneHudStatus fallbackStatus();

	// The one scene every method below actually drives — productionScene_
	// when installed (see installProductionScene()), fakeScene_ otherwise.
	// fakeScene_ itself is never removed or repurposed: GlRestorationHarness
	// and devScene() both still address it directly and are unaffected by
	// whether a production scene is also installed.
	IEcopunkScene& activeScene() {
		return productionScene_ != nullptr ? *productionScene_ : static_cast<IEcopunkScene&>(fakeScene_);
	}
	const IEcopunkScene& activeScene() const {
		return productionScene_ != nullptr ? *productionScene_ : static_cast<const IEcopunkScene&>(fakeScene_);
	}

	FakeScene fakeScene_;
	IEcopunkScene* productionScene_ = nullptr; // not owned; nullptr = fakeScene_-only behavior (today's default)
	bool didSetup_ = false;
	bool hasBeenActivated_ = false;

	SceneHudStatus cachedStatus_ = fallbackStatus();
	SceneCapabilities cachedCapabilities_{};
	std::optional<videoeffects::EffectActivityStatus> cachedEffectActivityStatus_;
	EffectActivitySource effectActivitySourceOverride_;

	// Temporal Production Scene #2 Migration — see installProductionScene()'s
	// own comment. Empty whenever the installed production scene (or no
	// scene at all) has no real effect-activity owner of its own.
	EffectActivitySource productionEffectActivitySource_;
};
