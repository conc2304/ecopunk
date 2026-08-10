#include "SceneManager.h"

#include "ofLog.h"

SceneHudStatus SceneManager::fallbackStatus() {
	SceneHudStatus s;
	s.schemaVersion = 1;
	s.sceneId = "";
	s.displayName = "(no active scene)";
	s.health = SceneHealth::Loading;
	s.message = "SceneManager: no scene has been activated yet";
	return s;
}

void SceneManager::setup(const SceneServices& services) {
	activeScene().setup(services);
	didSetup_ = true;
}

void SceneManager::activateScene() {
	activeScene().activate();
	// Cache capabilities exactly once, here, on activation — never
	// re-queried per frame. See class header comment, discipline #1.
	cachedCapabilities_ = activeScene().capabilities();
	hasBeenActivated_ = true;
}

void SceneManager::updateActiveScene(float dt) {
	activeScene().update(dt);
}

void SceneManager::captureSceneStatus() {
	// The single, atomic, once-per-frame status pull. See class header
	// comment, discipline #2. Must be called exactly once per completed
	// runtime frame and never again before the next update().
	cachedStatus_ = activeScene().hudStatus();
}

void SceneManager::captureEffectActivityStatus() {
	// Separate pull from captureSceneStatus() above — the task's own
	// instruction: canonical effect activity must not be derived from
	// SceneHudStatus::activeEffects.
	//
	// Final Shared Effects Source-of-Truth Seam Proof session: if a test
	// has installed an override source (see
	// setEffectActivitySourceOverrideForTesting() in the header), that
	// source is the authoritative pull instead of fakeScene_ — this is
	// the exact seam that lets a real production owner (TFEffectPicker)
	// stand in for FakeScene without duplicating or touching any of the
	// surrounding forwarding code below/in ExperienceRuntime::draw().
	if (effectActivitySourceOverride_) {
		cachedEffectActivityStatus_ = effectActivitySourceOverride_();
		return;
	}
	// Temporal Production Scene #2 Migration: the generic production seam
	// — see installProductionScene()'s own comment. Only consulted when a
	// production scene is actually installed AND that scene supplied a
	// real source (Blob's own call site passes none, so this stays empty
	// and behavior for Blob is byte-for-byte unchanged from before this
	// migration).
	if (productionScene_ != nullptr && productionEffectActivitySource_) {
		cachedEffectActivityStatus_ = productionEffectActivitySource_();
		return;
	}
	cachedEffectActivityStatus_ = fakeScene_.currentEffectActivityStatus();
}

void SceneManager::drawActiveScene() {
	activeScene().drawToCurrentTarget();
}

void SceneManager::deactivateScene() {
	activeScene().deactivate();
}

void SceneManager::shutdown() {
	activeScene().shutdown();
}

bool SceneManager::dispatchSceneCommand(SceneCommand command) {
	return activeScene().executeCommand(command);
}

void SceneManager::handleSceneSwitchCommand(RuntimeCommand command) {
	if (command != RuntimeCommand::NextScene && command != RuntimeCommand::PreviousScene) {
		ofLogWarning("SceneManager") << "handleSceneSwitchCommand() called with a non-switch "
			"RuntimeCommand — see ExperienceRuntime::handleRuntimeCommand for the intended split";
		return;
	}
	// Increment 1 has exactly one resident scene — real scene switching
	// (SceneTransitionPhase progression, load/fade, capability discovery
	// on the incoming scene) is Stage B scope, explicitly not built here.
	ofLogNotice("SceneManager") << "scene-switch command received ("
		<< (command == RuntimeCommand::NextScene ? "NextScene" : "PreviousScene")
		<< ") — not implemented in this increment; only one scene (\""
		<< activeScene().sceneId() << "\") is resident";
}

SceneManagerStatus SceneManager::status() const {
	SceneManagerStatus s;
	s.activeSceneId = hasBeenActivated_ ? activeScene().sceneId() : std::string();
	s.pendingSceneId = std::nullopt;
	s.transitionPhase = SceneTransitionPhase::Idle;
	s.transitionProgress = 0.0f;
	s.message = std::nullopt;
	return s;
}

glm::ivec2 SceneManager::activeSceneNativeRenderSize() const {
	return activeScene().nativeRenderSize();
}
