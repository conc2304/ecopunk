#pragma once

#include "FakeScene.h"
#include "SceneContract.h"
#include "SceneSwitchController.h"

#include <functional>
#include <string>
#include <vector>

// SceneManager — owns the production scene registry, active/pending scene
// ownership, and the static-frame scene transition model
// (Scene-HUD-Contract-v1.md §6/§10).
//
// RT-003 (Architecture-authorized for the Blob + Temporal pair ONLY): the
// former single non-owning productionScene_ slot is replaced by a small,
// ordered registry of real production IEcopunkScene entries (ExperienceRuntime
// registers exactly BlobProductionScene then TemporalProductionScene) plus
// in-process NextScene/PreviousScene switching. Every switch/ownership
// decision lives in sceneswitch::Controller (SceneSwitchController.h, OF-free
// and unit-tested); this class performs the real IEcopunkScene calls and the
// cached status/capability/effect publication for each phase.
//
// Disciplines this class enforces by construction:
//
//   1. capabilities() is queried exactly once per successful activation,
//      cached, and never re-queried until the next activation. During a
//      transition (FadingOut / Loading / Failed) no capabilities are
//      published, so no control can reach a deactivated or not-yet-active
//      scene; Blob and Temporal capability sets are never merged.
//   2. hudStatus() is queried at most once per completed runtime frame,
//      immediately after update(), and only on frames where the active owner
//      runs live (beginFrame() returned true). Transition-only static frames
//      do not pull status at all — they publish either the frozen outgoing
//      snapshot (FadingOut) or a neutral transition status (Loading/Failed).
//   3. The scene output FBO is never touched here — ExperienceRuntime owns
//      it, including reallocation when an incoming scene's native size
//      differs, and retains the outgoing scene's last rendered frame in it
//      during non-live frames.
//
// With no production scenes registered (the GlRestorationHarness path), the
// tooling-only FakeScene remains the one resident scene, exactly as before
// RT-003; it is never a production registry entry and never a production
// effect source.
class SceneManager {
public:
	using EffectActivitySource = std::function<std::optional<videoeffects::EffectActivityStatus>()>;

	// Lifecycle/publication counters per registered scene — test/evidence
	// instrumentation only (RT-003 report), never read by production logic.
	struct EntryCounters {
		int setupCalls = 0;
		int activateCalls = 0;
		int successfulActivations = 0;
		int deactivateCalls = 0;
		int shutdownCalls = 0;
		int capabilityQueries = 0;
		int statusPulls = 0;
		int effectPulls = 0;
		int updateCalls = 0;
		int drawCalls = 0;
	};

	// -- Registry (call before setup()) -------------------------------------

	// Appends a real production scene to the registry. Registration order is
	// the NextScene ring order. effectSource is the scene's canonical
	// EffectActivityStatus owner (DEC-015); an empty function means the scene
	// has no approved canonical producer and its frames publish
	// HudFrameData.effects = std::nullopt. Non-owning.
	void registerProductionScene(IEcopunkScene* scene, EffectActivitySource effectSource = EffectActivitySource());

	// Selects the startup scene by its own stable sceneId(). Defaults to the
	// first registered entry. Returns false (startup unchanged) if no entry
	// has that ID.
	bool setStartupScene(const std::string& sceneId);

	// -- Startup / per-frame --------------------------------------------------

	// Sets up the startup scene only; every other registered scene is set up
	// lazily, at most once, the first time it is switched to.
	void setup(const SceneServices& services);

	// Activates the current owner of record and caches its capabilities
	// exactly once. Used for startup and by the lifecycle harnesses
	// (deactivate/reactivate of the current scene). Rejected (logged no-op)
	// while a transition is in progress.
	void activateScene();

	// Advances any in-progress transition by one frame and performs that
	// frame's lifecycle calls. Call exactly once at the start of every
	// runtime frame. Returns true when the active scene runs live this frame
	// (update + one status pull + draw); false on transition-only static
	// frames, when the caller must not update/draw any scene.
	bool beginFrame();

	// True if the frame most recently planned by beginFrame() is a live
	// active-scene frame (always true on the FakeScene-only tooling path).
	bool activeSceneLiveThisFrame() const { return lastPlanLive_; }

	// True exactly on the frame an incoming scene's activation succeeded —
	// ExperienceRuntime uses this to resize the runtime-owned scene FBO.
	bool activationSucceededThisFrame() const { return lastActivationSucceeded_; }

	void updateActiveScene(float dt);

	// Captures hudStatus() exactly once — call once per runtime frame,
	// immediately after updateActiveScene(), only on live frames. A call on a
	// non-live frame is a no-op (the frozen/neutral status stays published).
	void captureSceneStatus();

	// The authoritative Shared Effects snapshot (DEC-015), captured once per
	// runtime frame — a separate pull from captureSceneStatus(), never derived
	// from SceneHudStatus::activeEffects.
	//
	// Priority (highest first):
	//   1. effectActivitySourceOverride_ — test-only.
	//   2. Registered production scenes: the owner's own effect source on live
	//      frames (std::nullopt when the owner has none — Blob); the frozen
	//      outgoing value during FadingOut; std::nullopt during Loading/Failed.
	//   3. No production scenes registered (tooling only): FakeScene's value.
	void captureEffectActivityStatus();

	// Test-only. Installs (or, with an empty function, removes) an override
	// source for captureEffectActivityStatus(). Never used by production code.
	void setEffectActivitySourceOverrideForTesting(EffectActivitySource source) {
		effectActivitySourceOverride_ = std::move(source);
	}

	void drawActiveScene();
	void deactivateScene();

	// Final teardown: shuts down every registered scene that was ever set up
	// (each exactly once), or FakeScene on the tooling path.
	void shutdown();

	// Scene-HUD-Contract-v1.md §9: "the scene's own executeCommand() returning
	// false is sufficient". Rejected (false) whenever there is no live owner —
	// during FadingOut, Loading, Failed, or after shutdown().
	bool dispatchSceneCommand(SceneCommand command);

	// NextScene/PreviousScene (RuntimeCommand). Only accepted while Idle with
	// a registered pair; requests during any non-Idle phase (including
	// Failed) are rejected and never queued. Returns the controller's result.
	sceneswitch::RequestResult handleSceneSwitchCommand(RuntimeCommand command);

	const SceneHudStatus& currentSceneStatus() const { return cachedStatus_; }
	const SceneCapabilities& activeCapabilities() const { return cachedCapabilities_; }
	const std::optional<videoeffects::EffectActivityStatus>& currentEffectActivityStatus() const {
		return cachedEffectActivityStatus_;
	}

	SceneManagerStatus status() const;
	glm::ivec2 activeSceneNativeRenderSize() const;

	// -- Development/test-only access --------------------------------------

	FakeScene& devScene() { return fakeScene_; }
	const FakeScene& devScene() const { return fakeScene_; }

	bool hasProductionScenes() const { return !entries_.empty(); }
	std::vector<std::string> registeredSceneIdsForTesting() const;
	const EntryCounters* countersForTesting(const std::string& sceneId) const;
	const sceneswitch::Controller& switchControllerForTesting() const { return controller_; }
	int transitionStatusPublicationsForTesting() const { return neutralStatusPublications_; }

	// Transition timing (frames). Internal runtime constants, not
	// product-facing tuning — see SceneSwitchController.h.
	static constexpr int kFadeOutFrames = 12;
	static constexpr int kFadeInFrames = 12;

private:
	struct Entry {
		IEcopunkScene* scene = nullptr;
		EffectActivitySource effectSource;
		bool lifecycleActive = false; // activate() called more recently than deactivate()
		bool didShutdown = false;
		EntryCounters counters;
	};

	// sceneswitch::LifecycleTarget implementation — the real IEcopunkScene
	// calls the controller sequences. Private nested adapter so the target
	// methods are not part of SceneManager's own public surface.
	class Target : public sceneswitch::LifecycleTarget {
	public:
		explicit Target(SceneManager& owner) : owner_(owner) {}
		void setupEntry(int index) override { owner_.setupEntry(index); }
		void activateEntry(int index) override { owner_.activateEntry(index); }
		void deactivateEntry(int index) override { owner_.deactivateEntry(index); }
	private:
		SceneManager& owner_;
	};

	static SceneHudStatus fallbackStatus();
	static SceneHudStatus transitionStatus(const std::string& message);

	void setupEntry(int index);
	void activateEntry(int index);
	void deactivateEntry(int index);
	void queryAndCacheCapabilities(int index);

	Entry* ownerEntry();
	const Entry* ownerEntry() const;
	std::string sceneIdAt(int index) const;

	FakeScene fakeScene_;
	std::vector<Entry> entries_;
	int startupIndex_ = 0;
	sceneswitch::Controller controller_;
	Target target_{*this};
	SceneServices baseServices_{};

	bool didSetup_ = false;
	bool hasBeenActivated_ = false;
	bool didShutdown_ = false;
	bool lastPlanLive_ = true;
	bool lastActivationSucceeded_ = false;
	sceneswitch::Publication lastPublication_ = sceneswitch::Publication::Live;
	int neutralStatusPublications_ = 0;
	std::optional<std::string> managerMessage_;

	SceneHudStatus cachedStatus_ = fallbackStatus();
	SceneCapabilities cachedCapabilities_{};
	std::optional<videoeffects::EffectActivityStatus> cachedEffectActivityStatus_;
	EffectActivitySource effectActivitySourceOverride_;
};
